/*
  NAZAR bring-up 03 - MPU-6050 vibration (I2C 0x68, raw registers)
  Wiring: VCC 3V3, GND, SDA 21, SCL 22, AD0 -> GND.
  Prints the acceleration size |a| (about 1.00 g at rest, whatever the tilt) and the
  vibration RMS / peak over the last 100 ms, like the main firmware.
  Test: board still -> vib about 0.00x g. Tap the table or run the motor -> vib jumps.
*/
#include <Wire.h>

const uint8_t ADDR = 0x68;
bool ok = false;
float mean = 0;
bool meanSet = false;
float sumSq = 0, peak = 0;
int count = 0;
uint32_t nextSample = 0, lastPrint = 0;

bool readRegs(uint8_t reg, uint8_t *b, uint8_t n) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADDR, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) b[i] = Wire.read();
  return true;
}

void writeReg(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  Wire.write(v);
  Wire.endTransmission();
}

bool initMpu() {
  uint8_t who = 0;
  if (!readRegs(0x75, &who, 1)) {
    Serial.println("FAIL MPU-6050 does not answer at 0x68: check wiring, AD0 to GND, run 01_i2c_scan");
    return false;
  }
  Serial.printf("WHO_AM_I = 0x%02X ", who);
  if (who == 0x68) Serial.println("(genuine MPU-6050)");
  else if (who == 0x70 || who == 0x72 || who == 0x98) Serial.println("(clone - works the same)");
  else { Serial.println("(unexpected chip)"); return false; }
  writeReg(0x6B, 0x01);  // wake up
  writeReg(0x1C, 0x08);  // +-4 g
  writeReg(0x1A, 0x02);  // ~94 Hz low-pass
  Serial.println("PASS MPU-6050 ready");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(21, 22, 100000);
  Wire.setTimeOut(20);
  Serial.println("\nNAZAR 03 MPU-6050 test");
}

void loop() {
  uint32_t now = millis();
  if (!ok) {
    ok = initMpu();
    if (!ok) delay(2000);
    return;
  }
  if ((int32_t)(now - nextSample) >= 0) {  // 200 Hz
    nextSample = now + 5;
    uint8_t b[6];
    if (!readRegs(0x3B, b, 6)) { Serial.println("FAIL read error"); ok = false; return; }
    float ax = (int16_t)(b[0] << 8 | b[1]) / 8192.0, ay = (int16_t)(b[2] << 8 | b[3]) / 8192.0,
          az = (int16_t)(b[4] << 8 | b[5]) / 8192.0;
    float mag = sqrt(ax * ax + ay * ay + az * az);
    if (!meanSet) { mean = mag; meanSet = true; }
    mean += (mag - mean) * 0.01;
    float d = fabs(mag - mean);
    sumSq += d * d;
    if (d > peak) peak = d;
    count++;
  }
  if (now - lastPrint >= 100) {
    lastPrint = now;
    if (count) Serial.printf("|a| %.3f g   vib %.3f g   peak %.3f g\n", mean, sqrt(sumSq / count), peak);
    sumSq = peak = 0;
    count = 0;
  }
}
