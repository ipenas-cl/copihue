#!/usr/bin/env bash
# Genera el corpus de ensamblador del modo hosted (Linux ELF) para el inventario
# de DESIGN_NATIVE.md: cada programa de tests/ y examples/ que run_tests.sh
# ejecuta, compilado con -S a -O0 y a -O2.
#
#   tools/native/corpus.sh DIR [compilador]
#
# Deja DIR/O0/*.asm y DIR/O2/*.asm, y escribe en stdout cuántos compiló.
#
# Variables: CORPUS_FLAGS="-c" añade opciones al compilador (con -c, el texto de
# un objeto relocalizable: externs, un global por definición); CORPUS_PARTS=1
# suma los fuentes de tests/link/parts, que solo tienen sentido con -c.
set -u
cd "$(dirname "$0")/../.."
OUT=${1:?uso: corpus.sh DIR [compilador]}
CC=${2:-compiler/bootstrap-c/copihue}
[ -x "$CC" ] || gcc -O2 -o "$CC" compiler/bootstrap-c/copihue.c || exit 1
CC=$PWD/$CC
mkdir -p "$OUT/O0" "$OUT/O2"
ok=0; bad=0
EXTRA=
[ "${CORPUS_PARTS:-0}" = 1 ] && EXTRA=$(ls tests/link/parts/*.lap | grep -v -e '/fs_' -e '/flat_')    # bits 32 and named sections: nasm's, not the built-in assembler's
for src in $(ls tests/*.lap tests/*/*.lap examples/*.lap examples/*/*.lap examples/*/*/*.lap 2>/dev/null \
    | grep -v -e '^tests/errors/' -e '^tests/freestanding/' -e '^tests/flat/' \
              -e '^tests/boot/' -e '^tests/usermode/' -e '^tests/ctx/' -e '^tests/native/' -e '^tests/align/' -e '^tests/axe1/' -e '^tests/ginit/' -e '^tests/ms64/' -e '^tests/sret/' -e '^tests/pic/' -e '^tests/efi/' -e '^tests/assign/kernel' -e '^tests/assign/store_call_flat' -e '^tests/axe2/') $EXTRA; do
  name=$(echo "${src%.lap}" | tr '/' '_')
  for o in 0 2; do
    if "$CC" -O$o ${CORPUS_FLAGS:-} -S -o "$OUT/O$o/$name.asm" "$src" >/dev/null 2>&1; then
      ok=$((ok+1))
    else
      bad=$((bad+1)); rm -f "$OUT/O$o/$name.asm"
    fi
  done
done
echo "compilados=$ok fallidos=$bad"
