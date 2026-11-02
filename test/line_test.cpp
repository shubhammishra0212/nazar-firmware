// line_test.cpp - runs the real nazar_line/line.cpp on a laptop with a fake clock.
// No ESP32 needed. Build and run:   make -C test     (or: sh test/run.sh)
//
// Every scenario drives the IR input and the commands exactly like the sketch does
// (update() once per millisecond) and checks the messages and the belt state.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../nazar_line/line.h"

// ---------- fake world
static std::vector<std::string> events;  // "part 1", "inspect 1", "go 1 pass", "gate 1", "log ..."
static uint32_t now = 0;
static int failures = 0, checks = 0;
static uint32_t gateFiredAt[16];
static int gateFiredCount = 0;

static void evPart(uint32_t n) { events.push_back("part " + std::to_string(n)); }
static void evInspect(uint32_t n) { events.push_back("inspect " + std::to_string(n)); }
static void evGo(uint32_t n, const char *res) { events.push_back("go " + std::to_string(n) + " " + res); }
static void evGate(uint32_t n) {
  events.push_back("gate " + std::to_string(n));
  if (gateFiredCount < 16) gateFiredAt[gateFiredCount++] = now;
}
static void evLog(const char *m) { events.push_back(std::string("log ") + m); }
static const LineEvents EV = {evPart, evInspect, evGo, evGate, evLog};

struct Rig {
  Line line;
  LineCfg cfg;
  bool ir = false;
  explicit Rig(uint32_t stops = 1) {
    events.clear();
    gateFiredCount = 0;
    now = 1000;
    cfg.stops = stops;
    line.begin(&EV, &cfg, now, false);
  }
  void run(uint32_t ms) {  // advance the clock 1 ms at a time
    for (uint32_t i = 0; i < ms; i++) {
      now++;
      line.update(now, ir);
    }
  }
  void pulse(uint32_t ms = 40) {  // a washer passes the IR sensor
    ir = true;
    run(ms);
    ir = false;
  }
  // runs until `ev` appears (max `limit` ms); returns the time it appeared, 0 if never
  uint32_t until(const std::string &ev, uint32_t limit = 20000) {
    for (uint32_t i = 0; i < limit; i++) {
      for (auto &e : events) if (e == ev) return now;
      run(1);
    }
    return 0;
  }
};

static bool has(const std::string &ev) {
  for (auto &e : events) if (e == ev) return true;
  return false;
}

#define CHECK(cond, what)                                              \
  do {                                                                 \
    checks++;                                                          \
    if (!(cond)) {                                                     \
      failures++;                                                      \
      printf("    FAIL line %d: %s   (%s)\n", __LINE__, what, #cond);  \
    }                                                                  \
  } while (0)

static void title(const char *t) { printf("%s\n", t); }

// ---------- scenarios
static void testPass() {
  title("1. PASS: part stops under the camera, belt restarts after dwell_ms, no gate");
  Rig r;
  CHECK(r.line.beltOn() && r.line.mode() == Mode::RUN, "boots in RUN with the belt on");
  r.ir = true;
  uint32_t edge = r.until("part 1");
  CHECK(edge == 1000 + 1 + IR_DEBOUNCE_MS, "part message after the 20 ms debounce");
  r.ir = false;
  uint32_t stopAt = r.until("inspect 1");
  CHECK(stopAt - edge == r.cfg.stop_ms, "belt runs stop_ms after the IR edge, then inspect");
  CHECK(!r.line.beltOn() && r.line.mode() == Mode::INSPECT, "belt off, mode INSPECT");
  r.run(300);
  CHECK(r.line.pass() == nullptr, "PASS accepted");
  CHECK(r.line.pass() != nullptr, "second verdict refused");
  uint32_t goAt = r.until("go 1 pass");
  CHECK(goAt - stopAt == r.cfg.dwell_ms, "go exactly dwell_ms after the stop");
  CHECK(r.line.beltOn() && r.line.mode() == Mode::RUN, "belt back on");
  r.run(3000);
  CHECK(gateFiredCount == 0, "gate never fires for a PASS");
  CHECK(r.line.pass() != nullptr, "PASS with no part under the camera is refused");
}

static void testFail() {
  title("2. FAIL: gate fires after gate_ms of belt running time");
  Rig r;
  r.pulse();
  r.until("inspect 1");
  r.run(100);
  CHECK(r.line.fail() == nullptr, "FAIL accepted");
  uint32_t goAt = r.until("go 1 reject");
  CHECK(goAt != 0, "go with res reject");
  uint32_t gateAt = r.until("gate 1");
  CHECK(gateAt - goAt == r.cfg.gate_ms, "gate fires gate_ms after the restart");
  CHECK(r.line.pendingGates() == 0, "gate queue empty again");
}

static void testStopFreezesGate() {
  title("3. STOP mid-countdown freezes the gate timer, START continues it");
  Rig r;
  r.pulse();
  r.until("inspect 1");
  r.line.fail();
  uint32_t goAt = r.until("go 1 reject");
  r.run(300);
  CHECK(r.line.stop() == nullptr, "STOP accepted");
  CHECK(!r.line.beltOn() && r.line.mode() == Mode::STOP, "belt off, mode STOP");
  r.pulse();  // someone waves a hand at the sensor while stopped
  r.run(3000);
  CHECK(!has("gate 1"), "no gate while stopped");
  CHECK(!has("part 2"), "IR edge ignored while stopped");
  CHECK(r.line.manualBelt(true) == nullptr && r.line.beltOn(), "BELT 1 works in STOP");
  r.run(500);
  CHECK(!has("gate 1"), "manual belt does not move the gate timer");
  uint32_t startAt = now;
  CHECK(r.line.start() == nullptr, "START accepted");
  uint32_t gateAt = r.until("gate 1");
  CHECK(gateAt - startAt == r.cfg.gate_ms - 300, "gate fires after the remaining 500 ms of running");
  (void)goAt;
}

static void testStopDuringInspection() {
  title("4. STOP during inspection freezes dwell / wait; timeout still counts running time only");
  Rig r;
  r.pulse();
  uint32_t stopAt = r.until("inspect 1");
  r.run(1000);
  r.line.stop();
  r.run(2000);
  r.line.start();
  uint32_t goAt = r.until("go 1 timeout");
  CHECK(goAt - stopAt == r.cfg.wait_ms + 2000, "timeout at wait_ms plus the 2 s stop");
  CHECK(gateFiredCount == 0, "timeout part passes (no gate)");
}

static void testTimeout() {
  title("5. Timeout: no verdict within wait_ms -> go timeout, part passes");
  Rig r;
  r.pulse();
  uint32_t stopAt = r.until("inspect 1");
  uint32_t goAt = r.until("go 1 timeout");
  CHECK(goAt - stopAt == r.cfg.wait_ms, "go timeout exactly wait_ms after the stop");
  CHECK(r.line.beltOn(), "belt back on");
  CHECK(r.line.fail() != nullptr, "late FAIL refused");
}

static void testHoldResume() {
  title("6. HOLD on the part under the camera -> RESUME releases it as hold-released");
  Rig r;
  r.pulse();
  r.until("inspect 1");
  CHECK(r.line.hold() == nullptr, "HOLD accepted");
  CHECK(r.line.mode() == Mode::HOLD && !r.line.beltOn(), "mode HOLD, belt off");
  CHECK(r.line.fail() != nullptr, "verdict during hold refused");
  r.run(10000);
  CHECK(!has("go 1 timeout"), "a held part never times out");
  CHECK(r.line.resume() == nullptr, "RESUME accepted");
  CHECK(has("go 1 hold-released"), "go res hold-released");
  r.run(2000);
  CHECK(gateFiredCount == 0, "no gate for a released part");
  CHECK(r.line.mode() == Mode::RUN && r.line.beltOn(), "back to RUN");

  title("   HOLD with no part: belt off until RESUME, IR ignored");
  r.line.hold();
  r.pulse();
  r.run(500);
  CHECK(!r.line.beltOn() && !has("part 2"), "belt off and no part while held");
  r.line.resume();
  CHECK(r.line.beltOn(), "belt on after RESUME");
}

static void testQueue() {
  title("7. Two FAIL parts close together: each gets its own gate timer");
  Rig r;
  r.pulse(40);
  r.run(60);   // second washer 100 ms behind the first
  r.pulse(40);
  CHECK(has("part 1") && has("part 2"), "both parts counted");
  r.until("inspect 1");
  r.line.fail();
  uint32_t go1 = r.until("go 1 reject");
  uint32_t stop2 = r.until("inspect 2");
  CHECK(stop2 - go1 == 100, "part 2 stops 100 ms of running later");
  r.line.fail();
  uint32_t go2 = r.until("go 2 reject");
  CHECK(r.line.pendingGates() == 2, "two gate timers pending");
  r.until("gate 2");
  CHECK(gateFiredCount == 2, "both gates fired");
  // part 1 ran 100 ms before part 2 stopped, then 700 ms more after go 2
  CHECK(gateFiredAt[0] - go2 == r.cfg.gate_ms - 100, "gate 1 counted only running time");
  CHECK(gateFiredAt[1] - go2 == r.cfg.gate_ms, "gate 2 gate_ms after its own go");
}

static void testContinuous() {
  title("8. stops 0 (continuous): belt never stops, FAIL gates the most recent part");
  Rig r(0);
  r.pulse();
  uint32_t edge1 = 1000 + 1 + IR_DEBOUNCE_MS;  // first tick sees the pin, +20 ms debounce
  r.until("inspect 1");
  CHECK(r.line.beltOn() && r.line.mode() == Mode::RUN, "belt keeps running at the camera");
  r.run(200);
  CHECK(r.line.fail() == nullptr, "FAIL accepted for the most recent part");
  CHECK(r.line.fail() != nullptr, "second verdict for the same part refused");
  uint32_t gateAt = r.until("gate 1");
  CHECK(gateAt - edge1 == r.cfg.stop_ms + r.cfg.gate_ms, "gate at stop_ms + gate_ms of running after the IR edge");
  CHECK(!has("go 1 reject") && !has("go 1 pass"), "no go message in continuous mode");
}

static void testDebounce() {
  title("9. IR debounce: a 10 ms glitch is not a part");
  Rig r;
  r.pulse(10);
  r.run(500);
  CHECK(!has("part 1") && r.line.parts() == 0, "glitch ignored");
  r.pulse(25);
  CHECK(has("part 1"), "25 ms pulse counts");
  r.line.resetCounters();
  CHECK(r.line.parts() == 0, "RESET clears the part counter");
}

static void testMillisWrap() {
  title("11. millis() wraps past 0xFFFFFFFF (day 49.7) in the middle of an inspection");
  Rig r;
  now = 0xFFFFFFFFu - 100;
  r.line.begin(&EV, &r.cfg, now, false);
  r.pulse();
  uint32_t stopAt = r.until("inspect 1");
  r.line.fail();
  uint32_t goAt = r.until("go 1 reject");
  CHECK(goAt - stopAt == r.cfg.dwell_ms, "dwell correct across the wrap");
  CHECK(r.until("gate 1") - goAt == r.cfg.gate_ms, "gate timing correct across the wrap");
}

static void testManualBeltRules() {
  title("10. BELT only in STOP");
  Rig r;
  CHECK(r.line.manualBelt(false) != nullptr, "BELT refused in RUN");
  r.line.stop();
  r.line.manualBelt(true);
  r.line.start();
  r.line.stop();
  CHECK(!r.line.beltOn(), "manual belt is cleared by START");
}

int main() {
  testPass();
  testFail();
  testStopFreezesGate();
  testStopDuringInspection();
  testTimeout();
  testHoldResume();
  testQueue();
  testContinuous();
  testDebounce();
  testManualBeltRules();
  testMillisWrap();
  printf("\n%d checks, %d failed -> %s\n", checks, failures, failures ? "FAIL" : "PASS");
  return failures ? 1 : 0;
}
