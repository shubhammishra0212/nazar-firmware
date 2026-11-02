// line.h - the conveyor state machine (RUN / INSPECT / HOLD / STOP).
//
// Pure logic: no Arduino calls. The sketch feeds it the time and the raw IR pin level
// once per loop; it reports what happened through the LineEvents callbacks and the
// caller drives the relay from beltOn(). This is what makes it testable on a laptop
// (see test/line_test.cpp).
#pragma once
#include <stdint.h>
#include "config.h"

enum class Mode : uint8_t { RUN, INSPECT, HOLD, STOP };
const char *modeName(Mode m);

// What the line tells the outside world. Any pointer may be null.
struct LineEvents {
  void (*part)(uint32_t n);                 // part n passed the IR sensor
  void (*inspect)(uint32_t n);              // part n is under the camera
  void (*go)(uint32_t n, const char *res);  // belt restarted: pass / reject / hold-released / timeout
  void (*fireGate)(uint32_t n);             // open the reject gate for part n now
  void (*log)(const char *msg);             // free text
};

class Line {
 public:
  void begin(const LineEvents *ev, const LineCfg *cfg, uint32_t nowMs, bool startStopped);

  // Call every loop. irPresent = true while the FC-51 sees something (pin LOW).
  void update(uint32_t nowMs, bool irPresent);

  // Commands. Each returns nullptr when accepted, otherwise the reason it was refused.
  const char *pass();
  const char *fail();
  const char *hold();
  const char *resume();
  const char *stop();
  const char *start();
  const char *manualBelt(bool on);  // only in STOP
  void resetCounters();

  // State for telemetry.
  Mode mode() const;
  bool beltOn() const;
  bool ir() const { return irStable_; }
  uint32_t parts() const { return parts_; }
  int pendingGates() const { return gateCount_; }
  int pendingArrivals() const { return arrCount_; }
  uint32_t runClock() const { return runClock_; }

 private:
  enum Verdict : uint8_t { V_NONE, V_PASS, V_FAIL };
  struct Timer { uint32_t n; uint32_t left; };  // "left" counts belt running time only

  bool running() const { return !stopped_ && !held_ && !inspecting_; }
  void onRisingEdge();
  void finishInspection(const char *res, bool reject);
  void queueGate(uint32_t n, uint32_t left);
  void say(const char *msg) const;

  const LineEvents *ev_ = nullptr;
  const LineCfg *cfg_ = nullptr;
  uint32_t last_ = 0;

  // IR debounce
  bool irRaw_ = false, irStable_ = false;
  uint32_t irChangedAt_ = 0;

  // operator / AI states
  bool stopped_ = false, held_ = false, manualBelt_ = false;

  // inspection of the part under the camera
  bool inspecting_ = false;
  uint32_t inspN_ = 0, inspElapsed_ = 0;
  Verdict verdict_ = V_NONE;

  // counters and clocks
  uint32_t parts_ = 0;
  uint32_t runClock_ = 0;  // total belt running time (auto mode), ms

  // continuous mode: the most recent part can still get a verdict
  uint32_t lastN_ = 0, lastIrRun_ = 0;
  bool lastDecided_ = true;

  Timer arr_[ARRIVAL_SLOTS];  // IR -> camera
  int arrCount_ = 0;
  Timer gate_[GATE_SLOTS];    // camera -> gate
  int gateCount_ = 0;
};
