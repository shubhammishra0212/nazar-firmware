// gate.h - MG996R reject gate on GPIO13 (50 Hz LEDC PWM, ESP32 core 3.x API).
#pragma once
#include <stdint.h>

void gateBegin();
void gateFire(uint32_t n, uint32_t openMs);  // open now for openMs; n = part (0 = manual REJECT)
void gateUpdate(uint32_t nowMs);              // closes the gate when its time is up
bool gateIsOpen();
uint32_t gateRejects();                       // gate firings since boot / RESET
void gateResetCount();
