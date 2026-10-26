#!/usr/bin/env python3
"""Time the same while with s[i] and with s.ptr[i]. Not part of make test."""
import datetime
import statistics
import subprocess
import sys
import time

ROUNDS = 100000
EXPECT = 523776 * ROUNDS
CC = sys.argv[1] if len(sys.argv) > 1 else "compiler/bootstrap-c/copihue"
PIN = ["taskset", "-c", "0"]


def compile_one(src, opt, dest):
    r = subprocess.run([CC, opt, "-o", dest, src], capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        raise SystemExit(1)


def once(bin):
    t0 = time.perf_counter()
    r = subprocess.run(PIN + [bin], capture_output=True)
    ms = (time.perf_counter() - t0) * 1000.0
    if r.returncode != 0:
        sys.stderr.write(r.stderr.decode())
        raise SystemExit(f"{bin} exit {r.returncode}")
    got = r.stdout.decode().strip()
    if got != str(EXPECT):
        raise SystemExit(f"{bin} checksum {got}, expected {EXPECT}")
    return ms


def measure(bin):
    once(bin)  # warmup
    samples = [once(bin) for _ in range(7)]
    samples.sort()
    return samples[3], samples[0], samples[6], samples


def main():
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    print(f"window {stamp}")
    print(f"checksum {EXPECT}")
    for opt in ("-O2", "-O0"):
        compile_one("bench/slice/checked.lap", opt, "/tmp/slice_checked")
        compile_one("bench/slice/unchecked.lap", opt, "/tmp/slice_unchecked")
        c = measure("/tmp/slice_checked")
        u = measure("/tmp/slice_unchecked")
        print(f"{opt} checked {c[0]:.1f} ({c[1]:.1f}–{c[2]:.1f}) {['%.1f' % x for x in c[3]]}")
        print(f"{opt} unchecked {u[0]:.1f} ({u[1]:.1f}–{u[2]:.1f}) {['%.1f' % x for x in u[3]]}")


if __name__ == "__main__":
    main()
