#!/usr/bin/env bash
# Assignment targets (0.44.4). Called by tests/run_tests.sh with the compiler path; also runnable alone:
#   tests/assign/run.sh [path/to/copihue]
#
# 1. Every tests/errors/assign_*.lap is refused in each way the compiler builds (native -O0 and -O2,
#    --nasm, -c, --freestanding, --flat) with the message of its golden file, and leaves no output.
# 2. A store through a call result is in the assembly (-S, -O0 and -O2, hosted, --freestanding, --flat),
#    in the native object and in the nasm object: a dword store, volatile where the pointee is.
# 3. QEMU runs tests/assign/kernel.lap (the stores, with values past 32 bits) at -O0 and -O2.
#    Without qemu-system-x86_64 and perl it is skipped; CMP_REQUIRE=1 (the CI) makes that a failure.
# The executables are Linux ELF files and are not run here; the kernel is what runs.
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT" || exit 1
CC=${1:-$ROOT/compiler/bootstrap-c/copihue}
[ -x "$CC" ] || { echo "tests/assign/run.sh: no compiler at $CC" >&2; exit 2; }
CC=$(cd "$(dirname "$CC")" && pwd)/$(basename "$CC")
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
fails=0; checks=0
bad() { fails=$((fails+1)); echo "FAIL tests/assign: $*"; }
ok() { checks=$((checks+1)); }

# 1. refusals
for src in tests/errors/assign_*.lap; do
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
    # the line number is the golden's
    wl=$(grep -m1 'error:' "tests/expected/errors/$name.err" | sed -E 's/^[^:]*:([0-9]+):.*/\1/')
    gl=$(grep -m1 'error:' "$WORK/err.txt" | sed -E 's/^[^:]*:([0-9]+):.*/\1/')
    [ "$wl" = "$gl" ] || bad "$name [$mode]: line $gl, expected $wl"
    for f in prog.out prog.o prog.asm prog.bin output.asm output.o; do
      [ -e "$WORK/$f" ] && bad "$name [$mode]: left $f behind"
    done
    ok
  done
done

# 2. stores in the assembly
count() { grep -c -F -e "$1" "$2"; }
stores() {  # $1 = label, $2 = asm file
  local d w l
  d=$(count 'mov dword [rbx], eax  ; volatile' "$2"); w=$(count 'mov word [rbx], ax' "$2"); l=$(count 'mov eax, [rbx]  ; volatile' "$2")
  [ "$d" = 2 ] && [ "$w" = 1 ] && [ "$l" -ge 1 ] || bad "$1: expected 2 volatile dword stores, 1 word store, a volatile load; found $d, $w, $l"
  ok
}
for opt in -O0 -O2; do
  "$CC" -S $opt -o "$WORK/s.asm" tests/assign/store_call.lap 2>"$WORK/err.txt" && stores "hosted $opt" "$WORK/s.asm" || { bad "hosted $opt: $(cat "$WORK/err.txt")"; }
  "$CC" --freestanding $opt -o "$WORK/f.asm" tests/assign/store_call_flat.lap 2>"$WORK/err.txt" && stores "freestanding $opt" "$WORK/f.asm" || { bad "freestanding $opt: $(cat "$WORK/err.txt")"; }
  ( cd "$WORK" && "$CC" --flat 0x10000 -S $opt -o fl.asm "$ROOT/tests/assign/store_call_flat.lap" ) 2>"$WORK/err.txt" && stores "flat $opt" "$WORK/fl.asm" || { bad "flat $opt: $(cat "$WORK/err.txt")"; }
done
# the objects: the compiler's own (-c) and nasm's. A dword store through rbx, twice, whatever the syntax.
objstores() {  # $1 = label, $2 = object
  local n
  if ! objdump -d "$2" >"$WORK/dis.txt" 2>/dev/null || ! grep -q 'elf64' "$WORK/dis.txt"; then
    echo "skip tests/assign $1 object check: objdump here does not read ELF"; return
  fi
  n=$(grep -c -E 'mov[l]?[[:space:]]+%eax,[[:space:]]*\(%rbx\)' "$WORK/dis.txt")
  [ "$n" = 2 ] || bad "$1 object: expected 2 dword stores through rbx after the call, found $n"
  n=$(grep -c -E 'mov[w]?[[:space:]]+%ax,[[:space:]]*\(%rbx\)' "$WORK/dis.txt")
  [ "$n" = 1 ] || bad "$1 object: expected 1 word store through rbx, found $n"
  ok
}
if command -v objdump >/dev/null 2>&1; then
  for opt in -O0 -O2; do
    ( cd "$WORK" && "$CC" -c $opt -o nat.o "$ROOT/tests/assign/store_call.lap" ) 2>"$WORK/err.txt" && objstores "native $opt" "$WORK/nat.o" || bad "native -c $opt: $(cat "$WORK/err.txt")"
    if command -v nasm >/dev/null 2>&1; then
      "$CC" -S $opt -o "$WORK/n.asm" tests/assign/store_call.lap 2>/dev/null &&
        nasm -f elf64 -o "$WORK/nasm.o" "$WORK/n.asm" 2>/dev/null && objstores "nasm $opt" "$WORK/nasm.o" || bad "nasm $opt: could not assemble the -S output"
    fi
  done
else
  echo "skip tests/assign object check: no objdump"
fi

# 3. QEMU
if command -v qemu-system-x86_64 >/dev/null 2>&1 && command -v perl >/dev/null 2>&1; then
  if ! ( cd "$WORK" && "$CC" --boot -o boot.bin "$ROOT/tests/usermode/boot.lap" 2>err.txt ); then
    bad "boot sector: $(cat "$WORK/err.txt")"
  else
    for opt in -O0 -O2; do
      if ! ( cd "$WORK" && "$CC" $opt --flat 0x8000 -o kern.bin "$ROOT/tests/assign/kernel.lap" 2>err.txt ); then
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
      grep -q ASSIGN "$WORK/serial.txt" 2>/dev/null || bad "kernel $opt: COM1 should be ASSIGN, got '$(cat "$WORK/serial.txt" 2>/dev/null)'"
      ok
    done
  fi
elif [ "${CMP_REQUIRE:-0}" = 1 ]; then
  bad "CMP_REQUIRE=1 and qemu-system-x86_64 or perl is missing"
else
  echo "skip tests/assign QEMU run: qemu-system-x86_64 or perl is not installed (CMP_REQUIRE=1 makes this an error)"
fi

echo "tests/assign: $checks checks, $fails failed"
[ "$fails" = 0 ]
