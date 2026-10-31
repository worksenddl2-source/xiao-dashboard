// XIAO ESP32-S3 HTTP Dashboard
// - Reports the touch sensor to the FastAPI backend over HTTPS (POST /device/report)
// - The response carries the LED state set by logged-in users -> works from any network
// Touch pin: D0 (GPIO1, TOUCH1)
// USER LED: GPIO21 (active-low: LOW = on, HIGH = off)

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "secrets.h"

const int TOUCH_PIN = D0;
const float TOUCH_RATIO = 1.1;  // touched when value > baseline * 1.1
const unsigned long REPORT_MS = 1000;

WiFiClientSecure tls;
HTTPClient http;

uint32_t baseline = 0;
uint32_t touchValue = 0;
bool touched = false;
bool ledOn = false;
unsigned long lastReport = 0;

void setLed(bool on) {
  if (on != ledOn) Serial.printf("LED -> %s\n", on ? "on" : "off");
  ledOn = on;
  digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
}

void report() {
  char body[128];
  snprintf(body, sizeof(body), "{\"value\":%lu,\"baseline\":%lu,\"touched\":%s,\"rssi\":%d}",
           touchValue, baseline, touched ? "true" : "false", WiFi.RSSI());

  http.begin(tls, String(API_URL) + "/device/report");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_KEY);
  int code = http.POST(body);

  if (code == 200) {
    // Response is {"led":true} or {"led":false}
    setLed(http.getString().indexOf("true") >= 0);
  } else {
    // -1 etc. = network error; 502/503 = server waking up (Render free plan sleeps)
    Serial.printf("report failed: %d %s\n", code, code > 0 ? http.getString().c_str() : "");
  }
  http.end();
}

void calibrateTouch() {
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
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // LED off

  calibrateTouch();
  connectWiFi();

  // Encrypted, but the server certificate is not verified (fine for a class project)
  tls.setInsecure();
  http.setReuse(true);  // keep the TLS connection open between reports
  http.setTimeout(15000);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  touchValue = touchRead(TOUCH_PIN);
  bool nowTouched = touchValue > baseline * TOUCH_RATIO;

  // Slowly track baseline while untouched (snap down if it is too high)
  if (!nowTouched) {
    baseline = (touchValue < baseline) ? touchValue : (baseline * 15 + touchValue) / 16;
  }

  // Report right away when the touch state changes, otherwise every second
  bool changed = nowTouched != touched;
  touched = nowTouched;
  if (changed || millis() - lastReport >= REPORT_MS) {
    lastReport = millis();
    report();
  }

  delay(20);
}
