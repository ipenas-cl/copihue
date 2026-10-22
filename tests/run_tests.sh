#!/usr/bin/env bash
# Copihue regression suite.
#
#   tests/run_tests.sh            run everything
#   tests/run_tests.sh --update   regenerate golden files (review the diff!)
#
# 1. Program tests: every tests/**/*.cph and examples/*.cph is compiled at -O0
#    and -O2, run with a timeout, and its stdout + exit code compared with
#    tests/expected/<path>.out. Both optimization levels must match the same
#    golden file. Heap/stack addresses (9+ digit numbers) are masked.
# 2. Error tests: every tests/errors/*.cph must FAIL to compile and its
#    diagnostics must match tests/expected/errors/<name>.err.
# 3. Freestanding tests: tests/freestanding/*.cph compile with --freestanding,
#    the output must assemble with nasm, and contain no _start or syscall.
set -u
cd "$(dirname "$0")/.."
ROOT=$PWD
CC=$ROOT/compiler/bootstrap-c/copihuec
UPDATE=0; [ "${1:-}" = "--update" ] && UPDATE=1
gcc -O2 -Wall -o "$CC" compiler/bootstrap-c/copihuec.c || exit 1

# Programs that are not batch tests (servers / need a peer) or need input.
# They are covered by tests/net_test.sh instead.
SKIP=" examples/simple_api_server.cph examples/realtime_api_monitor.cph tests/test_http_simple.cph tests/http/test_http_client.cph tests/http/test_http_post.cph tests/http/test_suite_http.cph "

WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0; failed=()

mask() { sed -E 's/[0-9]{9,}/ADDR/g'; }

run_program() {  # $1 = source, $2 = opt level -> prints masked stdout + rc
  local src=$1 opt=$2
  ( cd "$WORK" && rm -f prog data.txt && "$CC" "$opt" -o prog "$ROOT/$src" >/dev/null 2>compile.err ) \
    || { echo "COMPILE ERROR"; sed "s|$ROOT/||g" "$WORK/compile.err"; return; }
  ( cd "$WORK" && timeout 5 ./prog </dev/null 2>&1; echo "exit=$?" ) | mask
}

check() {  # $1 = name, $2 = golden file, $3 = actual file
  if [ $UPDATE = 1 ]; then mkdir -p "$(dirname "$2")"; cp "$3" "$2"; fi
  if [ -f "$2" ] && cmp -s "$2" "$3"; then pass=$((pass+1));
  else fail=$((fail+1)); failed+=("$1"); echo "FAIL $1"; [ -f "$2" ] && diff "$2" "$3" | head -10; fi
}

for src in $(ls tests/*.cph tests/*/*.cph examples/*.cph | grep -v -e '^tests/errors/' -e '^tests/freestanding/'); do
  case "$SKIP" in *" $src "*) continue ;; esac
  golden=tests/expected/${src%.cph}.out
  run_program "$src" -O0 > "$WORK/o0"
  run_program "$src" -O2 > "$WORK/o2"
  if ! cmp -s "$WORK/o0" "$WORK/o2"; then
    fail=$((fail+1)); failed+=("$src (-O0 vs -O2)"); echo "FAIL $src: -O0 and -O2 differ"; diff "$WORK/o0" "$WORK/o2" | head -10; continue
  fi
  grep -q "^COMPILE ERROR" "$WORK/o0" && { fail=$((fail+1)); failed+=("$src"); echo "FAIL $src:"; cat "$WORK/o0"; continue; }
  grep -qE "^exit=(12[4-9]|13[0-9]|14[0-9])$" "$WORK/o0" && { fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: crashed or timed out ($(tail -1 "$WORK/o0"))"; continue; }
  check "$src" "$golden" "$WORK/o0"
done

for src in tests/errors/*.cph; do
  name=$(basename "$src" .cph)
  flags=$(sed -n 's|^// flags: ||p' "$src" | head -1)
  if ( cd "$WORK" && "$CC" -S $flags -o out.asm "$ROOT/$src" >/dev/null 2>err.txt ); then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: compiled but should have failed"; continue
  fi
  sed "s|$ROOT/||g" "$WORK/err.txt" > "$WORK/err.norm"
  check "$src" "tests/expected/errors/$name.err" "$WORK/err.norm"
done

for src in tests/freestanding/*.cph; do
  name=$(basename "$src" .cph)
  if ! ( cd "$WORK" && "$CC" --freestanding -o fs.asm "$ROOT/$src" 2>err.txt ); then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src:"; cat "$WORK/err.txt"; continue
  fi
  if grep -qE '_start|^\s*syscall' "$WORK/fs.asm" || ! nasm -f elf64 "$WORK/fs.asm" -o "$WORK/fs.o"; then
    fail=$((fail+1)); failed+=("$src"); echo "FAIL $src: not freestanding or does not assemble"; continue
  fi
  # Keep only instructions (drop comments/blank lines) as the golden output.
  grep -vE '^\s*(;|$)' "$WORK/fs.asm" | grep -v '^; Generated' > "$WORK/fs.norm"
  check "$src" "tests/expected/freestanding/$name.asm" "$WORK/fs.norm"
done

rm -f "$CC"
echo
echo "passed: $pass  failed: $fail"
[ $fail = 0 ] || { printf '  %s\n' "${failed[@]}"; exit 1; }
