#!/usr/bin/env bash
# The ELF files `copihue --native` writes, read by GNU binutils (docs/DESIGN_NATIVE.md, M5).
#   tests/x64/run_readelf.sh
# Needs readelf and objdump (binutils); without them it says so and exits 0
# (CMP_REQUIRE=1 turns that into an error, as the CI wants). tests/x64/run_elf.sh checks the same
# structure with a reader of its own and works anywhere; this is the independent opinion.
# For a program without data and for two with data, at -O0 and -O2, it checks that:
#   - readelf -h / -S / -s -W print no warning or error;
#   - the section table lists .text, .symtab, .strtab, .shstrtab (and .data when there is data);
#   - _start is GLOBAL FUNC in .text, the other functions LOCAL FUNC, the data labels LOCAL OBJECT;
#   - objdump -d shows <_start> and <main> with the instructions between them.
set -u
cd "$(dirname "$0")/../.."
for t in readelf objdump; do
  command -v "$t" >/dev/null 2>&1 || {
    echo "readelf: SKIPPED, $t not found"
    [ "${CMP_REQUIRE:-0}" = 1 ] && { echo "readelf: CMP_REQUIRE=1, so this is an error" >&2; exit 1; }
    exit 0
  }
done
make copihue >/dev/null || exit 1
W=build/native/readelf
mkdir -p "$W"
rc=0
bad() { echo "FAIL $1"; rc=1; }
for src in tests/hello.lap tests/test_global_string_use.lap tests/test_malloc_struct.lap; do
  for o in 0 2; do
    n="$src -O$o"
    ./copihue -O$o --native -o "$W/p" "$src" >/dev/null 2>"$W/err" || { bad "$n: --native: $(head -1 "$W/err")"; continue; }
    readelf -hSsW "$W/p" >"$W/out" 2>"$W/rerr"
    [ -s "$W/rerr" ] && { bad "$n: readelf complains: $(head -2 "$W/rerr" | tr '\n' ' ')"; continue; }
    grep -qi 'warning' "$W/out" && { bad "$n: readelf warns: $(grep -i warning "$W/out" | head -1)"; continue; }
    for s in .text .symtab .strtab .shstrtab; do
      grep -q " $s " "$W/out" || bad "$n: no section $s"
    done
    grep -Eq 'FUNC +GLOBAL +DEFAULT +[0-9]+ _start$' "$W/out" || bad "$n: _start is not GLOBAL FUNC"
    grep -Eq 'FUNC +LOCAL +DEFAULT +[0-9]+ main$' "$W/out" || bad "$n: main is not LOCAL FUNC"
    if grep -q '^ *\[ *[0-9]*\] \.data ' "$W/out"; then
      grep -Eq 'OBJECT +LOCAL +DEFAULT +[0-9]+ ' "$W/out" || bad "$n: has .data but no LOCAL OBJECT symbol"
    fi
    objdump -d "$W/p" >"$W/dis" 2>"$W/derr"
    [ -s "$W/derr" ] && { bad "$n: objdump complains: $(head -1 "$W/derr")"; continue; }
    grep -Eq '^[0-9a-f]+ <_start>:$' "$W/dis" && grep -Eq '^[0-9a-f]+ <main>:$' "$W/dis" || bad "$n: objdump -d does not show <_start> and <main>"
    grep -q 'syscall' "$W/dis" || bad "$n: objdump -d shows no syscall"
    echo "ok   $n"
  done
done
[ $rc = 0 ] && echo "readelf: OK" || echo "readelf: FAIL"
exit $rc
