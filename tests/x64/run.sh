#!/usr/bin/env bash
# Compares compiler/native/x64.c with nasm, byte for byte.
#   tests/x64/run.sh
# Needs nasm, a C compiler and ./copihue (it is built if missing). Runs:
#   1. the hand-written vectors in tests/x64/vectors.asm and the edge cases in tests/x64/edge.asm,
#      and the saved corpus snapshot in tests/x64/cases.asm
#   2. invalid input in tests/x64/reject.asm (x64.c and nasm must both refuse it)
#   3. rel8 / rel32 branch forms
#   4. every instruction the compiler emits for tests/ and examples/ (-O0 and -O2)
#   5. M2, tests/x64/run_asm.sh: whole programs through compiler/native/asm.c (labels, jumps, call)
set -u
cd "$(dirname "$0")/../.."
command -v nasm >/dev/null || { echo "x64: nasm not found" >&2; exit 1; }
W=build/native/x64test
mkdir -p "$W"
"${CC:-cc}" -O2 -Wall -Wextra -o "$W/test_x64" tests/x64/test_x64.c compiler/native/x64.c compiler/native/x64_line.c || exit 1
[ -x ./copihue ] || make copihue >/dev/null || exit 1
tools/native/corpus.sh "$W/corpus" ./copihue >/dev/null || exit 1
rc=0
echo "== vectors";  "$W/test_x64" "$W" vectors tests/x64/vectors.asm || rc=1
echo "== edge";     "$W/test_x64" "$W" vectors tests/x64/edge.asm || rc=1
echo "== cases";    "$W/test_x64" "$W" vectors tests/x64/cases.asm || rc=1
echo "== reject";   "$W/test_x64" "$W" reject tests/x64/reject.asm || rc=1
echo "== branches"; "$W/test_x64" "$W" branches || rc=1
echo "== corpus";   "$W/test_x64" "$W" corpus "$W"/corpus/O0/*.asm "$W"/corpus/O2/*.asm || rc=1
echo "== M2: labels, jumps, call (asm.c, whole programs)"
ASM_CORPUS="$W/corpus" tests/x64/run_asm.sh || rc=1
[ $rc = 0 ] && echo "x64: OK" || echo "x64: FAIL"
exit $rc
