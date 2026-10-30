// XIAO ESP32-S3 BLE control (no Wi-Fi)
// Device name: XIAO_ESP32S3
// - Touch characteristic (READ, NOTIFY): text "value,touched,baseline" e.g. "17590,0,17580"
//   notified every 200 ms while a phone is connected
// - LED characteristic (READ, WRITE, NOTIFY): write "1"/"0" (text) or 0x01/0x00 (byte)
// Works with nRF Connect or the Web Bluetooth page (web/ble.html).
// Touch pin: D0 (GPIO1, TOUCH1)
// USER LED: GPIO21 (active-low: LOW = on, HIGH = off)
// Note: ESP32-S3 has BLE only (no Bluetooth Classic). Attach the antenna.

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define DEVICE_NAME  "XIAO_ESP32S3"
#define SERVICE_UUID "7f1c0001-5a3e-4c8b-9d2a-1b6e4f0a9c11"
#define TOUCH_UUID   "7f1c0002-5a3e-4c8b-9d2a-1b6e4f0a9c11"
#define LED_UUID     "7f1c0003-5a3e-4c8b-9d2a-1b6e4f0a9c11"

const int TOUCH_PIN = D0;
const float TOUCH_RATIO = 1.1;  // touched when value > baseline * 1.1
const uint32_t NOTIFY_MS = 200;

BLEServer *pServer;
BLECharacteristic *touchChar;
BLECharacteristic *ledChar;

uint32_t baseline = 0;
uint32_t touchValue = 0;
bool touched = false;
volatile bool ledOn = false;
volatile bool connected = false;
volatile bool restartAdvertising = false;

void applyLed() {
  digitalWrite(LED_BUILTIN, ledOn ? LOW : HIGH);
  ledChar->setValue(ledOn ? "1" : "0");
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) {
    connected = true;
    Serial.println("Phone connected");
  }
  void onDisconnect(BLEServer *server) {
    connected = false;
    restartAdvertising = true;  // advertising stops on connect, so start it again
    Serial.println("Phone disconnected");
  }
};

class LedCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) {
    String v = c->getValue();
    if (v.length() == 0) return;
    // Accept "1"/"0", "on"/"off" text or a 0x01/0x00 byte
    char first = v[0];
    ledOn = first == '1' || first == 0x01 || v.equalsIgnoreCase("on");
    applyLed();
    ledChar->notify();
    Serial.printf("LED -> %s\n", ledOn ? "ON" : "OFF");
  }
};

void addDescription(BLECharacteristic *c, const char *text) {
  BLEDescriptor *d = new BLEDescriptor(BLEUUID((uint16_t)0x2901));  // user description (shown in nRF Connect)
  d->setValue(text);
  c->addDescriptor(d);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // LED off

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

  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  BLEService *service = pServer->createService(SERVICE_UUID);

  touchChar = service->createCharacteristic(
    TOUCH_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  touchChar->addDescriptor(new BLE2902());
  addDescription(touchChar, "Touch value,touched,baseline");

  ledChar = service->createCharacteristic(
    LED_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  ledChar->setCallbacks(new LedCallbacks());
  ledChar->addDescriptor(new BLE2902());
  addDescription(ledChar, "LED write 1=on 0=off");

  applyLed();
  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);  // helps with iPhone connections
  adv->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE advertising as " DEVICE_NAME);
}

void loop() {
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

  static uint32_t lastNotify = 0;
  static bool lastTouched = false;
  if (millis() - lastNotify >= NOTIFY_MS || touched != lastTouched) {
    lastNotify = millis();
    lastTouched = touched;
    char buf[40];
    snprintf(buf, sizeof(buf), "%lu,%d,%lu", (unsigned long)touchValue, touched ? 1 : 0, (unsigned long)baseline);
    touchChar->setValue(buf);
    if (connected) touchChar->notify();
  }

  if (restartAdvertising) {
    restartAdvertising = false;
    delay(300);  // give the stack time to finish the disconnect
    BLEDevice::startAdvertising();
    Serial.println("Advertising again");
  }
}
