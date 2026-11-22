#!/usr/bin/env python3
"""Socket client for the daemon tests: behaviours ds4-ctl would never produce.

usage: sockclient.py <socket> <mode> [args...]
  raw                   read stdin, send it as one unterminated command, print the reply
  idle <seconds>        connect, send nothing, hold the connection open
  subscribe <n> <secs>  open n `test` subscriptions and hold them
  reset <n>             n times: send `status`, then close with a TCP-style RST
  fuzz <n> <seed>       send n random commands built from the daemon's verbs;
                        prints how many connections were refused
"""
import random
import socket
import struct
import sys
import time


def connect(path, timeout=5.0):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(timeout)
    s.connect(path)
    return s


def raw(path):
    s = connect(path)
    s.sendall(sys.stdin.buffer.read())
    s.shutdown(socket.SHUT_WR)
    out = b""
    while True:
        chunk = s.recv(4096)
        if not chunk:
            break
        out += chunk
    sys.stdout.write(out.decode(errors="replace"))


def idle(path, seconds):
    s = connect(path)
    time.sleep(seconds)
    s.close()


def subscribe(path, count, seconds):
    held = []
    for _ in range(count):
        try:
            s = connect(path, 2.0)
            s.sendall(b"test")
            held.append(s)
        except OSError:
            break
    print(len(held))
    sys.stdout.flush()
    time.sleep(seconds)


def reset(path, count):
    for _ in range(count):
        try:
            s = connect(path, 2.0)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
            s.sendall(b"status")
            s.close()
        except OSError:
            pass


VERBS = [
    "status", "set-type", "set-backend", "set-name", "set-hide-method", "set-mac-squat",
    "forget-mac", "led", "create-virtual", "destroy-virtual", "release-physical",
    "resume-physical", "input", "identify", "test", "test all", "led --reset",
]
TOKENS = ["ds4", "dualsense", "xbox360", "none", "hidden", "uhid", "functionfs", "on", "off",
          "--reset", "-1", "0", "255", "256", "4294967295", "4294967296", "99999999999999999999",
          "0x10", "", " ", "\x00", "\x1b[2J", "%s%s%s", "../../etc/passwd", "A" * 300, "ds4\x00x"]


def fuzz(path, count, seed):
    rng = random.Random(seed)
    refused = 0
    for _ in range(count):
        parts = [rng.choice(VERBS)] + [rng.choice(TOKENS) for _ in range(rng.randint(0, 8))]
        payload = " ".join(parts).encode(errors="replace")[:300]
        if rng.random() < 0.1:
            payload = bytes(rng.getrandbits(8) for _ in range(rng.randint(1, 400)))
        try:
            s = connect(path, 1.0)
            s.sendall(payload)
            s.shutdown(socket.SHUT_WR)
            try:
                s.recv(4096)
            except OSError:
                pass
            s.close()
        except (ConnectionRefusedError, FileNotFoundError):
            refused += 1
        except OSError:
            pass
    print(refused)


if __name__ == "__main__":
    path, mode, args = sys.argv[1], sys.argv[2], sys.argv[3:]
    if mode == "raw":
        raw(path)
    elif mode == "idle":
        idle(path, float(args[0]))
    elif mode == "subscribe":
        subscribe(path, int(args[0]), float(args[1]))
    elif mode == "reset":
        reset(path, int(args[0]))
    elif mode == "fuzz":
        fuzz(path, int(args[0]), int(args[1]))
    else:
        sys.exit("unknown mode " + mode)
