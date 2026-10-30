// XIAO ESP32-S3 AP mode dashboard (no internet needed)
// - The board creates its own Wi-Fi: SSID "XIAO-ESP32S3", password "xiao1234"
// - Connect a phone to it and open http://192.168.4.1/
// - LED on/off from the web, live touch sensor value
// Touch pin: D0 (GPIO1, TOUCH1)
// USER LED: GPIO21 (active-low: LOW = on, HIGH = off)

#include <WiFi.h>
#include <WebServer.h>

const char *AP_SSID = "XIAO-ESP32S3";
const char *AP_PASS = "xiao1234";  // at least 8 characters

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
<title>XIAO AP 패널</title>
<style>
  :root { --bg:#eef1f0; --panel:#fff; --fg:#17201d; --sub:#5e6b66; --line:#d5dcd9; --pcb:#1f6f54;
          --led:#f29d1f; --touch:#1f9c74; --off:#c9d1ce; --bad:#c8412f; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#0f1513; --panel:#18201d; --fg:#e4ebe8; --sub:#93a39d; --line:#2a3531; --pcb:#4fbf95;
            --led:#f6ae40; --touch:#3cc495; --off:#34403c; --bad:#ef6a57; }
  }
  * { box-sizing:border-box; }
  body { margin:0; padding:24px 16px; background:var(--bg); color:var(--fg);
         font-family:system-ui,-apple-system,"Malgun Gothic",sans-serif; font-size:15px; }
  .wrap { max-width:480px; margin:0 auto; display:flex; flex-direction:column; gap:14px; }
  h1 { font-size:20px; margin:0; }
  .sub { color:var(--sub); font-size:13px; font-family:ui-monospace,Consolas,monospace; }
  .panel { background:var(--panel); border:1px solid var(--line); border-radius:12px; padding:18px;
           display:flex; flex-direction:column; gap:14px; }
  .label { font-size:12px; letter-spacing:.06em; text-transform:uppercase; color:var(--sub); font-family:ui-monospace,Consolas,monospace; }
  .row { display:flex; align-items:center; gap:16px; }
  .lamp { width:54px; height:54px; border-radius:50%; background:var(--off); flex:none; transition:background .15s, box-shadow .15s; }
  .lamp.led.on { background:var(--led); box-shadow:0 0 22px var(--led); }
  .lamp.touch.on { background:var(--touch); box-shadow:0 0 22px var(--touch); }
  .state { font-size:22px; font-weight:700; }
  .detail { font-size:12.5px; color:var(--sub); font-family:ui-monospace,Consolas,monospace; font-variant-numeric:tabular-nums; }
  .value { font-size:34px; font-weight:700; font-variant-numeric:tabular-nums; font-family:ui-monospace,Consolas,monospace; }
  .meter { height:8px; border-radius:4px; background:var(--off); overflow:hidden; }
  .meter span { display:block; height:100%; width:0; background:var(--touch); transition:width .15s; }
  .btns { display:grid; grid-template-columns:1fr 1fr; gap:10px; }
  button { font:inherit; font-weight:600; padding:14px; border-radius:10px; border:1px solid var(--line);
           background:var(--bg); color:var(--fg); cursor:pointer; }
  button.primary { background:var(--pcb); border-color:var(--pcb); color:var(--panel); }
  .err { color:var(--bad); }
  @media (prefers-reduced-motion: reduce) { * { transition:none !important; } }
</style>
</head>
<body>
<div class="wrap">
  <h1>XIAO ESP32-S3 AP 패널</h1>
  <div class="sub" id="meta">Wi-Fi XIAO-ESP32S3 · 192.168.4.1 · 인터넷 없음</div>

  <section class="panel">
    <div class="label">User LED · GPIO21</div>
    <div class="row"><div class="lamp led" id="ledLamp"></div><div class="state" id="ledState">-</div></div>
    <div class="btns">
      <button type="button" class="primary" onclick="setLed(1)">켜기</button>
      <button type="button" onclick="setLed(0)">끄기</button>
    </div>
  </section>

  <section class="panel">
    <div class="label">터치 센서 · D0 (TOUCH1)</div>
    <div class="row">
      <div class="lamp touch" id="touchLamp"></div>
      <div><div class="state" id="touchState">-</div><div class="detail" id="baseline"></div></div>
    </div>
    <div class="value" id="touchValue">-</div>
    <div class="meter" aria-hidden="true"><span id="touchMeter"></span></div>
  </section>
</div>
<script>
const $ = id => document.getElementById(id);
function render(s) {
  $('ledLamp').classList.toggle('on', s.led);
  $('ledState').textContent = s.led ? '켜짐' : '꺼짐';
  $('touchLamp').classList.toggle('on', s.touched);
  $('touchState').textContent = s.touched ? '터치 중' : '터치 없음';
  $('touchValue').textContent = s.value;
  $('baseline').textContent = '기준 ' + s.baseline + ' · 터치 판정 ' + Math.round(s.baseline * 1.1) + ' 초과';
  // 0% at baseline, 100% at 1.5x baseline
  const pct = Math.min(100, Math.max(0, (s.value / s.baseline - 1) / 0.5 * 100));
  $('touchMeter').style.width = pct + '%';
  $('meta').textContent = 'Wi-Fi XIAO-ESP32S3 · 192.168.4.1 · 접속 기기 ' + s.clients + '대';
  $('meta').classList.remove('err');
}
async function poll() {
  try {
    const r = await fetch('/api/status', { cache: 'no-store' });
    render(await r.json());
  } catch (e) {
    $('meta').textContent = '보드와 연결이 끊겼습니다. 와이파이 XIAO-ESP32S3에 연결돼 있는지 확인하세요.';
    $('meta').classList.add('err');
  }
  setTimeout(poll, 200);
}
async function setLed(on) {
  const r = await fetch('/api/led?on=' + on, { method: 'POST' });
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
  s += ",\"clients\":" + String(WiFi.softAPgetStationNum());
  s += "}";
  return s;
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleStatus() {
  server.sendHeader("Cache-Control", "no-store");
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

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP started: ");
  Serial.print(AP_SSID);
  Serial.print("  IP: ");
  Serial.println(WiFi.softAPIP());

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

  static uint32_t lastLog = 0;
  if (millis() - lastLog >= 2000) {
    lastLog = millis();
    Serial.printf("AP %s  IP=%s  clients=%d  led=%d  touched=%d  value=%lu\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum(),
                  ledOn, touched, touchValue);
  }
}
