# PROGRESS (nazar-firmware)

Restart rule: read this file and `git log` first; never redo a step marked Done.

## Done (all steps of the brief)
- `.gitignore`, `README.md`
- main sketch `nazar_line/` (fw 2.0.0): config.h, line.h/.cpp, sensors.h/.cpp, gate.h/.cpp,
  belt.h/.cpp, proto.h/.cpp, nazar_line.ino
- `bringup/01_i2c_scan` .. `08_relay_belt`
- `tools/serial_check.py` (stdlib termios); `--selftest` PASS (10 good + 4 bad contract lines,
  pty round trip PING -> pong/hello, HOLD -> ack)
- `test/line_test.cpp` + `test/Makefile`: `make -C test` -> 55 checks PASS (11 scenarios);
  breaking the gate countdown on purpose makes 8 checks fail
- `docs/firmware.md` -> `docs/NAZAR-01-Firmware.pdf` (5 pages, checked visually), copied to
  `../PDFs/NAZAR-01-Firmware.pdf`

## Compile results (esp32:esp32:esp32, core 3.3.12, --warnings all; no warnings in our files)
| Sketch | Flash | RAM |
|---|---|---|
| nazar_line | 316091 B (24%) | 24112 B (7%) |
| 01_i2c_scan | 293996 B | |
| 02_ina219 | 294336 B | |
| 03_mpu6050 | 294940 B | |
| 04_bh1750 | 293976 B | |
| 05_ds18b20 | 276620 B | |
| 06_fc51_ir | 272124 B | |
| 07_servo_gate | 278987 B | |
| 08_relay_belt | 272456 B | |

## Not verified (no board connected)
- real sensor readings / scaling, servo angles, relay release by floating pin,
  real stop_ms / gate_ms for the belt, loop timing under load

## Next
- on site: bring-up 01-08, upload nazar_line, `python3 tools/serial_check.py`, tune stop_ms / gate_ms
