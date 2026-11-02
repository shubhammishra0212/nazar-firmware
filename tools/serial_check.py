#!/usr/bin/env python3
"""NAZAR serial check - talk to the line ESP32 and validate its JSON lines against CONTRACT.md.

Python 3 standard library only (termios), no pyserial. Close the Arduino Serial Monitor
and the NAZAR Console first: only one program can hold the port.

    python3 tools/serial_check.py                       # PING, listen 5 s, summary
    python3 tools/serial_check.py -s 20 -q              # listen 20 s, hide tel lines
    python3 tools/serial_check.py "SET stop_ms 200" PASS  # send commands after the PING
    python3 tools/serial_check.py -p /dev/cu.usbserial-0001
    python3 tools/serial_check.py --selftest            # no board: checks itself on a fake port

Exit code 0 = every JSON line matched the contract, 1 = problems found, 2 = port error.
"""
import argparse
import glob
import json
import os
import select
import sys
import termios
import threading
import time

MODES = {"RUN", "INSPECT", "HOLD", "STOP"}
GO_RES = {"pass", "reject", "hold-released", "timeout"}
SENSORS = ("ina219", "mpu6050", "bh1750", "ds18b20")
CFG_KEYS = ("stop_ms", "dwell_ms", "wait_ms", "gate_ms", "gate_open_ms", "stops")
# telemetry keys each sensor owns (missing sensor = keys left out)
TEL_SENSOR_KEYS = {"ina219": ("v", "ma"), "mpu6050": ("vib", "vpk"), "bh1750": ("lux",), "ds18b20": ("temp",)}
TEL_ALWAYS = ("ms", "ir", "parts", "rej", "gate", "belt", "mode")


def is_int(x):
    return isinstance(x, int) and not isinstance(x, bool)


def is_num(x):
    return isinstance(x, (int, float)) and not isinstance(x, bool)


def validate(msg):
    """Return a list of problems for one decoded JSON object ([] = matches the contract)."""
    p = []
    if not isinstance(msg, dict):
        return ["not a JSON object"]
    t = msg.get("t")
    if not isinstance(t, str):
        return ['no "t" field']

    def need(key, check, what):
        if key not in msg:
            p.append(f'{t}: missing "{key}"')
        elif not check(msg[key]):
            p.append(f'{t}: "{key}" should be {what}, got {msg[key]!r}')

    def only(keys):
        extra = set(msg) - set(keys) - {"t"}
        if extra:
            p.append(f"{t}: unexpected keys {sorted(extra)}")

    if t == "tel":
        need("ms", is_int, "int")
        for k in ("ir", "gate", "belt"):
            need(k, lambda x: x in (0, 1) and is_int(x), "0 or 1")
        for k in ("parts", "rej"):
            need(k, lambda x: is_int(x) and x >= 0, "int >= 0")
        need("mode", lambda x: x in MODES, "/".join(sorted(MODES)))
        for keys in TEL_SENSOR_KEYS.values():
            there = [k for k in keys if k in msg]
            if there and len(there) != len(keys):
                p.append(f"tel: {keys} must come together, got only {there}")
            for k in there:
                if not is_num(msg[k]):
                    p.append(f'tel: "{k}" should be a number, got {msg[k]!r}')
        only(TEL_ALWAYS + tuple(k for ks in TEL_SENSOR_KEYS.values() for k in ks))
    elif t == "hello":
        need("fw", lambda x: isinstance(x, str), "text")
        need("sensors", lambda x: isinstance(x, dict) and all(isinstance(x.get(s), bool) for s in SENSORS),
             "object with 4 booleans " + ",".join(SENSORS))
        need("cfg", lambda x: isinstance(x, dict) and all(is_int(x.get(k)) for k in CFG_KEYS),
             "object with ints " + ",".join(CFG_KEYS))
        only(("fw", "sensors", "cfg"))
    elif t in ("part", "inspect", "gate"):
        need("n", lambda x: is_int(x) and x >= 0, "int >= 0")
        only(("n",))
    elif t == "go":
        need("n", lambda x: is_int(x) and x >= 0, "int >= 0")
        need("res", lambda x: x in GO_RES, "/".join(sorted(GO_RES)))
        only(("n", "res"))
    elif t == "ack":
        need("cmd", lambda x: isinstance(x, str), "text")
        only(("cmd",))
    elif t in ("err", "log"):
        need("msg", lambda x: isinstance(x, str), "text")
        only(("msg",))
    elif t == "pong":
        only(())
    else:
        p.append(f"unknown type {t!r} (the host would ignore it)")
    return p


# ---------- serial port with termios
def open_port(path, baud=115200):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    iflag, oflag, cflag, lflag, _, _, cc = attrs
    iflag = 0                                   # raw: no CR/LF translation, no flow control
    oflag = 0
    lflag = 0
    cflag = (cflag & ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)) | termios.CS8 | termios.CREAD | termios.CLOCAL
    if hasattr(termios, "CRTSCTS"):
        cflag &= ~termios.CRTSCTS
    speed = getattr(termios, f"B{baud}")
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, [iflag, oflag, cflag, lflag, speed, speed, cc])
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def send(fd, text):
    os.write(fd, (text.strip() + "\n").encode())


def read_lines(fd, seconds):
    """Yield decoded text lines until `seconds` have passed."""
    buf = b""
    end = time.time() + seconds
    while True:
        left = end - time.time()
        if left <= 0:
            return
        r, _, _ = select.select([fd], [], [], min(left, 0.2))
        if not r:
            continue
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not chunk:
            time.sleep(0.05)
            continue
        buf += chunk
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            yield raw.decode("utf-8", "replace").strip("\r")


# ---------- the check
def run(fd, commands, seconds, quiet, out=sys.stdout):
    counts, problems, last_hello, tel_keys = {}, [], None, set()
    noise = 0
    send(fd, "PING")
    pending = list(commands)
    started = time.time()
    for line in read_lines(fd, seconds):
        if pending and time.time() - started > 0.5:  # give PING its answer first
            cmd = pending.pop(0)
            print(f">> {cmd}", file=out)
            send(fd, cmd)
        if not line.strip():
            continue
        try:
            msg = json.loads(line)
        except ValueError:
            noise += 1
            print(f"   (not JSON, ignored) {line[:100]}", file=out)
            continue
        t = msg.get("t") if isinstance(msg, dict) else None
        counts[t] = counts.get(t, 0) + 1
        bad = validate(msg)
        problems += bad
        if t == "hello":
            last_hello = msg
        if t == "tel":
            tel_keys |= set(msg)
        if bad or not (quiet and t == "tel"):
            print(("!! " if bad else "   ") + line, file=out)
            for b in bad:
                print(f"      -> {b}", file=out)
    for cmd in pending:  # very short listen: still send what was asked
        send(fd, cmd)

    print("\n--- summary", file=out)
    print("messages: " + (", ".join(f"{k}={v}" for k, v in sorted(counts.items(), key=str)) or "none"), file=out)
    if noise:
        print(f"non-JSON lines: {noise} (boot ROM noise is normal right after a reset)", file=out)
    tel_n = counts.get("tel", 0)
    if tel_n:
        rate = tel_n / seconds
        print(f"telemetry: {tel_n} lines in {seconds:.0f} s = {rate:.1f}/s (contract: 10/s)", file=out)
    if last_hello:
        print(f"firmware: {last_hello.get('fw')}   cfg: {last_hello.get('cfg')}", file=out)
        for s in SENSORS:
            said = last_hello.get("sensors", {}).get(s)
            seen = any(k in tel_keys for k in TEL_SENSOR_KEYS[s])
            note = "" if not tel_n or said == seen else "  (hello and telemetry disagree)"
            print(f"  {s:8s} {'present' if said else 'MISSING'}{note}", file=out)
    else:
        print("no hello received: wrong port, wrong baud, or the board is not running nazar_line", file=out)
        problems.append("no hello")
    if problems:
        print(f"PROBLEMS: {len(problems)}", file=out)
        return 1
    print("OK: every line matches CONTRACT.md", file=out)
    return 0


def find_port():
    ports = sorted(glob.glob("/dev/cu.usbserial*") + glob.glob("/dev/cu.SLAB*") + glob.glob("/dev/ttyUSB*"))
    return ports[0] if ports else None


# ---------- self test on a pseudo-terminal (no board needed)
def selftest():
    import pty
    good = [
        '{"t":"hello","fw":"2.0.0","sensors":{"ina219":true,"mpu6050":true,"bh1750":false,"ds18b20":true},'
        '"cfg":{"stop_ms":150,"dwell_ms":1200,"wait_ms":5000,"gate_ms":800,"gate_open_ms":600,"stops":1}}',
        '{"t":"tel","ms":183402,"v":11.92,"ma":312.4,"vib":0.042,"vpk":0.110,"temp":34.6,'
        '"ir":0,"parts":17,"rej":3,"gate":0,"belt":1,"mode":"RUN"}',
        '{"t":"part","n":18}', '{"t":"inspect","n":18}', '{"t":"go","n":18,"res":"pass"}',
        '{"t":"gate","n":18}', '{"t":"ack","cmd":"HOLD"}', '{"t":"err","msg":"unknown command FOO"}',
        '{"t":"pong"}', '{"t":"log","msg":"hi"}',
    ]
    bad = [
        '{"t":"tel","ms":1,"v":12.0,"ir":0,"parts":1,"rej":0,"gate":0,"belt":1,"mode":"RUN"}',  # v without ma
        '{"t":"tel","ms":1,"ir":2,"parts":1,"rej":0,"gate":0,"belt":1,"mode":"GO"}',             # ir 2, bad mode
        '{"t":"go","n":3,"res":"maybe"}',
        '{"t":"hello","fw":"2.0.0","sensors":{},"cfg":{}}',
    ]
    fails = 0
    for s in good:
        if validate(json.loads(s)):
            print("selftest: good line rejected:", s, validate(json.loads(s)))
            fails += 1
    for s in bad:
        if not validate(json.loads(s)):
            print("selftest: bad line accepted:", s)
            fails += 1

    master, slave = pty.openpty()
    name = os.ttyname(slave)
    got = []

    def fake_board():  # answers PING like the firmware, then streams telemetry
        buf = b""
        end = time.time() + 2.5
        while time.time() < end:
            r, _, _ = select.select([master], [], [], 0.1)
            if r:
                buf += os.read(master, 1024)
                while b"\n" in buf:
                    cmd, buf = buf.split(b"\n", 1)
                    got.append(cmd.decode().strip())
                    if cmd.strip().upper() == b"PING":
                        os.write(master, b'\x00ets Jun  8 2016 boot noise\r\n{"t":"pong"}\n' + good[0].encode() + b"\n")
                    else:
                        os.write(master, b'{"t":"ack","cmd":"' + cmd.strip().upper() + b'"}\n')
            os.write(master, good[1].encode() + b"\n")

    th = threading.Thread(target=fake_board, daemon=True)
    th.start()
    fd = open_port(name)
    import io
    sink = io.StringIO()
    rc = run(fd, ["HOLD"], 2.0, True, out=sink)
    os.close(fd)
    th.join()
    if rc != 0 or got[:1] != ["PING"] or "HOLD" not in got:
        print(sink.getvalue())
        print("selftest: port round trip failed, board got", got)
        fails += 1
    print("selftest:", "PASS" if not fails else f"FAIL ({fails})",
          f"- {len(good)} good + {len(bad)} bad sample lines, pty round trip with PING/HOLD")
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("commands", nargs="*", help="commands to send after PING, e.g. HOLD or 'SET stop_ms 200'")
    ap.add_argument("-p", "--port", help="serial port (default: first /dev/cu.usbserial*)")
    ap.add_argument("-s", "--seconds", type=float, default=5, help="how long to listen (default 5)")
    ap.add_argument("-q", "--quiet", action="store_true", help="do not print valid tel lines")
    ap.add_argument("--selftest", action="store_true", help="test this script without a board")
    a = ap.parse_args()
    if a.selftest:
        sys.exit(selftest())
    port = a.port or find_port()
    if not port:
        print("no USB serial port found: plug in the ESP32 (CP2102 shows as /dev/cu.usbserial-0001)")
        sys.exit(2)
    try:
        fd = open_port(port)
    except OSError as e:
        print(f"cannot open {port}: {e} (is the Serial Monitor or NAZAR Console still open?)")
        sys.exit(2)
    print(f"port {port} @ 115200, listening {a.seconds:.0f} s")
    try:
        rc = run(fd, a.commands, a.seconds, a.quiet)
    finally:
        os.close(fd)
    sys.exit(rc)


if __name__ == "__main__":
    main()
