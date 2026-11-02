/*
  NAZAR bring-up 08 - belt relay (IN -> GPIO26, module VCC on 5.5 V)
  Commands: 1 = belt on, 0 = belt off (Newline). Starts OFF.
  Our 1-CH board switches the coil with a BC547 (NPN), so it is ACTIVE HIGH:
  ON  = pin HIGH (3.3 V) -> relay clicks, red LED lights, 5.5 V reaches the motor.
  OFF = pin LOW          -> relay released, motor stops.
  Using an active-LOW module (H/L jumper on L) instead? Set ACTIVE_HIGH to false.
*/
const int PIN = 26;
const bool ACTIVE_HIGH = true;

void belt(bool on) {
  if (ACTIVE_HIGH) {
    digitalWrite(PIN, on ? HIGH : LOW);
    pinMode(PIN, OUTPUT);
  } else if (on) {
    digitalWrite(PIN, LOW);
    pinMode(PIN, OUTPUT);
  } else {
    pinMode(PIN, INPUT);  // active-LOW module: float so its pull-up releases the relay
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
