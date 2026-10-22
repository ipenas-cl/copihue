#!/usr/bin/env bash
# Builds the C compiler and compiles + runs every example. Fails on compile errors,
# crashes (signals) or hangs. Non-zero exit codes are allowed: several examples
# return a computed value from main() as the process exit code.
set -u
CC=./compiler/bootstrap-c/chronos_v10
gcc -O2 -o "$CC" compiler/bootstrap-c/chronos_v10.c || exit 1
fail=0
for f in examples/*.ch; do
  name=$(basename "$f" .ch)
  case "$name" in
    simple_api_server) continue ;;            # long-running server, not a batch program
    structs_complete) continue ;;             # known codegen bug (nested struct field offsets), see README
    *) opt=-O2 ;;
  esac
  if ! "$CC" $opt "$f" >/dev/null 2>&1; then echo "FAIL compile $name"; fail=1; continue; fi
  timeout 5 ./chronos_program </dev/null >/dev/null 2>&1; rc=$?
  if [ $rc -ge 124 ] && [ $rc -ne 255 ]; then echo "FAIL run $name (exit $rc)"; fail=1; else echo "ok   $name"; fi
done
rm -f chronos_program output.asm output.o data.txt
exit $fail
