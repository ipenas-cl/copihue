#!/usr/bin/env bash
# Prueba del codificador x86-64 directo (compiler/native/x64enc*).
#
#   tests/native/run.sh             compara contra nasm los casos guardados
#   tests/native/run.sh --corpus    además compila tests/ y examples/ con -S,
#                                   y compara contra nasm cada instrucción
#                                   distinta que el codificador acepta
#   tests/native/run.sh --regen     regenera tests/native/cases.asm desde el corpus
#
# Para cada archivo de casos: nasm -f bin produce los bytes esperados y
# nx_test los suyos; cmp los compara byte a byte. Si difieren, se imprime la
# primera instrucción que no coincide con ambas codificaciones.
set -u
cd "$(dirname "$0")/../.."
ROOT=$PWD
MODE=${1:-}
command -v nasm >/dev/null || { echo "native: falta nasm"; exit 1; }

WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
SRC="compiler/native/x64enc.c compiler/native/x64enc_encode.c"
gcc -O2 -Wall -Wextra -o "$WORK/nx_test" tests/native/nx_test.c $SRC || exit 1
gcc -O2 -Wall -Wextra -o "$WORK/asminv" tools/native/nxinv.c $SRC || exit 1

pass=0; fail=0

# nasm ve las equ al final, como en el ensamblador real (referencia hacia adelante).
mk_nasm() {  # $1 = casos, $2 = salida .asm
  { echo "bits 64"; grep -v ' equ ' "$1"; grep ' equ ' "$1"; } > "$2"
}

check_file() {  # $1 = nombre, $2 = archivo de casos
  local name=$1 cases=$2
  mk_nasm "$cases" "$WORK/ref.asm"
  if ! nasm -f bin "$WORK/ref.asm" -o "$WORK/ref.bin" 2>"$WORK/nasm.err"; then
    echo "FAIL $name: nasm rechaza el archivo"; head -5 "$WORK/nasm.err"; fail=$((fail+1)); return
  fi
  if ! "$WORK/nx_test" "$cases" "$WORK/mine.bin" 2>"$WORK/nx.err"; then
    echo "FAIL $name: el codificador rechaza lineas"; head -5 "$WORK/nx.err"; fail=$((fail+1)); return
  fi
  if cmp -s "$WORK/ref.bin" "$WORK/mine.bin"; then
    echo "ok   $name: $(tail -1 "$WORK/nx.err")"
    pass=$((pass+1))
  else
    echo "FAIL $name: los bytes difieren"
    # Localiza la primera instruccion distinta: offset del primer byte distinto.
    local off; off=$(cmp -l "$WORK/ref.bin" "$WORK/mine.bin" 2>/dev/null | head -1 | { read -r o _; echo "$o"; })
    [ -z "$off" ] && off=1
    "$WORK/nx_test" "$cases" --at $((off - 1)) | sed 's/^/     primera diferencia en: /'
    fail=$((fail+1))
  fi
}

reject() {  # el codificador debe negarse a codificar esta linea
  if "$WORK/nx_test" --one "$1" >/dev/null 2>&1; then
    echo "FAIL rechazo: '$1' deberia rechazarse"; fail=$((fail+1))
  else pass=$((pass+1)); fi
}

check_file "casos del corpus (tests/native/cases.asm)" tests/native/cases.asm
check_file "casos de borde (tests/native/edge.asm)" tests/native/edge.asm

# Formas fuera del subconjunto: deben rechazarse, no codificarse mal.
reject "jmp .L1"
reject "call main"
reject "mov rax, str_0"
reject "mov rax, [str_0]"
reject "mov [rax], 1"
reject "add rax, 0x1FFFFFFFF"
reject "mov rax, [rsp*2]"
reject "movdqu xmm2, [rel .L42]"
reject "frobnicate rax"

if [ "$MODE" = "--corpus" ] || [ "$MODE" = "--regen" ]; then
  tools/native/corpus.sh "$WORK/corpus" || exit 1
  "$WORK/asminv" --cases "$WORK"/corpus/O0/*.asm "$WORK"/corpus/O2/*.asm | LC_ALL=C sort -u > "$WORK/cases_new.asm"
  [ -s "$WORK/cases_new.asm" ] || { echo "FAIL corpus vacio"; exit 1; }
  if [ "$MODE" = "--regen" ]; then
    cp "$WORK/cases_new.asm" tests/native/cases.asm
    echo "regenerado tests/native/cases.asm ($(wc -l < tests/native/cases.asm) lineas)"
  else
    check_file "corpus completo recien generado" "$WORK/cases_new.asm"
  fi
  echo
  "$WORK/asminv" "$WORK"/corpus/O0/*.asm "$WORK"/corpus/O2/*.asm | sed -n '1,9p'
fi

echo
echo "native: $pass ok, $fail fallos"
[ $fail = 0 ]
