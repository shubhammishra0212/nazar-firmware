// belt.cpp - see belt.h.
#include <Arduino.h>
#include "config.h"
#include "belt.h"

static bool on_ = false;

static void drive(bool run) {
  if (RELAY_ACTIVE_HIGH) {
    digitalWrite(PIN_BELT_RELAY, run ? HIGH : LOW);  // set the level first, then enable the output
    pinMode(PIN_BELT_RELAY, OUTPUT);
  } else if (run) {
    digitalWrite(PIN_BELT_RELAY, LOW);  // set the level first so the pin never glitches HIGH
    pinMode(PIN_BELT_RELAY, OUTPUT);
  } else {
    pinMode(PIN_BELT_RELAY, INPUT);     // released: relay off, belt stops
  }
  on_ = run;
}

void beltBegin() { drive(false); }

void beltSet(bool run) {
  if (run != on_) drive(run);
}

bool beltIsOn() { return on_; }
