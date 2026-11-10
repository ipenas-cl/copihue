#!/usr/bin/env bash
# Hidden-pointer returns (0.45.0): a struct by value and a slice. Called by
# tests/run_tests.sh with the compiler path; also runnable alone:   tests/sret/run.sh [path/to/copihue]
#
# 1. Every tests/errors/sret_*.lap is refused in each way the compiler builds (native -O0 and -O2,
#    --nasm, -c, --freestanding, --flat) with the first message of its golden file, on its line,
#    and leaves no output.
# 2. The positive programs (tests/sret/progs.txt) build in each of those ways, and the assembly of
#    each (-S -O0 and -O2, --freestanding, --flat) assembles with nasm.
# 3. QEMU runs tests/sret/kernel.lap (returns) and tests/sret/kernel_field.lap (str / []T fields)
#    at -O0 and -O2 (--flat 0x8000, freestanding code generation).
#    Without qemu-system-x86_64 and perl it is skipped; CMP_REQUIRE=1 (the CI) makes that a failure.
# The hosted executables are Linux ELF files and are not run here; tests/run_tests.sh runs them on
# Linux and compares them with tests/expected/tests/lang.
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT" || exit 1
CC=${1:-$ROOT/compiler/bootstrap-c/copihue}
[ -x "$CC" ] || { echo "tests/sret/run.sh: no compiler at $CC" >&2; exit 2; }
CC=$(cd "$(dirname "$CC")" && pwd)/$(basename "$CC")
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
fails=0; checks=0
bad() { fails=$((fails+1)); echo "FAIL tests/sret: $*"; }
ok() { checks=$((checks+1)); }
HAVE_NASM=0; command -v nasm >/dev/null 2>&1 && HAVE_NASM=1

# 1. refusals
for src in tests/errors/sret_*.lap; do
  [ -e "$src" ] || continue
  name=$(basename "$src" .lap)
  want=$(grep -m1 'error:' "tests/expected/errors/$name.err" | sed 's/^[^ ]*: error: //')
  # --flat needs an entry point; the same source with main renamed keeps every line number.
  sed 's/^fn main()/fn _start()/' "$src" > "$WORK/flat_$name.lap"
  S=$ROOT/$src; F=$WORK/flat_$name.lap
  for mode in "-O0 -o prog.out $S" "-O2 -o prog.out $S" "--nasm -O2 -o prog.out $S" "-c -o prog.o $S" \
              "--freestanding -o prog.asm $S" "--flat 0x10000 -o prog.bin $F"; do
    rm -f "$WORK"/prog.out "$WORK"/prog.o "$WORK"/prog.asm "$WORK"/prog.bin "$WORK"/output.asm "$WORK"/output.o
    # shellcheck disable=SC2086
    ( cd "$WORK" && "$CC" $mode >/dev/null 2>err.txt ) && { bad "$name [$mode] compiled but should have failed"; continue; }
    got=$(grep -m1 'error:' "$WORK/err.txt" | sed 's/^[^ ]*: error: //')
    [ "$got" = "$want" ] || bad "$name [$mode]: message '$got', expected '$want'"
    wl=$(grep -m1 'error:' "tests/expected/errors/$name.err" | sed -E 's/^[^:]*:([0-9]+):.*/\1/')
    gl=$(grep -m1 'error:' "$WORK/err.txt" | sed -E 's/^[^:]*:([0-9]+):.*/\1/')
    [ "$wl" = "$gl" ] || bad "$name [$mode]: line $gl, expected $wl"
    for f in prog.out prog.o prog.asm prog.bin output.asm output.o; do
      [ -e "$WORK/$f" ] && bad "$name [$mode]: left $f behind"
    done
    ok
  done
done

# 2. the positive programs in every mode
while IFS= read -r src; do
  case $src in ''|'#'*) continue ;; esac
  S=$ROOT/$src
  for opt in -O0 -O2; do
    ( cd "$WORK" && "$CC" $opt -o nat.out "$S" ) 2>"$WORK/err.txt" && [ -s "$WORK/nat.out" ] || bad "$src native $opt: $(head -3 "$WORK/err.txt")"
    ok
    ( cd "$WORK" && "$CC" -c $opt -o nat.o "$S" ) 2>"$WORK/err.txt" && [ -s "$WORK/nat.o" ] || bad "$src -c $opt: $(head -3 "$WORK/err.txt")"
    ok
    "$CC" -S $opt -o "$WORK/h.asm" "$S" 2>"$WORK/err.txt" || bad "$src -S $opt: $(head -3 "$WORK/err.txt")"
    ok
    if [ $HAVE_NASM = 1 ] && [ -s "$WORK/h.asm" ]; then
      # The --nasm path assembles this same text.
      nasm -f elf64 -o "$WORK/h.o" "$WORK/h.asm" 2>"$WORK/err.txt" || bad "$src -S $opt: nasm refused it: $(head -3 "$WORK/err.txt")"
      ok
    fi
  done
done < tests/sret/progs.txt
# The kernels are the freestanding and flat case (print needs the hosted runtime).
KERNELS="tests/sret/kernel.lap tests/sret/kernel_field.lap"
for k in $KERNELS; do
  for opt in -O0 -O2; do
    "$CC" --freestanding $opt -o "$WORK/f.asm" "$k" 2>"$WORK/err.txt" || bad "$k --freestanding $opt: $(head -3 "$WORK/err.txt")"
    ( cd "$WORK" && "$CC" --flat 0x8000 -S $opt -o fl.asm "$ROOT/$k" ) 2>"$WORK/err.txt" || bad "$k --flat -S $opt: $(head -3 "$WORK/err.txt")"
    ok
    if [ $HAVE_NASM = 1 ] && [ -s "$WORK/f.asm" ]; then
      nasm -f elf64 -o "$WORK/f.o" "$WORK/f.asm" 2>"$WORK/err.txt" || bad "$k --freestanding $opt: nasm refused it: $(head -3 "$WORK/err.txt")"
      ok
    fi
  done
done

# 3. QEMU
if command -v qemu-system-x86_64 >/dev/null 2>&1 && command -v perl >/dev/null 2>&1; then
  if ! ( cd "$WORK" && "$CC" --boot -o boot.bin "$ROOT/tests/usermode/boot.lap" 2>err.txt ); then
    bad "boot sector: $(cat "$WORK/err.txt")"
  else
    for k in $KERNELS; do
    for opt in -O0 -O2; do
      if ! ( cd "$WORK" && "$CC" $opt --flat 0x8000 -o kern.bin "$ROOT/$k" 2>err.txt ); then
        bad "$k $opt: $(cat "$WORK/err.txt")"; continue
      fi
      if [ "$(wc -c < "$WORK/kern.bin" | tr -d ' ')" -gt 8192 ]; then bad "$k $opt: larger than the 16 sectors the boot sector reads"; continue; fi
      dd if=/dev/zero of="$WORK/pad.bin" bs=8192 count=1 2>/dev/null
      dd if="$WORK/kern.bin" of="$WORK/pad.bin" conv=notrunc 2>/dev/null
      cat "$WORK/boot.bin" "$WORK/pad.bin" > "$WORK/disk.img"
      rm -f "$WORK/serial.txt"
      perl -e 'my $p = fork(); if (!$p) { exec @ARGV; exit 127 } $SIG{ALRM} = sub { kill "KILL", $p }; alarm 8; waitpid($p, 0);' \
        qemu-system-x86_64 -display none -boot a -drive file="$WORK/disk.img",format=raw,if=floppy,index=0 \
        -serial file:"$WORK/serial.txt" </dev/null >/dev/null 2>&1 || true
      grep -q SRET "$WORK/serial.txt" 2>/dev/null || bad "$k $opt: COM1 should be SRET, got '$(cat "$WORK/serial.txt" 2>/dev/null)'"
      ok
    done
    done
  fi
elif [ "${CMP_REQUIRE:-0}" = 1 ]; then
  bad "CMP_REQUIRE=1 and qemu-system-x86_64 or perl is missing"
else
  echo "skip tests/sret QEMU run: qemu-system-x86_64 or perl is not installed (CMP_REQUIRE=1 makes this an error)"
fi

echo "tests/sret: $checks checks, $fails failed"
[ "$fails" = 0 ]
