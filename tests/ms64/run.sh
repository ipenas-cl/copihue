#!/usr/bin/env bash
# The Microsoft x64 convention (0.44.6). Called by tests/run_tests.sh with the compiler path; also
# runnable alone:   tests/ms64/run.sh [path/to/copihue]
#
# 1. Every tests/errors/ms64_*.lap is refused in each way the compiler builds (native -O0 and -O2,
#    --nasm, -c, --freestanding, --flat) with the first message of its golden file, on its line,
#    and leaves no output.
# 2. The assembly: an ms64 call aligns rsp, reserves the shadow space, and loads rcx, rdx, r8, r9
#    (tests/lang/ms64_fn.lap, -O0 and -O2); an ms64 function has its entry thunk; no program
#    without ms64 has either (the System V calls are the ones they were). The assembly assembles
#    with nasm, and the native build (executable and -c) of tests/lang/ms64_fn.lap succeeds.
# 3. QEMU runs tests/ms64/kernel.lap at -O0 and -O2: callees in assembly read rcx, rdx, r8, r9 and
#    the stack arguments (0, 1, 4, 5, 6, and 8 of them), check rsp modulo 16 and the shadow space;
#    callees in Copihue are called back and forth and must keep rdi, rsi, rbx, r13.
#    Without qemu-system-x86_64 and perl it is skipped; CMP_REQUIRE=1 (the CI) makes that a failure.
# The hosted executables are Linux ELF files and are not run here; tests/lang/ms64_call.lap and
# ms64_fn.lap are run by tests/run_tests.sh on Linux and compared with tests/expected/tests/lang.
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT" || exit 1
CC=${1:-$ROOT/compiler/bootstrap-c/copihue}
[ -x "$CC" ] || { echo "tests/ms64/run.sh: no compiler at $CC" >&2; exit 2; }
CC=$(cd "$(dirname "$CC")" && pwd)/$(basename "$CC")
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
fails=0; checks=0
bad() { fails=$((fails+1)); echo "FAIL tests/ms64: $*"; }
ok() { checks=$((checks+1)); }

# 1. refusals
for src in tests/errors/ms64_*.lap; do
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

# 2. the assembly
HAVE_NASM=0; command -v nasm >/dev/null 2>&1 && HAVE_NASM=1
want_line() {  # $1 = label, $2 = asm file, $3 = fixed text
  grep -qF -- "$3" "$2" || bad "$1: '$3' is not in the assembly"
  ok
}
for opt in -O0 -O2; do
  "$CC" -S $opt -o "$WORK/m.asm" tests/lang/ms64_fn.lap 2>"$WORK/err.txt" || { bad "ms64_fn $opt: $(cat "$WORK/err.txt")"; continue; }
  want_line "ms64_fn $opt" "$WORK/m.asm" "    and rsp, -16"
  want_line "ms64_fn $opt" "$WORK/m.asm" "    mov rcx, [r10+"
  want_line "ms64_fn $opt" "$WORK/m.asm" "    mov r9, [r10+"
  want_line "ms64_fn $opt" "$WORK/m.asm" "    mov [rsp+32], rax"
  want_line "ms64_fn $opt" "$WORK/m.asm" "    mov rsp, [rsp+"
  want_line "ms64_fn $opt" "$WORK/m.asm" "    call __ms64_m8"
  want_line "ms64_fn $opt" "$WORK/m.asm" "    call rbx"
  want_line "ms64_fn $opt" "$WORK/m.asm" "    mov r8, [rbp+48]"
  want_line "ms64_fn $opt" "$WORK/m.asm" "    pop rsi"
  # System V programs: not one of these lines.
  for sv in tests/lang/call_args.lap tests/lang/eight_args.lap tests/hello.lap; do
    "$CC" -S $opt -o "$WORK/s.asm" "$sv" 2>"$WORK/err.txt" || { bad "$sv $opt: $(cat "$WORK/err.txt")"; continue; }
    if grep -qF -e "and rsp, -16" -e "__ms64_" "$WORK/s.asm"; then bad "$sv $opt: a System V program has ms64 code"; fi
    ok
  done
  if [ $HAVE_NASM = 1 ]; then
    nasm -f elf64 -o "$WORK/m.o" "$WORK/m.asm" 2>"$WORK/err.txt" || bad "ms64_fn $opt: nasm refused the assembly: $(head -3 "$WORK/err.txt")"
    ok
    "$CC" -S $opt -o "$WORK/c.asm" tests/lang/ms64_call.lap 2>"$WORK/err.txt" || bad "ms64_call $opt: $(cat "$WORK/err.txt")"
    nasm -f elf64 -o "$WORK/c.o" "$WORK/c.asm" 2>"$WORK/err.txt" || bad "ms64_call $opt: nasm refused the assembly: $(head -3 "$WORK/err.txt")"
    ok
  fi
  ( cd "$WORK" && "$CC" $opt -o nat.out "$ROOT/tests/lang/ms64_fn.lap" ) 2>"$WORK/err.txt" && [ -s "$WORK/nat.out" ] || bad "native $opt: $(cat "$WORK/err.txt")"
  ok
  ( cd "$WORK" && "$CC" -c $opt -o nat.o "$ROOT/tests/lang/ms64_fn.lap" ) 2>"$WORK/err.txt" && [ -s "$WORK/nat.o" ] || bad "native -c $opt: $(cat "$WORK/err.txt")"
  ok
  # the same program with --nasm
  if [ "$(uname -s)" = Linux ] && [ $HAVE_NASM = 1 ]; then
    ( cd "$WORK" && "$CC" --nasm $opt -o nasm.out "$ROOT/tests/lang/ms64_fn.lap" ) 2>"$WORK/err.txt" && [ -s "$WORK/nasm.out" ] || bad "--nasm $opt: $(cat "$WORK/err.txt")"
    ok
  fi
done

# 3. QEMU
if command -v qemu-system-x86_64 >/dev/null 2>&1 && command -v perl >/dev/null 2>&1; then
  if ! ( cd "$WORK" && "$CC" --boot -o boot.bin "$ROOT/tests/usermode/boot.lap" 2>err.txt ); then
    bad "boot sector: $(cat "$WORK/err.txt")"
  else
    for opt in -O0 -O2; do
      if ! ( cd "$WORK" && "$CC" $opt --flat 0x8000 -o kern.bin "$ROOT/tests/ms64/kernel.lap" 2>err.txt ); then
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
      grep -q MS64 "$WORK/serial.txt" 2>/dev/null || bad "kernel $opt: COM1 should be MS64, got '$(cat "$WORK/serial.txt" 2>/dev/null)'"
      ok
    done
  fi
elif [ "${CMP_REQUIRE:-0}" = 1 ]; then
  bad "CMP_REQUIRE=1 and qemu-system-x86_64 or perl is missing"
else
  echo "skip tests/ms64 QEMU run: qemu-system-x86_64 or perl is not installed (CMP_REQUIRE=1 makes this an error)"
fi

echo "tests/ms64: $checks checks, $fails failed"
[ "$fails" = 0 ]
