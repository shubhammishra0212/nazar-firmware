// proto.cpp - see proto.h. Command handling lives here too: it only calls the
// line / gate / belt APIs and reports ack or err.
#include <Arduino.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "config.h"
#include "proto.h"
#include "line.h"
#include "gate.h"
#include "belt.h"

extern Line g_line;    // defined in nazar_line.ino
extern LineCfg g_cfg;

static const size_t RX_MAX = 48;
static char rx[RX_MAX + 1];
static size_t rxLen = 0;
static bool rxOverflow = false;

// ---------- output helpers
static void sendLine(const char *s) {
  Serial.print(s);
  Serial.print('\n');
}

// copies src into dst as JSON string content (quotes, backslashes and control chars escaped)
static void jsonEscape(char *dst, size_t cap, const char *src) {
  size_t o = 0;
  for (; *src && o + 7 < cap; src++) {
    unsigned char c = (unsigned char)*src;
    if (c == '"' || c == '\\') {
      dst[o++] = '\\';
      dst[o++] = (char)c;
    } else if (c < 0x20) {
      o += snprintf(dst + o, cap - o, "\\u%04x", c);
    } else {
      dst[o++] = (char)c;
    }
  }
  dst[o] = 0;
}

static void sendText(const char *type, const char *key, const char *text) {
  char esc[96], out[128];
  jsonEscape(esc, sizeof esc, text);
  snprintf(out, sizeof out, "{\"t\":\"%s\",\"%s\":\"%s\"}", type, key, esc);
  sendLine(out);
}

static void sendN(const char *type, uint32_t n) {
  char out[48];
  snprintf(out, sizeof out, "{\"t\":\"%s\",\"n\":%lu}", type, (unsigned long)n);
  sendLine(out);
}

// ---------- messages
void protoBegin() {
  Serial.setTxBufferSize(1024);  // a full telemetry burst never blocks the loop
  Serial.begin(115200);
}

void protoHello() {
  const SensorPresence &p = sensorsPresent();
  char out[320];
  snprintf(out, sizeof out,
           "{\"t\":\"hello\",\"fw\":\"%s\","
           "\"sensors\":{\"ina219\":%s,\"mpu6050\":%s,\"bh1750\":%s,\"ds18b20\":%s},"
           "\"cfg\":{\"stop_ms\":%lu,\"dwell_ms\":%lu,\"wait_ms\":%lu,\"gate_ms\":%lu,"
           "\"gate_open_ms\":%lu,\"stops\":%lu,\"cam\":%lu}}",
           NAZAR_FW_VERSION, p.ina219 ? "true" : "false", p.mpu6050 ? "true" : "false",
           p.bh1750 ? "true" : "false", p.ds18b20 ? "true" : "false",
           (unsigned long)g_cfg.stop_ms, (unsigned long)g_cfg.dwell_ms, (unsigned long)g_cfg.wait_ms,
           (unsigned long)g_cfg.gate_ms, (unsigned long)g_cfg.gate_open_ms, (unsigned long)g_cfg.stops,
           (unsigned long)g_cfg.cam);
  sendLine(out);
}

void protoTel(const SensorFrame &f) {
  char out[256];
  size_t o = snprintf(out, sizeof out, "{\"t\":\"tel\",\"ms\":%lu", (unsigned long)millis());
  if (f.hasPower) o += snprintf(out + o, sizeof out - o, ",\"v\":%.2f,\"ma\":%.1f", f.v, f.ma);
  if (f.hasVib) o += snprintf(out + o, sizeof out - o, ",\"vib\":%.3f,\"vpk\":%.3f", f.vib, f.vpk);
  if (f.hasLux) o += snprintf(out + o, sizeof out - o, ",\"lux\":%.1f", f.lux);
  if (f.hasTemp) o += snprintf(out + o, sizeof out - o, ",\"temp\":%.1f", f.temp);
  snprintf(out + o, sizeof out - o,
           ",\"ir\":%d,\"parts\":%lu,\"rej\":%lu,\"gate\":%d,\"belt\":%d,\"mode\":\"%s\"}",
           g_line.ir() ? 1 : 0, (unsigned long)g_line.parts(), (unsigned long)gateRejects(),
           gateIsOpen() ? 1 : 0, beltIsOn() ? 1 : 0, modeName(g_line.mode()));
  sendLine(out);
}

void protoPart(uint32_t n) { sendN("part", n); }
void protoInspect(uint32_t n) { sendN("inspect", n); }
void protoGate(uint32_t n) { sendN("gate", n); }

void protoGo(uint32_t n, const char *res) {
  char out[64];
  snprintf(out, sizeof out, "{\"t\":\"go\",\"n\":%lu,\"res\":\"%s\"}", (unsigned long)n, res);
  sendLine(out);
}

void protoAck(const char *cmd) { sendText("ack", "cmd", cmd); }
void protoErr(const char *msg) { sendText("err", "msg", msg); }
void protoLog(const char *msg) { sendText("log", "msg", msg); }

// ---------- commands
static void reply(const char *cmd, const char *refused) {
  if (refused) protoErr(refused);
  else protoAck(cmd);
}

static bool parseInt(const char *s, long &v) {
  if (!s || !*s) return false;
  char *end;
  v = strtol(s, &end, 10);
  return *end == 0;
}

static void cmdSet(const char *line, const char *key, const char *val) {
  long v;
  if (!key || !parseInt(val, v)) {
    protoErr("SET needs <key> <int>");
    return;
  }
  uint32_t *field = nullptr;
  if (!strcmp(key, "STOP_MS")) field = &g_cfg.stop_ms;
  else if (!strcmp(key, "DWELL_MS")) field = &g_cfg.dwell_ms;
  else if (!strcmp(key, "WAIT_MS")) field = &g_cfg.wait_ms;
  else if (!strcmp(key, "GATE_MS")) field = &g_cfg.gate_ms;
  else if (!strcmp(key, "GATE_OPEN_MS")) field = &g_cfg.gate_open_ms;
  else if (!strcmp(key, "STOPS")) field = &g_cfg.stops;
  else if (!strcmp(key, "CAM")) field = &g_cfg.cam;
  if (!field) {
    char msg[64];
    snprintf(msg, sizeof msg, "unknown SET key %s", key);
    protoErr(msg);
    return;
  }
  long maxV = (field == &g_cfg.stops || field == &g_cfg.cam) ? 1 : (long)CFG_MAX_MS;
  if (v < 0 || v > maxV) {
    char msg[64];
    snprintf(msg, sizeof msg, "%s must be 0..%ld", key, maxV);
    protoErr(msg);
    return;
  }
  *field = (uint32_t)v;
  protoAck(line);
  protoHello();
}

static void runCommand(char *line) {
  // trim + upper-case in place
  while (*line == ' ' || *line == '\t') line++;
  size_t len = strlen(line);
  while (len && (line[len - 1] == ' ' || line[len - 1] == '\t')) line[--len] = 0;
  if (!len) return;
  for (size_t i = 0; i < len; i++) line[i] = (char)toupper((unsigned char)line[i]);

  // split into up to three words (the original stays intact in `full` for the ack)
  char full[RX_MAX + 1];
  strncpy(full, line, sizeof full);
  full[RX_MAX] = 0;
  char *save = nullptr;
  char *w0 = strtok_r(line, " \t", &save);
  char *w1 = strtok_r(nullptr, " \t", &save);
  char *w2 = strtok_r(nullptr, " \t", &save);

  if (!strcmp(w0, "PING")) {
    sendLine("{\"t\":\"pong\"}");
    protoHello();
  } else if (!strcmp(w0, "SEEN")) {
    reply("SEEN", g_line.seen());
  } else if (!strcmp(w0, "PASS")) {
    reply("PASS", g_line.pass());
  } else if (!strcmp(w0, "FAIL")) {
    reply("FAIL", g_line.fail());
  } else if (!strcmp(w0, "HOLD")) {
    reply("HOLD", g_line.hold());
  } else if (!strcmp(w0, "RESUME")) {
    reply("RESUME", g_line.resume());
  } else if (!strcmp(w0, "STOP")) {
    reply("STOP", g_line.stop());
  } else if (!strcmp(w0, "START")) {
    reply("START", g_line.start());
  } else if (!strcmp(w0, "REJECT")) {
    protoAck("REJECT");
    gateFire(0, g_cfg.gate_open_ms);  // n = 0: manual test, not a part
  } else if (!strcmp(w0, "BELT")) {
    if (!w1 || (strcmp(w1, "1") && strcmp(w1, "0"))) protoErr("BELT needs 1 or 0");
    else reply(full, g_line.manualBelt(w1[0] == '1'));
  } else if (!strcmp(w0, "RESET")) {
    g_line.resetCounters();
    gateResetCount();
    protoAck("RESET");
  } else if (!strcmp(w0, "SET")) {
    cmdSet(full, w1, w2);
  } else {
    char msg[80];
    snprintf(msg, sizeof msg, "unknown command %s", w0);
    protoErr(msg);
  }
}

void protoPoll() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (rxOverflow) protoErr("command too long");
      else if (rxLen) {
        rx[rxLen] = 0;
        runCommand(rx);
      }
      rxLen = 0;
      rxOverflow = false;
    } else if (rxLen < RX_MAX) {
      rx[rxLen++] = c;
    } else {
      rxOverflow = true;
    }
  }
}
