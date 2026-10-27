#!/usr/bin/env bash
# Compares compiler/native/x64.c with nasm, byte for byte.
#   tests/x64/run.sh
# Needs nasm, a C compiler and ./copihue (it is built if missing). Runs:
#   1. the hand-written vectors in tests/x64/vectors.asm
#   2. rel8 / rel32 branch forms
#   3. every instruction the compiler emits for tests/ and examples/ (-O0 and -O2)
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
echo "== branches"; "$W/test_x64" "$W" branches || rc=1
echo "== corpus";   "$W/test_x64" "$W" corpus "$W"/corpus/O0/*.asm "$W"/corpus/O2/*.asm || rc=1
[ $rc = 0 ] && echo "x64: OK" || echo "x64: FAIL"
exit $rc
