/*
  NAZAR bring-up 02 - INA219 belt power monitor (I2C 0x40, raw registers, no library)
  Wiring: VCC 3V3, GND, SDA 21, SCL 22. Power path: 5.5 V + -> S1 -> VIN+ , VIN- -> motor (via relay).
  Prints bus voltage and current twice a second.
  Test: with the belt off, mA is about 0. Run the belt (sketch 08 or a jumper) and mA should
  jump to roughly 150-400 mA, V should read close to 5.5 V.
*/
#include <Wire.h>

const uint8_t ADDR = 0x40;
const float SHUNT_OHM = 0.1;  // R100 on the board

bool readReg(uint8_t reg, uint16_t &val) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADDR, (uint8_t)2) != 2) return false;
  val = (uint16_t)Wire.read() << 8;
  val |= Wire.read();
  return true;
}

bool init219() {
  Wire.beginTransmission(ADDR);
  Wire.write(0x00);
  Wire.write(0x39);  // 32 V range, +-320 mV shunt, 12-bit, continuous
  Wire.write(0x9F);
  return Wire.endTransmission() == 0;
}

bool ok = false;

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(21, 22, 100000);
  Wire.setTimeOut(20);
  Serial.println("\nNAZAR 02 INA219 test");
}

void loop() {
  if (!ok) {
    ok = init219();
    Serial.println(ok ? "PASS INA219 found at 0x40" :
                        "FAIL INA219 not at 0x40: run 01_i2c_scan, check VCC/GND/SDA/SCL (A0/A1 pads must be open)");
    if (!ok) { delay(2000); return; }
  }
  uint16_t bus, sh;
  if (!readReg(0x02, bus) || !readReg(0x01, sh)) {
    Serial.println("FAIL read error - loose wire?");
    ok = false;
    delay(1000);
    return;
  }
  float v = (bus >> 3) * 0.004;
  float ma = (int16_t)sh * 0.01 / SHUNT_OHM;
  Serial.printf("INA219: %.2f V  %.1f mA", v, ma);
  if (v < 1.0) Serial.print("   (no voltage on VIN-: is the 5.5 V rail on and S1 on?)");
  if (ma < -5) Serial.print("   (negative: VIN+ and VIN- are swapped)");
  Serial.println();
  delay(500);
}
