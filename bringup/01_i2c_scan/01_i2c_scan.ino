/*
  NAZAR bring-up 01 - I2C bus scan (SDA 21, SCL 22)
  Upload, open Serial Monitor at 115200. Every 2 s it lists every address that answers.
  Expected with everything wired: 0x23 (BH1750), 0x40 (INA219), 0x68 (MPU-6050).
*/
#include <Wire.h>

struct Known { uint8_t addr; const char *name; };
const Known KNOWN[] = {{0x23, "BH1750 light"}, {0x40, "INA219 power"}, {0x68, "MPU-6050 vibration"}};

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(21, 22, 100000);
  Wire.setTimeOut(20);
  Serial.println("\nNAZAR 01 I2C scan - SDA 21, SCL 22, 100 kHz");
}

void loop() {
  int found = 0;
  Serial.println("scanning...");
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() != 0) continue;
    found++;
    const char *name = "unknown device";
    for (const Known &k : KNOWN) if (k.addr == a) name = k.name;
    Serial.printf("  0x%02X  %s\n", a, name);
  }
  for (const Known &k : KNOWN) {
    Wire.beginTransmission(k.addr);
    bool ok = Wire.endTransmission() == 0;
    Serial.printf("  %s %-20s at 0x%02X\n", ok ? "PASS" : "FAIL", k.name, k.addr);
  }
  if (found == 0) {
    Serial.println("  nothing answers: check 3V3 + GND to each board, SDA->21, SCL->22,");
    Serial.println("  and that SDA/SCL are not swapped.");
  }
  delay(2000);
}
