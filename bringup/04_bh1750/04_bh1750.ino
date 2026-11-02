/*
  NAZAR bring-up 04 - BH1750 light sensor (I2C 0x23, ADDR pin to GND or open)
  Wiring: VCC 3V3, GND, SDA 21, SCL 22.
  Prints lux twice a second. Test: cover it with your hand, the number should drop
  close to 0; switch the LED tunnel on and it should rise a lot.
*/
#include <Wire.h>

const uint8_t ADDR = 0x23;
bool ok = false;

bool cmd(uint8_t c) {
  Wire.beginTransmission(ADDR);
  Wire.write(c);
  return Wire.endTransmission() == 0;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(21, 22, 100000);
  Wire.setTimeOut(20);
  Serial.println("\nNAZAR 04 BH1750 test");
}

void loop() {
  if (!ok) {
    ok = cmd(0x01) && cmd(0x10);  // power on, continuous high-res
    if (!ok) {
      Serial.println("FAIL BH1750 not at 0x23: check wiring (ADDR pin must be LOW/open), run 01_i2c_scan");
      delay(2000);
      return;
    }
    delay(180);  // first measurement takes up to 180 ms
  }
  if (Wire.requestFrom(ADDR, (uint8_t)2) != 2) {
    Serial.println("FAIL read error");
    ok = false;
    return;
  }
  uint16_t raw = (uint16_t)Wire.read() << 8;
  raw |= Wire.read();
  Serial.printf("BH1750 found at 0x23: %.1f lx - cover it with your hand, the number should drop\n", raw / 1.2);
  delay(500);
}
