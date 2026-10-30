// XIAO ESP32-S3 <-> Supabase
// - Reads LED command from table public.xiao_device (id = 'xiao')
// - Reports touch state to the same row
// - Serves a LAN dashboard with webcam, LED and touch (see lan_server.h) and reports its IP to the same row
// The web dashboard talks to Supabase, so it works from outside the LAN.
// Touch pin: D0 (GPIO1, TOUCH1)
// USER LED: GPIO21 (active-low: LOW = on, HIGH = off)
// Build with PSRAM enabled: --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include "secrets.h"
#include "lan_server.h"

const int TOUCH_PIN = D0;
const float TOUCH_RATIO = 1.1;  // touched when value > baseline * 1.1

const char *ROW_URL = SUPABASE_URL "/rest/v1/xiao_device?id=eq.xiao&select=led";

const uint32_t POLL_MS = 500;        // how often to read the LED command
const uint32_t HEARTBEAT_MS = 5000;  // report state even if nothing changed

WiFiClientSecure tls;
HTTPClient http;

bool reportedTouched = false;
uint32_t lastReport = 0;
uint32_t lastPoll = 0;

// Parse "led":true/false from the response body
void readLed(const String &body) {
  if (pendingLed != -1) return;  // LAN page change not synced yet, keep it
  int i = body.indexOf("\"led\":");
  if (i < 0) return;
  bool on = body.startsWith("true", i + 6);
  if (on != ledOn) {
    ledOn = on;
    applyLed();
    Serial.printf("LED -> %s\n", ledOn ? "ON" : "OFF");
  }
}

// GET (body empty) reads the LED; PATCH (body set) reports state and returns the LED too
bool request(const char *method, const String &body) {
  http.begin(tls, ROW_URL);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Content-Type", "application/json");
  if (body.length()) http.addHeader("Prefer", "return=representation");

  int code = http.sendRequest(method, body);
  bool ok = code == 200;
  if (ok) {
    readLed(http.getString());
  } else {
    Serial.printf("%s failed: %d %s\n", method, code, http.errorToString(code).c_str());
  }
  http.end();
  return ok;
}

void report() {
  String body = "{\"touched\":" + String(touched ? "true" : "false") +
                ",\"touch_value\":" + String(touchValue) +
                ",\"baseline\":" + String(baseline) +
                ",\"rssi\":" + String(WiFi.RSSI()) +
                ",\"local_ip\":\"" + WiFi.localIP().toString() + "\"" +
                ",\"camera_ok\":" + String(cameraReady ? "true" : "false") + "}";
  if (request("PATCH", body)) {
    reportedTouched = touched;
  }
  lastReport = millis();
  lastPoll = millis();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  applyLed();

  // Measure untouched baseline (do not touch during boot)
  // First readings after init are unstable, so discard them
  delay(500);
  for (int i = 0; i < 50; i++) {
    touchRead(TOUCH_PIN);
    delay(10);
  }
  uint64_t sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += touchRead(TOUCH_PIN);
    delay(10);
  }
  baseline = sum / 20;

  connectWiFi();

  cameraReady = initCamera();
  startLanServer();
  if (MDNS.begin("xiao")) {
    MDNS.addService("http", "tcp", 80);
  }

  tls.setInsecure();  // skip certificate check (simple setup; traffic is still encrypted)
  http.setReuse(true);  // keep the HTTPS connection open between requests
  http.setTimeout(5000);

  report();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost, reconnecting");
    connectWiFi();
  }

  static uint32_t lastRead = 0;
  if (millis() - lastRead >= 50) {
    lastRead = millis();
    touchValue = touchRead(TOUCH_PIN);
    touched = touchValue > baseline * TOUCH_RATIO;

    // Slowly track baseline while untouched (snap down if it is too high)
    if (!touched) {
      baseline = (touchValue < baseline) ? touchValue : (baseline * 15 + touchValue) / 16;
    }
  }

  if (pendingLed != -1) {
    // Push LED change made on the LAN page so the remote dashboard sees it
    int want = pendingLed;
    pendingLed = -1;
    if (!request("PATCH", want ? "{\"led\":true}" : "{\"led\":false}") && pendingLed == -1) {
      pendingLed = want;  // retry next loop (e.g. internet down)
    }
    lastPoll = millis();
  } else if (touched != reportedTouched || millis() - lastReport >= HEARTBEAT_MS) {
    report();
  } else if (millis() - lastPoll >= POLL_MS) {
    lastPoll = millis();
    request("GET", "");
  }
}
