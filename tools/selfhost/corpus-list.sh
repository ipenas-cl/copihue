#!/bin/sh
# Lista el corpus de igualdad del autoalojamiento (docs/SELFHOST_S0.md §2): los
# programas .lap que tests/run_tests.sh (sección 1) compila y ejecuta por la ruta
# nativa, sin los que --native rechaza (asm(), naked, interrupt fn, syscall fn).
# Solo imprime la lista, una ruta por línea; no compara nada.
#
#   tools/selfhost/corpus-list.sh [compilador]
#
# El compilador (por defecto compiler/bootstrap-c/copihue; hay que construirlo
# antes) decide qué se queda fuera: --native estricto, en -O0 y en -O2.
# La lista vigente está en tools/selfhost/corpus.txt.
# Temporales: SELFHOST_TMP (por defecto build/s18/corpus).
set -u
cd "$(dirname "$0")/../.."
CC=${1:-compiler/bootstrap-c/copihue}
[ -x "$CC" ] || { echo "falta el compilador: $CC (make, o gcc -O2 -o $CC compiler/bootstrap-c/copihue.c)" >&2; exit 1; }
CC=$PWD/$CC
# Mismos globs, mismas exclusiones y mismo SKIP que la sección 1 de tests/run_tests.sh.
SKIP=" examples/simple_api_server.lap examples/realtime_api_monitor.lap tests/test_http_simple.lap tests/http/test_http_client.lap tests/http/test_http_post.lap tests/http/test_suite_http.lap "
W=${SELFHOST_TMP:-build/s18/corpus}
mkdir -p "$W"
for src in $(ls tests/*.lap tests/*/*.lap examples/*.lap examples/*/*.lap examples/*/*/*.lap | grep -v -e '^tests/errors/' -e '^tests/freestanding/' -e '^tests/flat/' -e '^tests/boot/' -e '^tests/usermode/' -e '^tests/ctx/' -e '^tests/native/' -e '^tests/align/' -e '^tests/assign/' -e '^tests/axe1/' -e '^tests/ginit/' -e '^tests/ms64/' -e '^tests/pic/' -e '^tests/efi/' -e '^tests/axe2/' -e '^tests/tools/'); do
  case "$SKIP" in *" $src "*) continue ;; esac
  if "$CC" --native -O0 -o "$W/prog" "$src" >/dev/null 2>&1 && "$CC" --native -O2 -o "$W/prog" "$src" >/dev/null 2>&1; then
    echo "$src"
  fi
done
