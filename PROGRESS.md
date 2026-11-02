# PROGRESS (nazar-firmware)

Restart rule: read this file and `git log` first; never redo a step marked Done.

## Done
- repo skeleton, `.gitignore`
- main sketch `nazar_line/`: config.h, line.h/.cpp, sensors.h/.cpp, gate.h/.cpp, belt.h/.cpp,
  proto.h/.cpp, nazar_line.ino. Compiled clean (esp32:esp32:esp32, core 3.3.12, --warnings all:
  no warnings in our files; OneWire lib has its own). 316103 B flash (24%), 24112 B RAM (7%).
- `bringup/01_i2c_scan` .. `08_relay_belt` written; 01 compiles (294 KB); 02-08 compile running
- `tools/serial_check.py` (stdlib termios). `--selftest` PASS.
- `test/line_test.cpp` + `test/Makefile`: `make -C test` -> 55 checks PASS (11 scenarios);
  a mutation (gate timer never counting down) makes 8 checks fail, so the test has teeth.
- `README.md`
- `docs/firmware.md` -> `docs/NAZAR-01-Firmware.pdf` (5 pages, checked visually), copied to
  `../PDFs/NAZAR-01-Firmware.pdf`

## In progress
- bring-up compile results; recompile nazar_line after the line.cpp reorder

## Next (in order)
1. finish compile table (bring-up 03-08 + nazar_line recompile), then report to the lead
