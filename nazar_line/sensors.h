// sensors.h - INA219, MPU-6050, BH1750 (raw Wire, no libraries) and DS18B20.
// Every sensor is optional: a missing one is retried every SENSOR_RETRY_MS and its
// keys are simply left out of the telemetry line.
#pragma once
#include <stdint.h>

struct SensorPresence {
  bool ina219 = false, mpu6050 = false, bh1750 = false, ds18b20 = false;
};

// One telemetry snapshot. has* = false means "leave the key out".
struct SensorFrame {
  bool hasPower = false;  float v = 0, ma = 0;
  bool hasVib = false;    float vib = 0, vpk = 0;
  bool hasLux = false;    float lux = 0;
  bool hasTemp = false;   float temp = 0;
};

void sensorsBegin();
// Call every loop: MPU sampling at 200 Hz, DS18B20 conversions, retries.
// Returns true when a sensor appeared or disappeared (send a new hello).
bool sensorsUpdate(uint32_t nowMs);
// Called every TEL_PERIOD_MS: reads INA219 + BH1750 and closes the vibration window.
void sensorsSnapshot(SensorFrame &f);
const SensorPresence &sensorsPresent();
