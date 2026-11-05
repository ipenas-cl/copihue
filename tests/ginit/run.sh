#!/usr/bin/env bash
# Initializers of globals (0.44.5). Called by tests/run_tests.sh with the compiler path; also runnable alone:
#   tests/ginit/run.sh [path/to/copihue]
#
# 1. Every tests/errors/ginit_*.lap is refused in each way the compiler builds (native -O0 and -O2,
#    --nasm, -c, --freestanding, --flat, --axe1, --axe2, --elf) with the first message of its golden file, on its line, and
#    leaves no output.
# 2. The constant expressions are folded into .data: the lines of tests/ginit/data.txt are in the
#    assembly of tests/lang/global_init.lap (-S, -O0 and -O2) and of tests/ginit/kernel.lap
#    (--freestanding and --flat -S, -O0 and -O2), the assembly assembles with nasm, the
#    native build (-O0, -O2, -c) succeeds, and no initialized global is missing from the assembly.
# 3. QEMU runs tests/ginit/kernel.lap (the values read back, past 32 and 63 bits) at -O0 and -O2.
#    Without qemu-system-x86_64 and perl it is skipped; CMP_REQUIRE=1 (the CI) makes that a failure.
# The hosted executables are Linux ELF files and are not run here; the kernel is what runs.
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT" || exit 1
CC=${1:-$ROOT/compiler/bootstrap-c/copihue}
[ -x "$CC" ] || { echo "tests/ginit/run.sh: no compiler at $CC" >&2; exit 2; }
CC=$(cd "$(dirname "$CC")" && pwd)/$(basename "$CC")
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
fails=0; checks=0
bad() { fails=$((fails+1)); echo "FAIL tests/ginit: $*"; }
ok() { checks=$((checks+1)); }

# 1. refusals
for src in tests/errors/ginit_*.lap; do
  name=$(basename "$src" .lap)
  want=$(grep -m1 'error:' "tests/expected/errors/$name.err" | sed 's/^[^ ]*: error: //')
  # --flat needs an entry point; the same source with main renamed keeps every line number.
  sed 's/^fn main()/fn _start()/' "$src" > "$WORK/flat_$name.lap"
  S=$ROOT/$src; F=$WORK/flat_$name.lap
  for mode in "-O0 -o prog.out $S" "-O2 -o prog.out $S" "--nasm -O2 -o prog.out $S" "-c -o prog.o $S" \
              "--freestanding -o prog.asm $S" "--flat 0x10000 -o prog.bin $F" "--flat 0x400000 --axe1 -o prog.bin $F" \
              "--flat 0x400000 --axe2 -o prog.bin $F" "--flat 0x10000 --elf -o prog.bin $F"; do
    rm -f "$WORK"/prog.out "$WORK"/prog.o "$WORK"/prog.asm "$WORK"/prog.bin "$WORK"/output.asm "$WORK"/output.o
    # shellcheck disable=SC2086
    ( cd "$WORK" && "$CC" $mode >/dev/null 2>err.txt ) && { bad "$name [$mode] compiled but should have failed"; continue; }
    got=$(grep -m1 'error:' "$WORK/err.txt" | sed 's/^[^ ]*: error: //')
    [ "$got" = "$want" ] || bad "$name [$mode]: message '$got', expected '$want'"
    wl=$(grep -m1 'error:' "tests/expected/errors/$name.err" | sed -E 's/^[^:]*:([0-9]+):.*/\1/')
    gl=$(grep -m1 'error:' "$WORK/err.txt" | sed -E 's/^[^:]*:([0-9]+):.*/\1/')
    [ "$wl" = "$gl" ] || bad "$name [$mode]: line $gl, expected $wl"
    if grep -q 'undefined\|not defined' "$WORK/err.txt"; then bad "$name [$mode]: a symbol error reached the assembler"; fi
    for f in prog.out prog.o prog.asm prog.bin output.asm output.o; do
      [ -e "$WORK/$f" ] && bad "$name [$mode]: left $f behind"
    done
    ok
  done
done

# 2. the values in .data
HAVE_NASM=0; command -v nasm >/dev/null 2>&1 && HAVE_NASM=1
datachk() {  # $1 = label, $2 = asm file, $3 = data file section name
  local line miss=0
  while IFS= read -r line; do
    case $line in ''|'#'*) continue ;; esac
    grep -qxF -- "$line" "$2" || { bad "$1: missing '$line' in the assembly"; miss=1; }
  done < "tests/ginit/data_$3.txt"
  ok
}
for opt in -O0 -O2; do
  "$CC" -S $opt -o "$WORK/h.asm" tests/lang/global_init.lap 2>"$WORK/err.txt" && datachk "hosted $opt" "$WORK/h.asm" hosted || bad "hosted $opt: $(cat "$WORK/err.txt")"
  if [ $HAVE_NASM = 1 ] && [ -s "$WORK/h.asm" ]; then
    nasm -f elf64 -o "$WORK/h.o" "$WORK/h.asm" 2>"$WORK/err.txt" || bad "hosted $opt: nasm refused the assembly: $(head -3 "$WORK/err.txt")"
    ok
  fi
  "$CC" --freestanding $opt -o "$WORK/f.asm" tests/ginit/kernel.lap 2>"$WORK/err.txt" && datachk "freestanding $opt" "$WORK/f.asm" kernel || bad "freestanding $opt: $(cat "$WORK/err.txt")"
  ( cd "$WORK" && "$CC" --flat 0x8000 -S $opt -o fl.asm "$ROOT/tests/ginit/kernel.lap" ) 2>"$WORK/err.txt" && datachk "flat $opt" "$WORK/fl.asm" kernel || bad "flat $opt: $(cat "$WORK/err.txt")"
  # the compiler's own back end: executable and object
  ( cd "$WORK" && "$CC" $opt -o nat.out "$ROOT/tests/lang/global_init.lap" ) 2>"$WORK/err.txt" && [ -s "$WORK/nat.out" ] || bad "native $opt: $(cat "$WORK/err.txt")"
  ok
  ( cd "$WORK" && "$CC" -c $opt -o nat.o "$ROOT/tests/lang/global_init.lap" ) 2>"$WORK/err.txt" && [ -s "$WORK/nat.o" ] || bad "native -c $opt: $(cat "$WORK/err.txt")"
  ok
  if command -v objdump >/dev/null 2>&1 && objdump -t "$WORK/nat.o" 2>/dev/null | grep -q 'elf64'; then
    for sym in neg k wide tbl bytes msg; do
      objdump -t "$WORK/nat.o" | grep -E "[[:space:]]$sym\$" | grep -q '\.data' || bad "native -c $opt: '$sym' is not defined in .data"
    done
    ok
  else
    echo "skip tests/ginit object symbol check: objdump here does not read ELF"
  fi
done

# 3. QEMU
if command -v qemu-system-x86_64 >/dev/null 2>&1 && command -v perl >/dev/null 2>&1; then
  if ! ( cd "$WORK" && "$CC" --boot -o boot.bin "$ROOT/tests/usermode/boot.lap" 2>err.txt ); then
    bad "boot sector: $(cat "$WORK/err.txt")"
  else
    for opt in -O0 -O2; do
      if ! ( cd "$WORK" && "$CC" $opt --flat 0x8000 -o kern.bin "$ROOT/tests/ginit/kernel.lap" 2>err.txt ); then
        bad "kernel $opt: $(cat "$WORK/err.txt")"; continue
      fi
      if [ "$(wc -c < "$WORK/kern.bin" | tr -d ' ')" -gt 8192 ]; then bad "kernel $opt: larger than the 16 sectors the boot sector reads"; continue; fi
      dd if=/dev/zero of="$WORK/pad.bin" bs=8192 count=1 2>/dev/null
      dd if="$WORK/kern.bin" of="$WORK/pad.bin" conv=notrunc 2>/dev/null
      cat "$WORK/boot.bin" "$WORK/pad.bin" > "$WORK/disk.img"
      rm -f "$WORK/serial.txt"
      perl -e 'my $p = fork(); if (!$p) { exec @ARGV; exit 127 } $SIG{ALRM} = sub { kill "KILL", $p }; alarm 8; waitpid($p, 0);' \
        qemu-system-x86_64 -display none -boot a -drive file="$WORK/disk.img",format=raw,if=floppy,index=0 \
        -serial file:"$WORK/serial.txt" </dev/null >/dev/null 2>&1 || true
      grep -q GINIT "$WORK/serial.txt" 2>/dev/null || bad "kernel $opt: COM1 should be GINIT, got '$(cat "$WORK/serial.txt" 2>/dev/null)'"
      ok
    done
  fi
elif [ "${CMP_REQUIRE:-0}" = 1 ]; then
  bad "CMP_REQUIRE=1 and qemu-system-x86_64 or perl is missing"
else
  echo "skip tests/ginit QEMU run: qemu-system-x86_64 or perl is not installed (CMP_REQUIRE=1 makes this an error)"
fi

echo "tests/ginit: $checks checks, $fails failed"
[ "$fails" = 0 ]
