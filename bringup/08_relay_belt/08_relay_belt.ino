/*
  NAZAR bring-up 08 - belt relay (IN -> GPIO26, module VCC on 5.5 V, active LOW)
  Commands: 1 = belt on, 0 = belt off (Newline). Starts OFF.
  ON  = pin OUTPUT LOW  -> relay clicks, its LED lights, 5.5 V reaches the motor.
  OFF = pin INPUT (floating) -> the module's own pull-up turns the relay off.
        (Writing 3.3 V HIGH may NOT switch a 5 V module off - that is why we float it.)
  If it is the other way round, the module's H/L jumper is on H: move it to L.
*/
const int PIN = 26;

void belt(bool on) {
  if (on) {
    digitalWrite(PIN, LOW);
    pinMode(PIN, OUTPUT);
  } else {
    pinMode(PIN, INPUT);
  }
  Serial.println(on ? "belt ON  (relay should click, LED on, motor runs)"
                    : "belt OFF (relay released, motor stops)");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nNAZAR 08 relay / belt test (GPIO26)");
  belt(false);
  Serial.println("type 1 or 0, then Enter");
}

void loop() {
  if (!Serial.available()) return;
  String s = Serial.readStringUntil('\n');
  s.trim();
  if (s == "1") belt(true);
  else if (s == "0") belt(false);
  else if (s.length()) Serial.println("? type 1 (on) or 0 (off)");
}
