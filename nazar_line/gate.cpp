// gate.cpp - see gate.h.
//
// Stop gate (kick 0, the default): for a reject the arm swings across the belt and STAYS there, so the rejected washer
// is stopped against it while the belt slides underneath; the operator takes it off. The next good washer releases the
// arm (gateRelease from the sketch). Kick (kick 1): the arm swings out for gate_open_ms of belt running time and back.
//
// Three things keep the MG996R calm on a 5.5 V rail it shares with the belt motor:
//   - the arm moves in small steps (GATE_OUT_DEG_S out, GATE_BACK_DEG_S back) instead of jumping, so the servo never
//     sees one big error at once and its start-up current kick is smaller;
//   - once the arm has been parked for GATE_IDLE_MS the pulses stop: an unloaded arm stays put by gear friction, and a
//     servo that gets no pulses cannot hunt (flutter) around its position because of motor noise on the rail;
//   - while across the belt it keeps its pulses, so it holds against the washer.
// Angles come from g_cfg (SET PARK_DEG / BLOCK_DEG), so they can be tuned while the line runs.
#include <Arduino.h>
#include "config.h"
#include "gate.h"
#include "proto.h"

extern LineCfg g_cfg;  // defined in nazar_line.ino

static bool out_ = false, timed = false, clockTime = false, pulsing = false;
static float angle = 0;  // angle the arm is being driven to right now, degrees
static uint32_t last = 0, outLeft = 0, parkedAt = 0, rejects = 0;

static void pulse(float deg) {
  uint32_t us = SERVO_MIN_US + (uint32_t)(deg * (SERVO_MAX_US - SERVO_MIN_US) / 180.0f);
  uint32_t full = (1UL << SERVO_BITS) - 1;
  ledcWrite(PIN_GATE_SERVO, us * full / (1000000UL / SERVO_HZ));  // pulse width -> duty
  pulsing = true;
}

static void quiet() {
  ledcWrite(PIN_GATE_SERVO, 0);  // no pulses: the servo motor is off and the arm rests
  pulsing = false;
}

void gateBegin() {
  ledcAttach(PIN_GATE_SERVO, SERVO_HZ, SERVO_BITS);  // core 3.x: pin-based, no channels
  angle = (float)g_cfg.park_deg;
  pulse(angle);  // where the arm is after a reset is unknown: park it once, then rest
  last = parkedAt = millis();
}

void gateFire(uint32_t n, uint32_t outMs) {
  out_ = true;
  timed = true;
  outLeft = outMs;  // a second fire while out just restarts the time
  clockTime = (n == 0);
  rejects++;
  protoGate(n);
}

void gateBlock(uint32_t n) {
  out_ = true;
  timed = false;
  rejects++;
  protoGate(n);
}

void gateRelease() {
  out_ = false;
  timed = false;
}

void gateUpdate(uint32_t now, bool beltRunning) {
  uint32_t dt = now - last;
  last = now;
  if (out_ && timed && (clockTime || beltRunning)) {
    outLeft = outLeft > dt ? outLeft - dt : 0;
    if (!outLeft) out_ = false;
  }
  float target = (float)(out_ ? g_cfg.block_deg : g_cfg.park_deg);
  if (angle != target) {
    float step = (out_ ? GATE_OUT_DEG_S : GATE_BACK_DEG_S) * dt / 1000.0f;
    angle = fabsf(target - angle) <= step ? target : angle + (target > angle ? step : -step);
    pulse(angle);
    parkedAt = now;
  } else if (out_) {
    if (!pulsing) pulse(angle);
    parkedAt = now;
  } else if (pulsing && now - parkedAt >= GATE_IDLE_MS) {
    quiet();
  }
}

bool gateIsOut() { return out_; }
uint32_t gateRejects() { return rejects; }
void gateResetCount() { rejects = 0; }
