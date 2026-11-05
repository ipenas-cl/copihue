#!/usr/bin/env bash
# --pe-efi (0.44.8). Called by tests/run_tests.sh with the compiler path; also runnable alone:
#   tests/efi/run.sh [path/to/copihue]
#
# 1. Option rules: --pe-efi with --flat, --boot, --elf, --axe1, --axe2, -c, --sizes and --native is
#    refused with its message and leaves no file; -S is accepted (the assembly has org 0x1000 and
#    default rel); the default output is a.efi.
# 2. tests/errors/pe_*.lap are refused under --pe-efi (-O0 and -O2), with the message and the
#    line:column of their golden file, leaving no a.efi and no output.asm. (The generic loop of
#    run_tests.sh also compares the whole text with -S.)
# 3. Structure, with no firmware: tests/efi/pecheck.c reads tests/efi/app.lap built at -O0, -O2 and
#    --nasm and checks the headers, the section table, the entry and .reloc. -O0 and --nasm give
#    the same bytes; building twice gives the same bytes. objdump -p is a second opinion when the
#    installed objdump reads PE files (it is not required).
# 4. Mutation: the same file with one header field broken on purpose (subsystem, Machine, magic,
#    the PE signature, the entry point outside the code, SizeOfImage, a writable and executable
#    section) must fail pecheck.
# 5. QEMU with OVMF: the file is BOOTX64.EFI of a FAT directory; the serial port must show EFI-OK
#    and QEMU must exit by itself (the application calls ResetSystem). The mutation, the same file
#    with the subsystem changed to 3, must not print EFI-OK.
#    OVMF is looked for in $OVMF_CODE (and $OVMF_VARS), the Homebrew qemu (edk2-x86_64-code.fd), and
#    Debian/Ubuntu (package ovmf: /usr/share/OVMF, /usr/share/ovmf). Without qemu-system-x86_64 or
#    OVMF, 5 is skipped; CMP_REQUIRE=1 (the CI) makes that a failure.
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT" || exit 1
CC=${1:-$ROOT/compiler/bootstrap-c/copihue}
[ -x "$CC" ] || { echo "tests/efi/run.sh: no compiler at $CC" >&2; exit 2; }
CC=$(cd "$(dirname "$CC")" && pwd)/$(basename "$CC")
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
fails=0; checks=0
bad() { fails=$((fails+1)); echo "FAIL tests/efi: $*"; }
ok() { checks=$((checks+1)); }
APP=$ROOT/tests/efi/app.lap

# 1. option rules
refuse_opt() {  # $1 = expected message, rest = arguments
  want=$1; shift
  rm -f "$WORK/a.efi" "$WORK/o.bin" "$WORK/o.asm" "$WORK/o.elf" "$WORK/o.o" "$WORK/output.asm"
  ( cd "$WORK" && "$CC" "$@" >/dev/null 2>err.txt ) && { bad "[$*] compiled but should have been refused"; return; }
  got=$(grep -m1 'error:' "$WORK/err.txt" | sed 's/^[^ ]* error: //')
  [ "$got" = "$want" ] || bad "[$*]: message '$got', expected '$want'"
  for f in a.efi o.bin o.asm o.elf o.o; do [ -e "$WORK/$f" ] && bad "[$*]: left $f behind"; done
  ok
}
refuse_opt "--pe-efi cannot be combined with --flat: --pe-efi sets the layout itself (the text is at RVA 0x1000 and the image is position independent)" --pe-efi --flat 0x1000 "$APP"
refuse_opt "--pe-efi cannot be combined with --flat: --pe-efi sets the layout itself (the text is at RVA 0x1000 and the image is position independent)" --flat 0x1000 --pe-efi -o o.bin "$APP"
refuse_opt "--pe-efi cannot be combined with --boot: a boot sector is not a PE file" --pe-efi --boot -o o.bin "$ROOT/tests/usermode/boot.lap"
refuse_opt "--pe-efi cannot be combined with --elf: the output is a PE file, not an ELF file" --pe-efi --elf -o o.elf "$APP"
refuse_opt "--pe-efi cannot be combined with --axe1: the output is a PE file, not an AXE1 file" --pe-efi --axe1 -o o.bin "$APP"
refuse_opt "--pe-efi cannot be combined with --axe2: the output is a PE file, not an AXE2 file" --axe2 --pe-efi -o o.bin "$APP"
refuse_opt "--pe-efi cannot be combined with -c: the output is a PE executable, not a relocatable object" --pe-efi -c -o o.o "$APP"
refuse_opt "--pe-efi cannot be combined with --sizes: that is for --flat images" --pe-efi --sizes -o o.bin "$APP"
refuse_opt "--pe-efi cannot be combined with --native: that is the hosted ELF back end" --pe-efi --native -o o.bin "$APP"

# -S writes the assembly of the PE image, which is the --flat 0x1000 --pic one
for opt in -O0 -O2; do
  rm -f "$WORK/o.asm"
  ( cd "$WORK" && "$CC" $opt -S --pe-efi -o o.asm "$APP" ) 2>"$WORK/err.txt" || { bad "-S --pe-efi $opt: $(head -2 "$WORK/err.txt")"; continue; }
  grep -qx 'org 0x1000' "$WORK/o.asm" && grep -qx 'default rel' "$WORK/o.asm" || bad "-S --pe-efi $opt: no 'org 0x1000' or 'default rel'"
  grep -q 'section .data align=4096' "$WORK/o.asm" && grep -q 'section .bss align=4096' "$WORK/o.asm" || bad "-S --pe-efi $opt: .data and .bss are not on their own pages"
  ( cd "$WORK" && "$CC" $opt -S --flat 0x1000 --pic -o f.asm "$APP" ) 2>/dev/null
  cmp -s "$WORK/o.asm" "$WORK/f.asm" && bad "-S --pe-efi $opt: the same as --flat 0x1000 --pic, which does not page the sections"
  ok
done

# 2. the programs that cannot be an EFI application
for src in tests/errors/pe_*.lap; do
  [ -e "$src" ] || continue
  name=$(basename "$src" .lap)
  want=$(grep -m1 'error:' "tests/expected/errors/$name.err" | sed 's/^[^ ]*: error: //')
  wl=$(grep -m1 'error:' "tests/expected/errors/$name.err" | sed -E 's/^[^:]*:([0-9]+):([0-9]+):.*/\1:\2/')
  for opt in -O0 -O2; do
    rm -f "$WORK/a.efi" "$WORK/p.efi" "$WORK/output.asm"
    ( cd "$WORK" && "$CC" $opt --pe-efi -o p.efi "$ROOT/$src" >/dev/null 2>err.txt ) && { bad "$name $opt compiled but should have failed"; continue; }
    got=$(grep -m1 'error:' "$WORK/err.txt" | sed 's/^[^ ]* error: //')
    [ "$got" = "$want" ] || bad "$name $opt: message '$got', expected '$want'"
    gl=$(grep -m1 'error:' "$WORK/err.txt" | sed -E 's/^[^:]*:([0-9]+):([0-9]+):.*/\1:\2/')
    [ "$wl" = "$gl" ] || bad "$name $opt: position $gl, expected $wl"
    for f in a.efi p.efi output.asm; do [ -e "$WORK/$f" ] && bad "$name $opt: left $f behind"; done
    ok
  done
done

# 3. structure
if ! gcc -O2 -Wall -Wextra -Werror -o "$WORK/pecheck" "$ROOT/tests/efi/pecheck.c" 2>"$WORK/err.txt"; then
  bad "pecheck.c does not build: $(head -3 "$WORK/err.txt")"
else
  for opt in -O0 -O2; do
    ( cd "$WORK" && "$CC" $opt --pe-efi -o "app$opt.efi" "$APP" ) 2>"$WORK/err.txt" || { bad "app $opt: $(head -2 "$WORK/err.txt")"; continue; }
    out=$("$WORK/pecheck" "$WORK/app$opt.efi" 2>&1) || bad "app $opt: $out"
    ok
    ( cd "$WORK" && "$CC" $opt --pe-efi -o "again$opt.efi" "$APP" ) 2>/dev/null
    cmp -s "$WORK/app$opt.efi" "$WORK/again$opt.efi" || bad "app $opt: two builds differ"
    ok
    ( cd "$WORK" && "$CC" $opt --nasm --pe-efi -o "nasm$opt.efi" "$APP" ) 2>"$WORK/err.txt" || bad "app $opt --nasm: $(head -2 "$WORK/err.txt")"
    cmp -s "$WORK/app$opt.efi" "$WORK/nasm$opt.efi" || bad "app $opt: --nasm gives other bytes"
    ok
  done
  # the default output name
  ( cd "$WORK" && rm -f a.efi && "$CC" --pe-efi "$APP" ) 2>/dev/null
  [ -s "$WORK/a.efi" ] && cmp -s "$WORK/a.efi" "$WORK/app-O0.efi" || bad "the default output is not a.efi (or differs from -o)"
  ok
  # the layout the compiler claims: entry in .text, .data and .bss on pages, 4 sections
  "$WORK/pecheck" "$WORK/app-O2.efi" | grep -q 'sections=4' || bad "app -O2: expected 4 sections (.text .data .bss .reloc)"
  ok
  if [ -e "$WORK/app-O0.efi" ]; then
    if objdump -p "$WORK/app-O0.efi" 2>/dev/null | grep -q '^Magic.*PE32+'; then
      op=$(objdump -p "$WORK/app-O0.efi" 2>/dev/null)
      printf '%s\n' "$op" | grep -q '^Subsystem.*(EFI application)' || bad "objdump does not read Subsystem 10 (EFI application)"
      printf '%s\n' "$op" | grep -q '^AddressOfEntryPoint *0*1000$' || bad "objdump does not read AddressOfEntryPoint 0x1000"
      objdump -h "$WORK/app-O0.efi" 2>/dev/null | awk '$2==".text"||$2==".data"||$2==".bss"||$2==".reloc"{n++} END{exit n==4?0:1}' || bad "objdump -h does not list .text .data .bss .reloc"
      ok
    else
      echo "skip objdump second opinion: objdump on this machine does not read PE files"
    fi
  fi

  # 4. mutation: one broken field and pecheck must complain
  if [ -s "$WORK/app-O0.efi" ]; then
    mutate() {  # $1 = description, $2 = offset, $3 = bytes as printf escapes
      cp "$WORK/app-O0.efi" "$WORK/mut.efi"
      printf "$3" | dd of="$WORK/mut.efi" bs=1 seek="$2" conv=notrunc 2>/dev/null
      if "$WORK/pecheck" "$WORK/mut.efi" >/dev/null 2>&1; then bad "mutation '$1': pecheck still accepts the file"; fi
      ok
    }
    OPT=$((0x40 + 4 + 20))                  # start of the optional header
    SEC=$((OPT + 240))                      # start of the section table
    mutate "subsystem 3"                 $((OPT + 68)) '\003'
    mutate "subsystem 11"                $((OPT + 68)) '\013'
    mutate "Machine i386"                $((0x44)) '\114\001'
    mutate "magic PE32"                  $((OPT)) '\013\001'
    mutate "PE signature"                $((0x40)) 'XX'
    mutate "MZ signature"                0 'ZM'
    mutate "entry outside the sections"  $((OPT + 16)) '\000\000\100\000'
    mutate "entry in the data page"      $((OPT + 16)) '\000\040\000\000'
    mutate "SizeOfImage too small"       $((OPT + 56)) '\000\020\000\000'
    mutate "SectionAlignment 0x800"      $((OPT + 32)) '\000\010\000\000'
    mutate "FileAlignment 0x300"         $((OPT + 36)) '\000\003\000\000'
    mutate ".text writable"              $((SEC + 36)) '\040\000\000\340'
    mutate "section RVA off the page"    $((SEC + 40 + 12)) '\100\040\000\000'
    mutate "RELOCS_STRIPPED"             $((0x56)) '\043\000'
    mutate "reloc directory empty"       $((OPT + 112 + 5 * 8 + 4)) '\000\000\000\000'
    mutate "raw data past the file"      $((SEC + 16)) '\000\000\100\000'
    mutate "PointerToRawData unaligned"  $((SEC + 20)) '\001\002\000\000'
    # control: the unmodified file is accepted
    "$WORK/pecheck" "$WORK/app-O0.efi" >/dev/null 2>&1 || bad "mutation control: the unmodified file is refused"
    ok
  fi
fi

# 5. QEMU with OVMF
find_ovmf() {  # sets OVMF_C and OVMF_V (OVMF_V empty: OVMF_C is one file for -bios)
  OVMF_C=""; OVMF_V=""
  if [ -n "${OVMF_CODE:-}" ] && [ -r "$OVMF_CODE" ]; then OVMF_C=$OVMF_CODE; OVMF_V=${OVMF_VARS:-}; return 0; fi
  local q
  q=$(brew --prefix qemu 2>/dev/null || true)
  for d in "$q/share/qemu" /opt/homebrew/opt/qemu/share/qemu /usr/local/opt/qemu/share/qemu /usr/share/qemu; do
    if [ -r "$d/edk2-x86_64-code.fd" ]; then
      OVMF_C=$d/edk2-x86_64-code.fd
      for v in "$d/edk2-i386-vars.fd" "$d/edk2-x86_64-vars.fd"; do [ -r "$v" ] && { OVMF_V=$v; break; }; done
      [ -n "$OVMF_V" ] && return 0
      OVMF_C=""
    fi
  done
  for d in /usr/share/OVMF /usr/share/ovmf /usr/share/edk2/ovmf /usr/share/edk2-ovmf/x64; do
    for pair in OVMF_CODE_4M.fd:OVMF_VARS_4M.fd OVMF_CODE.fd:OVMF_VARS.fd; do
      if [ -r "$d/${pair%%:*}" ] && [ -r "$d/${pair##*:}" ]; then OVMF_C=$d/${pair%%:*}; OVMF_V=$d/${pair##*:}; return 0; fi
    done
    if [ -r "$d/OVMF.fd" ]; then OVMF_C=$d/OVMF.fd; return 0; fi
  done
  return 1
}
boot_efi() {  # $1 = .efi, $2 = max seconds; sets SERIAL_TXT and EXITED (1 when QEMU exited by itself)
  rm -rf "$WORK/esp"; mkdir -p "$WORK/esp/EFI/BOOT"
  cp "$1" "$WORK/esp/EFI/BOOT/BOOTX64.EFI"
  rm -f "$WORK/serial.txt"; : > "$WORK/serial.txt"
  local drives
  if [ -n "$OVMF_V" ]; then
    cp "$OVMF_V" "$WORK/vars.fd"
    drives=(-drive "if=pflash,format=raw,unit=0,readonly=on,file=$OVMF_C" -drive "if=pflash,format=raw,unit=1,file=$WORK/vars.fd")
  else
    drives=(-bios "$OVMF_C")
  fi
  qemu-system-x86_64 -machine q35 -m 256 -display none -no-reboot "${drives[@]}" \
    -drive "format=raw,file=fat:rw:$WORK/esp" -serial "file:$WORK/serial.txt" -net none </dev/null >"$WORK/qemu.out" 2>&1 &
  local pid=$! i=0
  EXITED=0
  while [ $i -lt $(($2 * 2)) ]; do
    sleep 0.5; i=$((i+1))
    if ! kill -0 $pid 2>/dev/null; then EXITED=1; break; fi
  done
  kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
  SERIAL_TXT=$(tr -d '\000' < "$WORK/serial.txt" | tr -d '\r' | LC_ALL=C sed 's/\x1b\[[0-9;=?]*[A-Za-z]//g')
}
if command -v qemu-system-x86_64 >/dev/null 2>&1 && find_ovmf && [ -s "$WORK/app-O0.efi" ]; then
  for opt in -O0 -O2; do
    [ -s "$WORK/app$opt.efi" ] || continue
    boot_efi "$WORK/app$opt.efi" 120
    case "$SERIAL_TXT" in *EFI-OK*) ;; *) bad "QEMU $opt: COM1 should show EFI-OK, got '$(printf '%s' "$SERIAL_TXT" | tail -c 200)' (qemu: $(head -2 "$WORK/qemu.out"))";; esac
    case "$SERIAL_TXT" in *EFI-FAIL*) bad "QEMU $opt: an EFI-FAIL check: $(printf '%s' "$SERIAL_TXT" | grep -a EFI-FAIL)";; esac
    case "$SERIAL_TXT" in *EFI-CONOUT*) ;; *) bad "QEMU $opt: ConOut->OutputString text EFI-CONOUT is not on the console";; esac
    [ "$EXITED" = 1 ] || bad "QEMU $opt: the application did not end with ResetSystem (QEMU still running)"
    ok
  done
  # mutation: subsystem 3 (Windows CUI) is not an EFI application; the firmware must not run it
  cp "$WORK/app-O0.efi" "$WORK/mut3.efi"
  printf '\003' | dd of="$WORK/mut3.efi" bs=1 seek=$((0x40 + 4 + 20 + 68)) conv=notrunc 2>/dev/null
  boot_efi "$WORK/mut3.efi" 25
  case "$SERIAL_TXT" in *EFI-OK*|*EFI-CONOUT*) bad "QEMU mutation: the file with subsystem 3 still ran; the test does not detect it";; esac
  ok
elif [ "${CMP_REQUIRE:-0}" = 1 ]; then
  bad "CMP_REQUIRE=1 and qemu-system-x86_64 or OVMF (OVMF_CODE, qemu's edk2-x86_64-code.fd, the ovmf package) is missing"
else
  echo "skip tests/efi QEMU run: qemu-system-x86_64 or OVMF is not installed (CMP_REQUIRE=1 makes this an error)"
fi

echo "tests/efi: $checks checks, $fails failed"
[ "$fails" = 0 ]
