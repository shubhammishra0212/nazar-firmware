# nazar-firmware

ESP32 firmware for **NAZAR** (INDUX 5.0, Team Parakh): a mini conveyor carries washers through
a light tunnel, a camera + AI judges each one, and this board runs the belt, reads the machine
sensors and fires the reject gate. It talks to the NAZAR Console on the Mac over USB serial,
one JSON object per line.

The protocol, timings and pin map are defined in **CONTRACT.md** in the hub repo
[github.com/shubhammishra0212/nazar](https://github.com/shubhammishra0212/nazar). If this
README and the contract disagree, the contract wins.

## Folders

| Path | What |
|---|---|
| `nazar_line/` | The real firmware (version 2.0.0). `config.h` pins and defaults, `line.*` state machine, `sensors.*`, `gate.*`, `belt.*`, `proto.*` (JSON out, commands in) |
| `bringup/01..08` | One tiny sketch per part, to test each sensor alone while wiring |
| `tools/serial_check.py` | Opens the port, sends PING, checks every line against the contract (Python stdlib only) |
| `test/` | Host-side test of the state machine with a fake clock: `make -C test` |
| `docs/firmware.md` | Plain-language explanation (also as PDF) |

## Hardware and pins

ESP32 DevKitC 38-pin (WROOM, CP2102, shows up as `/dev/cu.usbserial-0001`).

| Part | Connection | Notes |
|---|---|---|
| INA219 (belt V / mA) | I²C SDA 21, SCL 22, addr 0x40 | 0.1 Ω shunt in the 5.5 V belt line |
| MPU-6050 (vibration) | I²C, addr 0x68 | clones answer WHO_AM_I 0x70/0x72/0x98, accepted; uses \|a\| so any mounting angle works |
| BH1750 (tunnel light) | I²C, addr 0x23 | continuous 1 lx mode |
| DS18B20 (motor temp) | GPIO 4 | 4.7 kΩ pull-up DQ to 3V3; read once a second without blocking |
| FC-51 IR (part sensor) | GPIO 18 | LOW = part present, 20 ms debounce |
| MG996R reject gate | GPIO 13 via 330 Ω | 50 Hz LEDC PWM, closed 0°, open 70° (`config.h`) |
| Belt relay IN | GPIO 26 | active HIGH (BC547 board): HIGH = run, LOW = stop; `RELAY_ACTIVE_HIGH` in `config.h` |

The belt motor is a 12 V 25GA-370, run at about half speed from the 5.5 V buck and switched by the relay. Only the LED strip uses the 12 V buck.

## Build and upload

Libraries: **OneWire** and **DallasTemperature** (Library Manager). Board: *ESP32 Dev Module*,
esp32 core 3.x (tested to compile with 3.3.12).

```sh
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile --fqbn esp32:esp32:esp32 nazar_line
"$CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml upload  --fqbn esp32:esp32:esp32 -p /dev/cu.usbserial-0001 nazar_line
python3 tools/serial_check.py          # PING + 5 s of lines, validated
```

If the upload hangs at `Connecting....`, hold the BOOT button until it starts writing.
Close the Serial Monitor / NAZAR Console before uploading or running `serial_check.py`.

## Protocol summary (details: CONTRACT.md sections 1-2)

Out, 115200 baud, one JSON per line:

```json
{"t":"tel","ms":183402,"v":11.92,"ma":312.4,"vib":0.042,"vpk":0.110,"lux":812.5,"temp":34.6,"ir":0,"parts":17,"rej":3,"gate":0,"belt":1,"mode":"RUN"}
```

every 100 ms (a missing sensor's keys are left out), plus `hello` (boot, PING, sensor change),
`part`, `inspect`, `go`, `gate`, `ack`, `err`, `pong`, `log`.

In, plain text: `PING`, `PASS`, `FAIL`, `HOLD`, `RESUME`, `STOP`, `START`, `REJECT`,
`BELT 1|0` (STOP mode only), `RESET`, `SET <stop_ms|dwell_ms|wait_ms|gate_ms|gate_open_ms|stops> <int>`.

Line cycle: IR edge → `part` → belt runs `stop_ms` → belt stops, `inspect` → verdict
(at least `dwell_ms`, at most `wait_ms`, else `timeout` = pass) → `go` → for a FAIL the gate
fires after `gate_ms` of belt **running** time. STOP and HOLD freeze every timer.

Choices the contract leaves open (documented here so the console can rely on them):

- After a reboot the line starts in `RUN` with the belt on (`BOOT_STOPPED` in `config.h`).
- `REJECT` sends `{"t":"gate","n":0}`; `n:0` means manual, not a part. Every gate firing counts in `rej`.
- `stops 0`: `inspect` is still sent when a part reaches the camera, but the belt keeps
  running and `mode` stays `RUN`; no `go` follows. `FAIL` gates the most recent part at
  `stop_ms + gate_ms` of running time after its IR edge.
- `PASS`/`FAIL` with no part waiting, a second verdict, or a verdict while the part is on
  HOLD → `err`. `RESUME`/`START`/`HOLD`/`STOP` are always acked (repeating them is harmless).
- IR edges are ignored while the belt is off (STOP, HOLD or INSPECT).

## Bring-up order (while wiring)

Upload each sketch in `bringup/`, open the Serial Monitor at 115200, follow the PASS/FAIL hints.

1. `01_i2c_scan` - should list 0x23, 0x40, 0x68
2. `02_ina219` - about 0 mA belt off, a few hundred mA belt on
3. `03_mpu6050` - |a| about 1.00 g, vib jumps when you tap the table
4. `04_bh1750` - drops when you cover it
5. `05_ds18b20` - rises when you hold the probe
6. `06_fc51_ir` - `PART n seen` for every washer
7. `07_servo_gate` - `o` / `c` / `s` / angle; tune open and closed angles here
8. `08_relay_belt` - `1` / `0` switch the belt

Then upload `nazar_line` and run `python3 tools/serial_check.py`.

## Tests

```sh
make -C test                              # 55 checks of the state machine, no board needed
python3 tools/serial_check.py --selftest  # validator + termios code on a fake serial port
```
