// XIAO ESP32-S3 MQTT Dashboard
// - Connects to a local Mosquitto broker (see mosquitto.conf / start-broker.bat)
// - Publishes the touch sensor, LED is controlled over MQTT
// Touch pin: D0 (GPIO1, TOUCH1)
// USER LED: GPIO21 (active-low: LOW = on, HIGH = off)
//
// Topics:
//   xiao/status    "online" / "offline"  (retained, offline = last will)
//   xiao/touch     {"value":17590,"baseline":17580,"touched":false}
//   xiao/led       "on" / "off"          (retained, current LED state)
//   xiao/led/set   "on" / "off" / "toggle"  <- send this to control the LED

#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

const int TOUCH_PIN = D0;
const float TOUCH_RATIO = 1.1;  // touched when value > baseline * 1.1
const unsigned long TOUCH_PUBLISH_MS = 200;

const char* TOPIC_STATUS = "xiao/status";
const char* TOPIC_TOUCH = "xiao/touch";
const char* TOPIC_LED = "xiao/led";
const char* TOPIC_LED_SET = "xiao/led/set";

WiFiClient espClient;
PubSubClient client(espClient);
String clientId;

uint32_t baseline = 0;
uint32_t touchValue = 0;
bool touched = false;
bool ledOn = false;
unsigned long lastTouchPublish = 0;
unsigned long lastReconnect = 0;

void publishLed() {
  client.publish(TOPIC_LED, ledOn ? "on" : "off", true);
}

void publishTouch() {
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"value\":%lu,\"baseline\":%lu,\"touched\":%s}",
           touchValue, baseline, touched ? "true" : "false");
  client.publish(TOPIC_TOUCH, buf);
}

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
  publishLed();
}

void onMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  msg.toLowerCase();
  Serial.printf("[MQTT] %s <- %s\n", topic, msg.c_str());

  if (strcmp(topic, TOPIC_LED_SET) == 0) {
    if (msg == "on" || msg == "1") setLed(true);
    else if (msg == "off" || msg == "0") setLed(false);
    else if (msg == "toggle") setLed(!ledOn);
  }
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

bool connectMqtt() {
  Serial.printf("Connecting to MQTT %s:%d ... ", MQTT_HOST, MQTT_PORT);
  // Last will: the broker publishes "offline" if the board disappears
  if (client.connect(clientId.c_str(), TOPIC_STATUS, 1, true, "offline")) {
    Serial.println("connected");
    client.publish(TOPIC_STATUS, "online", true);
    client.subscribe(TOPIC_LED_SET);
    publishLed();
    return true;
  }
  Serial.printf("failed, rc=%d\n", client.state());
  return false;
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

  clientId = "xiao-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(onMessage);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  if (!client.connected()) {
    // Retry every 3 s without blocking the touch reading
    if (millis() - lastReconnect > 3000) {
      lastReconnect = millis();
      connectMqtt();
    }
  }
  client.loop();

  touchValue = touchRead(TOUCH_PIN);
  bool nowTouched = touchValue > baseline * TOUCH_RATIO;

  // Slowly track baseline while untouched (snap down if it is too high)
  if (!nowTouched) {
    baseline = (touchValue < baseline) ? touchValue : (baseline * 15 + touchValue) / 16;
  }

  // Publish right away when the touch state changes, otherwise every 200 ms
  bool changed = nowTouched != touched;
  touched = nowTouched;
  if (client.connected() && (changed || millis() - lastTouchPublish >= TOUCH_PUBLISH_MS)) {
    lastTouchPublish = millis();
    publishTouch();
  }

  delay(20);
}
