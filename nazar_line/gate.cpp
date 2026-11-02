// gate.cpp - see gate.h.
#include <Arduino.h>
#include "config.h"
#include "gate.h"
#include "proto.h"

static bool open_ = false;
static uint32_t openedAt = 0, openFor = 0, rejects = 0;

static void servoDeg(int deg) {
  uint32_t us = SERVO_MIN_US + (uint32_t)deg * (SERVO_MAX_US - SERVO_MIN_US) / 180;
  uint32_t full = (1UL << SERVO_BITS) - 1;
  ledcWrite(PIN_GATE_SERVO, us * full / (1000000UL / SERVO_HZ));  // pulse width -> duty
}

void gateBegin() {
  ledcAttach(PIN_GATE_SERVO, SERVO_HZ, SERVO_BITS);  // core 3.x: pin-based, no channels
  servoDeg(GATE_CLOSED_DEG);
}

void gateFire(uint32_t n, uint32_t openMs) {
  servoDeg(GATE_OPEN_DEG);
  open_ = true;
  openedAt = millis();
  openFor = openMs;  // a second fire while open just restarts the open time
  rejects++;
  protoGate(n);
}

void gateUpdate(uint32_t now) {
  if (open_ && now - openedAt >= openFor) {
    servoDeg(GATE_CLOSED_DEG);
    open_ = false;
  }
}

bool gateIsOpen() { return open_; }
uint32_t gateRejects() { return rejects; }
void gateResetCount() { rejects = 0; }
