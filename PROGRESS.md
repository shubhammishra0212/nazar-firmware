# PROGRESS (nazar-firmware)

Restart rule: read this file and `git log` first; never redo a step marked Done.

## Done
- repo skeleton, `.gitignore`
- main sketch `nazar_line/`: config.h, line.h/.cpp, sensors.h/.cpp, gate.h/.cpp, belt.h/.cpp,
  proto.h/.cpp, nazar_line.ino. Compiles clean (esp32:esp32:esp32, core 3.3.12, --warnings all:
  no warnings in our files; OneWire lib has its own). 316103 B flash (24%), 24112 B RAM (7%).

## In progress
- bring-up sketches

## Next (in order)
2. `bringup/01_i2c_scan` .. `08_relay_belt`, compile each, push
3. `tools/serial_check.py` (stdlib only)
4. `test/` host logic test of `line.cpp` (clang++)
5. `README.md`
6. `docs/firmware.md` -> `docs/NAZAR-01-Firmware.pdf`, copy to `../PDFs/`
