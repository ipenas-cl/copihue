#!/usr/bin/env bash
# Structure of the ELF executables written by compiler/native/elf.c (M4), without running them.
#   tests/x64/run_elf.sh
# Needs nasm, a C compiler and ./copihue (built if missing). Runs:
#   1. the small programs in tests/x64/elf_cases.asm (blocks split by `;---`)
#   2. programs the writer must refuse
#   3. the programs the compiler emits for tests/ and examples/ (-O0 and -O2), built in memory
#   4. a few real `copihue --native` executables, checked as files (mode 0755, same bytes
#      as the library builds, same structure)
# What is checked is in tests/x64/test_elf.c. Running the result needs Linux: tests/run_tests.sh --native.
set -u
cd "$(dirname "$0")/../.."
command -v nasm >/dev/null || { echo "elf: nasm not found" >&2; exit 1; }
W=build/native/elftest
mkdir -p "$W"
"${CC:-cc}" -O2 -Wall -Wextra -o "$W/test_elf" tests/x64/test_elf.c compiler/native/elf.c compiler/native/asm.c compiler/native/x64.c compiler/native/x64_line.c || exit 1
make copihue >/dev/null || exit 1
if [ -z "${ASM_CORPUS:-}" ]; then
  tools/native/corpus.sh "$W/corpus" ./copihue >/dev/null || exit 1
  ASM_CORPUS=$W/corpus
fi
rc=0
echo "== cases";  "$W/test_elf" "$W" cases tests/x64/elf_cases.asm || rc=1
echo "== reject"; "$W/test_elf" "$W" reject || rc=1
echo "== corpus"; "$W/test_elf" "$W" files "$ASM_CORPUS"/O0/*.asm "$ASM_CORPUS"/O2/*.asm || rc=1
echo "== copihue --native (files)"
for src in tests/hello.lap tests/test_global_string_use.lap tests/test_struct_math.lap tests/test_malloc_struct.lap examples/lowlat/spsc_ring.lap; do
  [ -f "$src" ] || continue
  for o in 0 2; do
    ./copihue -O$o -S -o "$W/p.asm" "$src" >/dev/null 2>&1 || { echo "FAIL $src: -S"; rc=1; continue; }
    ./copihue -O$o --native -o "$W/p.elf" "$src" >/dev/null 2>"$W/p.err" || { echo "FAIL $src -O$o --native:"; cat "$W/p.err"; rc=1; continue; }
    "$W/test_elf" "$W" exe "$W/p.elf" "$W/p.asm" | grep -v '^nasm:' | sed "s|^|$src -O$o: |" | tail -1
    "$W/test_elf" "$W" exe "$W/p.elf" "$W/p.asm" >/dev/null || rc=1
  done
done
[ $rc = 0 ] && echo "elf: OK" || echo "elf: FAIL"
exit $rc
