// LAN dashboard for XIAO ESP32-S3 Sense (same Wi-Fi only, works without internet)
// - http://<board IP>/            dashboard: camera + LED + touch
// - http://<board IP>/api/status  JSON state
// - http://<board IP>/api/led?on=1|0 (POST) set LED, synced to Supabase by the main loop
// - http://<board IP>/capture     single JPEG
// - http://<board IP>:81/stream   MJPEG stream (one viewer at a time)
// Camera needs PSRAM enabled (FQBN option PSRAM=opi).
#pragma once

#include "esp_camera.h"
#include "esp_http_server.h"
#include "device_state.h"

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

static httpd_handle_t pageHttpd = NULL;
static httpd_handle_t streamHttpd = NULL;
static bool cameraReady = false;

static const char DASHBOARD_HTML[] = R"rawliteral(
<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>XIAO 내부망 패널</title>
<style>
  :root { --bg:#eef1f0; --panel:#fff; --fg:#17201d; --sub:#5e6b66; --line:#d5dcd9; --pcb:#1f6f54;
          --led:#f29d1f; --touch:#1f9c74; --off:#c9d1ce; --bad:#c8412f; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#0f1513; --panel:#18201d; --fg:#e4ebe8; --sub:#93a39d; --line:#2a3531; --pcb:#4fbf95;
            --led:#f6ae40; --touch:#3cc495; --off:#34403c; --bad:#ef6a57; }
  }
  * { box-sizing:border-box; }
  body { margin:0; padding:24px 16px; background:var(--bg); color:var(--fg);
         font-family:"Malgun Gothic",system-ui,sans-serif; font-size:15px; }
  .wrap { max-width:720px; margin:0 auto; display:flex; flex-direction:column; gap:14px; }
  h1 { font-size:20px; margin:0; }
  .sub { color:var(--sub); font-size:13px; font-family:ui-monospace,Consolas,monospace; }
  .panel { background:var(--panel); border:1px solid var(--line); border-radius:12px; padding:16px;
           display:flex; flex-direction:column; gap:12px; }
  .label { font-size:12px; letter-spacing:.06em; text-transform:uppercase; color:var(--sub); font-family:ui-monospace,Consolas,monospace; }
  .frame { background:#000; border-radius:8px; overflow:hidden; aspect-ratio:4/3; display:flex; align-items:center; justify-content:center; }
  .frame img { width:100%; height:100%; object-fit:contain; display:block; }
  .msg { color:#fff; font-size:14px; padding:16px; text-align:center; }
  .grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(240px,1fr)); gap:14px; }
  .row { display:flex; align-items:center; gap:14px; }
  .lamp { width:46px; height:46px; border-radius:50%; background:var(--off); flex:none; transition:background .15s, box-shadow .15s; }
  .lamp.led.on { background:var(--led); box-shadow:0 0 20px var(--led); }
  .lamp.touch.on { background:var(--touch); box-shadow:0 0 20px var(--touch); }
  .state { font-size:20px; font-weight:700; }
  .detail { font-size:12.5px; color:var(--sub); font-family:ui-monospace,Consolas,monospace; font-variant-numeric:tabular-nums; }
  .btns { display:flex; gap:10px; flex-wrap:wrap; }
  .btns > * { flex:1; }
  a.btn, button { font:inherit; font-weight:600; padding:10px 14px; border-radius:9px; border:1px solid var(--line);
                  background:var(--bg); color:var(--fg); text-decoration:none; cursor:pointer; text-align:center; }
  .primary { background:var(--pcb) !important; border-color:var(--pcb) !important; color:var(--panel) !important; }
  .err { color:var(--bad); }
</style>
</head>
<body>
<div class="wrap">
  <h1>XIAO ESP32-S3 내부망 패널</h1>
  <div class="sub" id="meta">같은 와이파이 전용 · 인터넷 없이도 동작</div>

  <section class="panel">
    <div class="label">카메라</div>
    <div class="frame" id="frame"><div class="msg">스트림 연결 중...</div></div>
    <div class="btns">
      <button type="button" id="toggle">일시정지</button>
      <a class="btn" href="/capture" target="_blank">스냅샷</a>
    </div>
  </section>

  <div class="grid">
    <section class="panel">
      <div class="label">User LED · GPIO21</div>
      <div class="row"><div class="lamp led" id="ledLamp"></div><div class="state" id="ledState">-</div></div>
      <div class="btns">
        <button type="button" class="primary" onclick="setLed(1)">켜기</button>
        <button type="button" onclick="setLed(0)">끄기</button>
      </div>
    </section>
    <section class="panel">
      <div class="label">터치 센서 · D0</div>
      <div class="row">
        <div class="lamp touch" id="touchLamp"></div>
        <div><div class="state" id="touchState">-</div><div class="detail" id="touchDetail"></div></div>
      </div>
    </section>
  </div>

  <a class="btn" href="https://worksenddl2-source.github.io/xiao-dashboard/">원격 대시보드 (외부망)</a>
</div>
<script>
const $ = id => document.getElementById(id);

// Camera stream
const frame = $('frame'), toggle = $('toggle');
const streamUrl = 'http://' + location.hostname + ':81/stream';
let img = null;
function start() {
  img = document.createElement('img');
  img.alt = '카메라 영상';
  img.onerror = () => { frame.innerHTML = '<div class="msg err">스트림을 열 수 없습니다. 다른 사람이 보고 있거나 카메라가 연결되지 않았습니다.</div>'; img = null; toggle.textContent = '다시 연결'; };
  img.src = streamUrl + '?t=' + Date.now();
  frame.replaceChildren(img);
  toggle.textContent = '일시정지';
}
function stop() {
  if (img) { img.src = ''; img = null; }
  frame.innerHTML = '<div class="msg">일시정지됨</div>';
  toggle.textContent = '재생';
}
toggle.onclick = () => img ? stop() : start();

// LED + touch
function render(s) {
  $('ledLamp').classList.toggle('on', s.led);
  $('ledState').textContent = s.led ? '켜짐' : '꺼짐';
  $('touchLamp').classList.toggle('on', s.touched);
  $('touchState').textContent = s.touched ? '터치 중' : '터치 없음';
  $('touchDetail').textContent = '값 ' + s.value + ' / 기준 ' + s.baseline;
  $('meta').textContent = '같은 와이파이 전용 · ' + location.hostname + ' · Wi-Fi ' + s.rssi + ' dBm';
  $('meta').classList.remove('err');
  if (!s.camera && !img) frame.innerHTML = '<div class="msg">카메라를 찾지 못했습니다. Sense 확장보드가 꽂혀 있는지 확인하세요.</div>';
}
async function poll() {
  try {
    const r = await fetch('/api/status', { cache: 'no-store' });
    render(await r.json());
  } catch (e) {
    $('meta').textContent = '보드와 연결이 끊겼습니다';
    $('meta').classList.add('err');
  }
  setTimeout(poll, 250);
}
async function setLed(on) {
  const r = await fetch('/api/led?on=' + on, { method: 'POST' });
  render(await r.json());
}

start();
poll();
</script>
</body>
</html>
)rawliteral";

static esp_err_t sendStatus(httpd_req_t *req) {
  char json[160];
  snprintf(json, sizeof(json),
           "{\"led\":%s,\"touched\":%s,\"value\":%lu,\"baseline\":%lu,\"rssi\":%d,\"camera\":%s}",
           ledOn ? "true" : "false", touched ? "true" : "false",
           (unsigned long)touchValue, (unsigned long)baseline, WiFi.RSSI(),
           cameraReady ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_sendstr(req, json);
}

static esp_err_t indexHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, DASHBOARD_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t statusHandler(httpd_req_t *req) {
  return sendStatus(req);
}

static esp_err_t ledHandler(httpd_req_t *req) {
  char query[16], value[4];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
      httpd_query_key_value(query, "on", value, sizeof(value)) == ESP_OK) {
    ledOn = value[0] == '1';
    applyLed();
    pendingLed = ledOn ? 1 : 0;  // main loop writes this to Supabase
  }
  return sendStatus(req);
}

static esp_err_t captureHandler(httpd_req_t *req) {
  camera_fb_t *fb = cameraReady ? esp_camera_fb_get() : NULL;
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=xiao.jpg");
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
  config.frame_size = FRAMESIZE_VGA;  // 640x480
  config.jpeg_quality = 12;           // lower = better quality, bigger frames
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
  return true;
}

void startLanServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;

  httpd_uri_t indexUri = { "/", HTTP_GET, indexHandler, NULL };
  httpd_uri_t statusUri = { "/api/status", HTTP_GET, statusHandler, NULL };
  httpd_uri_t ledUri = { "/api/led", HTTP_POST, ledHandler, NULL };
  httpd_uri_t captureUri = { "/capture", HTTP_GET, captureHandler, NULL };
  if (httpd_start(&pageHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(pageHttpd, &indexUri);
    httpd_register_uri_handler(pageHttpd, &statusUri);
    httpd_register_uri_handler(pageHttpd, &ledUri);
    httpd_register_uri_handler(pageHttpd, &captureUri);
  }

  // Stream runs on its own server so the dashboard stays responsive while streaming
  config.server_port = 81;
  config.ctrl_port = 32769;
  httpd_uri_t streamUri = { "/stream", HTTP_GET, streamHandler, NULL };
  if (httpd_start(&streamHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(streamHttpd, &streamUri);
  }
}
