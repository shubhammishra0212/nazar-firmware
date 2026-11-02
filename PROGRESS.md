# PROGRESS (nazar-firmware)

Restart rule: read this file and `git log` first; never redo a step marked Done.

## Done
- repo skeleton, `.gitignore`

## In progress
- main sketch `nazar_line/` (config.h, proto, sensors, line, gate, belt, .ino)

## Next (in order)
1. compile main sketch with arduino-cli, push
2. `bringup/01_i2c_scan` .. `08_relay_belt`, compile each, push
3. `tools/serial_check.py` (stdlib only)
4. `test/` host logic test of `line.cpp` (clang++)
5. `README.md`
6. `docs/firmware.md` -> `docs/NAZAR-01-Firmware.pdf`, copy to `../PDFs/`
