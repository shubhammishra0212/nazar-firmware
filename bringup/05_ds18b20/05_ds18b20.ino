/*
  NAZAR bring-up 05 - DS18B20 motor temperature (GPIO4, 4.7 k pull-up DQ -> 3V3)
  Libraries: OneWire, DallasTemperature.
  Non-blocking like the main firmware: ask for a conversion, come back 400 ms later.
  Test: room temperature (25-35 C). Hold the probe in your fingers, it should rise.
  -127 or "no sensor" = no pull-up, DQ on the wrong pin, or VCC/GND swapped (it gets hot!).
*/
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire ow(4);
DallasTemperature ds(&ow);
DeviceAddress addr;
bool ok = false;
uint32_t askedAt = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nNAZAR 05 DS18B20 test (GPIO4)");
}

void loop() {
  if (!ok) {
    ds.begin();
    ok = ds.getDeviceCount() > 0 && ds.getAddress(addr, 0);
    if (!ok) {
      Serial.println("FAIL no DS18B20 on GPIO4: check the 4.7 k pull-up from DQ to 3V3 and VCC/GND");
      delay(2000);
      return;
    }
    Serial.printf("PASS DS18B20 found (%d on the bus)\n", ds.getDeviceCount());
    ds.setResolution(addr, 11);
    ds.setWaitForConversion(false);
    ds.requestTemperaturesByAddress(addr);
    askedAt = millis();
  }
  if (millis() - askedAt < 1000) return;  // 1 reading per second (conversion needs 375 ms)
  float c = ds.getTempC(addr);
  if (c == DEVICE_DISCONNECTED_C) {
    Serial.println("FAIL sensor stopped answering (-127): loose DQ wire?");
    ok = false;
    return;
  }
  Serial.printf("DS18B20: %.2f C - hold the probe in your fingers, it should rise\n", c);
  ds.requestTemperaturesByAddress(addr);
  askedAt = millis();
}
