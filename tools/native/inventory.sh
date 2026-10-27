#!/usr/bin/env bash
# Inventory of the assembly the hosted code generator emits.
#
#   tools/native/inventory.sh [--list-uncovered] [--top N] [DIR]
#
# Compiles tests/ and examples/ with -S at -O0 and -O2 into DIR (default
# build/native/corpus), then runs asminv on both levels and on the union.
# Needs ./copihue (make) and a C compiler.
set -u
cd "$(dirname "$0")/../.."
ARGS=(); DIR=build/native/corpus
while [ $# -gt 0 ]; do
  case $1 in
    --list-uncovered) ARGS+=("$1") ;;
    --top) ARGS+=("$1" "$2"); shift ;;
    *) DIR=$1 ;;
  esac
  shift
done
mkdir -p build/native
"${CC:-cc}" -O2 -Wall -o build/native/asminv tools/native/asminv.c compiler/native/x64.c compiler/native/x64_line.c || exit 1
tools/native/corpus.sh "$DIR" ./copihue >/dev/null || exit 1
for lvl in O0 O2; do
  echo "## $lvl"; echo
  build/native/asminv "${ARGS[@]+"${ARGS[@]}"}" "$DIR"/$lvl/*.asm | sed -n '1,4p'
  echo
done
echo "## O0 + O2"; echo
build/native/asminv "${ARGS[@]+"${ARGS[@]}"}" "$DIR"/O0/*.asm "$DIR"/O2/*.asm
