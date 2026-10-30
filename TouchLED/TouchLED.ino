// XIAO ESP32-S3 Touch LED
// Touch pin: D0 (GPIO1, TOUCH1)
// USER LED: GPIO21 (active-low: LOW = on, HIGH = off)
// On ESP32-S3, touchRead() value RISES when touched.

const int TOUCH_PIN = D0;
const float TOUCH_RATIO = 1.1;  // touched when value > baseline * 1.1

uint32_t baseline = 0;

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
}

void loop() {
  uint32_t value = touchRead(TOUCH_PIN);
  bool touched = value > baseline * TOUCH_RATIO;

  digitalWrite(LED_BUILTIN, touched ? LOW : HIGH);

  // Slowly track baseline while untouched (snap down if it is too high)
  if (!touched) {
    baseline = (value < baseline) ? value : (baseline * 15 + value) / 16;
  }

  Serial.printf("baseline=%lu value=%lu touched=%d\n", baseline, value, touched);
  delay(50);
}
