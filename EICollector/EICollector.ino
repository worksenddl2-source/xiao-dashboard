// XIAO ESP32-S3 Sense image collector for Edge Impulse
// - http://<board IP>/ (or http://xiao.local/)  collector dashboard (see collector_page.h)
// - http://<board IP>/capture                    single JPEG (X-Width / X-Height headers)
// - http://<board IP>/api/status                 camera settings + Wi-Fi state as JSON
// - http://<board IP>/api/camera?var=..&val=..   change a camera setting
// - http://<board IP>:81/stream                  MJPEG stream (one viewer at a time)
// Images are stored in the browser (IndexedDB), not on the board, and downloaded as ZIP per class.
// USER LED (GPIO21, active-low) blinks on every capture.
// Build with PSRAM enabled: --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi

#include <WiFi.h>
#include <ESPmDNS.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "secrets.h"
#include "collector_page.h"

// XIAO ESP32-S3 Sense camera pins (from xiao.md)
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39
#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

#define PART_BOUNDARY "xiaoframeboundary"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Resolutions offered in the dashboard (buffers are sized for the largest one, VGA)
struct FrameSizeName {
  const char *name;
  framesize_t size;
};
static const FrameSizeName FRAME_SIZES[] = {
  { "96x96", FRAMESIZE_96X96 },
  { "160x120", FRAMESIZE_QQVGA },
  { "240x240", FRAMESIZE_240X240 },
  { "320x240", FRAMESIZE_QVGA },
  { "320x320", FRAMESIZE_320X320 },
  { "640x480", FRAMESIZE_VGA },
};

static httpd_handle_t pageHttpd = NULL;
static httpd_handle_t streamHttpd = NULL;
static bool cameraReady = false;
static volatile uint32_t ledOffAt = 0;  // capture blink, turned off by loop()

static const char *frameSizeName(framesize_t size) {
  for (const auto &f : FRAME_SIZES) {
    if (f.size == size) return f.name;
  }
  return "?";
}

static void allowCors(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Expose-Headers", "X-Width, X-Height");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
}

static esp_err_t sendStatus(httpd_req_t *req) {
  char json[320];
  if (cameraReady) {
    sensor_t *s = esp_camera_sensor_get();
    snprintf(json, sizeof(json),
             "{\"camera\":true,\"pid\":%u,\"framesize\":\"%s\",\"quality\":%d,\"brightness\":%d,"
             "\"contrast\":%d,\"vflip\":%d,\"hmirror\":%d,\"rssi\":%d,\"ip\":\"%s\",\"psram_free\":%u}",
             s->id.PID, frameSizeName(s->status.framesize), s->status.quality, s->status.brightness,
             s->status.contrast, s->status.vflip, s->status.hmirror, WiFi.RSSI(),
             WiFi.localIP().toString().c_str(), (unsigned)ESP.getFreePsram());
  } else {
    snprintf(json, sizeof(json), "{\"camera\":false,\"rssi\":%d,\"ip\":\"%s\"}", WiFi.RSSI(),
             WiFi.localIP().toString().c_str());
  }
  httpd_resp_set_type(req, "application/json");
  allowCors(req);
  return httpd_resp_sendstr(req, json);
}

static esp_err_t indexHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, COLLECTOR_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t statusHandler(httpd_req_t *req) {
  return sendStatus(req);
}

// /api/camera?var=framesize&val=240x240 | quality | brightness | contrast | vflip | hmirror
static esp_err_t cameraHandler(httpd_req_t *req) {
  char query[64], var[16], val[16];
  if (!cameraReady || httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "var", var, sizeof(var)) != ESP_OK ||
      httpd_query_key_value(query, "val", val, sizeof(val)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "need var and val");
    return ESP_FAIL;
  }

  sensor_t *s = esp_camera_sensor_get();
  int v = atoi(val);
  int res = -1;
  if (!strcmp(var, "framesize")) {
    for (const auto &f : FRAME_SIZES) {
      if (!strcmp(f.name, val)) res = s->set_framesize(s, f.size);
    }
  } else if (!strcmp(var, "quality")) {
    res = s->set_quality(s, constrain(v, 4, 63));
  } else if (!strcmp(var, "brightness")) {
    res = s->set_brightness(s, constrain(v, -2, 2));
  } else if (!strcmp(var, "contrast")) {
    res = s->set_contrast(s, constrain(v, -2, 2));
  } else if (!strcmp(var, "vflip")) {
    res = s->set_vflip(s, v ? 1 : 0);
  } else if (!strcmp(var, "hmirror")) {
    res = s->set_hmirror(s, v ? 1 : 0);
  }
  if (res != 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "setting not supported");
    return ESP_FAIL;
  }
  Serial.printf("Camera: %s = %s\n", var, val);
  return sendStatus(req);
}

static esp_err_t captureHandler(httpd_req_t *req) {
  camera_fb_t *fb = cameraReady ? esp_camera_fb_get() : NULL;
  if (!fb) {
    allowCors(req);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  digitalWrite(LED_BUILTIN, LOW);
  ledOffAt = millis() + 60;

  char w[8], h[8];
  snprintf(w, sizeof(w), "%u", fb->width);
  snprintf(h, sizeof(h), "%u", fb->height);
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "X-Width", w);
  httpd_resp_set_hdr(req, "X-Height", h);
  allowCors(req);
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

static esp_err_t streamHandler(httpd_req_t *req) {
  if (!cameraReady) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char part[64];
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
      break;
    }
    size_t hlen = snprintf(part, sizeof(part), STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, part, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;  // viewer closed the page
  }
  return res;
}

bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;  // allocate for the largest size, then switch down below
  config.jpeg_quality = 10;           // lower = better quality, bigger files
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (!psramFound()) {
    Serial.println("Camera: PSRAM not found (compile with PSRAM=opi)");
    return false;
  }
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera: init failed 0x%x (is the Sense board attached?)\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  Serial.printf("Camera: sensor PID 0x%x ready\n", s->id.PID);
  // OV3660 / OV5640 image comes out flipped on this board
  if (s->id.PID == OV3660_PID || s->id.PID == OV5640_PID) {
    s->set_vflip(s, 1);
  }
  // Square images suit Edge Impulse image models (they are resized to 96x96 etc. anyway)
  s->set_framesize(s, FRAMESIZE_240X240);
  return true;
}

void startServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;

  httpd_uri_t indexUri = { "/", HTTP_GET, indexHandler, NULL };
  httpd_uri_t statusUri = { "/api/status", HTTP_GET, statusHandler, NULL };
  httpd_uri_t cameraUri = { "/api/camera", HTTP_GET, cameraHandler, NULL };
  httpd_uri_t captureUri = { "/capture", HTTP_GET, captureHandler, NULL };
  if (httpd_start(&pageHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(pageHttpd, &indexUri);
    httpd_register_uri_handler(pageHttpd, &statusUri);
    httpd_register_uri_handler(pageHttpd, &cameraUri);
    httpd_register_uri_handler(pageHttpd, &captureUri);
  }

  // Stream runs on its own server so captures stay responsive while streaming
  config.server_port = 81;
  config.ctrl_port = 32769;
  httpd_uri_t streamUri = { "/stream", HTTP_GET, streamHandler, NULL };
  if (httpd_start(&streamHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(streamHttpd, &streamUri);
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // keeps the stream and bursts smooth
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Collector: http://");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // active-low: off

  cameraReady = initCamera();
  connectWiFi();
  startServer();
  if (MDNS.begin("xiao")) {
    MDNS.addService("http", "tcp", 80);
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost, reconnecting");
    connectWiFi();
  }
  if (ledOffAt && (int32_t)(millis() - ledOffAt) >= 0) {
    ledOffAt = 0;
    digitalWrite(LED_BUILTIN, HIGH);
  }

  // Print the address now and then so it can be found from the serial monitor
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 10000) {
    lastPrint = millis();
    Serial.printf("Collector: http://%s  (http://xiao.local)\n", WiFi.localIP().toString().c_str());
  }
  delay(10);
}
