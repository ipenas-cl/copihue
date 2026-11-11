#!/usr/bin/env bash
# Silent memory corruption (0.45.1): struct arguments, p->f[i], &a[i], array literals; and (0.45.2) the
# bound of a local array of structs. Called by tests/run_tests.sh with the compiler path; also runnable
# alone:   tests/memfix/run.sh [path/to/copihue]
#
# 1. Every tests/errors/memfix_*.lap is refused in each way the compiler builds (native -O0 and -O2,
#    --nasm, -c, --freestanding, --flat) with the first message of its golden file, on its line,
#    and leaves no output.
# 2. The programs of tests/memfix/progs.txt build in each of those ways, and the assembly of each
#    (-S -O0 and -O2) assembles with nasm. The kernel builds with --freestanding and --flat.
# 3. QEMU runs tests/memfix/kernel.lap and tests/memfix/kernel_addr.lap at -O0 and -O2 (--flat 0x8000),
#    and kernel_addr.lap again with --checked (the bounds of &a[i] compiled in). COM1 must say MEMOK.
#    tests/memfix/kernel_struct.lap and kernel_struct_store.lap (--checked) then read and write past
#    their array of structs: the check faults, so COM1 must never say LEAK, which each kernel writes
#    after that access.
#    Without qemu-system-x86_64 and perl it is skipped; CMP_REQUIRE=1 (the CI) makes that a failure.
# The hosted executables are Linux ELF files and are not run here; tests/run_tests.sh runs them on
# Linux and compares them with tests/expected/tests/lang.
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT" || exit 1
CC=${1:-$ROOT/compiler/bootstrap-c/copihue}
[ -x "$CC" ] || { echo "tests/memfix/run.sh: no compiler at $CC" >&2; exit 2; }
CC=$(cd "$(dirname "$CC")" && pwd)/$(basename "$CC")
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
fails=0; checks=0
bad() { fails=$((fails+1)); echo "FAIL tests/memfix: $*"; }
ok() { checks=$((checks+1)); }
HAVE_NASM=0; command -v nasm >/dev/null 2>&1 && HAVE_NASM=1

# 1. refusals
for src in tests/errors/memfix_*.lap; do
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

# 2. the programs in every mode
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
done < tests/memfix/progs.txt
# The kernels are the freestanding and flat case (print needs the hosted runtime).
for K in tests/memfix/kernel.lap tests/memfix/kernel_addr.lap tests/memfix/kernel_struct.lap \
         tests/memfix/kernel_struct_store.lap; do
for opt in -O0 -O2; do
  "$CC" --freestanding $opt -o "$WORK/f.asm" "$K" 2>"$WORK/err.txt" || bad "$K --freestanding $opt: $(head -3 "$WORK/err.txt")"
  ( cd "$WORK" && "$CC" --flat 0x8000 -S $opt -o fl.asm "$ROOT/$K" ) 2>"$WORK/err.txt" || bad "$K --flat -S $opt: $(head -3 "$WORK/err.txt")"
  ok
  if [ $HAVE_NASM = 1 ] && [ -s "$WORK/f.asm" ]; then
    nasm -f elf64 -o "$WORK/f.o" "$WORK/f.asm" 2>"$WORK/err.txt" || bad "$K --freestanding $opt: nasm refused it: $(head -3 "$WORK/err.txt")"
    ok
  fi
done
done

# 3. QEMU
if command -v qemu-system-x86_64 >/dev/null 2>&1 && command -v perl >/dev/null 2>&1; then
  if ! ( cd "$WORK" && "$CC" --boot -o boot.bin "$ROOT/tests/usermode/boot.lap" 2>err.txt ); then
    bad "boot sector: $(cat "$WORK/err.txt")"
  else
    for kb in "kernel.lap -O0" "kernel.lap -O2" "kernel_addr.lap -O0" "kernel_addr.lap -O2" \
              "kernel_addr.lap -O0 --checked" "kernel_addr.lap -O2 --checked" \
              "kernel_struct.lap -O0 --checked" "kernel_struct.lap -O2 --checked" \
              "kernel_struct_store.lap -O0 --checked" "kernel_struct_store.lap -O2 --checked"; do
      K=tests/memfix/${kb%% *}; build=${kb#* }
      # shellcheck disable=SC2086
      if ! ( cd "$WORK" && "$CC" $build --flat 0x8000 -o kern.bin "$ROOT/$K" 2>err.txt ); then
        bad "$K $build: $(cat "$WORK/err.txt")"; continue
      fi
      if [ "$(wc -c < "$WORK/kern.bin" | tr -d ' ')" -gt 8192 ]; then bad "$K $build: larger than the 16 sectors the boot sector reads"; continue; fi
      dd if=/dev/zero of="$WORK/pad.bin" bs=8192 count=1 2>/dev/null
      dd if="$WORK/kern.bin" of="$WORK/pad.bin" conv=notrunc 2>/dev/null
      cat "$WORK/boot.bin" "$WORK/pad.bin" > "$WORK/disk.img"
      rm -f "$WORK/serial.txt"
      perl -e 'my $p = fork(); if (!$p) { exec @ARGV; exit 127 } $SIG{ALRM} = sub { kill "KILL", $p }; alarm 8; waitpid($p, 0);' \
        qemu-system-x86_64 -display none -boot a -drive file="$WORK/disk.img",format=raw,if=floppy,index=0 \
        -serial file:"$WORK/serial.txt" </dev/null >/dev/null 2>&1 || true
      grep -q MEMOK "$WORK/serial.txt" 2>/dev/null || bad "$K $build: COM1 should be MEMOK, got '$(cat "$WORK/serial.txt" 2>/dev/null)'"
      grep -q LEAK "$WORK/serial.txt" 2>/dev/null && bad "$K $build: an access past the array was not stopped (COM1 says LEAK)"
      ok
    done
  fi
elif [ "${CMP_REQUIRE:-0}" = 1 ]; then
  bad "CMP_REQUIRE=1 and qemu-system-x86_64 or perl is missing"
else
  echo "skip tests/memfix QEMU run: qemu-system-x86_64 or perl is not installed (CMP_REQUIRE=1 makes this an error)"
fi

echo "tests/memfix: $checks checks, $fails failed"
[ "$fails" = 0 ]
