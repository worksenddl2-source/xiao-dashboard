// XIAO ESP32-S3 Web Dashboard
// - Web page served by the board itself (http://<board IP>/ or http://xiao.local/)
// - LED on/off from the web
// - Live touch sensor state on the web
// Touch pin: D0 (GPIO1, TOUCH1)
// USER LED: GPIO21 (active-low: LOW = on, HIGH = off)

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "secrets.h"

const int TOUCH_PIN = D0;
const float TOUCH_RATIO = 1.1;  // touched when value > baseline * 1.1

WebServer server(80);

uint32_t baseline = 0;
uint32_t touchValue = 0;
bool touched = false;
bool ledOn = false;

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>XIAO ESP32-S3</title>
<style>
  :root { --bg:#f4f5f7; --card:#fff; --text:#1d1f23; --sub:#6b7280; --line:#e5e7eb;
          --on:#f59e0b; --off:#d1d5db; --touch:#10b981; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#111317; --card:#1b1e24; --text:#e8eaed; --sub:#9aa0a6; --line:#2c3038; --off:#3a3f48; }
  }
  * { box-sizing:border-box; }
  body { margin:0; padding:24px 16px; background:var(--bg); color:var(--text);
         font-family:system-ui,-apple-system,"Segoe UI","Malgun Gothic",sans-serif; }
  .wrap { max-width:520px; margin:0 auto; }
  h1 { font-size:20px; margin:0 0 4px; }
  .meta { color:var(--sub); font-size:13px; margin-bottom:20px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:20px; margin-bottom:16px; }
  .card h2 { font-size:15px; margin:0 0 16px; color:var(--sub); font-weight:600; }
  .row { display:flex; align-items:center; gap:16px; }
  .dot { width:56px; height:56px; border-radius:50%; background:var(--off); transition:background .15s, box-shadow .15s; flex:none; }
  .dot.led.on { background:var(--on); box-shadow:0 0 24px var(--on); }
  .dot.touch.on { background:var(--touch); box-shadow:0 0 24px var(--touch); }
  .state { font-size:22px; font-weight:700; }
  .small { color:var(--sub); font-size:13px; margin-top:4px; }
  .btns { display:flex; gap:10px; margin-top:18px; }
  button { flex:1; padding:12px; font-size:15px; font-weight:600; border-radius:10px; border:1px solid var(--line);
           background:var(--card); color:var(--text); cursor:pointer; }
  button.primary { background:var(--on); border-color:var(--on); color:#1d1f23; }
  .offline { color:#ef4444; }
</style>
</head>
<body>
<div class="wrap">
  <h1>XIAO ESP32-S3 대시보드</h1>
  <div class="meta" id="meta">연결 중...</div>

  <div class="card">
    <h2>LED</h2>
    <div class="row">
      <div class="dot led" id="ledDot"></div>
      <div><div class="state" id="ledState">-</div></div>
    </div>
    <div class="btns">
      <button class="primary" onclick="setLed(1)">켜기</button>
      <button onclick="setLed(0)">끄기</button>
    </div>
  </div>

  <div class="card">
    <h2>터치 센서 (D0)</h2>
    <div class="row">
      <div class="dot touch" id="touchDot"></div>
      <div>
        <div class="state" id="touchState">-</div>
        <div class="small" id="touchValue"></div>
      </div>
    </div>
  </div>
</div>
<script>
function render(s) {
  document.getElementById('ledDot').classList.toggle('on', s.led);
  document.getElementById('ledState').textContent = s.led ? '켜짐' : '꺼짐';
  document.getElementById('touchDot').classList.toggle('on', s.touched);
  document.getElementById('touchState').textContent = s.touched ? '터치 중' : '터치 없음';
  document.getElementById('touchValue').textContent = '값 ' + s.value + ' / 기준 ' + s.baseline;
  document.getElementById('meta').textContent = 'IP ' + s.ip + ' · 신호 ' + s.rssi + ' dBm';
  document.getElementById('meta').classList.remove('offline');
}
async function poll() {
  try {
    const r = await fetch('/api/status', {cache:'no-store'});
    render(await r.json());
  } catch (e) {
    document.getElementById('meta').textContent = '보드와 연결이 끊겼습니다';
    document.getElementById('meta').classList.add('offline');
  }
  setTimeout(poll, 200);
}
async function setLed(on) {
  const r = await fetch('/api/led?on=' + on, {method:'POST'});
  render(await r.json());
}
poll();
</script>
</body>
</html>
)rawliteral";

void applyLed() {
  digitalWrite(LED_BUILTIN, ledOn ? LOW : HIGH);
}

String statusJson() {
  String s = "{";
  s += "\"led\":" + String(ledOn ? "true" : "false");
  s += ",\"touched\":" + String(touched ? "true" : "false");
  s += ",\"value\":" + String(touchValue);
  s += ",\"baseline\":" + String(baseline);
  s += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  s += ",\"rssi\":" + String(WiFi.RSSI());
  s += "}";
  return s;
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleStatus() {
  server.send(200, "application/json", statusJson());
}

void handleLed() {
  if (server.hasArg("on")) {
    ledOn = server.arg("on") == "1";
    applyLed();
  }
  server.send(200, "application/json", statusJson());
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

  if (MDNS.begin("xiao")) {
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/led", HTTP_POST, handleLed);
  server.begin();
}

void loop() {
  server.handleClient();

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

  // Print IP periodically so it can be found from the serial monitor
  static uint32_t lastLog = 0;
  if (millis() - lastLog >= 2000) {
    lastLog = millis();
    Serial.printf("IP=%s led=%d touched=%d value=%lu\n",
                  WiFi.localIP().toString().c_str(), ledOn, touched, touchValue);
  }
}
