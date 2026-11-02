/*
  NAZAR bring-up 07 - MG996R reject gate (signal GPIO13 through 330 R, servo on the 5.5 V rail)
  50 Hz PWM with ledcAttach (esp32 core 3.x). Commands in Serial Monitor (Newline):
    o  open (70 deg)      c  close (0 deg)      s  sweep open/close 3 times
    <number>  go to that angle, e.g. 45
  Find the right closed / open angles here, then copy them into config.h of nazar_line.
  Jitter or ESP32 resets when the servo moves = servo power too weak or no common GND.
*/
const int PIN = 13;
const int CLOSED = 0, OPEN = 70;
const uint8_t BITS = 14;

void goDeg(int deg) {
  deg = constrain(deg, 0, 180);
  uint32_t us = 500 + (uint32_t)deg * 2000 / 180;          // 500..2500 us
  ledcWrite(PIN, us * ((1UL << BITS) - 1) / 20000);         // 20 ms period
  Serial.printf("servo -> %d deg (%lu us pulse)\n", deg, (unsigned long)us);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nNAZAR 07 servo gate test (GPIO13)");
  if (!ledcAttach(PIN, 50, BITS)) Serial.println("FAIL ledcAttach refused GPIO13");
  goDeg(CLOSED);
  Serial.println("type o / c / s or an angle, then Enter");
}

void loop() {
  if (!Serial.available()) return;
  String s = Serial.readStringUntil('\n');
  s.trim();
  if (s == "o") goDeg(OPEN);
  else if (s == "c") goDeg(CLOSED);
  else if (s == "s") {
    for (int i = 0; i < 3; i++) {  // blocking is fine in a bring-up sketch
      goDeg(OPEN);
      delay(600);
      goDeg(CLOSED);
      delay(600);
    }
    Serial.println("sweep done - did the arm move fully both ways?");
  } else if (s.length() && isDigit(s[0])) goDeg(s.toInt());
  else if (s.length()) Serial.println("? use o, c, s or an angle 0-180");
}
