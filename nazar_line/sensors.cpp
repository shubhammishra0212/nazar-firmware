// sensors.cpp - see sensors.h.
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>
#include "config.h"
#include "sensors.h"

static SensorPresence present;
static uint8_t failsIna = 0, failsMpu = 0, failsBh = 0;

// ---------- raw I2C helpers
static bool i2cPing(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool i2cWrite(uint8_t addr, const uint8_t *data, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(data, len);
  return Wire.endTransmission() == 0;
}

static bool i2cReadReg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // repeated start
  if (Wire.requestFrom(addr, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

// ---------- INA219: belt motor bus voltage and current
static bool inaInit() {
  // config 0x399F: 32 V bus range, +-320 mV shunt range, 12-bit, continuous shunt + bus
  const uint8_t cfg[3] = {0x00, 0x39, 0x9F};
  return i2cPing(I2C_INA219) && i2cWrite(I2C_INA219, cfg, 3);
}

static bool inaRead(float &v, float &ma) {
  uint8_t b[2];
  if (!i2cReadReg(I2C_INA219, 0x02, b, 2)) return false;
  v = (((uint16_t)b[0] << 8 | b[1]) >> 3) * 0.004f;  // 4 mV per bit
  if (!i2cReadReg(I2C_INA219, 0x01, b, 2)) return false;
  int16_t shunt = (int16_t)((uint16_t)b[0] << 8 | b[1]);  // 10 uV per bit
  ma = shunt * 0.01f / INA219_SHUNT_OHM;                    // mV / ohm = mA
  return true;
}

// ---------- MPU-6050: vibration from |a| (board is mounted tilted ~77 deg, so no single axis)
static float vibMean = 0;
static bool vibMeanSet = false;
static float vibSumSq = 0, vibPeak = 0;
static uint16_t vibCount = 0;
static uint32_t nextMpuMs = 0;

static bool mpuInit() {
  uint8_t who = 0;
  if (!i2cReadReg(I2C_MPU6050, 0x75, &who, 1)) return false;
  // genuine parts say 0x68; clones answer 0x70, 0x72 or 0x98 and work the same
  if (who != 0x68 && who != 0x70 && who != 0x72 && who != 0x98) return false;
  const uint8_t wake[2] = {0x6B, 0x01};   // out of sleep, clock = gyro X PLL
  const uint8_t accel[2] = {0x1C, 0x08};  // +-4 g (vibration on top of 1 g gravity)
  const uint8_t dlpf[2] = {0x1A, 0x02};   // ~94 Hz low-pass
  if (!i2cWrite(I2C_MPU6050, wake, 2)) return false;
  i2cWrite(I2C_MPU6050, accel, 2);
  i2cWrite(I2C_MPU6050, dlpf, 2);
  vibMeanSet = false;
  return true;
}

static void mpuSample() {
  uint8_t b[6];
  if (!i2cReadReg(I2C_MPU6050, 0x3B, b, 6)) {
    if (++failsMpu >= SENSOR_MAX_FAILS) present.mpu6050 = false;
    return;
  }
  failsMpu = 0;
  float ax = (int16_t)((uint16_t)b[0] << 8 | b[1]) / 8192.0f;  // 8192 LSB/g at +-4 g
  float ay = (int16_t)((uint16_t)b[2] << 8 | b[3]) / 8192.0f;
  float az = (int16_t)((uint16_t)b[4] << 8 | b[5]) / 8192.0f;
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  if (!vibMeanSet) {
    vibMean = mag;
    vibMeanSet = true;
  }
  vibMean += (mag - vibMean) * VIB_MEAN_ALPHA;  // slow average = gravity
  float d = fabsf(mag - vibMean);
  vibSumSq += d * d;
  if (d > vibPeak) vibPeak = d;
  vibCount++;
}

// ---------- BH1750: light in the camera tunnel
static bool bhInit() {
  const uint8_t on = 0x01, cont = 0x10;  // power on, continuous 1 lx resolution
  return i2cWrite(I2C_BH1750, &on, 1) && i2cWrite(I2C_BH1750, &cont, 1);
}

static bool bhRead(float &lux) {
  if (Wire.requestFrom(I2C_BH1750, (uint8_t)2) != 2) return false;
  uint16_t raw = (uint16_t)Wire.read() << 8;
  raw |= Wire.read();
  lux = raw / 1.2f;
  return true;
}

// ---------- DS18B20: motor can temperature, non-blocking
static OneWire oneWire(PIN_DS18B20);
static DallasTemperature dallas(&oneWire);
static DeviceAddress dsAddr;
static float dsTemp = NAN;
static uint32_t dsRequestedAt = 0;
static bool dsWaiting = false;

static bool dsInit() {
  dallas.begin();
  if (dallas.getDeviceCount() == 0 || !dallas.getAddress(dsAddr, 0)) return false;
  dallas.setResolution(dsAddr, 11);
  dallas.setWaitForConversion(false);  // never block the loop for 375 ms
  dsWaiting = false;
  dsTemp = NAN;
  return true;
}

static void dsUpdate(uint32_t now) {
  if (!present.ds18b20) return;
  if (!dsWaiting) {
    if (now - dsRequestedAt >= DS_PERIOD_MS) {
      dallas.requestTemperaturesByAddress(dsAddr);
      dsRequestedAt = now;
      dsWaiting = true;
    }
    return;
  }
  if (now - dsRequestedAt < DS_CONVERT_MS) return;
  dsWaiting = false;
  float c = dallas.getTempC(dsAddr);
  if (c == DEVICE_DISCONNECTED_C) {
    present.ds18b20 = false;
    dsTemp = NAN;
  } else if (c != 85.0f) {  // 85.0 = power-on value, not a real reading
    dsTemp = c;
  }
}

// ---------- public
void sensorsBegin() {
  Wire.begin(PIN_SDA, PIN_SCL, I2C_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
  present.ina219 = inaInit();
  present.mpu6050 = mpuInit();
  present.bh1750 = bhInit();
  present.ds18b20 = dsInit();
  nextMpuMs = millis();
}

bool sensorsUpdate(uint32_t now) {
  static SensorPresence reported = present;
  static uint32_t lastRetry = 0;

  if (present.mpu6050 && (int32_t)(now - nextMpuMs) >= 0) {
    nextMpuMs += MPU_PERIOD_MS;
    if ((int32_t)(now - nextMpuMs) > 50) nextMpuMs = now + MPU_PERIOD_MS;  // loop was slow: skip ahead
    mpuSample();
  }
  dsUpdate(now);

  if (now - lastRetry >= SENSOR_RETRY_MS) {
    lastRetry = now;
    if (present.ina219) present.ina219 = i2cPing(I2C_INA219);
    else if (inaInit()) { present.ina219 = true; failsIna = 0; }
    if (present.mpu6050) present.mpu6050 = i2cPing(I2C_MPU6050);
    else if (mpuInit()) { present.mpu6050 = true; failsMpu = 0; nextMpuMs = now; }
    if (present.bh1750) present.bh1750 = i2cPing(I2C_BH1750);
    else if (bhInit()) { present.bh1750 = true; failsBh = 0; }
    if (!present.ds18b20) present.ds18b20 = dsInit();
  }

  bool changed = reported.ina219 != present.ina219 || reported.mpu6050 != present.mpu6050 ||
                 reported.bh1750 != present.bh1750 || reported.ds18b20 != present.ds18b20;
  reported = present;
  return changed;
}

void sensorsSnapshot(SensorFrame &f) {
  f = SensorFrame();
  if (present.ina219) {
    if (inaRead(f.v, f.ma)) { f.hasPower = true; failsIna = 0; }
    else if (++failsIna >= SENSOR_MAX_FAILS) present.ina219 = false;
  }
  if (present.mpu6050 && vibCount > 0) {
    f.hasVib = true;
    f.vib = sqrtf(vibSumSq / vibCount);
    f.vpk = vibPeak;
  }
  vibSumSq = 0;
  vibPeak = 0;
  vibCount = 0;
  if (present.bh1750) {
    if (bhRead(f.lux)) { f.hasLux = true; failsBh = 0; }
    else if (++failsBh >= SENSOR_MAX_FAILS) present.bh1750 = false;
  }
  if (present.ds18b20 && !isnan(dsTemp)) {
    f.hasTemp = true;
    f.temp = dsTemp;
  }
}

const SensorPresence &sensorsPresent() { return present; }
