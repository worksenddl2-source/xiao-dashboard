// XIAO ESP32-S3 Blink
// USER LED: GPIO21 (active-low: LOW = on, HIGH = off)

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);   // LED on
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);  // LED off
  delay(100);
}
