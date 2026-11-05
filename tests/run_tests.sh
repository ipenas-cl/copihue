#!/usr/bin/env bash
# Copihue regression suite.
#
#   tests/run_tests.sh            run everything
#   tests/run_tests.sh --update   regenerate golden files (review the diff!)
#
# 1. Program tests: every tests/**/*.lap, examples/*.lap, examples/*/*.lap,
#    and examples/*/*/*.lap (examples/lowlat and its benches) is compiled at -O0
#    and -O2, run with a timeout, and its stdout + exit code compared with
#    tests/expected/<path>.out. Both optimization levels must match the same
#    golden file. Heap/stack addresses (9+ digit numbers) are masked.
# 2. Error tests: every tests/errors/*.lap must FAIL to compile and its
#    diagnostics must match tests/expected/errors/<name>.err.
# 3. Freestanding tests: tests/freestanding/*.lap compile with --freestanding,
#    the output must assemble with nasm, and contain no _start. A `syscall`
#    instruction is allowed only when the source calls syscall().
# 4. Flat binaries: tests/flat/*.lap at --flat 0x10000.
# 4b. Absolute constant addresses outside disp32 (tests/*/abs64.lap): the asm
#    holds the full address in a register and nasm gives no "exceeds bounds".
# 4d. Stack probing: a frame of 4096 bytes or more touches every page (assembly in every mode, nasm,
#    golden), and QEMU must take the page fault on a guard page under the stack (tests/stackprobe).
# 4g. Assignment targets (0.44.4): tests/errors/assign_*.lap are refused (native, --nasm, -c, --freestanding,
#    --flat) with no output; a store through a call result is in the assembly and in both objects; QEMU runs
#    tests/assign/kernel.lap. All of it in tests/assign/run.sh.
# 4h. Initializers of globals (0.44.5): tests/errors/ginit_*.lap are refused in every way the compiler builds;
#    the constant expressions are in .data (assembly of tests/lang/global_init.lap and tests/ginit/kernel.lap);
#    QEMU runs tests/ginit/kernel.lap. All of it in tests/ginit/run.sh.
# 4i. Microsoft x64 convention (0.44.6): tests/errors/ms64_*.lap are refused in every way the compiler builds; the
#    call sequence is in the assembly of tests/lang/ms64_fn.lap and no System V program has it; QEMU runs
#    tests/ms64/kernel.lap. All of it in tests/ms64/run.sh.
# 4j. --pic (0.44.7): tests/errors/pic_*.lap are refused under --flat --pic; the assembly of tests/pic/kernel.lap
#    has no absolute address of a label; the image is the same at two --flat bases; QEMU runs it at two
#    load addresses and must fail without --pic. All of it in tests/pic/run.sh.
# 4k. --pe-efi (0.44.8): tests/errors/pe_*.lap are refused under --pe-efi; tests/efi/app.lap becomes a PE32+ file
#    that a C checker reads (and fails when a header field is broken) and that QEMU with OVMF runs as an EFI
#    application. All of it in tests/efi/run.sh.
# 5. Boot sectors: tests/boot/*.lap, and QEMU must print COPIHUE.
# 6. User mode: tests/usermode, and QEMU must print USER.
# 7. Context switch: tests/ctx, and QEMU must print COPIHUE.
# 8. Relocatable objects: tests/link/parts, two -c files linked with ld.
#    A second pair shares `module mem` across two objects (`mem__add`).
# 9. --native refusals: tests/native/*.lap and the option combinations --native
#    cannot take must fail with the message in tests/expected/native/<name>.err.
#
#
# The default build is the compiler's own: it assembles and writes the ELF itself, with no nasm and
# no ld, for every program and every -c object. A program it cannot take (user assembly, naked,
# interrupt and syscall functions) is built with nasm and ld with a warning; sections 1 and 8 fail
# if anything else falls back.
#
#   tests/run_tests.sh --nasm     only 1, with --nasm (nasm and ld, the way before the built-in back end):
#                                 same programs, -O0 and -O2, same golden files. Linux only.
#   tests/run_tests.sh --native   only 1, with --native (the default, but strict: no fallback to nasm):
#                                 the programs that need nasm are skipped, by name. Linux only.
#
# The executables are Linux ELF files. On another system (macOS) the compiler still builds every
# program, and that is all section 1 can check there; the sections that run or link them (1's output
# check, 8) are skipped with a notice. The CI is Linux and runs everything.
set -u
cd "$(dirname "$0")/.."
ROOT=$PWD
CC=$ROOT/compiler/bootstrap-c/copihue
UPDATE=0; NATIVE=0; NASM=0; NATFLAG=
for a in "$@"; do
  case $a in
    --update) UPDATE=1 ;;
    --native) NATIVE=1; NATFLAG=--native ;;
    --nasm) NASM=1; NATFLAG=--nasm ;;
    *) echo "usage: tests/run_tests.sh [--update | --native | --nasm]" >&2; exit 2 ;;
  esac
done
CAN_RUN=0; [ "$(uname -s)" = Linux ] && CAN_RUN=1
# macOS has no timeout(1); perl is there and kills the program after N seconds.
# (QEMU handles SIGALRM itself, so the child is killed with SIGKILL from the parent.)
command -v timeout >/dev/null 2>&1 || timeout() {
  perl -e 'my $t = shift; my $p = fork(); if (!$p) { exec @ARGV; exit 127 } $SIG{ALRM} = sub { kill "KILL", $p }; alarm $t; waitpid($p, 0); exit($? >> 8)' "$@"
}
if [ $NATIVE = 1 ] || [ $NASM = 1 ]; then
  [ $UPDATE = 0 ] || { echo "--native and --nasm do not regenerate golden files; run --update without them" >&2; exit 2; }
  [ $CAN_RUN = 1 ] || { echo "run_tests.sh --native/--nasm run the ELF executables the compiler writes, so they need Linux (this is $(uname -s)); tests/x64/run_elf.sh checks their structure anywhere" >&2; exit 2; }
fi
if [ $CAN_RUN = 0 ]; then
  [ $UPDATE = 0 ] || { echo "--update runs the programs, so it needs Linux (this is $(uname -s))" >&2; exit 2; }
  echo "notice: this is $(uname -s), not Linux: the programs are compiled (with the built-in back end) but not run, and the link tests (section 8) are skipped"
fi
gcc -O2 -Wall -o "$CC" compiler/bootstrap-c/copihue.c || exit 1

# Programs that are not batch tests (servers / need a peer) or need input.
# They are covered by tests/net_test.sh instead.
SKIP=" examples/simple_api_server.lap examples/realtime_api_monitor.lap tests/test_http_simple.lap tests/http/test_http_client.lap tests/http/test_http_post.lap tests/http/test_suite_http.lap "

WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0; failed=(); skipped=(); compiled=0; fellback=()
FELL=0

mask() { sed -E 's/[0-9]{9,}/ADDR/g'; }

run_program() {  # $1 = source, $2 = opt level -> prints masked stdout + rc (or COMPILED where nothing can run); sets FELL
  local src=$1 opt=$2
  ( cd "$WORK" && rm -f prog data.txt && "$CC" "$opt" $NATFLAG -o prog "$ROOT/$src" >/dev/null 2>compile.err ) \
    || {
      if [ "$NATFLAG" = --native ] && grep -q -e '--native does not take this program' "$WORK/compile.err"; then echo "NATIVE REFUSED"; return; fi
      # Off Linux the fallback to nasm and ld cannot finish (ld is not an ELF linker there).
      if [ $CAN_RUN = 0 ] && grep -q 'falling back to nasm' "$WORK/compile.err"; then FELL=1; echo "NEEDS NASM"; return; fi
      echo "COMPILE ERROR"; sed "s|$ROOT/||g" "$WORK/compile.err"; return
    }
  grep -q 'falling back to nasm' "$WORK/compile.err" && FELL=1
  [ $CAN_RUN = 1 ] || { echo "COMPILED"; return; }
  ( cd "$WORK" && timeout 5 ./prog </dev/null 2>&1; echo "exit=$?" ) | mask
}

check() {  # $1 = name, $2 = golden file, $3 = actual file
  if [ $UPDATE = 1 ]; then mkdir -p "$(dirname "$2")"; cp "$3" "$2"; fi
  if [ -f "$2" ] && cmp -s "$2" "$3"; then pass=$((pass+1));
  else fail=$((fail+1)); failed+=("$1"); echo "FAIL $1"; [ -f "$2" ] && diff "$2" "$3" | head -10; fi
}

for src in $(ls tests/*.lap tests/*/*.lap examples/*.lap examples/*/*.lap examples/*/*/*.lap | grep -v -e '^tests/errors/' -e '^tests/freestanding/' -e "^tests/flat/" -e "^tests/boot/" -e '^tests/usermode/' -e '^tests/ctx/' -e '^tests/native/' -e '^tests/align/' -e '^tests/assign/' -e '^tests/axe1/' -e '^tests/ginit/' -e '^tests/ms64/' -e '^tests/pic/' -e '^tests/efi/' -e '^tests/axe2/' -e '^tests/tools/'); do
  case "$SKIP" in *" $src "*) continue ;; esac
  golden=tests/expected/${src%.lap}.out
  FELL=0
  run_program "$src" -O0 > "$WORK/o0"
  run_program "$src" -O2 > "$WORK/o2"
  # --native refuses user assembly, naked/interrupt/syscall functions (a clear error, no fallback).
  # Skipping is only fine for a program that has one of them; anything else is a failure.
  if grep -q '^NATIVE REFUSED' "$WORK/o0" "$WORK/o2"; then
    if grep -qE 'asm\(|naked|interrupt fn|syscall fn' "$src"; then
      skipped+=("$src"); echo "skip $src: --native refuses it (user assembly or naked/interrupt/syscall function)"
    else
      fail=$((fail+1)); failed+=("$src (--native refused it)"); echo "FAIL $src: --native refused a program with none of those"
    fi
    continue
  fi
  # By default the built-in back end takes every program except the ones with user assembly or
  # naked/interrupt/syscall functions, which fall back to nasm and ld with a warning. Anything
  # else that falls back is a gap in the built-in assembler: a failure, not a silent pass.
  if [ $FELL = 1 ] && [ $NASM = 0 ]; then
    if grep -qE 'asm\(|naked|interrupt fn|syscall fn' "$src"; then
      fellback+=("$src")
      if grep -q '^NEEDS NASM' "$WORK/o0" "$WORK/o2"; then skipped+=("$src"); echo "skip $src: needs nasm and ld (user assembly or naked/interrupt/syscall function), which cannot finish on $(uname -s)"; continue; fi
    else
      fail=$((fail+1)); failed+=("$src (fell back to nasm)"); echo "FAIL $src: the built-in assembler did not take it (fell back to nasm):"
      ( cd "$WORK" && "$CC" -O0 -o prog "$ROOT/$src" 2>&1 >/dev/null | head -3 ); continue
    fi
  fi
  if ! cmp -s "$WORK/o0" "$WORK/o2"; then
    fail=$((fail+1)); failed+=("$src (-O0 vs -O2)"); echo "FAIL $src: -O0 and -O2 differ"; diff "$WORK/o0" "$WORK/o2" | head -10; continue
  fi
  grep -q "^COMPILE ERROR" "$WORK/o0" && { fail=$((fail+1)); failed+=("$src"); echo "FAIL $src:"; cat "$WORK/o0"; continue; }
  if [ $CAN_RUN = 0 ]; then compiled=$((compiled+1)); continue; fi
  grep -qE "^exit=(12[4-9]|13[0-9]|14[0-9])$" "$WORK/o0" && { fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: crashed or timed out ($(tail -1 "$WORK/o0"))"; continue; }
  check "$src" "$golden" "$WORK/o0"
done

if [ $NATIVE = 1 ] || [ $NASM = 1 ]; then
  rm -f "$CC"
  echo
  echo "passed: $pass  failed: $fail  skipped: ${#skipped[@]}  (programs only, built with $NATFLAG)"
  [ $fail = 0 ] || { printf '  %s\n' "${failed[@]}"; exit 1; }
  exit 0
fi
echo
echo "programs: ${#fellback[@]} built with nasm and ld because they use user assembly or naked/interrupt/syscall functions; the rest by the built-in back end"

# 9. --native refuses what the built-in assembler does not take, with a clear message,
# and writes no file. Nothing is run, so this works on any system.
for src in tests/native/*.lap; do
  name=$(basename "$src" .lap)
  rm -f "$WORK/nat.out" "$WORK/output.asm"
  if ( cd "$WORK" && "$CC" --native -o nat.out "$ROOT/$src" >/dev/null 2>err.txt ); then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: --native should have refused it"; continue
  fi
  if [ -e "$WORK/nat.out" ] || [ -e "$WORK/output.asm" ]; then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: --native left a file behind"; continue
  fi
  sed "s|$ROOT/||g" "$WORK/err.txt" > "$WORK/err.norm"
  check "$src (--native)" "tests/expected/native/$name.err" "$WORK/err.norm"
done
for combo in "-S" "--freestanding" "--flat 0x10000" "--boot"; do
  tag=$(echo "$combo" | tr -d ' -' | tr -c 'A-Za-z0-9\n' _)
  if ( cd "$WORK" && "$CC" --native $combo -o nat.out "$ROOT/tests/hello.lap" >/dev/null 2>err.txt ); then
    fail=$((fail+1)); failed+=("--native $combo"); echo "FAIL --native $combo: should have been refused"; continue
  fi
  check "--native $combo" "tests/expected/native/combo_$tag.err" "$WORK/err.txt"
done
if ( cd "$WORK" && "$CC" --native --nasm -o nat.out "$ROOT/tests/hello.lap" >/dev/null 2>err.txt ) || ! grep -q -e '--native and --nasm cannot be combined' "$WORK/err.txt"; then
  fail=$((fail+1)); failed+=("--native --nasm"); echo "FAIL --native --nasm: should have been refused"; cat "$WORK/err.txt"
else pass=$((pass+1)); fi

# 9b. The built-in back end is the default. No nasm, no ld, no output.asm on disk; the fallback to nasm
# is announced; -c writes an ET_REL object. Nothing is run, so this works on any system.
rm -rf "$WORK/def" "$WORK/fakebin" && mkdir -p "$WORK/def" "$WORK/fakebin"
# nasm and ld that record the call and fail: the default build must not run either.
for t in nasm ld; do printf '#!/bin/sh\necho %s >> "%s/tools.log"\nexit 1\n' "$t" "$WORK" > "$WORK/fakebin/$t"; chmod +x "$WORK/fakebin/$t"; done
if ! ( cd "$WORK/def" && PATH="$WORK/fakebin:$PATH" "$CC" -o hello "$ROOT/tests/hello.lap" >/dev/null 2>err.txt && PATH="$WORK/fakebin:$PATH" "$CC" -c -o hello.o "$ROOT/tests/hello.lap" >>/dev/null 2>>err.txt ) || [ -s "$WORK/def/err.txt" ] || [ -e "$WORK/tools.log" ]; then
  fail=$((fail+1)); failed+=("default build"); echo "FAIL default build: did not compile, printed something or ran nasm/ld ($(cat "$WORK/tools.log" 2>/dev/null | tr '\n' ' ')):"; cat "$WORK/def/err.txt"
elif [ -e "$WORK/def/output.asm" ] || [ -e "$WORK/def/output.o" ]; then
  fail=$((fail+1)); failed+=("default build files"); echo "FAIL default build left output.asm or output.o behind"
elif [ "$(head -c 4 "$WORK/def/hello" | od -An -c | tr -d ' ')" != '177ELF' ] || [ "$(od -An -tx1 -j16 -N2 "$WORK/def/hello" | tr -d ' ')" != "0200" ]; then
  fail=$((fail+1)); failed+=("default build elf"); echo "FAIL default build: not an ELF ET_EXEC file"
else pass=$((pass+1)); fi
if ! ( cd "$WORK/def" && "$CC" -c -o hello.o "$ROOT/tests/hello.lap" >/dev/null 2>err.txt ) || [ -s "$WORK/def/err.txt" ] || [ -e "$WORK/def/output.asm" ]; then
  fail=$((fail+1)); failed+=("default -c"); echo "FAIL default -c: did not compile, printed something or left output.asm:"; cat "$WORK/def/err.txt"
elif [ "$(od -An -tx1 -j16 -N2 "$WORK/def/hello.o" | tr -d ' ')" != "0100" ] || [ "$(od -An -tx1 -j40 -N8 "$WORK/def/hello.o" | tr -d ' ')" = "0000000000000000" ]; then
  fail=$((fail+1)); failed+=("default -c elf"); echo "FAIL default -c: not an ET_REL file with section headers"
else pass=$((pass+1)); fi
# The fallback is announced, names the construct and its line, and builds with nasm (the link step may fail off Linux).
for src in tests/native/refuse_asm.lap tests/native/refuse_naked.lap; do
  ( cd "$WORK/def" && "$CC" -o fb "$ROOT/$src" >/dev/null 2>err.txt )
  if ! grep -q 'warning: the built-in assembler does not take this program (line [0-9]*: .*); falling back to nasm and ld' "$WORK/def/err.txt"; then
    fail=$((fail+1)); failed+=("$src (fallback warning)"); echo "FAIL $src: no fallback warning:"; cat "$WORK/def/err.txt"
  else pass=$((pass+1)); fi
  rm -f "$WORK/def/fb" "$WORK/def/output.asm" "$WORK/def/output.o"
  ( cd "$WORK/def" && "$CC" -c -o fb.o "$ROOT/$src" >/dev/null 2>err.txt )
  if ! grep -q 'falling back to nasm$' "$WORK/def/err.txt" || ! [ -s "$WORK/def/fb.o" ]; then
    fail=$((fail+1)); failed+=("$src (-c fallback)"); echo "FAIL $src -c: no fallback warning or no object:"; cat "$WORK/def/err.txt"
  else pass=$((pass+1)); fi
  rm -f "$WORK/def/fb.o" "$WORK/def/output.asm"
done
# --freestanding -c is nasm's by design: silent.
if ! ( cd "$WORK/def" && "$CC" --freestanding -c -o fs.o "$ROOT/tests/link/parts/sec_plain.lap" >/dev/null 2>err.txt ) || [ -s "$WORK/def/err.txt" ] || ! [ -s "$WORK/def/fs.o" ]; then
  fail=$((fail+1)); failed+=("--freestanding -c"); echo "FAIL --freestanding -c: did not build or printed something:"; cat "$WORK/def/err.txt"
else pass=$((pass+1)); fi
# --nasm keeps output.asm and output.o (it needs a Linux ld to finish).
if [ $CAN_RUN = 1 ]; then
  if ! ( cd "$WORK/def" && "$CC" --nasm -o viejo "$ROOT/tests/hello.lap" >/dev/null 2>err.txt ) || [ ! -s "$WORK/def/output.asm" ] || [ ! -s "$WORK/def/output.o" ] || [ ! -x "$WORK/def/viejo" ]; then
    fail=$((fail+1)); failed+=("--nasm"); echo "FAIL --nasm: did not build with nasm and ld or left no output.asm/output.o:"; cat "$WORK/def/err.txt"
  else pass=$((pass+1)); fi
else echo "skip --nasm build check: ld on $(uname -s) does not link ELF files"; fi

for src in tests/errors/*.lap; do
  name=$(basename "$src" .lap)
  flags=$(sed -n 's|^// flags: ||p' "$src" | head -1)
  if ( cd "$WORK" && "$CC" -S $flags -o out.asm "$ROOT/$src" >/dev/null 2>err.txt ); then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: compiled but should have failed"; continue
  fi
  sed "s|$ROOT/||g" "$WORK/err.txt" > "$WORK/err.norm"
  check "$src" "tests/expected/errors/$name.err" "$WORK/err.norm"
done

for src in tests/freestanding/*.lap; do
  name=$(basename "$src" .lap)
  if ! ( cd "$WORK" && "$CC" --freestanding -o fs.asm "$ROOT/$src" 2>err.txt ); then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src:"; cat "$WORK/err.txt"; continue
  fi
  # A lone `syscall` is the freestanding instruction. The hosted runtime
  # is still rejected: it is a compile error, and a stray syscall from a
  # source that never calls syscall() fails this check.
  # `__text_start` is a bounds label, not the hosted entry point.
  if grep -qE '(^|[^A-Za-z0-9_])_start([^A-Za-z0-9_]|$)' "$WORK/fs.asm" || \
     { grep -qE '^\s*syscall' "$WORK/fs.asm" && ! grep -q 'syscall(' "$src"; } || \
     ! nasm -f elf64 "$WORK/fs.asm" -o "$WORK/fs.o"; then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: not freestanding or does not assemble"; continue
  fi
  # Keep only instructions (drop comments/blank lines) as the golden output.
  grep -vE '^\s*(;|$)' "$WORK/fs.asm" | grep -v '^; Generated' > "$WORK/fs.norm"
  check "$src" "tests/expected/freestanding/$name.asm.txt" "$WORK/fs.norm"
  # // also-o2: the same instruction stream at -O2 (volatile must not move).
  if grep -q '^// also-o2' "$src"; then
    if ! ( cd "$WORK" && "$CC" -O2 --freestanding -o fs2.asm "$ROOT/$src" 2>err.txt ); then
      fail=$((fail+1)); failed+=("$src -O2"); echo "FAIL $src -O2:"; cat "$WORK/err.txt"; continue
    fi
    if grep -qE '(^|[^A-Za-z0-9_])_start([^A-Za-z0-9_]|$)' "$WORK/fs2.asm" || \
       { grep -qE '^\s*syscall' "$WORK/fs2.asm" && ! grep -q 'syscall(' "$src"; } || \
       ! nasm -f elf64 "$WORK/fs2.asm" -o "$WORK/fs2.o"; then
      fail=$((fail+1)); failed+=("$src -O2"); echo "FAIL $src -O2: not freestanding or does not assemble"; continue
    fi
    grep -vE '^\s*(;|$)' "$WORK/fs2.asm" | grep -v '^; Generated' > "$WORK/fs2.norm"
    check "$src -O2" "tests/expected/freestanding/$name.asm.txt" "$WORK/fs2.norm"
  fi
done

# 4. Flat-binary tests: tests/flat/*.lap build with --flat 0x10000 into a raw
#    image; _start must be at offset 0 and the bytes must match the golden hexdump.
for src in tests/flat/*.lap; do
  name=$(basename "$src" .lap)
  # abs64 is checked on its assembly by section 4b: the bytes of a flat image
  # depend on the NASM version, and there is no golden hexdump for it.
  [ "$name" = abs64 ] && continue
  if ! ( cd "$WORK" && "$CC" --flat 0x10000 -o flat.bin "$ROOT/$src" 2>err.txt && "$CC" --flat 0x10000 -S -o flat.asm "$ROOT/$src" 2>>err.txt ); then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src:"; cat "$WORK/err.txt"; continue
  fi
  if [ "$(grep -m1 -E "^[A-Za-z_][A-Za-z0-9_]*:$" <(sed -n "/^section .text/,\$p" "$WORK/flat.asm"))" != "_start:" ]; then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: _start is not the first function"; continue
  fi
  od -An -tx1 -v "$WORK/flat.bin" | tr -s ' ' | sed -e 's/ *$//' -e '/^$/d' > "$WORK/flat.hex"
  check "$src" "tests/expected/flat/$name.hex" "$WORK/flat.hex"
  if grep -q '^// also-o2' "$src"; then
    if ! ( cd "$WORK" && "$CC" -O2 --flat 0x10000 -o flat2.bin "$ROOT/$src" 2>>err.txt ); then
      fail=$((fail+1)); failed+=("$src -O2"); echo "FAIL $src -O2:"; cat "$WORK/err.txt"; continue
    fi
    if ! cmp -s "$WORK/flat.bin" "$WORK/flat2.bin"; then
      fail=$((fail+1)); failed+=("$src (-O0 vs -O2)"); echo "FAIL $src: -O0 and -O2 images differ"; continue
    fi
    pass=$((pass+1))
  fi
done

# 4c. align(N) fn and the AXE2 image (0.44.1). Bytes are read with od -j/-N and compared as
#     hex strings, so this works with the BSD od of macOS as well as GNU's.
hexat() { od -An -v -tx1 -j "$2" -N "$3" "$1" | tr -d ' \n'; }     # file offset count
hexall() { { od -An -v -tx1 "$1" | tr -d ' \n' | fold -w 64; echo; }; }
le32() { printf '%02x%02x%02x%02x' $(($1 & 255)) $((($1 >> 8) & 255)) $((($1 >> 16) & 255)) $((($1 >> 24) & 255)); }

# align(N) before fn, in --flat: second() on page 1 (0x11000), third() on a 256 boundary, int3 between.
src=tests/flat/align_fn.lap
if ! ( cd "$WORK" && "$CC" --flat 0x10000 -S -o af.asm "$ROOT/$src" 2>err.txt && "$CC" --flat 0x10000 -o af.bin "$ROOT/$src" 2>>err.txt && "$CC" -O2 --flat 0x10000 -o af2.bin "$ROOT/$src" 2>>err.txt ); then
  fail=$((fail+1)); failed+=("$src (align)"); echo "FAIL $src (align):"; cat "$WORK/err.txt"
else
  grep -vE '^\s*(;|$)' "$WORK/af.asm" | grep -v '^; Generated' > "$WORK/af.norm"
  check "$src (assembly)" tests/expected/flat/align_fn.asm.txt "$WORK/af.norm"
  if ! cmp -s "$WORK/af.bin" "$WORK/af2.bin"; then
    fail=$((fail+1)); failed+=("$src (align -O0 vs -O2)"); echo "FAIL $src: -O0 and -O2 images differ"
  elif [ "$(hexat "$WORK/af.bin" 4095 2)" != cc55 ] || [ "$(hexat "$WORK/af.bin" 4351 2)" != cc55 ]; then
    fail=$((fail+1)); failed+=("$src (align bytes)"); echo "FAIL $src: the functions are not at 0x1000 and 0x1100 behind int3 padding"
  else pass=$((pass+1)); fi
fi
# The same align(N) in a hosted program and in a -c object is the same directive in front of the label.
if ! ( cd "$WORK" && "$CC" -S -o ah.asm "$ROOT/tests/lang/align_fn.lap" 2>err.txt && "$CC" -c -o ah.o "$ROOT/tests/lang/align_fn.lap" 2>>err.txt ); then
  fail=$((fail+1)); failed+=("tests/lang/align_fn.lap (-S, -c)"); echo "FAIL tests/lang/align_fn.lap -S/-c:"; cat "$WORK/err.txt"
elif [ "$(grep -c '^align [0-9]*, db 0xCC$' "$WORK/ah.asm")" != 2 ] || ! grep -qx 'align 4096, db 0xCC' "$WORK/ah.asm" || ! grep -qx 'align 64, db 0xCC' "$WORK/ah.asm"; then
  fail=$((fail+1)); failed+=("tests/lang/align_fn.lap (asm)"); echo "FAIL tests/lang/align_fn.lap: expected 'align 4096, db 0xCC' and 'align 64, db 0xCC' once each"
else pass=$((pass+1)); fi

# AXE2: --flat 0x400000 --axe2 writes the 32-byte header and the image; --sizes prints the section sizes.
src=tests/axe2/layout.lap
if ! ( cd "$WORK" && rm -f output.axe2.* &&
       "$CC" --flat 0x400000 --axe2 --sizes -o l0.axe "$ROOT/$src" >sizes0.txt 2>err.txt &&
       "$CC" -O2 --flat 0x400000 --axe2 --sizes -o l2.axe "$ROOT/$src" >sizes2.txt 2>>err.txt &&
       "$CC" --flat 0x400000 --axe2 -S -o l.asm "$ROOT/$src" 2>>err.txt &&
       "$CC" --flat 0x400000 -o lp.bin "$ROOT/$src" 2>>err.txt &&
       "$CC" --flat 0x400000 --sizes -o lq.bin "$ROOT/$src" >sizesq.txt 2>>err.txt ); then
  fail=$((fail+1)); failed+=("$src (axe2)"); echo "FAIL $src (axe2):"; cat "$WORK/err.txt"
elif ! cmp -s "$WORK/l0.axe" "$WORK/l2.axe" || ! cmp -s "$WORK/sizes0.txt" "$WORK/sizes2.txt"; then
  fail=$((fail+1)); failed+=("$src (axe2 -O0 vs -O2)"); echo "FAIL $src: -O0 and -O2 differ"
elif ls "$WORK"/output.axe2.* >/dev/null 2>&1; then
  fail=$((fail+1)); failed+=("$src (axe2 temp files)"); echo "FAIL $src: output.axe2.* left behind"
else
  grep -vE '^\s*(;|$)' "$WORK/l.asm" | grep -v '^; Generated' > "$WORK/l.norm"
  check "$src (assembly)" tests/expected/axe2/layout.asm.txt "$WORK/l.norm"
  check "$src (sizes)" tests/expected/axe2/layout.sizes "$WORK/sizes0.txt"
  hexall "$WORK/l0.axe" > "$WORK/l.hex"
  check "$src (file)" tests/expected/axe2/layout.hex "$WORK/l.hex"
  # The header, field by field: magic, image bytes, entry, text bytes, memory size, reserved.
  bad=""
  [ "$(hexat "$WORK/l0.axe" 0 4)" = 41584532 ] || bad="$bad magic;"
  [ "$(hexat "$WORK/l0.axe" 4 4)" = "$(le32 8204)" ] || bad="$bad image bytes;"
  [ "$(hexat "$WORK/l0.axe" 8 8)" = 0000400000000000 ] || bad="$bad entry;"
  [ "$(hexat "$WORK/l0.axe" 16 4)" = "$(le32 8192)" ] || bad="$bad text bytes;"
  [ "$(hexat "$WORK/l0.axe" 20 4)" = "$(le32 8276)" ] || bad="$bad memory size;"
  [ "$(hexat "$WORK/l0.axe" 24 8)" = 0000000000000000 ] || bad="$bad reserved;"
  # The image: _start (push rbp) at its byte 0, second() on page 1, the data on page 2 (table, counter),
  # and no bytes for the bss: the file is the header plus 8192 + 12.
  [ "$(hexat "$WORK/l0.axe" 32 1)" = 55 ] || bad="$bad _start at image byte 0;"
  [ "$(hexat "$WORK/l0.axe" 4128 1)" = 55 ] || bad="$bad second() at 0x1000;"
  [ "$(hexat "$WORK/l0.axe" 8224 12)" = 010203040700000000000000 ] || bad="$bad data on page 2;"
  [ "$(wc -c < "$WORK/l0.axe" | tr -d ' ')" = 8236 ] || bad="$bad file size;"
  # --sizes alone: the raw image is the one --flat always wrote, and text/data/bss are those of the AXE2 build.
  cmp -s "$WORK/lp.bin" "$WORK/lq.bin" || bad="$bad --sizes changed the raw image;"
  [ "$(cat "$WORK/sizesq.txt")" = "$(head -3 "$WORK/sizes0.txt")" ] || bad="$bad --sizes text/data/bss;"
  if [ -n "$bad" ]; then fail=$((fail+1)); failed+=("$src (axe2 header)"); echo "FAIL $src (axe2):$bad"; else pass=$((pass+1)); fi
fi
# 16 pages is accepted: the image is under 65536 bytes and the memory size is exactly 65536.
src=tests/axe2/max_pages.lap
if ! ( cd "$WORK" && "$CC" --flat 0x400000 --axe2 -o mp.axe "$ROOT/$src" 2>err.txt ); then
  fail=$((fail+1)); failed+=("$src (axe2)"); echo "FAIL $src (axe2):"; cat "$WORK/err.txt"
else
  img=$(( $(wc -c < "$WORK/mp.axe" | tr -d ' ') - 32 ))
  if [ "$(hexat "$WORK/mp.axe" 4 4)" != "$(le32 $img)" ] || [ "$(hexat "$WORK/mp.axe" 16 8)" != "$(le32 65536)$(le32 65536)" ]; then
    fail=$((fail+1)); failed+=("$src (axe2 header)"); echo "FAIL $src: header does not say image=$img text=65536 memory=65536"
  else pass=$((pass+1)); fi
fi
# Over 16 pages in the file or in memory, the entry outside the text, a section the header cannot describe:
# an error, nothing on stdout, no output file, no temporary file.
for n in pages bss entry section; do
  src=tests/axe2/err_$n.lap
  rm -f "$WORK/e.axe" "$WORK"/output.axe2.*
  if ( cd "$WORK" && "$CC" --flat 0x400000 --axe2 --sizes -o e.axe "$ROOT/$src" >out.txt 2>err.txt ); then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: compiled but should have failed"; continue
  fi
  if [ -s "$WORK/out.txt" ] || [ -e "$WORK/e.axe" ] || ls "$WORK"/output.axe2.* >/dev/null 2>&1; then
    fail=$((fail+1)); failed+=("$src (left files)"); echo "FAIL $src: wrote stdout, the output file or a temporary file"; continue
  fi
  grep 'error:' "$WORK/err.txt" | sed "s|$ROOT/||g" > "$WORK/err.norm"
  check "$src" "tests/expected/axe2/err_$n.err" "$WORK/err.norm"
done
# The option combinations that --axe2 and --sizes refuse (exit status 2, a one-line message).
while IFS='|' read -r flags want; do
  rm -f "$WORK/c.out"
  # shellcheck disable=SC2086
  ( cd "$WORK" && "$CC" $flags -o c.out "$ROOT/tests/hello.lap" >/dev/null 2>err.txt ); rc=$?
  if [ $rc != 2 ] || ! grep -qF -- "$want" "$WORK/err.txt" || [ -e "$WORK/c.out" ]; then
    fail=$((fail+1)); failed+=("copihue $flags"); echo "FAIL copihue $flags: want exit 2 and '$want', got exit $rc:"; cat "$WORK/err.txt"
  else pass=$((pass+1)); fi
done <<'EOF'
--axe2|--axe2 requires --flat 0x400000
--axe2 --boot|--axe2 requires --flat 0x400000
--axe2 --flat 0x10000|--axe2 images load at 0x400000
--axe2 --flat 0x400000 --elf|--axe2 cannot be combined with --elf
--sizes|--sizes requires --flat
--sizes --boot|--sizes requires --flat
--sizes --flat 0x10000 --elf|--sizes cannot be combined with --elf
--axe1|--axe1 requires --flat 0x400000
--axe1 --boot|--axe1 requires --flat 0x400000
--axe1 --flat 0x10000|--axe1 images load at 0x400000
--axe1 --flat 0x400000 --elf|--axe1 cannot be combined with --elf
--axe1 --axe2 --flat 0x400000|--axe1 and --axe2 cannot be combined
EOF

# AXE1: --flat 0x400000 --axe1 writes a 16-byte header (magic, payload size, entry) and the image, one page at most.
src=tests/axe1/page.lap
if ! ( cd "$WORK" && rm -f output.axe2.* &&
       "$CC" --flat 0x400000 --axe1 --sizes -o p0.axe "$ROOT/$src" >sizes0.txt 2>err.txt &&
       "$CC" -O2 --flat 0x400000 --axe1 --sizes -o p2.axe "$ROOT/$src" >sizes2.txt 2>>err.txt &&
       "$CC" --flat 0x400000 --axe1 -S -o p.asm "$ROOT/$src" 2>>err.txt &&
       "$CC" --flat 0x400000 -o pp.bin "$ROOT/$src" 2>>err.txt ); then
  fail=$((fail+1)); failed+=("$src (axe1)"); echo "FAIL $src (axe1):"; cat "$WORK/err.txt"
elif ! cmp -s "$WORK/p0.axe" "$WORK/p2.axe" || ! cmp -s "$WORK/sizes0.txt" "$WORK/sizes2.txt"; then
  fail=$((fail+1)); failed+=("$src (axe1 -O0 vs -O2)"); echo "FAIL $src: -O0 and -O2 differ"
elif ls "$WORK"/output.axe2.* >/dev/null 2>&1; then
  fail=$((fail+1)); failed+=("$src (axe1 temp files)"); echo "FAIL $src: output.axe2.* left behind"
else
  grep -vE '^\s*(;|$)' "$WORK/p.asm" | grep -v '^; Generated' > "$WORK/p.norm"
  check "$src (assembly)" tests/expected/axe1/page.asm.txt "$WORK/p.norm"
  check "$src (sizes)" tests/expected/axe1/page.sizes "$WORK/sizes0.txt"
  hexall "$WORK/p0.axe" > "$WORK/p.hex"
  check "$src (file)" tests/expected/axe1/page.hex "$WORK/p.hex"
  # The header, field by field: magic, payload bytes, entry. The payload is the flat image, unchanged.
  bad=""
  [ "$(hexat "$WORK/p0.axe" 0 4)" = 41584531 ] || bad="$bad magic;"
  [ "$(hexat "$WORK/p0.axe" 4 4)" = "$(le32 144)" ] || bad="$bad payload bytes;"
  [ "$(hexat "$WORK/p0.axe" 8 8)" = 0000400000000000 ] || bad="$bad entry;"
  [ "$(hexat "$WORK/p0.axe" 16 1)" = 55 ] || bad="$bad _start at payload byte 0;"
  [ "$(hexat "$WORK/p0.axe" 152 8)" = 8877665544332211 ] || bad="$bad data after the text;"
  [ "$(wc -c < "$WORK/p0.axe" | tr -d ' ')" = 160 ] || bad="$bad file size;"
  [ "$(tail -c 144 "$WORK/p0.axe" | od -An -v -tx1 | tr -d ' \n')" = "$(od -An -v -tx1 "$WORK/pp.bin" | tr -d ' \n')" ] || bad="$bad payload is not the flat image;"
  if [ -n "$bad" ]; then fail=$((fail+1)); failed+=("$src (axe1 header)"); echo "FAIL $src (axe1):$bad"; else pass=$((pass+1)); fi
fi
# More than one page, a bss that does not fit, the entry outside the text, no _start, a section the header
# cannot describe: an error, nothing on stdout, no output file, no temporary file.
for n in big bss entry section nostart; do
  src=tests/axe1/err_$n.lap
  rm -f "$WORK/e.axe" "$WORK"/output.axe2.*
  if ( cd "$WORK" && "$CC" --flat 0x400000 --axe1 --sizes -o e.axe "$ROOT/$src" >out.txt 2>err.txt ); then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: compiled but should have failed"; continue
  fi
  if [ -s "$WORK/out.txt" ] || [ -e "$WORK/e.axe" ] || ls "$WORK"/output.axe2.* >/dev/null 2>&1; then
    fail=$((fail+1)); failed+=("$src (left files)"); echo "FAIL $src: wrote stdout, the output file or a temporary file"; continue
  fi
  grep 'error:' "$WORK/err.txt" | sed "s|$ROOT/||g" > "$WORK/err.norm"
  check "$src" "tests/expected/axe1/err_$n.err" "$WORK/err.norm"
done

# align(N) before a global let, in --flat 0x400000: head 0x401000, page (align 4096) 0x402000, tail right after it,
# zbuf (align 4096, the first thing in the bss) 0x403000 so the bss has a page of its own, mark behind it.
# The addresses are checked in the code (the imm32 of each lea), in --sizes (data_at, bss_at) and in the bytes
# of the data page. --axe1 refuses it: the page does not fit in one.
src=tests/align/let.lap
if ! ( cd "$WORK" && rm -f output.axe2.* &&
       "$CC" --flat 0x400000 -S -o lt.asm "$ROOT/$src" 2>err.txt &&
       "$CC" --flat 0x400000 -o lt0.bin "$ROOT/$src" 2>>err.txt &&
       "$CC" -O2 --flat 0x400000 -o lt2.bin "$ROOT/$src" 2>>err.txt &&
       "$CC" --flat 0x400000 --axe2 --sizes -o lt.axe "$ROOT/$src" >lt.sizes 2>>err.txt ); then
  fail=$((fail+1)); failed+=("$src (align let)"); echo "FAIL $src (align let):"; cat "$WORK/err.txt"
elif ! cmp -s "$WORK/lt0.bin" "$WORK/lt2.bin"; then
  fail=$((fail+1)); failed+=("$src (align let -O0 vs -O2)"); echo "FAIL $src: -O0 and -O2 differ"
else
  grep -vE '^\s*(;|$)' "$WORK/lt.asm" | grep -v '^; Generated' > "$WORK/lt.norm"
  check "$src (assembly)" tests/expected/align/let.asm.txt "$WORK/lt.norm"
  check "$src (sizes)" tests/expected/align/let.sizes "$WORK/lt.sizes"
  bad=""
  [ "$(hexat "$WORK/lt0.bin" 12 4)" = 00104000 ] || bad="$bad head at 0x401000;"
  [ "$(hexat "$WORK/lt0.bin" 24 4)" = 00204000 ] || bad="$bad page at 0x402000;"
  [ "$(hexat "$WORK/lt0.bin" 36 4)" = 20204000 ] || bad="$bad tail at 0x402020;"
  [ "$(hexat "$WORK/lt0.bin" 48 4)" = 00304000 ] || bad="$bad zbuf at 0x403000;"
  [ "$(hexat "$WORK/lt0.bin" 60 4)" = 10304000 ] || bad="$bad mark at 0x403010;"
  [ "$(hexat "$WORK/lt0.bin" 4096 8)" = 0500000000000000 ] || bad="$bad head's bytes;"
  [ "$(hexat "$WORK/lt0.bin" 8192 16)" = 01000000000000000200000000000000 ] || bad="$bad page's bytes;"
  [ "$(hexat "$WORK/lt0.bin" 8224 8)" = 0600000000000000 ] || bad="$bad tail's bytes;"
  [ "$(wc -c < "$WORK/lt0.bin" | tr -d ' ')" = 8232 ] || bad="$bad file size (no bytes for the bss);"
  grep -qx 'data_at=0x401000' "$WORK/lt.sizes" || bad="$bad data_at;"
  grep -qx 'bss_at=0x403000' "$WORK/lt.sizes" || bad="$bad bss_at;"
  if ( cd "$WORK" && "$CC" --flat 0x400000 --axe1 -o lt1.axe "$ROOT/$src" >/dev/null 2>lt1.err ) || [ -e "$WORK/lt1.axe" ] \
     || ! grep -q -- '--axe1: the image is' "$WORK/lt1.err"; then bad="$bad --axe1 should refuse it;"; fi
  # An org that is not a multiple of 4096 still builds, with a warning that the let is aligned from the image start.
  if ! ( cd "$WORK" && "$CC" --flat 0x400800 -o lt3.bin "$ROOT/$src" >/dev/null 2>lt3.err ) || ! grep -q 'is not a multiple of 4096' "$WORK/lt3.err"; then
    bad="$bad org warning;"
  fi
  if [ -n "$bad" ]; then fail=$((fail+1)); failed+=("$src (align let)"); echo "FAIL $src (align let):$bad"; else pass=$((pass+1)); fi
fi

# QEMU: align(4096) fn page() sits at 0x9000 in a kernel loaded at 0x8000 (tests/usermode/boot.lap reads it),
# runs, and reads the initialized data after the text. COM1 must show PAGE.
# Needs qemu-system-x86_64 and timeout (perl stands in on macOS). Without them it is skipped with a notice;
# CMP_REQUIRE=1 (the CI) makes that a failure.
if command -v qemu-system-x86_64 >/dev/null 2>&1 && command -v perl >/dev/null 2>&1; then
  if ! ( cd "$WORK" && "$CC" --boot -o aboot.bin "$ROOT/tests/usermode/boot.lap" 2>err.txt &&
         "$CC" --flat 0x8000 -o akern0.bin "$ROOT/tests/align/kernel.lap" 2>>err.txt &&
         "$CC" -O2 --flat 0x8000 -o akern2.bin "$ROOT/tests/align/kernel.lap" 2>>err.txt ); then
    fail=$((fail+1)); failed+=("tests/align/kernel.lap"); echo "FAIL tests/align/kernel.lap:"; cat "$WORK/err.txt"
  else
    for kern in akern0.bin akern2.bin; do
      if [ "$(wc -c < "$WORK/$kern" | tr -d ' ')" -gt 8192 ]; then
        fail=$((fail+1)); failed+=("tests/align $kern (size)"); echo "FAIL tests/align $kern: larger than the 16 sectors the boot sector reads"; continue
      fi
      dd if=/dev/zero of="$WORK/apad.bin" bs=8192 count=1 2>/dev/null
      dd if="$WORK/$kern" of="$WORK/apad.bin" conv=notrunc 2>/dev/null
      cat "$WORK/aboot.bin" "$WORK/apad.bin" > "$WORK/adisk.img"
      rm -f "$WORK/aserial.txt"
      timeout 8 qemu-system-x86_64 -display none -boot a \
        -drive file="$WORK/adisk.img",format=raw,if=floppy,index=0 \
        -serial file:"$WORK/aserial.txt" </dev/null >/dev/null 2>&1 || true
      if ! grep -q PAGE "$WORK/aserial.txt" 2>/dev/null; then
        fail=$((fail+1)); failed+=("tests/align $kern qemu"); echo "FAIL tests/align $kern: QEMU serial did not contain PAGE"; od -An -tx1c "$WORK/aserial.txt" 2>&1 | head -3; continue
      fi
      pass=$((pass+1))
    done
  fi
else
  if [ "${CMP_REQUIRE:-0}" = 1 ]; then
    fail=$((fail+1)); failed+=("tests/align qemu (missing tool)"); echo "FAIL tests/align: CMP_REQUIRE=1 and qemu-system-x86_64 or perl is missing"
  else
    echo "skip tests/align QEMU run: qemu-system-x86_64 or perl is not installed (CMP_REQUIRE=1 makes this an error)"
  fi
fi

# 4d. Stack probing (0.44.2). A frame of 4096 bytes or more allocates with a loop that touches every
# page; a smaller one keeps the single `sub rsp, __frame_F`. The assembly is read in every mode that
# has a prologue (hosted, --freestanding, --flat, --flat --axe2 including the interrupt and syscall
# functions), at -O0 and -O2, and the result must assemble. probe_check <label> <src> <probed fns>
# <plain fns> <compiler flags...>
probe_check() {
  local label=$1 src=$2 probed=$3 plain=$4; shift 4
  local asm="$WORK/probe.asm" bad="" f n=0
  if ! ( cd "$WORK" && "$CC" "$@" -S -o probe.asm "$ROOT/$src" 2>err.txt && "$CC" "$@" -o probe.out "$ROOT/$src" 2>>err.txt ); then
    fail=$((fail+1)); failed+=("$label"); echo "FAIL $label:"; cat "$WORK/err.txt"; return
  fi
  for f in $probed; do
    n=$((n+1))
    grep -qxF ".Lprobe_$f:" "$asm" || bad="$bad $f has no probe loop;"
    grep -qxF "    ja .Lprobe_$f" "$asm" || bad="$bad $f has no ja .Lprobe_$f;"
    grep -qxF "    sub rsp, __frame_$f" "$asm" && bad="$bad $f still has the single sub rsp;"
  done
  for f in $plain; do
    grep -qF ".Lprobe_$f" "$asm" && bad="$bad $f (under 4096) has a probe loop;"
    grep -qxF "    sub rsp, __frame_$f" "$asm" || bad="$bad $f lost its single sub rsp;"
  done
  [ "$(grep -cxF '    sub rsp, 4096' "$asm")" = $n ] || bad="$bad expected $n page steps (sub rsp, 4096);"
  [ "$(grep -cxF '    or qword [rsp], 0' "$asm")" = $((2*n)) ] || bad="$bad expected $((2*n)) touches (or qword [rsp], 0);"
  [ "$(grep -cxF '    cmp r11, 4096' "$asm")" = $n ] || bad="$bad expected $n cmp r11, 4096;"
  [ "$(grep -cxF '    sub rsp, r11' "$asm")" = $n ] || bad="$bad expected $n sub rsp, r11;"
  if [ -n "$bad" ]; then fail=$((fail+1)); failed+=("$label"); echo "FAIL $label:$bad"; else pass=$((pass+1)); fi
}
for opt in -O0 -O2; do
  probe_check "tests/stackprobe/frames.lap $opt (hosted)" tests/stackprobe/frames.lap "big" "small" $opt
  probe_check "tests/stackprobe/frames.lap $opt (--freestanding)" tests/stackprobe/frames.lap "big" "small" $opt --freestanding
  probe_check "tests/stackprobe/flat/frames.lap $opt (--flat)" tests/stackprobe/flat/frames.lap "big big_int big_sys" "small" $opt --flat 0x10000
  probe_check "tests/stackprobe/flat/frames.lap $opt (--flat --axe2)" tests/stackprobe/flat/frames.lap "big big_int big_sys" "small" $opt --flat 0x400000 --axe2
done
# The same assembly with nasm, which is the reference for the built-in assembler.
for mode in hosted freestanding flat; do
  case $mode in
    hosted)       src=tests/stackprobe/frames.lap; flags=""; fmt=elf64 ;;
    freestanding) src=tests/stackprobe/frames.lap; flags="--freestanding"; fmt=elf64 ;;
    flat)         src=tests/stackprobe/flat/frames.lap; flags="--flat 0x10000"; fmt=bin ;;
  esac
  label="$src (stack probe assembles with nasm, $mode)"
  if ( cd "$WORK" && "$CC" $flags -S -o sp.asm "$ROOT/$src" 2>err.txt && nasm -f $fmt -o sp.o sp.asm 2>>err.txt ); then pass=$((pass+1))
  else fail=$((fail+1)); failed+=("$label"); echo "FAIL $label:"; cat "$WORK/err.txt"; fi
done
# Golden assembly of the flat file at -O0: the whole prologue sequence, byte for byte as text.
if ( cd "$WORK" && "$CC" --flat 0x10000 -S -o spf.asm "$ROOT/tests/stackprobe/flat/frames.lap" 2>err.txt ); then
  grep -vE '^\s*(;|$)' "$WORK/spf.asm" | grep -v '^; Generated' > "$WORK/spf.norm"
  check "tests/stackprobe/flat/frames.lap (assembly)" tests/expected/stackprobe/frames.asm.txt "$WORK/spf.norm"
else fail=$((fail+1)); failed+=("tests/stackprobe/flat/frames.lap (assembly)"); echo "FAIL golden:"; cat "$WORK/err.txt"; fi

# 4e. QEMU: tests/stackprobe/flat/kernel.lap has a guard page (not present) 32KB under the stack and
# calls a function whose frame is 64KB. The sector is tests/usermode/boot.lap. COM1 must show S (the
# small frame is fine) and then PFG (page fault, CR2 inside the guard page); NOPF means the frame
# stepped over the guard. Needs qemu-system-x86_64 and perl; without them it is skipped with a notice,
# and CMP_REQUIRE=1 (the CI) makes that a failure. Nothing is claimed here that QEMU did not print.
if command -v qemu-system-x86_64 >/dev/null 2>&1 && command -v perl >/dev/null 2>&1; then
  if ! ( cd "$WORK" && "$CC" --boot -o sboot.bin "$ROOT/tests/usermode/boot.lap" 2>err.txt &&
         "$CC" --flat 0x8000 -o skern0.bin "$ROOT/tests/stackprobe/flat/kernel.lap" 2>>err.txt &&
         "$CC" -O2 --flat 0x8000 -o skern2.bin "$ROOT/tests/stackprobe/flat/kernel.lap" 2>>err.txt ); then
    fail=$((fail+1)); failed+=("tests/stackprobe/flat/kernel.lap"); echo "FAIL tests/stackprobe/flat/kernel.lap:"; cat "$WORK/err.txt"
  else
    for kern in skern0.bin skern2.bin; do
      if [ "$(wc -c < "$WORK/$kern" | tr -d ' ')" -gt 8192 ]; then
        fail=$((fail+1)); failed+=("tests/stackprobe $kern (size)"); echo "FAIL tests/stackprobe $kern: larger than the 16 sectors the boot sector reads"; continue
      fi
      dd if=/dev/zero of="$WORK/spad.bin" bs=8192 count=1 2>/dev/null
      dd if="$WORK/$kern" of="$WORK/spad.bin" conv=notrunc 2>/dev/null
      cat "$WORK/sboot.bin" "$WORK/spad.bin" > "$WORK/sdisk.img"
      rm -f "$WORK/sserial.txt"
      timeout 8 qemu-system-x86_64 -display none -boot a \
        -drive file="$WORK/sdisk.img",format=raw,if=floppy,index=0 \
        -serial file:"$WORK/sserial.txt" </dev/null >/dev/null 2>&1 || true
      if [ "$(cat "$WORK/sserial.txt" 2>/dev/null)" != SPFG ]; then
        fail=$((fail+1)); failed+=("tests/stackprobe $kern qemu"); echo "FAIL tests/stackprobe $kern: COM1 should be SPFG, got:"; od -An -c "$WORK/sserial.txt" 2>&1 | head -3; continue
      fi
      pass=$((pass+1))
    done
  fi
else
  if [ "${CMP_REQUIRE:-0}" = 1 ]; then
    fail=$((fail+1)); failed+=("tests/stackprobe qemu (missing tool)"); echo "FAIL tests/stackprobe: CMP_REQUIRE=1 and qemu-system-x86_64 or perl is missing"
  else
    echo "skip tests/stackprobe QEMU run: qemu-system-x86_64 or perl is not installed (CMP_REQUIRE=1 makes this an error)"
  fi
fi

# 5. Boot sectors: tests/boot/*.lap build with --boot into a 512-byte image
# 6. tests/usermode: boot sector plus a flat kernel. QEMU must print USER.
#    (org 0x7C00, signature 0xAA55). -O0 and -O2 must match. When QEMU is
#    installed, the image is booted from the floppy and COM1 must show COPIHUE.
# 4b. Absolute constant addresses that do not fit in a signed disp32 (0.42.4),
#     in --freestanding and --flat, at -O0 and -O2. The asm must hold the full
#     address in a register for 1, 2, 4 and 8 byte loads and stores, keep the
#     disp32 form for 0xFFFFFFFF80010000, and nasm must not warn that a value
#     exceeds bounds (that warning is the silent truncation).
for mode in freestanding flat; do
  if [ $mode = freestanding ]; then src=tests/freestanding/abs64.lap; flags="--freestanding"; fmt=elf64
  else src=tests/flat/abs64.lap; flags="--flat 0x10000"; fmt=bin; fi
  for opt in -O0 -O2; do
    label="$src $opt (absolute addresses)"
    if ! ( cd "$WORK" && "$CC" $opt $flags -S -o abs.asm "$ROOT/$src" 2>err.txt ); then
      fail=$((fail+1)); failed+=("$label"); echo "FAIL $label:"; cat "$WORK/err.txt"; continue
    fi
    bad=""
    for want in 'mov rax, 0xFFFF800000001000' 'mov rbx, 0xFFFF800000002000' 'mov rbx, 0xFFFF800000003000' \
                'movzx rax, byte [rax]' 'movzx rax, word [rax]' 'mov eax, [rax]' 'mov rax, [rax]' \
                'mov byte [rbx], 7' 'mov word [rbx], 7' 'mov dword [rbx], 7' 'mov qword [rbx], 7' \
                'mov byte [rbx], al' 'mov word [rbx], ax' 'mov dword [rbx], eax' 'mov [rbx], rax' \
                'movzx rax, byte [-2147418112]'; do
      grep -qF -- "$want" "$WORK/abs.asm" || bad="$bad missing '$want';"
    done
    # No raw 64-bit constant may be left as a memory operand.
    if grep -oE '\[-?[0-9]+\]' "$WORK/abs.asm" | tr -d '[]' \
         | awk '$1 < -2147483648 || $1 > 2147483647 { f = 1 } END { exit !f }'; then
      bad="$bad truncated address left as [N];"
    fi
    warn=$(nasm -f $fmt -o "$WORK/abs.out" "$WORK/abs.asm" 2>&1 | grep -E 'exceeds bounds')
    [ -n "$warn" ] && bad="$bad nasm: $warn;"
    if [ -n "$bad" ]; then fail=$((fail+1)); failed+=("$label"); echo "FAIL $label:$bad"; else pass=$((pass+1)); fi
  done
done

for src in tests/boot/*.lap; do
  name=$(basename "$src" .lap)
  if ! ( cd "$WORK" && "$CC" --boot -o boot.bin "$ROOT/$src" 2>err.txt && "$CC" -O2 --boot -o boot2.bin "$ROOT/$src" 2>>err.txt ); then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src:"; cat "$WORK/err.txt"; continue
  fi
  if ! cmp -s "$WORK/boot.bin" "$WORK/boot2.bin"; then
    fail=$((fail+1)); failed+=("$src (-O0 vs -O2)"); echo "FAIL $src: -O0 and -O2 images differ"; continue
  fi
  if [ "$(wc -c < "$WORK/boot.bin")" -ne 512 ] || [ "$(tail -c 2 "$WORK/boot.bin" | od -An -tx1 | tr -d ' \n')" != "55aa" ]; then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: not a 512-byte sector with signature 55 aa"; continue
  fi
  od -An -tx1 -v "$WORK/boot.bin" | tr -s ' ' | sed -e 's/ *$//' -e '/^$/d' > "$WORK/boot.hex"
  check "$src" "tests/expected/boot/$name.hex" "$WORK/boot.hex"
  if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    rm -f "$WORK/serial.txt"
    timeout 8 qemu-system-x86_64 -display none -boot a \
      -drive file="$WORK/boot.bin",format=raw,if=floppy,index=0 \
      -serial file:"$WORK/serial.txt" </dev/null >/dev/null 2>&1 || true
    if ! grep -q COPIHUE "$WORK/serial.txt"; then
      fail=$((fail+1)); failed+=("$src qemu"); echo "FAIL $src: QEMU serial did not contain COPIHUE"; od -An -tx1c "$WORK/serial.txt" | head; continue
    fi
    pass=$((pass+1))
  fi
done

# 6. User mode: a 512-byte real-mode sector plus a flat kernel at 0x8000.
#    The sector reads 16 sectors (8192 bytes) to 0x8000. -O0 and -O2 must
#    match for the sector and for the kernel. QEMU, when installed, must
#    print USER on COM1 for both images.
if [ -f tests/usermode/boot.lap ] && [ -f tests/usermode/kernel.lap ]; then
  if ! ( cd "$WORK" && "$CC" --boot -o uboot.bin "$ROOT/tests/usermode/boot.lap" 2>err.txt && "$CC" -O2 --boot -o uboot2.bin "$ROOT/tests/usermode/boot.lap" 2>>err.txt ); then
    fail=$((fail+1)); failed+=("tests/usermode/boot.lap"); echo "FAIL tests/usermode/boot.lap:"; cat "$WORK/err.txt"
  elif ! cmp -s "$WORK/uboot.bin" "$WORK/uboot2.bin"; then
    fail=$((fail+1)); failed+=("tests/usermode/boot.lap (-O0 vs -O2)"); echo "FAIL tests/usermode/boot.lap: -O0 and -O2 images differ"
  elif [ "$(wc -c < "$WORK/uboot.bin")" -ne 512 ] || [ "$(tail -c 2 "$WORK/uboot.bin" | od -An -tx1 | tr -d ' \n')" != "55aa" ]; then
    fail=$((fail+1)); failed+=("tests/usermode/boot.lap"); echo "FAIL tests/usermode/boot.lap: not a 512-byte sector with signature 55 aa"
  else
    od -An -tx1 -v "$WORK/uboot.bin" | tr -s ' ' | sed -e 's/ *$//' -e '/^$/d' > "$WORK/uboot.hex"
    check "tests/usermode/boot.lap" "tests/expected/usermode/boot.hex" "$WORK/uboot.hex"
    if ! ( cd "$WORK" && "$CC" --flat 0x8000 -o ukernel.bin "$ROOT/tests/usermode/kernel.lap" 2>err.txt && "$CC" -O2 --flat 0x8000 -o ukernel2.bin "$ROOT/tests/usermode/kernel.lap" 2>>err.txt ); then
      fail=$((fail+1)); failed+=("tests/usermode/kernel.lap"); echo "FAIL tests/usermode/kernel.lap:"; cat "$WORK/err.txt"
    else
      ksz=$(wc -c < "$WORK/ukernel.bin")
      ksz2=$(wc -c < "$WORK/ukernel2.bin")
      if ! cmp -s "$WORK/ukernel.bin" "$WORK/ukernel2.bin"; then
        fail=$((fail+1)); failed+=("tests/usermode/kernel.lap (-O0 vs -O2)"); echo "FAIL tests/usermode/kernel.lap: -O0 and -O2 images differ"
      elif [ "$ksz" -gt 8192 ] || [ "$ksz2" -gt 8192 ]; then
        fail=$((fail+1)); failed+=("tests/usermode/kernel.lap"); echo "FAIL tests/usermode/kernel.lap: image is larger than 16 sectors ($ksz / $ksz2)"
      else
        od -An -tx1 -v "$WORK/ukernel.bin" | tr -s ' ' | sed -e 's/ *$//' -e '/^$/d' > "$WORK/ukernel.hex"
        check "tests/usermode/kernel.lap" "tests/expected/usermode/kernel.hex" "$WORK/ukernel.hex"
        if command -v qemu-system-x86_64 >/dev/null 2>&1; then
          for kern in ukernel.bin ukernel2.bin; do
            dd if=/dev/zero of="$WORK/upad.bin" bs=8192 count=1 status=none
            dd if="$WORK/$kern" of="$WORK/upad.bin" conv=notrunc status=none
            cat "$WORK/uboot.bin" "$WORK/upad.bin" > "$WORK/udisk.img"
            rm -f "$WORK/userial.txt"
            timeout 8 qemu-system-x86_64 -display none -boot a \
              -drive file="$WORK/udisk.img",format=raw,if=floppy,index=0 \
              -serial file:"$WORK/userial.txt" </dev/null >/dev/null 2>&1 || true
            if ! grep -q USER "$WORK/userial.txt"; then
              fail=$((fail+1)); failed+=("tests/usermode $kern qemu"); echo "FAIL tests/usermode $kern: QEMU serial did not contain USER"; od -An -tx1c "$WORK/userial.txt" | head; continue
            fi
            pass=$((pass+1))
          done
        fi
      fi
    fi
  fi
fi

# 7. Context switch: same disk layout as tests/usermode (sector plus 16
#    sectors at 0x8000). QEMU must print COPIHUE: a #PF handler rewrites
#    the frame, an IRQ handler switches stacks, and restore_context returns.
if [ -f tests/ctx/boot.lap ] && [ -f tests/ctx/kernel.lap ]; then
  if ! ( cd "$WORK" && "$CC" --boot -o cboot.bin "$ROOT/tests/ctx/boot.lap" 2>err.txt && "$CC" -O2 --boot -o cboot2.bin "$ROOT/tests/ctx/boot.lap" 2>>err.txt ); then
    fail=$((fail+1)); failed+=("tests/ctx/boot.lap"); echo "FAIL tests/ctx/boot.lap:"; cat "$WORK/err.txt"
  elif ! cmp -s "$WORK/cboot.bin" "$WORK/cboot2.bin"; then
    fail=$((fail+1)); failed+=("tests/ctx/boot.lap (-O0 vs -O2)"); echo "FAIL tests/ctx/boot.lap: -O0 and -O2 images differ"
  elif [ "$(wc -c < "$WORK/cboot.bin")" -ne 512 ] || [ "$(tail -c 2 "$WORK/cboot.bin" | od -An -tx1 | tr -d ' \n')" != "55aa" ]; then
    fail=$((fail+1)); failed+=("tests/ctx/boot.lap"); echo "FAIL tests/ctx/boot.lap: not a 512-byte sector with signature 55 aa"
  else
    od -An -tx1 -v "$WORK/cboot.bin" | tr -s ' ' | sed -e 's/ *$//' -e '/^$/d' > "$WORK/cboot.hex"
    check "tests/ctx/boot.lap" "tests/expected/ctx/boot.hex" "$WORK/cboot.hex"
    if ! ( cd "$WORK" && "$CC" --flat 0x8000 -o ckernel.bin "$ROOT/tests/ctx/kernel.lap" 2>err.txt && "$CC" -O2 --flat 0x8000 -o ckernel2.bin "$ROOT/tests/ctx/kernel.lap" 2>>err.txt ); then
      fail=$((fail+1)); failed+=("tests/ctx/kernel.lap"); echo "FAIL tests/ctx/kernel.lap:"; cat "$WORK/err.txt"
    else
      ksz=$(wc -c < "$WORK/ckernel.bin")
      ksz2=$(wc -c < "$WORK/ckernel2.bin")
      if ! cmp -s "$WORK/ckernel.bin" "$WORK/ckernel2.bin"; then
        fail=$((fail+1)); failed+=("tests/ctx/kernel.lap (-O0 vs -O2)"); echo "FAIL tests/ctx/kernel.lap: -O0 and -O2 images differ"
      elif [ "$ksz" -gt 8192 ] || [ "$ksz2" -gt 8192 ]; then
        fail=$((fail+1)); failed+=("tests/ctx/kernel.lap"); echo "FAIL tests/ctx/kernel.lap: image is larger than 16 sectors ($ksz / $ksz2)"
      else
        od -An -tx1 -v "$WORK/ckernel.bin" | tr -s ' ' | sed -e 's/ *$//' -e '/^$/d' > "$WORK/ckernel.hex"
        check "tests/ctx/kernel.lap" "tests/expected/ctx/kernel.hex" "$WORK/ckernel.hex"
        if command -v qemu-system-x86_64 >/dev/null 2>&1; then
          for kern in ckernel.bin ckernel2.bin; do
            dd if=/dev/zero of="$WORK/cpad.bin" bs=8192 count=1 status=none
            dd if="$WORK/$kern" of="$WORK/cpad.bin" conv=notrunc status=none
            cat "$WORK/cboot.bin" "$WORK/cpad.bin" > "$WORK/cdisk.img"
            rm -f "$WORK/cserial.txt"
            timeout 8 qemu-system-x86_64 -display none -boot a \
              -drive file="$WORK/cdisk.img",format=raw,if=floppy,index=0 \
              -serial file:"$WORK/cserial.txt" </dev/null >/dev/null 2>&1 || true
            if ! grep -q COPIHUE "$WORK/cserial.txt"; then
              fail=$((fail+1)); failed+=("tests/ctx $kern qemu"); echo "FAIL tests/ctx $kern: QEMU serial did not contain COPIHUE"; od -An -tx1c "$WORK/cserial.txt" | head; continue
            fi
            pass=$((pass+1))
          done
        fi
      fi
    fi
  fi
fi

# Several files on one command line share one namespace. The hosted loop
# compiles a single path, so this block is separate. Helpers live under
# parts/ and are not matched by tests/*/*.lap.
# From here to the `-c --flat` check, programs are linked with ld and run: Linux only.
if [ $CAN_RUN = 0 ]; then
  echo "skip multi-file and -c link tests: they link with ld and run the result, and this is $(uname -s) (tests/x64/run_obj.sh compares the -c objects with nasm's here)"
else
cli_run() {
  local opt=$1
  ( cd "$WORK" && rm -f cli_prog && "$CC" "$opt" -o cli_prog \
      "$ROOT/tests/lang/parts/cli_a.lap" "$ROOT/tests/lang/parts/cli_main.lap" \
      >/dev/null 2>err.txt ) || { echo "COMPILE ERROR"; sed "s|$ROOT/||g" "$WORK/err.txt"; return; }
  ( cd "$WORK" && ./cli_prog </dev/null; echo "exit=$?" ) | mask
}
cli_run -O0 > "$WORK/cli0"
cli_run -O2 > "$WORK/cli2"
if ! cmp -s "$WORK/cli0" "$WORK/cli2"; then
  fail=$((fail+1)); failed+=("cli multi-file (-O0 vs -O2)"); echo "FAIL cli multi-file: -O0 and -O2 differ"
elif grep -q "^COMPILE ERROR" "$WORK/cli0"; then
  fail=$((fail+1)); failed+=("cli multi-file"); echo "FAIL cli multi-file:"; cat "$WORK/cli0"
else
  check "cli multi-file" "tests/expected/tests/lang/cli_multi.out" "$WORK/cli0"
fi
if ( cd "$WORK" && "$CC" -S -o out.asm \
    "$ROOT/tests/errors/parts/cli_dup_a.lap" "$ROOT/tests/errors/parts/cli_dup_b.lap" \
    >/dev/null 2>err.txt ); then
  fail=$((fail+1)); failed+=("cli duplicate"); echo "FAIL cli duplicate: compiled but should have failed"
else
  sed "s|$ROOT/||g" "$WORK/err.txt" > "$WORK/err.norm"
  check "cli duplicate" "tests/expected/errors/cli_dup.err" "$WORK/err.norm"
fi

# -c writes one ELF64 object. Two invocations, then ld, with no libc.
# Helpers live under tests/link/parts and are not matched by tests/*/*.lap.
link_run() {
  local opt=$1
  ( cd "$WORK" && rm -f mod_prog mod_add.o mod_main.o &&
    "$CC" "$opt" -c -o mod_add.o "$ROOT/tests/link/parts/mod_add.lap" &&
    "$CC" "$opt" -c -o mod_main.o "$ROOT/tests/link/parts/mod_main.lap" &&
    ld mod_add.o mod_main.o -o mod_prog ) >/dev/null 2>err.txt || {
      echo "COMPILE ERROR"; sed "s|$ROOT/||g" "$WORK/err.txt"; return; }
  ( cd "$WORK" && ./mod_prog </dev/null; echo "exit=$?" ) | mask
}
link_run -O0 > "$WORK/link0"
if ! nm "$WORK/mod_add.o" | grep -q ' T add' || \
   ! nm "$WORK/mod_add.o" | grep -q ' D bias' || \
   nm "$WORK/mod_add.o" | grep -q ' _start$' || \
   ! nm -u "$WORK/mod_main.o" | grep -q ' add$' || \
   ! nm -u "$WORK/mod_main.o" | grep -q ' bias$' || \
   ! nm "$WORK/mod_main.o" | grep -q ' T main' || \
   ! nm "$WORK/mod_main.o" | grep -q ' T _start'; then
  fail=$((fail+1)); failed+=("link symbols"); echo "FAIL link symbols"; nm "$WORK/mod_add.o"; nm "$WORK/mod_main.o"
else
  pass=$((pass+1))
fi
if ( cd "$WORK" && ld mod_add.o mod_add.o -o mod_dup >/dev/null 2>&1 ); then
  fail=$((fail+1)); failed+=("link duplicate"); echo "FAIL link duplicate: ld accepted two definitions of add"
else
  pass=$((pass+1))
fi
link_run -O2 > "$WORK/link2"
if ! cmp -s "$WORK/link0" "$WORK/link2"; then
  fail=$((fail+1)); failed+=("link -c (-O0 vs -O2)"); echo "FAIL link -c: -O0 and -O2 differ"; diff "$WORK/link0" "$WORK/link2" | head -10
elif grep -q "^COMPILE ERROR" "$WORK/link0"; then
  fail=$((fail+1)); failed+=("link -c"); echo "FAIL link -c:"; cat "$WORK/link0"
else
  check "link -c" "tests/expected/tests/link/reloc.out" "$WORK/link0"
fi
# Same module name in two objects. The definition exports mem__add and
# mem__bias. The caller object redeclares `module mem`, externs those
# symbols, and a flat file in that same -c invocation defines main.
modlink_run() {
  local opt=$1
  ( cd "$WORK" && rm -f mem_prog mem_def.o mem_use.o &&
    "$CC" "$opt" -c -o mem_def.o "$ROOT/tests/link/parts/mod_mem.lap" &&
    "$CC" "$opt" -c -o mem_use.o \
      "$ROOT/tests/link/parts/mod_mem_use.lap" "$ROOT/tests/link/parts/mod_mem_main.lap" &&
    ld mem_def.o mem_use.o -o mem_prog ) >/dev/null 2>err.txt || {
      echo "COMPILE ERROR"; sed "s|$ROOT/||g" "$WORK/err.txt"; return; }
  ( cd "$WORK" && ./mem_prog </dev/null; echo "exit=$?" ) | mask
}
modlink_run -O0 > "$WORK/mlink0"
if ! nm "$WORK/mem_def.o" | grep -q ' T mem__add' || \
   ! nm "$WORK/mem_def.o" | grep -q ' D mem__bias' || \
   nm "$WORK/mem_def.o" | grep -q ' _start$' || \
   ! nm -u "$WORK/mem_use.o" | grep -q ' mem__add$' || \
   ! nm -u "$WORK/mem_use.o" | grep -q ' mem__bias$' || \
   ! nm "$WORK/mem_use.o" | grep -q ' T mem__use_it' || \
   ! nm "$WORK/mem_use.o" | grep -q ' T main' || \
   ! nm "$WORK/mem_use.o" | grep -q ' T _start'; then
  fail=$((fail+1)); failed+=("module link symbols"); echo "FAIL module link symbols"; nm "$WORK/mem_def.o"; nm "$WORK/mem_use.o"
else
  pass=$((pass+1))
fi
modlink_run -O2 > "$WORK/mlink2"
if ! cmp -s "$WORK/mlink0" "$WORK/mlink2"; then
  fail=$((fail+1)); failed+=("module link (-O0 vs -O2)"); echo "FAIL module link: -O0 and -O2 differ"; diff "$WORK/mlink0" "$WORK/mlink2" | head -10
elif grep -q "^COMPILE ERROR" "$WORK/mlink0"; then
  fail=$((fail+1)); failed+=("module link"); echo "FAIL module link:"; cat "$WORK/mlink0"
else
  check "module link" "tests/expected/tests/link/module.out" "$WORK/mlink0"
fi
# A generic function is one strong symbol per instantiation (`max$1$i32`).
# The template is not a symbol. The caller object externs that spelling.
genlink_run() {
  local opt=$1
  ( cd "$WORK" && rm -f gen_prog gen_max.o gen_main.o &&
    "$CC" "$opt" -c -o gen_max.o "$ROOT/tests/link/parts/gen_max.lap" &&
    "$CC" "$opt" -c -o gen_main.o "$ROOT/tests/link/parts/gen_main.lap" &&
    ld gen_max.o gen_main.o -o gen_prog ) >/dev/null 2>err.txt || {
      echo "COMPILE ERROR"; sed "s|$ROOT/||g" "$WORK/err.txt"; return; }
  ( cd "$WORK" && ./gen_prog </dev/null; echo "exit=$?" ) | mask
}
genlink_run -O0 > "$WORK/glink0"
if ! nm "$WORK/gen_max.o" | grep -F -q ' T max$1$i32' || \
   nm "$WORK/gen_max.o" | awk '$3=="max"{found=1} END{exit !found}' || \
   nm "$WORK/gen_max.o" | grep -q ' _start$' || \
   ! nm -u "$WORK/gen_main.o" | grep -F -q ' max$1$i32' || \
   ! nm "$WORK/gen_main.o" | grep -q ' T main' || \
   ! nm "$WORK/gen_main.o" | grep -q ' T _start'; then
  fail=$((fail+1)); failed+=("generic link symbols"); echo "FAIL generic link symbols"; nm "$WORK/gen_max.o"; nm "$WORK/gen_main.o"
else
  pass=$((pass+1))
fi
genlink_run -O2 > "$WORK/glink2"
if ! cmp -s "$WORK/glink0" "$WORK/glink2"; then
  fail=$((fail+1)); failed+=("generic link (-O0 vs -O2)"); echo "FAIL generic link: -O0 and -O2 differ"; diff "$WORK/glink0" "$WORK/glink2" | head -10
elif grep -q "^COMPILE ERROR" "$WORK/glink0"; then
  fail=$((fail+1)); failed+=("generic link"); echo "FAIL generic link:"; cat "$WORK/glink0"
else
  check "generic link" "tests/expected/tests/link/generic.out" "$WORK/glink0"
fi
# `?` is ordinary code. The defining object exports `add1`. The enum is not
# an ELF symbol; both objects write the same `enum Result`.
reslink_run() {
  local opt=$1
  ( cd "$WORK" && rm -f res_prog res_def.o res_use.o &&
    "$CC" "$opt" -c -o res_def.o "$ROOT/tests/link/parts/result_def.lap" &&
    "$CC" "$opt" -c -o res_use.o "$ROOT/tests/link/parts/result_use.lap" &&
    ld res_def.o res_use.o -o res_prog ) >/dev/null 2>err.txt || {
      echo "COMPILE ERROR"; sed "s|$ROOT/||g" "$WORK/err.txt"; return; }
  ( cd "$WORK" && ./res_prog </dev/null; echo "exit=$?" ) | mask
}
reslink_run -O0 > "$WORK/rlink0"
if ! nm "$WORK/res_def.o" | grep -q ' T add1$' || \
   nm "$WORK/res_def.o" | grep -q ' _start$' || \
   ! nm -u "$WORK/res_use.o" | grep -q ' add1$' || \
   ! nm "$WORK/res_use.o" | grep -q ' T main' || \
   ! nm "$WORK/res_use.o" | grep -q ' T _start'; then
  fail=$((fail+1)); failed+=("result link symbols"); echo "FAIL result link symbols"; nm "$WORK/res_def.o"; nm "$WORK/res_use.o"
else
  pass=$((pass+1))
fi
reslink_run -O2 > "$WORK/rlink2"
if ! cmp -s "$WORK/rlink0" "$WORK/rlink2"; then
  fail=$((fail+1)); failed+=("result link (-O0 vs -O2)"); echo "FAIL result link: -O0 and -O2 differ"; diff "$WORK/rlink0" "$WORK/rlink2" | head -10
elif grep -q "^COMPILE ERROR" "$WORK/rlink0"; then
  fail=$((fail+1)); failed+=("result link"); echo "FAIL result link:"; cat "$WORK/rlink0"
else
  check "result link" "tests/expected/tests/link/result.out" "$WORK/rlink0"
fi
# A str parameter is the []u8 ABI. The defining object exports `slen`.
strlink_run() {
  local opt=$1
  ( cd "$WORK" && rm -f str_prog str_def.o str_use.o &&
    "$CC" "$opt" -c -o str_def.o "$ROOT/tests/link/parts/str_def.lap" &&
    "$CC" "$opt" -c -o str_use.o "$ROOT/tests/link/parts/str_use.lap" &&
    ld str_def.o str_use.o -o str_prog ) >/dev/null 2>err.txt || {
      echo "COMPILE ERROR"; sed "s|$ROOT/||g" "$WORK/err.txt"; return; }
  ( cd "$WORK" && ./str_prog </dev/null; echo "exit=$?" ) | mask
}
strlink_run -O0 > "$WORK/slink0"
if ! nm "$WORK/str_def.o" | grep -q ' T slen$' || \
   nm "$WORK/str_def.o" | grep -q ' _start$' || \
   ! nm -u "$WORK/str_use.o" | grep -q ' slen$' || \
   ! nm "$WORK/str_use.o" | grep -q ' T main' || \
   ! nm "$WORK/str_use.o" | grep -q ' T _start'; then
  fail=$((fail+1)); failed+=("str link symbols"); echo "FAIL str link symbols"; nm "$WORK/str_def.o"; nm "$WORK/str_use.o"
else
  pass=$((pass+1))
fi
strlink_run -O2 > "$WORK/slink2"
if ! cmp -s "$WORK/slink0" "$WORK/slink2"; then
  fail=$((fail+1)); failed+=("str link (-O0 vs -O2)"); echo "FAIL str link: -O0 and -O2 differ"; diff "$WORK/slink0" "$WORK/slink2" | head -10
elif grep -q "^COMPILE ERROR" "$WORK/slink0"; then
  fail=$((fail+1)); failed+=("str link"); echo "FAIL str link:"; cat "$WORK/slink0"
else
  check "str link" "tests/expected/tests/link/str.out" "$WORK/slink0"
fi
# The same instantiation inside a module is `mem__max$1$i32`.
mgenlink_run() {
  local opt=$1
  ( cd "$WORK" && rm -f mgen_prog mgen_def.o mgen_use.o &&
    "$CC" "$opt" -c -o mgen_def.o "$ROOT/tests/link/parts/gen_mem.lap" &&
    "$CC" "$opt" -c -o mgen_use.o \
      "$ROOT/tests/link/parts/gen_mem_use.lap" "$ROOT/tests/link/parts/gen_mem_main.lap" &&
    ld mgen_def.o mgen_use.o -o mgen_prog ) >/dev/null 2>err.txt || {
      echo "COMPILE ERROR"; sed "s|$ROOT/||g" "$WORK/err.txt"; return; }
  ( cd "$WORK" && ./mgen_prog </dev/null; echo "exit=$?" ) | mask
}
mgenlink_run -O0 > "$WORK/mglink0"
if ! nm "$WORK/mgen_def.o" | grep -F -q ' T mem__max$1$i32' || \
   nm "$WORK/mgen_def.o" | grep -q ' _start$' || \
   ! nm -u "$WORK/mgen_use.o" | grep -F -q ' mem__max$1$i32' || \
   ! nm "$WORK/mgen_use.o" | grep -q ' T mem__call_it' || \
   ! nm "$WORK/mgen_use.o" | grep -q ' T main' || \
   ! nm "$WORK/mgen_use.o" | grep -q ' T _start'; then
  fail=$((fail+1)); failed+=("generic module link symbols"); echo "FAIL generic module link symbols"; nm "$WORK/mgen_def.o"; nm "$WORK/mgen_use.o"
else
  pass=$((pass+1))
fi
mgenlink_run -O2 > "$WORK/mglink2"
if ! cmp -s "$WORK/mglink0" "$WORK/mglink2"; then
  fail=$((fail+1)); failed+=("generic module link (-O0 vs -O2)"); echo "FAIL generic module link: -O0 and -O2 differ"; diff "$WORK/mglink0" "$WORK/mglink2" | head -10
elif grep -q "^COMPILE ERROR" "$WORK/mglink0"; then
  fail=$((fail+1)); failed+=("generic module link"); echo "FAIL generic module link:"; cat "$WORK/mglink0"
else
  check "generic module link" "tests/expected/tests/link/generic_mod.out" "$WORK/mglink0"
fi
fi   # CAN_RUN
if ( cd "$WORK" && "$CC" -c --flat 0x10000 -o flat_bad.o "$ROOT/tests/link/parts/mod_add.lap" >/dev/null 2>err.txt ); then
  fail=$((fail+1)); failed+=("link -c --flat"); echo "FAIL link -c --flat: compiled"
elif ! grep -q "cannot be combined with --flat or --boot" "$WORK/err.txt"; then
  fail=$((fail+1)); failed+=("link -c --flat"); echo "FAIL link -c --flat:"; cat "$WORK/err.txt"
else
  pass=$((pass+1))
fi
if ! ( cd "$WORK" && "$CC" --freestanding -c -o sec.o "$ROOT/tests/link/parts/sec_plain.lap" >/dev/null 2>err.txt &&
       "$CC" --freestanding -c -S -o sec.asm "$ROOT/tests/link/parts/sec_plain.lap" >/dev/null 2>>err.txt &&
       nasm -f elf64 sec.asm -o sec2.o 2>nas.err ); then
  fail=$((fail+1)); failed+=("link section"); echo "FAIL link section:"; cat "$WORK/err.txt" "$WORK/nas.err"
elif grep -q vstart "$WORK/sec.asm" || grep -i -q vstart "$WORK/nas.err"; then
  fail=$((fail+1)); failed+=("link section vstart"); echo "FAIL link section: vstart leaked into -c"; cat "$WORK/nas.err"
else
  pass=$((pass+1))
fi

# Two freestanding bits 32 objects, linked to a raw image at 0x8000.
# tests/usermode/boot.lap jumps there in protected mode. QEMU must print COPIHUE.
link_fs() {
  local opt=$1 out=$2
  ( cd "$WORK" &&
    "$CC" "$opt" --freestanding -c -o fs_putc.o "$ROOT/tests/link/parts/fs_putc.lap" &&
    "$CC" "$opt" --freestanding -c -o fs_entry.o "$ROOT/tests/link/parts/fs_entry.lap" &&
    ld -T "$ROOT/tests/link/flat.ld" -e _start --oformat binary fs_entry.o fs_putc.o -o "$out" ) \
    >/dev/null 2>err.txt
}
if [ $CAN_RUN = 0 ]; then
  echo "skip freestanding link test: it links with a GNU ld script, and ld on $(uname -s) is not GNU ld"
elif ! ( cd "$WORK" && "$CC" --boot -o link_boot.bin "$ROOT/tests/usermode/boot.lap" >/dev/null 2>err.txt ); then
  fail=$((fail+1)); failed+=("link boot"); echo "FAIL link boot:"; cat "$WORK/err.txt"
elif ! link_fs -O0 fs0.bin || ! link_fs -O2 fs2.bin; then
  fail=$((fail+1)); failed+=("link freestanding"); echo "FAIL link freestanding:"; cat "$WORK/err.txt"
elif [ "$(wc -c < "$WORK/fs0.bin")" -gt 8192 ] || [ "$(wc -c < "$WORK/fs2.bin")" -gt 8192 ]; then
  fail=$((fail+1)); failed+=("link freestanding"); echo "FAIL link freestanding: image larger than 16 sectors"
else
  pass=$((pass+1))
  if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    for kern in fs0.bin fs2.bin; do
      dd if=/dev/zero of="$WORK/lpad.bin" bs=8192 count=1 status=none
      dd if="$WORK/$kern" of="$WORK/lpad.bin" conv=notrunc status=none
      cat "$WORK/link_boot.bin" "$WORK/lpad.bin" > "$WORK/ldisk.img"
      rm -f "$WORK/lserial.txt"
      timeout 8 qemu-system-x86_64 -display none -boot a \
        -drive file="$WORK/ldisk.img",format=raw,if=floppy,index=0 \
        -serial file:"$WORK/lserial.txt" </dev/null >/dev/null 2>&1 || true
      if ! grep -q COPIHUE "$WORK/lserial.txt"; then
        fail=$((fail+1)); failed+=("link $kern qemu"); echo "FAIL link $kern: QEMU serial did not contain COPIHUE"; od -An -tx1c "$WORK/lserial.txt" | head; continue
      fi
      pass=$((pass+1))
    done
  fi
fi

# Hosted -O2: byte copy, byte bump, and a power-of-two struct walk.
if ! ( cd "$WORK" && "$CC" -O2 -S -o o2mem.asm "$ROOT/tests/lang/o2_mem.lap" >/dev/null 2>err.txt ); then
  fail=$((fail+1)); failed+=("o2_mem asm"); echo "FAIL o2_mem asm:"; cat "$WORK/err.txt"
elif ! grep -q 'rep movsb' "$WORK/o2mem.asm" || ! grep -q 'paddb' "$WORK/o2mem.asm" || \
     ! grep -q 'shl rax, 4' "$WORK/o2mem.asm" || ! grep -q 'psadbw' "$WORK/o2mem.asm" || \
     ! grep -q 'add byte' "$WORK/o2mem.asm"; then
  fail=$((fail+1)); failed+=("o2_mem asm"); echo "FAIL o2_mem asm: rewrite missing"
else
  pass=$((pass+1))
fi

# Hosted -O2: a discarded fetch_add is lock add on the cell, including a
# struct field. A used fetch_add stays lock xadd.
if ! ( cd "$WORK" && "$CC" -O2 -S -o o2at.asm "$ROOT/tests/lang/o2_atomic.lap" >/dev/null 2>err.txt ); then
  fail=$((fail+1)); failed+=("o2_atomic asm"); echo "FAIL o2_atomic asm:"; cat "$WORK/err.txt"
elif ! grep -q 'lock add qword \[cell\], 1' "$WORK/o2at.asm" || \
     ! grep -q 'lock add qword \[pair\], 1' "$WORK/o2at.asm" || \
     ! grep -q 'lock add qword \[pair+8\], 2' "$WORK/o2at.asm" || \
     ! grep -q 'lock xadd' "$WORK/o2at.asm"; then
  fail=$((fail+1)); failed+=("o2_atomic asm"); echo "FAIL o2_atomic asm: lock add missing"
else
  pass=$((pass+1))
fi

# []T. Hosted checks s[i], a for that assigns the index, and a for that
# assigns the slice (three jae .Lslice_trap_). for x in, for i in 0..s.len,
# for i in 0..4 on an array of at least 4 elements, s.ptr[i], and unsafe
# add none. One shorter array keeps the local-array trap (three mentions of
# one .Lbounds_error_). unsafe { a[9] } adds none. --freestanding and --flat
# omit the slice trap and the local-array trap, and do not emit syscall,
# unless --checked, which brings both back.
slice_traps() { grep -c 'jae \.Lslice_trap_' "$1" || true; }
if ! ( cd "$WORK" &&
       "$CC" -O0 -S -o slice0.asm "$ROOT/tests/link/parts/slice_chk.lap" >/dev/null 2>err.txt &&
       "$CC" -O2 -S -o slice2.asm "$ROOT/tests/link/parts/slice_chk.lap" >/dev/null 2>>err.txt &&
       "$CC" --freestanding -S -o slicefs.asm "$ROOT/tests/link/parts/slice_chk.lap" >/dev/null 2>>err.txt &&
       "$CC" --freestanding --checked -S -o slicefsc.asm "$ROOT/tests/link/parts/slice_chk.lap" >/dev/null 2>>err.txt &&
       "$CC" --flat 0x10000 -S -o sliceflat.asm "$ROOT/tests/link/parts/slice_chk.lap" >/dev/null 2>>err.txt &&
       "$CC" --flat 0x10000 --checked -S -o sliceflatc.asm "$ROOT/tests/link/parts/slice_chk.lap" >/dev/null 2>>err.txt ); then
  fail=$((fail+1)); failed+=("slice bounds asm"); echo "FAIL slice bounds asm:"; cat "$WORK/err.txt"
else
  b0=$(grep -c 'Lbounds_error_' "$WORK/slice0.asm" || true)
  b2=$(grep -c 'Lbounds_error_' "$WORK/slice2.asm" || true)
  bfs=$(grep -c 'Lbounds_error_' "$WORK/slicefs.asm" || true)
  bflat=$(grep -c 'Lbounds_error_' "$WORK/sliceflat.asm" || true)
  bfsc=$(grep -c 'Lbounds_error_' "$WORK/slicefsc.asm" || true)
  bflatc=$(grep -c 'Lbounds_error_' "$WORK/sliceflatc.asm" || true)
  if [ "$(slice_traps "$WORK/slice0.asm")" != 3 ] || [ "$(slice_traps "$WORK/slice2.asm")" != 3 ] || \
     [ "$b0" != 3 ] || [ "$b2" != 3 ] || [ "$bfs" != 0 ] || [ "$bflat" != 0 ] || \
     [ "$(slice_traps "$WORK/slicefs.asm")" != 0 ] || \
     [ "$(slice_traps "$WORK/sliceflat.asm")" != 0 ] || \
     grep -qE '^[[:space:]]*syscall' "$WORK/slicefs.asm" || \
     grep -qE '^[[:space:]]*syscall' "$WORK/sliceflat.asm" || \
     [ "$(slice_traps "$WORK/slicefsc.asm")" != 3 ] || \
     [ "$(slice_traps "$WORK/sliceflatc.asm")" != 3 ] || \
     [ "$bfsc" != 3 ] || [ "$bflatc" != 3 ]; then
    fail=$((fail+1)); failed+=("slice bounds asm"); echo "FAIL slice bounds asm: trap counts"
    echo "o0=$(slice_traps "$WORK/slice0.asm") o2=$(slice_traps "$WORK/slice2.asm") bounds=$b0/$b2 fs=$(slice_traps "$WORK/slicefs.asm") fsc=$(slice_traps "$WORK/slicefsc.asm") flat=$(slice_traps "$WORK/sliceflat.asm") flatc=$(slice_traps "$WORK/sliceflatc.asm") arr=$bfs/$bflat/$bfsc/$bflatc"
  else
    pass=$((pass+1))
  fi
fi

# --flat --elf joins several files and one module into an ELF64 executable.
# Named sections keep the nasm vstart. The section bytes are the raw --flat
# image. This image is not booted: .console is at 0x12000, not at its file
# offset plus 0x10000.
if ! ( cd "$WORK" &&
       "$CC" --flat 0x10000 --elf -o k.elf \
         "$ROOT/tests/link/parts/flat_entry.lap" \
         "$ROOT/tests/link/parts/flat_console.lap" \
         "$ROOT/tests/link/parts/flat_note.lap" >/dev/null 2>err.txt &&
       "$CC" -O2 --flat 0x10000 --elf -o k2.elf \
         "$ROOT/tests/link/parts/flat_entry.lap" \
         "$ROOT/tests/link/parts/flat_console.lap" \
         "$ROOT/tests/link/parts/flat_note.lap" >/dev/null 2>>err.txt &&
       "$CC" --flat 0x10000 -o k.bin \
         "$ROOT/tests/link/parts/flat_entry.lap" \
         "$ROOT/tests/link/parts/flat_console.lap" \
         "$ROOT/tests/link/parts/flat_note.lap" >/dev/null 2>>err.txt &&
       "$CC" -O2 --flat 0x10000 -o k2.bin \
         "$ROOT/tests/link/parts/flat_entry.lap" \
         "$ROOT/tests/link/parts/flat_console.lap" \
         "$ROOT/tests/link/parts/flat_note.lap" >/dev/null 2>>err.txt ); then
  fail=$((fail+1)); failed+=("flat elf"); echo "FAIL flat elf:"; cat "$WORK/err.txt"
elif ! cmp -s "$WORK/k.bin" "$WORK/k2.bin" || ! cmp -s "$WORK/k.elf" "$WORK/k2.elf"; then
  fail=$((fail+1)); failed+=("flat elf"); echo "FAIL flat elf: -O0 and -O2 images differ"
elif ! gcc -O2 -Wall -Werror -o "$WORK/flat_elf_check" "$ROOT/tests/tools/flat_elf_check.c" 2>"$WORK/err.txt" ||
     ! "$WORK/flat_elf_check" "$WORK/k.elf" "$WORK/k.bin" 2>"$WORK/err.txt"
then
  fail=$((fail+1)); failed+=("flat elf"); echo "FAIL flat elf: ELF layout"; cat "$WORK/err.txt"
else
  pass=$((pass+1))
fi
if "$CC" --elf "$ROOT/tests/link/parts/flat_entry.lap" >/dev/null 2>"$WORK/err.txt"; then
  fail=$((fail+1)); failed+=("flat elf flags"); echo "FAIL flat elf: --elf without --flat succeeded"
elif ! grep -q "requires --flat" "$WORK/err.txt"; then
  fail=$((fail+1)); failed+=("flat elf flags"); echo "FAIL flat elf flags:"; cat "$WORK/err.txt"
else
  pass=$((pass+1))
fi
if "$CC" --boot --elf -o "$WORK/bootelf.bin" "$ROOT/tests/boot/section.lap" >/dev/null 2>"$WORK/err.txt"; then
  fail=$((fail+1)); failed+=("flat elf boot"); echo "FAIL flat elf: --elf with --boot succeeded"
elif ! grep -q "cannot be combined with --boot" "$WORK/err.txt"; then
  fail=$((fail+1)); failed+=("flat elf boot"); echo "FAIL flat elf boot:"; cat "$WORK/err.txt"
else
  pass=$((pass+1))
fi

# 4g. Assignment targets (0.44.4): what cannot be assigned is refused in every way the compiler builds,
# and a store through a call result is emitted (assembly, both objects, and a kernel run under QEMU).
# The details are in tests/assign/run.sh.
if asg=$(bash "$ROOT/tests/assign/run.sh" "$CC" 2>&1); then
  pass=$((pass+1)); echo "$asg" | grep -E '^skip|^tests/assign:'
else
  fail=$((fail+1)); failed+=("tests/assign"); echo "$asg" | head -40
fi

# 4h. Initializers of globals (0.44.5): a constant integer expression is folded into .data; anything else
# is refused on the let in every way the compiler builds, and a kernel reads the values back under QEMU.
# The details are in tests/ginit/run.sh.
if gin=$(bash "$ROOT/tests/ginit/run.sh" "$CC" 2>&1); then
  pass=$((pass+1)); echo "$gin" | grep -E '^skip|^tests/ginit:'
else
  fail=$((fail+1)); failed+=("tests/ginit"); echo "$gin" | head -40
fi

# 4i. Microsoft x64 convention (0.44.6): `ms64 fn` and a variable of type `ms64 fn`. The refusals, the call
# sequence in the assembly (System V programs unchanged), and a kernel that calls and defines ms64
# functions with 0, 1, 4, 5, 6, 8 and 9 arguments under QEMU. The details are in tests/ms64/run.sh.
if ms=$(bash "$ROOT/tests/ms64/run.sh" "$CC" 2>&1); then
  pass=$((pass+1)); echo "$ms" | grep -E '^skip|^tests/ms64:'
else
  fail=$((fail+1)); failed+=("tests/ms64"); echo "$ms" | head -40
fi

# 4j. --pic for --flat (0.44.7): the option rules, the refusals (tests/errors/pic_*.lap), the assembly with
# no absolute address of a label, the same bytes for --flat 0x10000 and --flat 0x40000, and a payload that
# QEMU runs at both addresses (the same program without --pic must fail at the second). The details are in
# tests/pic/run.sh.
if pic=$(bash "$ROOT/tests/pic/run.sh" "$CC" 2>&1); then
  pass=$((pass+1)); echo "$pic" | grep -E '^skip|^tests/pic:'
else
  fail=$((fail+1)); failed+=("tests/pic"); echo "$pic" | head -40
fi

# 4k. --pe-efi (0.44.8): the option rules, the refusals (tests/errors/pe_*.lap), the PE32+ file of
# tests/efi/app.lap read by a C checker (headers, sections, entry, .reloc) and broken on purpose field by
# field, and QEMU with OVMF running it as BOOTX64.EFI (CMP_REQUIRE=1 makes a missing qemu or OVMF an error).
# The details are in tests/efi/run.sh.
if efi=$(bash "$ROOT/tests/efi/run.sh" "$CC" 2>&1); then
  pass=$((pass+1)); echo "$efi" | grep -E '^skip|^tests/efi:'
else
  fail=$((fail+1)); failed+=("tests/efi"); echo "$efi" | head -40
fi

rm -f "$CC"
echo
echo "passed: $pass  failed: $fail  skipped: ${#skipped[@]}"
[ $CAN_RUN = 1 ] || echo "compiled but not run (this is $(uname -s)): $compiled programs at -O0 and -O2; their output was not checked"
[ $fail = 0 ] || { printf '  %s\n' "${failed[@]}"; exit 1; }
