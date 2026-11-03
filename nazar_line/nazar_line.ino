 /*
  NAZAR line controller - ESP32 firmware 2.1.0  (INDUX 5.0, Team Parakh)

  Board : ESP32 Dev Module (DevKitC WROOM), esp32 core 3.x      Serial: 115200, JSON lines
  Libs  : OneWire, DallasTemperature (everything else is raw Wire / LEDC)
  Proto : CONTRACT.md in github.com/shubhammishra0212/nazar  (sections 1-3)

  The loop never waits: every job checks millis() and returns at once.
    protoPoll      read commands from the Mac console
    sensorsUpdate  MPU-6050 at 200 Hz, DS18B20 conversions, retry missing sensors every 3 s
    g_line.update  IR debounce + RUN / INSPECT / HOLD / STOP state machine
                   (cam 1, the default: the console camera sends SEEN for each washer, the IR is only shown)
    gate / belt    servo closing time, relay follows the state machine
    telemetry      one "tel" line every 100 ms
*/
#include "config.h"
#include "proto.h"
#include "sensors.h"
#include "line.h"
#include "gate.h"
#include "belt.h"

Line g_line;
LineCfg g_cfg;

static void evPart(uint32_t n) { protoPart(n); }
static void evInspect(uint32_t n) { protoInspect(n); }
static void evGo(uint32_t n, const char *res) { protoGo(n, res); }
static void evGate(uint32_t n) { gateFire(n, g_cfg.gate_open_ms); }
static void evLog(const char *msg) { protoLog(msg); }
static const LineEvents events = {evPart, evInspect, evGo, evGate, evLog};

static uint32_t lastTel = 0;

void setup() {
  beltBegin();  // relay released before anything else: belt off during boot
  pinMode(PIN_IR, INPUT_PULLUP);
  protoBegin();
  gateBegin();
  sensorsBegin();
  g_line.begin(&events, &g_cfg, millis(), BOOT_STOPPED);
  protoLog("NAZAR line fw " NAZAR_FW_VERSION " boot");
  protoHello();
  lastTel = millis();
}

void loop() {
  uint32_t now = millis();
  protoPoll();
  if (sensorsUpdate(now)) protoHello();  // a sensor came or went
  g_line.update(now, digitalRead(PIN_IR) == LOW);
  beltSet(g_line.beltOn());
  gateUpdate(now);
  if (now - lastTel >= TEL_PERIOD_MS) {
    lastTel += TEL_PERIOD_MS;
    if (now - lastTel >= TEL_PERIOD_MS) lastTel = now;  // fell behind: do not burst
    SensorFrame f;
    sensorsSnapshot(f);
    protoTel(f);
  }
}
