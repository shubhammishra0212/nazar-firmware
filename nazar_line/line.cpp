// line.cpp - conveyor state machine. See line.h and CONTRACT.md section 2.
//
//   IR rising edge (cam 0) --> part msg, arrival timer = stop_ms of belt running time
//   SEEN from the console camera (cam 1) --> part msg, arrival timer 0 (the washer is already under the camera)
//   arrival timer 0 --> belt off, INSPECT, inspect msg
//   PASS / FAIL / timeout (after dwell_ms) --> go msg, belt on
//   FAIL --> gate timer = gate_ms of belt running time --> fireGate
//   HOLD / STOP --> belt off; running-time timers simply stop counting
#include "line.h"

const char *modeName(Mode m) {
  switch (m) {
    case Mode::RUN: return "RUN";
    case Mode::INSPECT: return "INSPECT";
    case Mode::HOLD: return "HOLD";
    case Mode::STOP: return "STOP";
  }
  return "RUN";
}

static uint32_t minus(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }

void Line::begin(const LineEvents *ev, const LineCfg *cfg, uint32_t nowMs, bool startStopped) {
  ev_ = ev;
  cfg_ = cfg;
  last_ = nowMs;
  irChangedAt_ = nowMs;
  stopped_ = startStopped;
}

void Line::say(const char *msg) const {
  if (ev_ && ev_->log) ev_->log(msg);
}

Mode Line::mode() const {
  if (stopped_) return Mode::STOP;
  if (held_) return Mode::HOLD;
  if (inspecting_) return Mode::INSPECT;
  return Mode::RUN;
}

bool Line::beltOn() const { return running() || (stopped_ && manualBelt_); }

void Line::update(uint32_t nowMs, bool irPresent) {
  uint32_t dt = nowMs - last_;  // unsigned: survives the millis() wrap
  last_ = nowMs;
  bool wasRunning = running();

  // 1. clocks: belt running time drives every position timer
  //    (first, so a timer started in this tick counts from the next one)
  if (wasRunning) {
    runClock_ += dt;
    for (int i = 0; i < arrCount_; i++) arr_[i].left = minus(arr_[i].left, dt);
    for (int i = 0; i < gateCount_; i++) gate_[i].left = minus(gate_[i].left, dt);
  }
  if (inspecting_ && !stopped_ && !held_) inspElapsed_ += dt;

  // 2. IR debounce: the level must stay the same for IR_DEBOUNCE_MS
  if (irPresent != irRaw_) {
    irRaw_ = irPresent;
    irChangedAt_ = nowMs;
  }
  if (irRaw_ != irStable_ && nowMs - irChangedAt_ >= IR_DEBOUNCE_MS) {
    irStable_ = irRaw_;
    // edges with the belt off are hands, not parts; with the camera trigger the IR is only shown, never counted
    if (irStable_ && wasRunning && !cfg_->cam) newPart(cfg_->stop_ms);
  }

  // 3. gate timers that reached the gate
  for (int i = 0; i < gateCount_;) {
    if (gate_[i].left == 0) {
      uint32_t n = gate_[i].n;
      for (int j = i; j < gateCount_ - 1; j++) gate_[j] = gate_[j + 1];
      gateCount_--;
      if (ev_ && ev_->fireGate) ev_->fireGate(n);
    } else {
      i++;
    }
  }

  // 4. parts that reached the camera (oldest first)
  while (arrCount_ > 0 && arr_[0].left == 0 && !inspecting_) {
    uint32_t n = arr_[0].n;
    for (int j = 0; j < arrCount_ - 1; j++) arr_[j] = arr_[j + 1];
    arrCount_--;
    if (ev_ && ev_->inspect) ev_->inspect(n);
    if (cfg_->stops) {  // stop-and-inspect: belt off until a verdict
      inspecting_ = true;
      inspN_ = n;
      inspElapsed_ = 0;
      verdict_ = V_NONE;
    }
  }

  // 5. the part under the camera: restart after dwell_ms once there is a verdict, or at wait_ms
  if (inspecting_ && !held_ && inspElapsed_ >= cfg_->dwell_ms) {
    if (verdict_ == V_PASS) finishInspection("pass", false);
    else if (verdict_ == V_FAIL) finishInspection("reject", true);
    else if (inspElapsed_ >= cfg_->wait_ms) finishInspection("timeout", false);
  }
}

void Line::newPart(uint32_t lead) {
  parts_++;
  if (ev_ && ev_->part) ev_->part(parts_);
  lastN_ = parts_;
  lastPartRun_ = runClock_;
  lastLead_ = lead;
  lastDecided_ = false;
  if (arrCount_ < ARRIVAL_SLOTS) {
    arr_[arrCount_++] = {parts_, lead};
  } else {
    say("arrival queue full: part will not stop under the camera");
  }
}

void Line::finishInspection(const char *res, bool reject) {
  inspecting_ = false;
  if (inspN_ == lastN_) lastDecided_ = true;
  if (ev_ && ev_->go) ev_->go(inspN_, res);
  if (reject) queueGate(inspN_, cfg_->gate_ms);
}

void Line::queueGate(uint32_t n, uint32_t left) {
  if (gateCount_ < GATE_SLOTS) {
    gate_[gateCount_++] = {n, left};
  } else {
    say("gate queue full: reject dropped");
  }
}

const char *Line::seen() {
  if (!cfg_->cam) return "camera trigger is off (SET CAM 1)";
  if (!running()) return "belt is not running";             // stopped, on hold or already inspecting
  if (arrCount_ > 0) return "a part is already on its way to the camera";
  newPart(0);  // inspected in the next update(): belt off at once in stop-and-inspect
  return nullptr;
}

const char *Line::pass() {
  if (inspecting_) {
    if (held_) return "part is on hold, send RESUME";
    if (verdict_ != V_NONE) return "verdict already given";
    verdict_ = V_PASS;
    return nullptr;
  }
  if (!cfg_->stops && !lastDecided_) {  // continuous: nothing to do for a good part
    lastDecided_ = true;
    return nullptr;
  }
  return "no part waiting for a verdict";
}

const char *Line::fail() {
  if (inspecting_) {
    if (held_) return "part is on hold, send RESUME";
    if (verdict_ != V_NONE) return "verdict already given";
    verdict_ = V_FAIL;
    return nullptr;
  }
  if (!cfg_->stops && !lastDecided_) {
    // continuous: the part never stopped, so the gate is due gate_ms after it passed the camera
    uint32_t dueAt = lastPartRun_ + lastLead_ + cfg_->gate_ms;
    queueGate(lastN_, minus(dueAt, runClock_));
    lastDecided_ = true;
    return nullptr;
  }
  return "no part waiting for a verdict";
}

const char *Line::hold() {
  held_ = true;
  return nullptr;
}

const char *Line::resume() {
  if (held_ && inspecting_) {  // the held part leaves without the gate
    held_ = false;
    finishInspection("hold-released", false);
    return nullptr;
  }
  held_ = false;
  return nullptr;
}

const char *Line::stop() {
  stopped_ = true;
  manualBelt_ = false;
  return nullptr;
}

const char *Line::start() {
  stopped_ = false;
  manualBelt_ = false;
  return nullptr;
}

const char *Line::manualBelt(bool on) {
  if (!stopped_) return "BELT only works in STOP mode";
  manualBelt_ = on;
  return nullptr;
}

void Line::resetCounters() { parts_ = 0; }
