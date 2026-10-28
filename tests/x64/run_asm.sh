#!/usr/bin/env bash
# Compares compiler/native/asm.c (labels, jump relaxation, call) with nasm, whole programs.
#   tests/x64/run_asm.sh
# Needs nasm, a C compiler and ./copihue (it is built if missing). Runs:
#   1. small hand-written programs in tests/x64/asm_cases.asm (blocks split by `;---`)
#   2. programs asm.c must refuse, in tests/x64/asm_reject.asm
#   3. the 466 programs the compiler emits for tests/ and examples/ (-O0 and -O2):
#      .text bytes and relocations equal to those of nasm -f elf64, plus the
#      count of short / long jumps nasm leaves
set -u
cd "$(dirname "$0")/../.."
command -v nasm >/dev/null || { echo "asm: nasm not found" >&2; exit 1; }
W=build/native/asmtest
mkdir -p "$W"
"${CC:-cc}" -O2 -Wall -Wextra -o "$W/test_asm" tests/x64/test_asm.c compiler/native/asm.c compiler/native/x64.c compiler/native/x64_line.c || exit 1
[ -x ./copihue ] || make copihue >/dev/null || exit 1
if [ -z "${ASM_CORPUS:-}" ]; then
  tools/native/corpus.sh "$W/corpus" ./copihue >/dev/null || exit 1
  ASM_CORPUS=$W/corpus
fi
rc=0
echo "== cases";  "$W/test_asm" "$W" cases tests/x64/asm_cases.asm || rc=1
echo "== reject"; "$W/test_asm" "$W" reject tests/x64/asm_reject.asm || rc=1
echo "== corpus"; "$W/test_asm" "$W" files "$ASM_CORPUS"/O0/*.asm "$ASM_CORPUS"/O2/*.asm || rc=1
[ $rc = 0 ] && echo "asm: OK" || echo "asm: FAIL"
exit $rc
