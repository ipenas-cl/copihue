#!/usr/bin/env bash
# --pic for --flat (0.44.7). Called by tests/run_tests.sh with the compiler path; also runnable alone:
#   tests/pic/run.sh [path/to/copihue]
#
# 1. Option rules: --pic without --flat, with --boot, --elf, --axe1 and --axe2 is refused, and so is
#    -c --pic.
# 2. tests/errors/pic_*.lap are refused under --flat --pic with the first message of their golden
#    file, on its line and column, leaving no output, and are not refused without --pic (the ones
#    that name a construct that is fine at a fixed address).
# 3. The assembly of tests/pic/kernel.lap (-O0, -O2): no label is used as an absolute address (an
#    awk check, independent of the compiler's own audit; the same check does find them in the
#    assembly built without --pic), `default rel` is there, and the assembly assembles with nasm.
# 4. The image: tests/pic/kernel.lap built with --flat 0x10000 --pic and with --flat 0x40000 --pic is
#    the same bytes (-O0 and -O2); built without --pic they differ.
# 5. QEMU: the payload is copied to 0x10000 (its --flat base) and to 0x40000 by a harness
#    (tests/pic/harness.in) and must print PIC both times. The mutation, the same program built
#    without --pic and run at 0x40000, must not print PIC.
#    Without qemu-system-x86_64 and perl, 5 is skipped; CMP_REQUIRE=1 (the CI) makes that a failure.
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT" || exit 1
CC=${1:-$ROOT/compiler/bootstrap-c/copihue}
[ -x "$CC" ] || { echo "tests/pic/run.sh: no compiler at $CC" >&2; exit 2; }
CC=$(cd "$(dirname "$CC")" && pwd)/$(basename "$CC")
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
fails=0; checks=0
bad() { fails=$((fails+1)); echo "FAIL tests/pic: $*"; }
ok() { checks=$((checks+1)); }
KERNEL=$ROOT/tests/pic/kernel.lap

# 1. option rules
refuse_opt() {  # $1 = expected message, rest = arguments
  want=$1; shift
  rm -f "$WORK/o.bin" "$WORK/o.asm" "$WORK/o.elf" "$WORK/o.o"
  ( cd "$WORK" && "$CC" "$@" >/dev/null 2>err.txt ) && { bad "[$*] compiled but should have been refused"; return; }
  got=$(grep -m1 'error:' "$WORK/err.txt" | sed 's/^[^ ]* error: //')
  [ "$got" = "$want" ] || bad "[$*]: message '$got', expected '$want'"
  ok
}
refuse_opt "--pic needs --flat <addr>: a position-independent image is a flat binary" --pic -S -o o.asm "$KERNEL"
refuse_opt "--pic needs --flat <addr>: a position-independent image is a flat binary" --pic --freestanding -S -o o.asm "$KERNEL"
refuse_opt "--pic needs --flat <addr>: a position-independent image is a flat binary" --pic -c -o o.o "$KERNEL"
refuse_opt "--pic cannot be combined with --boot: the BIOS loads a boot sector at 0x7C00" --pic --boot -o o.bin "$ROOT/tests/usermode/boot.lap"
refuse_opt "--pic cannot be combined with --elf: that ELF file has a fixed load address" --pic --flat 0x10000 --elf -o o.elf "$KERNEL"
refuse_opt "--pic cannot be combined with --axe1 or --axe2: those images load at 0x400000" --pic --flat 0x400000 --axe2 -o o.bin "$KERNEL"
refuse_opt "--pic cannot be combined with --axe1 or --axe2: those images load at 0x400000" --pic --flat 0x400000 --axe1 -o o.bin "$KERNEL"

# 2. the programs that cannot be position independent
for src in tests/errors/pic_*.lap; do
  [ -e "$src" ] || continue
  name=$(basename "$src" .lap)
  want=$(grep -m1 'error:' "tests/expected/errors/$name.err" | sed 's/^[^ ]*: error: //')
  wl=$(grep -m1 'error:' "tests/expected/errors/$name.err" | sed -E 's/^[^:]*:([0-9]+):([0-9]+):.*/\1/')
  wc=$(grep -m1 'error:' "tests/expected/errors/$name.err" | sed -E 's/^[^:]*:([0-9]+):([0-9]+):.*/\2/')
  for opt in -O0 -O2; do
    rm -f "$WORK/p.bin" "$WORK/p.asm" "$WORK/output.asm"
    ( cd "$WORK" && "$CC" $opt --flat 0x10000 --pic -o p.bin "$ROOT/$src" >/dev/null 2>err.txt ) && { bad "$name $opt compiled but should have failed"; continue; }
    got=$(grep -m1 'error:' "$WORK/err.txt" | sed 's/^[^ ]*: error: //')
    [ "$got" = "$want" ] || bad "$name $opt: message '$got', expected '$want'"
    gl=$(grep -m1 'error:' "$WORK/err.txt" | sed -E 's/^[^:]*:([0-9]+):([0-9]+):.*/\1/')
    gc=$(grep -m1 'error:' "$WORK/err.txt" | sed -E 's/^[^:]*:([0-9]+):([0-9]+):.*/\2/')
    [ "$wl:$wc" = "$gl:$gc" ] || bad "$name $opt: position $gl:$gc, expected $wl:$wc"
    for f in p.bin p.asm output.asm; do [ -e "$WORK/$f" ] && bad "$name $opt: left $f behind"; done
    ok
  done
  # the same source without --pic is not refused for that reason
  ( cd "$WORK" && "$CC" -O0 --flat 0x10000 -o np.bin "$ROOT/$src" >/dev/null 2>err.txt ) || bad "$name builds with --flat and no --pic, but: $(head -1 "$WORK/err.txt")"
  ok
done

# 3. the assembly
abs_labels() {  # $1 = assembly; prints each use of a label as an absolute address
  awk '
    /^[A-Za-z_.][A-Za-z0-9_.$@#~?]*:/ { l = $1; sub(/:.*/, "", l); lab[l] = 1 }
    { line[NR] = $0 }
    END {
      for (n = 1; n <= NR; n++) {
        s = line[n]; sub(/;.*/, "", s)
        if (s ~ /^[A-Za-z_.][A-Za-z0-9_.$@#~?]*:/) sub(/^[^:]*:/, "", s)
        gsub(/^[ \t]+|[ \t]+$/, "", s)
        if (s == "") continue
        m = s; sub(/[ \t].*/, "", m)
        if (m ~ /^(section|bits|org|default|align|times|resb|resw|resd|resq|global|extern)$/) continue
        if (s ~ /^[A-Za-z_.][A-Za-z0-9_.$@#~?]*[ \t]+equ[ \t]/) continue
        rest = s; sub(/^[^ \t]+[ \t]*/, "", rest)
        br = (m == "call" || m == "jmp" || m ~ /^j[a-z]+$/ || m ~ /^loop/)
        # memory operands
        t = rest
        while (match(t, /\[[^]]*\]/)) {
          inner = substr(t, RSTART + 1, RLENGTH - 2); t = substr(t, RSTART + RLENGTH)
          nt = split(inner, tok, /[^A-Za-z0-9_.$@#~?]+/); haslab = 0; hasreg = 0; hasabs = 0
          for (k = 1; k <= nt; k++) {
            if (tok[k] in lab) haslab = 1
            if (tok[k] ~ /^(r[a-z0-9]+|e[a-z]+|[a-d]x|[sd]i|[sb]p)$/ && tok[k] != "rel" && tok[k] != "rip") hasreg = 1
            if (tok[k] == "abs") hasabs = 1
          }
          if (haslab && (hasreg || hasabs)) print n ": " s
        }
        if (br) continue
        gsub(/\[[^]]*\]/, "", rest)
        # an operand is absolute if its labels do not cancel: end - start does, end does not
        no = split(rest, ops, /,/)
        for (k = 1; k <= no; k++) {
          o = ops[k]; net = 0; any = 0
          while (match(o, /[-+]? *[A-Za-z_.][A-Za-z0-9_.$@#~?]*/)) {
            piece = substr(o, RSTART, RLENGTH); o = substr(o, RSTART + RLENGTH)
            neg = (piece ~ /^ *-/); sub(/^ *[-+]? */, "", piece)
            if (piece in lab) { any = 1; net += neg ? -1 : 1 }
          }
          if (any && net != 0) { print n ": " s; break }
        }
      }
    }' "$1"
}
HAVE_NASM=0; command -v nasm >/dev/null 2>&1 && HAVE_NASM=1
for opt in -O0 -O2; do
  "$CC" -S $opt --flat 0x10000 --pic -o "$WORK/pic$opt.asm" "$KERNEL" 2>"$WORK/err.txt" || { bad "kernel $opt --pic: $(head -2 "$WORK/err.txt")"; continue; }
  "$CC" -S $opt --flat 0x10000 -o "$WORK/abs$opt.asm" "$KERNEL" 2>"$WORK/err.txt" || { bad "kernel $opt: $(head -2 "$WORK/err.txt")"; continue; }
  grep -qx 'default rel' "$WORK/pic$opt.asm" || bad "kernel $opt --pic: no 'default rel'"
  grep -qF 'lea rax, [rel ' "$WORK/pic$opt.asm" || bad "kernel $opt --pic: no lea rax, [rel ...]"
  ok
  n=$(abs_labels "$WORK/pic$opt.asm" | wc -l | tr -d ' ')
  [ "$n" = 0 ] || bad "kernel $opt --pic: $n uses of a label as an absolute address: $(abs_labels "$WORK/pic$opt.asm" | head -3 | tr '\n' '|')"
  ok
  n=$(abs_labels "$WORK/abs$opt.asm" | wc -l | tr -d ' ')
  [ "$n" -gt 0 ] || bad "kernel $opt without --pic: the check found no absolute label; the check does not work"
  ok
  # the assembly of a program without --pic is the same with no 'default rel'
  if grep -q 'default rel' "$WORK/abs$opt.asm"; then bad "kernel $opt without --pic has 'default rel'"; fi
  ok
  if [ $HAVE_NASM = 1 ]; then
    nasm -w-ea-absolute -f bin -o "$WORK/x.bin" "$WORK/pic$opt.asm" 2>"$WORK/err.txt" || bad "kernel $opt: nasm refused the assembly: $(head -2 "$WORK/err.txt")"
    ok
  fi
done

# 4. the image does not depend on where it was built to run
for opt in -O0 -O2; do
  ( cd "$WORK" && "$CC" $opt --flat 0x10000 --pic -o a$opt.bin "$KERNEL" 2>err.txt && "$CC" $opt --flat 0x40000 --pic -o b$opt.bin "$KERNEL" 2>>err.txt ) \
    || { bad "kernel $opt --pic: $(head -2 "$WORK/err.txt")"; continue; }
  cmp -s "$WORK/a$opt.bin" "$WORK/b$opt.bin" || bad "kernel $opt --pic: --flat 0x10000 and --flat 0x40000 give different bytes"
  ok
  ( cd "$WORK" && "$CC" $opt --flat 0x10000 -o c$opt.bin "$KERNEL" 2>err.txt && "$CC" $opt --flat 0x40000 -o d$opt.bin "$KERNEL" 2>>err.txt ) \
    || { bad "kernel $opt: $(head -2 "$WORK/err.txt")"; continue; }
  cmp -s "$WORK/c$opt.bin" "$WORK/d$opt.bin" && bad "kernel $opt without --pic: the two images are equal; the comparison proves nothing"
  ok
  [ "$(wc -c < "$WORK/a$opt.bin" | tr -d ' ')" -le 6144 ] || bad "kernel $opt: larger than the 6144 bytes the harness copies"
done

# 5. QEMU
boot_run() {  # $1 = disk image, $2 = seconds, $3 = serial file
  rm -f "$3"
  perl -e 'my ($t, @a) = @ARGV; my $p = fork(); if (!$p) { exec @a; exit 127 }
           my $f = $ENV{SERIAL}; my $n = 0;
           for (my $i = 0; $i < $t * 20; $i++) { select(undef, undef, undef, 0.05);
             my $s = -s $f; if ($s && $s >= 3) { $n++; last if $n > 6 } }
           kill "KILL", $p; waitpid($p, 0);' "$2" \
    qemu-system-x86_64 -display none -boot a -drive file="$1",format=raw,if=floppy,index=0 \
    -serial file:"$3" </dev/null >/dev/null 2>&1 || true
}
run_at() {  # $1 = payload.bin, $2 = load address, $3 = seconds; prints the serial output
  sed "s/@DEST@/$2/g" "$ROOT/tests/pic/harness.in" > "$WORK/harness.lap"
  ( cd "$WORK" && "$CC" --flat 0x8000 -o harness.bin harness.lap ) 2>"$WORK/err.txt" || { echo "HARNESS-BUILD-FAILED $(head -1 "$WORK/err.txt")"; return; }
  [ "$(wc -c < "$WORK/harness.bin" | tr -d ' ')" -le 1024 ] || { echo "HARNESS-TOO-BIG"; return; }
  dd if=/dev/zero of="$WORK/pad.bin" bs=8192 count=1 2>/dev/null
  dd if="$WORK/harness.bin" of="$WORK/pad.bin" conv=notrunc 2>/dev/null
  dd if="$1" of="$WORK/pad.bin" bs=1 seek=1024 conv=notrunc 2>/dev/null
  cat "$WORK/boot.bin" "$WORK/pad.bin" > "$WORK/disk.img"
  SERIAL="$WORK/serial.txt" boot_run "$WORK/disk.img" "$3" "$WORK/serial.txt"
  cat "$WORK/serial.txt" 2>/dev/null
}
if command -v qemu-system-x86_64 >/dev/null 2>&1 && command -v perl >/dev/null 2>&1; then
  if ! ( cd "$WORK" && "$CC" --boot -o boot.bin "$ROOT/tests/usermode/boot.lap" 2>err.txt ); then
    bad "boot sector: $(cat "$WORK/err.txt")"
  else
    for opt in -O0 -O2; do
      [ -s "$WORK/a$opt.bin" ] || continue
      for dest in 0x10000 0x40000; do
        got=$(run_at "$WORK/a$opt.bin" $dest 8)
        [ "$got" = PIC ] || bad "payload $opt at $dest: COM1 should be PIC, got '$got'"
        ok
      done
      # mutation: without --pic the image only works at its own base
      [ -s "$WORK/c$opt.bin" ] || continue
      got=$(run_at "$WORK/c$opt.bin" 0x10000 8)
      [ "$got" = PIC ] || bad "mutation control $opt: without --pic at its own base 0x10000 COM1 should be PIC, got '$got'"
      ok
      got=$(run_at "$WORK/c$opt.bin" 0x40000 5)
      [ "$got" != PIC ] || bad "mutation $opt: without --pic at 0x40000 still printed PIC; the test does not detect the problem"
      ok
    done
  fi
elif [ "${CMP_REQUIRE:-0}" = 1 ]; then
  bad "CMP_REQUIRE=1 and qemu-system-x86_64 or perl is missing"
else
  echo "skip tests/pic QEMU run: qemu-system-x86_64 or perl is not installed (CMP_REQUIRE=1 makes this an error)"
fi

echo "tests/pic: $checks checks, $fails failed"
[ "$fails" = 0 ]
