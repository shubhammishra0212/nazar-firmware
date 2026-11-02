// proto.h - the serial protocol of CONTRACT.md section 1 and 2.
// Out: one JSON object per line, built by hand with snprintf (no ArduinoJson).
// In:  plain-text commands, one per line, case-insensitive.
#pragma once
#include <stdint.h>
#include "sensors.h"

void protoBegin();
void protoPoll();  // read bytes from USB serial; runs a command for every complete line

void protoHello();
void protoTel(const SensorFrame &f);
void protoPart(uint32_t n);
void protoInspect(uint32_t n);
void protoGo(uint32_t n, const char *res);
void protoGate(uint32_t n);
void protoAck(const char *cmd);
void protoErr(const char *msg);
void protoLog(const char *msg);
