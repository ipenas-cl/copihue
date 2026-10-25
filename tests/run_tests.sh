#!/usr/bin/env bash
# Copihue regression suite.
#
#   tests/run_tests.sh            run everything
#   tests/run_tests.sh --update   regenerate golden files (review the diff!)
#
# 1. Program tests: every tests/**/*.lap and examples/*.lap is compiled at -O0
#    and -O2, run with a timeout, and its stdout + exit code compared with
#    tests/expected/<path>.out. Both optimization levels must match the same
#    golden file. Heap/stack addresses (9+ digit numbers) are masked.
# 2. Error tests: every tests/errors/*.lap must FAIL to compile and its
#    diagnostics must match tests/expected/errors/<name>.err.
# 3. Freestanding tests: tests/freestanding/*.lap compile with --freestanding,
#    the output must assemble with nasm, and contain no _start. A `syscall`
#    instruction is allowed only when the source calls syscall().
# 4. Flat binaries: tests/flat/*.lap at --flat 0x10000.
# 5. Boot sectors: tests/boot/*.lap, and QEMU must print COPIHUE.
# 6. User mode: tests/usermode, and QEMU must print USER.
# 7. Context switch: tests/ctx, and QEMU must print COPIHUE.
# 8. Relocatable objects: tests/link/parts, two -c files linked with ld.
set -u
cd "$(dirname "$0")/.."
ROOT=$PWD
CC=$ROOT/compiler/bootstrap-c/copihue
UPDATE=0; [ "${1:-}" = "--update" ] && UPDATE=1
gcc -O2 -Wall -o "$CC" compiler/bootstrap-c/copihue.c || exit 1

# Programs that are not batch tests (servers / need a peer) or need input.
# They are covered by tests/net_test.sh instead.
SKIP=" examples/simple_api_server.lap examples/realtime_api_monitor.lap tests/test_http_simple.lap tests/http/test_http_client.lap tests/http/test_http_post.lap tests/http/test_suite_http.lap "

WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0; failed=()

mask() { sed -E 's/[0-9]{9,}/ADDR/g'; }

run_program() {  # $1 = source, $2 = opt level -> prints masked stdout + rc
  local src=$1 opt=$2
  ( cd "$WORK" && rm -f prog data.txt && "$CC" "$opt" -o prog "$ROOT/$src" >/dev/null 2>compile.err ) \
    || { echo "COMPILE ERROR"; sed "s|$ROOT/||g" "$WORK/compile.err"; return; }
  ( cd "$WORK" && timeout 5 ./prog </dev/null 2>&1; echo "exit=$?" ) | mask
}

check() {  # $1 = name, $2 = golden file, $3 = actual file
  if [ $UPDATE = 1 ]; then mkdir -p "$(dirname "$2")"; cp "$3" "$2"; fi
  if [ -f "$2" ] && cmp -s "$2" "$3"; then pass=$((pass+1));
  else fail=$((fail+1)); failed+=("$1"); echo "FAIL $1"; [ -f "$2" ] && diff "$2" "$3" | head -10; fi
}

for src in $(ls tests/*.lap tests/*/*.lap examples/*.lap | grep -v -e '^tests/errors/' -e '^tests/freestanding/' -e "^tests/flat/" -e "^tests/boot/" -e '^tests/usermode/' -e '^tests/ctx/'); do
  case "$SKIP" in *" $src "*) continue ;; esac
  golden=tests/expected/${src%.lap}.out
  run_program "$src" -O0 > "$WORK/o0"
  run_program "$src" -O2 > "$WORK/o2"
  if ! cmp -s "$WORK/o0" "$WORK/o2"; then
    fail=$((fail+1)); failed+=("$src (-O0 vs -O2)"); echo "FAIL $src: -O0 and -O2 differ"; diff "$WORK/o0" "$WORK/o2" | head -10; continue
  fi
  grep -q "^COMPILE ERROR" "$WORK/o0" && { fail=$((fail+1)); failed+=("$src"); echo "FAIL $src:"; cat "$WORK/o0"; continue; }
  grep -qE "^exit=(12[4-9]|13[0-9]|14[0-9])$" "$WORK/o0" && { fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: crashed or timed out ($(tail -1 "$WORK/o0"))"; continue; }
  check "$src" "$golden" "$WORK/o0"
done

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
  if ! ( cd "$WORK" && "$CC" --flat 0x10000 -o flat.bin "$ROOT/$src" 2>err.txt && "$CC" --flat 0x10000 -S -o flat.asm "$ROOT/$src" 2>>err.txt ); then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src:"; cat "$WORK/err.txt"; continue
  fi
  if [ "$(grep -m1 -E "^[A-Za-z_][A-Za-z0-9_]*:$" <(sed -n "/^section .text/,\$p" "$WORK/flat.asm"))" != "_start:" ]; then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: _start is not the first function"; continue
  fi
  od -An -tx1 -v "$WORK/flat.bin" > "$WORK/flat.hex"
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

# 5. Boot sectors: tests/boot/*.lap build with --boot into a 512-byte image
# 6. tests/usermode: boot sector plus a flat kernel. QEMU must print USER.
#    (org 0x7C00, signature 0xAA55). -O0 and -O2 must match. When QEMU is
#    installed, the image is booted from the floppy and COM1 must show COPIHUE.
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
  od -An -tx1 -v "$WORK/boot.bin" > "$WORK/boot.hex"
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
    od -An -tx1 -v "$WORK/uboot.bin" > "$WORK/uboot.hex"
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
        od -An -tx1 -v "$WORK/ukernel.bin" > "$WORK/ukernel.hex"
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
    od -An -tx1 -v "$WORK/cboot.bin" > "$WORK/cboot.hex"
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
        od -An -tx1 -v "$WORK/ckernel.bin" > "$WORK/ckernel.hex"
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
if ! ( cd "$WORK" && "$CC" --boot -o link_boot.bin "$ROOT/tests/usermode/boot.lap" >/dev/null 2>err.txt ); then
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

rm -f "$CC"
echo
echo "passed: $pass  failed: $fail"
[ $fail = 0 ] || { printf '  %s\n' "${failed[@]}"; exit 1; }
