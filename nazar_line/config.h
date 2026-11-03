// config.h - pins, addresses and default timings for the NAZAR line ESP32.
// Pure C++ (no Arduino.h) so the host-side logic test can include it too.
// Pin map and timing defaults follow CONTRACT.md (hub repo) sections 2.2 and 3.
#pragma once
#include <stdint.h>

#define NAZAR_FW_VERSION "2.2.0"

// ---------- pins (ESP32 DevKitC, wiring v5 + relay)
constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;
constexpr int PIN_IR = 18;          // FC-51 OUT, LOW = part in front
constexpr int PIN_DS18B20 = 4;      // 4.7 k pull-up to 3V3
constexpr int PIN_GATE_SERVO = 13;  // MG996R signal through 330 R
constexpr int PIN_BELT_RELAY = 26;  // relay IN (see RELAY_ACTIVE_HIGH)
// Our 1-CH board drives the coil with a BC547 (NPN): IN HIGH = relay on = belt runs, LOW = stop.
// Set false for an optocoupler module with an H/L jumper on L (LOW = run, released/INPUT = stop).
constexpr bool RELAY_ACTIVE_HIGH = true;

// ---------- I2C
constexpr uint8_t I2C_INA219 = 0x40;
constexpr uint8_t I2C_MPU6050 = 0x68;
constexpr uint8_t I2C_BH1750 = 0x23;
constexpr uint32_t I2C_HZ = 100000;      // 100 kHz: safer than 400 kHz on long jumper wires
constexpr uint16_t I2C_TIMEOUT_MS = 20;  // a stuck bus can never freeze the loop for long

// ---------- sensors
constexpr float INA219_SHUNT_OHM = 0.1f;   // "R100" on the INA219 board
constexpr uint32_t MPU_PERIOD_MS = 5;      // 200 Hz vibration sampling
constexpr float VIB_MEAN_ALPHA = 0.01f;    // slow average of |a| (time constant ~0.5 s at 200 Hz)
constexpr uint32_t DS_PERIOD_MS = 1000;    // one temperature reading per second
constexpr uint32_t DS_CONVERT_MS = 400;    // 11-bit conversion needs 375 ms
constexpr uint32_t SENSOR_RETRY_MS = 3000; // missing sensors are looked for again this often
constexpr uint8_t SENSOR_MAX_FAILS = 10;   // this many failed reads in a row = sensor gone

// ---------- telemetry and line
constexpr uint32_t TEL_PERIOD_MS = 100;
constexpr uint32_t IR_DEBOUNCE_MS = 20;
constexpr bool BOOT_STOPPED = false;       // false = line starts in RUN (belt on) after a reboot
constexpr int ARRIVAL_SLOTS = 4;           // parts between the IR sensor and the camera
constexpr int GATE_SLOTS = 8;              // FAIL parts between the camera and the gate

// ---------- reject gate servo (MG996R)
// angles are in LineCfg (park_deg / block_deg) so they can be tuned with SET while the line runs
constexpr float GATE_OUT_DEG_S = 400.0f;    // swing across about as fast as an MG996R can go, in small steps
constexpr float GATE_BACK_DEG_S = 200.0f;   // park gently
constexpr uint32_t GATE_IDLE_MS = 600;      // parked this long -> pulses off (no hunting / flutter at rest)
constexpr uint32_t SERVO_HZ = 50;
constexpr uint8_t SERVO_BITS = 14;         // 20 ms / 16384 = 1.2 us per step
constexpr uint32_t SERVO_MIN_US = 500;     // pulse at 0 deg
constexpr uint32_t SERVO_MAX_US = 2500;    // pulse at 180 deg

// ---------- timing config (CONTRACT 2.2); changed at run time with SET <key> <int>
struct LineCfg {
  uint32_t stop_ms = 150;       // IR edge -> part centred under the camera (belt running time)
  uint32_t dwell_ms = 1200;     // minimum stop under the camera
  uint32_t wait_ms = 5000;      // no verdict by then -> go "timeout" (part passes)
  uint32_t gate_ms = 800;       // belt running time from restart until the arm swings across for a FAIL
  uint32_t gate_open_ms = 600;  // kick 1 only: how long the arm stays out (belt running time; clock time for REJECT)
  uint32_t stops = 1;           // 1 = stop-and-inspect, 0 = continuous
  uint32_t cam = 1;             // 1 = the console camera finds each washer (SEEN), IR ignored; 0 = FC-51 IR sensor
  uint32_t kick = 0;            // 0 = stop gate: arm across the belt for a reject until the next good part; 1 = kick
  uint32_t park_deg = 0;        // servo angle with the arm parked along the rail: washers pass (0..180)
  uint32_t block_deg = 90;      // servo angle with the arm square across the belt: the reject is stopped
};

constexpr uint32_t CFG_MAX_MS = 60000;
