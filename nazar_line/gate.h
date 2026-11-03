// gate.h - MG996R reject gate on GPIO13 (50 Hz LEDC PWM, ESP32 core 3.x API).
// "Out" = the arm is across the belt. Stop gate (kick 0): out until released. Kick (kick 1): out for a time.
#pragma once
#include <stdint.h>

void gateBegin();
void gateFire(uint32_t n, uint32_t outMs);           // kick: arm out now for outMs of belt running (n 0 = manual: clock time)
void gateBlock(uint32_t n);                          // stop gate: arm out now, stays until gateRelease(); n 0 = manual
void gateRelease();                                  // arm back to parked: washers pass
void gateUpdate(uint32_t nowMs, bool beltRunning);   // moves the arm, ends a kick, stops the pulses when parked
bool gateIsOut();
uint32_t gateRejects();                              // gate firings since boot / RESET
void gateResetCount();
