#!/usr/bin/env bash
# .text of `copihue --native` against the one nasm + ld produce (docs/DESIGN_NATIVE.md, M5).
#   tests/x64/run_text.sh                  every program of tests/ and examples/ that run_tests.sh builds, -O0 and -O2
#   tests/x64/run_text.sh FILE.lap ...     only those programs
#   CMP_LEVELS="2" tests/x64/run_text.sh   only that optimization level
#   CMP_REQUIRE=1 tests/x64/run_text.sh    a missing tool is an error (the CI sets it), not a skip
# Needs nasm, GNU ld for x86-64 and objcopy (binutils); without them it says so and exits 0.
# For each program and level:
#   nat  = copihue --native                  (no nasm, no ld)
#   ref  = nasm -f elf64 + ld                (what copihue --nasm does)
#   .text of both is extracted with `objcopy -O binary -j .text` and compared with cmp.
# Three outcomes:
#   same     the bytes are identical.
#   layout   different, but identical once ld places .data/.bss/.rodata where the native writer does
#            (ld --section-start): only the addresses in absolute references to data differ, which
#            depend on where each linker puts the data segment, not on the code. Counted and listed.
#   DIFF     different even then: a real difference in the code. Fails.
# With nm, the symbols of the native file must also exist in the ld one at the same address
# (labels without a dot, with the data placed as above). Programs --native refuses (user assembly,
# naked/interrupt/syscall functions) are skipped, as run_tests.sh --native does.
set -u
cd "$(dirname "$0")/../.."
ROOT=$PWD

skip() {
  echo "text: SKIPPED, $1"
  [ "${CMP_REQUIRE:-0}" = 1 ] && { echo "text: CMP_REQUIRE=1, so this is an error" >&2; exit 1; }
  exit 0
}
miss=
for t in nasm ld objcopy objdump; do command -v "$t" >/dev/null 2>&1 || miss="$miss $t"; done
[ -z "$miss" ] || skip "missing tools:$miss (the comparison needs nasm, GNU ld, objcopy and objdump)"
ld --help 2>&1 | grep -q elf_x86_64 || skip "this ld does not link x86-64 ELF (not GNU ld for x86-64?)"
HAVE_NM=0; command -v nm >/dev/null 2>&1 && HAVE_NM=1

make copihue >/dev/null || exit 1
CC=$ROOT/copihue
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT

# Same programs as tests/run_tests.sh builds (see there for why each is left out).
SKIP=" examples/simple_api_server.lap examples/realtime_api_monitor.lap tests/test_http_simple.lap tests/http/test_http_client.lap tests/http/test_http_post.lap tests/http/test_suite_http.lap "
if [ $# -gt 0 ]; then
  list=$*
else
  list=$(ls tests/*.lap tests/*/*.lap examples/*.lap examples/*/*.lap examples/*/*/*.lap | grep -v -e '^tests/errors/' -e '^tests/freestanding/' -e '^tests/flat/' -e '^tests/boot/' -e '^tests/usermode/' -e '^tests/ctx/' -e '^tests/native/' -e '^tests/align/' -e '^tests/axe1/' -e '^tests/ginit/' -e '^tests/ms64/' -e '^tests/pic/' -e '^tests/efi/' -e '^tests/assign/kernel' -e '^tests/assign/store_call_flat' -e '^tests/axe2/')
fi

# Address of a section in an executable (objdump -h: Idx Name Size VMA ...), or nothing.
vma() { objdump -h "$1" 2>/dev/null | awk -v s="$2" '$2 == s { print $4; exit }'; }

# name address of the labels without a dot (t T d D b B r R), minus what ld adds.
syms() { nm "$1" 2>/dev/null | awk 'NF == 3 && $2 ~ /^[tTdDbBrR]$/ && $3 !~ /\./ && $3 != "__bss_start" && $3 != "_edata" && $3 != "_end" { print $3, $1 }' | sort; }

n_same=0; n_layout=0; n_diff=0; n_skip=0; n_sym=0; n_sym_extra=0; n_fail=0
layout_list=(); diff_list=()
for src in $list; do
  case "$SKIP" in *" $src "*) continue ;; esac
  for o in ${CMP_LEVELS:-0 2}; do
    name="$src -O$o"
    rm -f "$WORK"/*
    if ! ( cd "$WORK" && "$CC" -O$o --native -o nat "$ROOT/$src" >/dev/null 2>nat.err ); then
      if grep -q -e '--native does not take this program' "$WORK/nat.err" && grep -qE 'asm\(|naked|interrupt fn|syscall fn' "$src"; then
        n_skip=$((n_skip+1)); continue
      fi
      echo "FAIL $name: --native: $(head -1 "$WORK/nat.err")"; n_fail=$((n_fail+1)); continue
    fi
    if ! ( cd "$WORK" && "$CC" -O$o -S -o p.asm "$ROOT/$src" >/dev/null 2>&1 && nasm -f elf64 p.asm -o p.o && ld p.o -o ref ) >"$WORK/ref.err" 2>&1; then
      echo "FAIL $name: nasm + ld: $(head -1 "$WORK/ref.err")"; n_fail=$((n_fail+1)); continue
    fi
    objcopy -O binary -j .text "$WORK/nat" "$WORK/nat.text" && objcopy -O binary -j .text "$WORK/ref" "$WORK/ref.text" \
      || { echo "FAIL $name: objcopy"; n_fail=$((n_fail+1)); continue; }
    cmp -s "$WORK/nat.text" "$WORK/ref.text" && { n_same=$((n_same+1)); ref=$WORK/ref; kind=same; } || kind=
    if [ -z "$kind" ]; then
      # Link again with the data where the native writer puts it.
      args="--section-start=.text=0x401000"
      for s in .rodata .data .bss; do
        a=$(vma "$WORK/nat" $s); [ -n "$a" ] && args="$args --section-start=$s=0x$a"
      done
      # shellcheck disable=SC2086
      if ! ( cd "$WORK" && ld $args p.o -o ref2 ) >"$WORK/ref.err" 2>&1; then
        echo "FAIL $name: ld $args: $(head -1 "$WORK/ref.err")"; n_fail=$((n_fail+1)); continue
      fi
      objcopy -O binary -j .text "$WORK/ref2" "$WORK/ref2.text"
      if cmp -s "$WORK/nat.text" "$WORK/ref2.text"; then
        n_layout=$((n_layout+1)); layout_list+=("$name"); ref=$WORK/ref2
      else
        n_diff=$((n_diff+1)); n_fail=$((n_fail+1)); diff_list+=("$name")
        echo "DIFF $name: .text differs from nasm + ld even with the data at the same addresses ($(wc -c <"$WORK/nat.text") vs $(wc -c <"$WORK/ref2.text") bytes); first bytes that differ (offset, native, ld; octal):"
        cmp -l "$WORK/nat.text" "$WORK/ref2.text" 2>&1 | head -5 | sed 's/^/    /'
        continue
      fi
    fi
    if [ $HAVE_NM = 1 ]; then
      syms "$WORK/nat" > "$WORK/nat.sym"; syms "$ref" > "$WORK/ref.sym"
      bad=$(comm -23 "$WORK/nat.sym" "$WORK/ref.sym")
      if [ -n "$bad" ]; then
        echo "FAIL $name: symbols of the native file missing from the ld one, or at another address:"; echo "$bad" | head -5 | sed 's/^/    /'
        n_fail=$((n_fail+1)); continue
      fi
      n_sym=$((n_sym+1))
      n_sym_extra=$((n_sym_extra + $(comm -13 "$WORK/nat.sym" "$WORK/ref.sym" | wc -l)))
    fi
  done
done

echo "text: $((n_same+n_layout+n_diff)) comparisons: $n_same identical, $n_layout identical once ld puts the data where --native does, $n_diff different; $n_skip skipped (--native refuses them)"
[ $HAVE_NM = 1 ] && echo "text: symbols: in $n_sym comparisons every symbol of the native file is in the ld one at the same address; $n_sym_extra ld symbols have no native counterpart" \
  || echo "text: symbols not compared (nm not found)"
if [ $n_layout -gt 0 ]; then
  echo "text: with ld's own layout the .text of these differs only in data addresses (first 10):"
  printf '    %s\n' "${layout_list[@]:0:10}"
fi
[ $n_fail = 0 ] && { echo "text: OK"; exit 0; }
echo "text: FAIL ($n_fail)"; exit 1
