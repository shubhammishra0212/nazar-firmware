/*
  NAZAR bring-up 06 - FC-51 IR part sensor (OUT -> GPIO18, LOW = something in front)
  Wiring: VCC 3V3 (or 5 V with OUT through a divider), GND, OUT 18.
  Prints every change (20 ms debounce, like the main firmware) and counts parts.
  Test: hold a washer 1-2 cm in front - "PART" and the board's green LED. Too far or
  always on: turn the blue potentiometer.
*/
const int PIN_IR = 18;
bool raw = false, stable = false;
uint32_t changedAt = 0, parts = 0, lastAlive = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(PIN_IR, INPUT_PULLUP);
  Serial.println("\nNAZAR 06 FC-51 IR test (GPIO18)");
  Serial.println(digitalRead(PIN_IR) == LOW ? "sensor sees something now (if nothing is there, turn the pot)"
                                            : "sensor clear - now pass a washer in front of it");
}

void loop() {
  uint32_t now = millis();
  bool present = digitalRead(PIN_IR) == LOW;
  if (present != raw) {
    raw = present;
    changedAt = now;
  }
  if (raw != stable && now - changedAt >= 20) {
    stable = raw;
    if (stable) Serial.printf("PART %lu seen\n", (unsigned long)++parts);
    else Serial.println("clear");
  }
  if (now - lastAlive >= 5000) {
    lastAlive = now;
    Serial.printf("(alive) ir=%d parts=%lu\n", stable ? 1 : 0, (unsigned long)parts);
  }
}
