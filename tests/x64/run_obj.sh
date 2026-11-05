#!/usr/bin/env bash
# Relocatable objects (-c) written by compiler/native/elf.c (M6), against nasm -f elf64.
#   tests/x64/run_obj.sh
# Needs nasm, a C compiler and ./copihue (built if missing). Runs:
#   1. the programs in tests/x64/obj_cases.asm (blocks split by `;---`) and the ones the writer must refuse
#   2. the text the compiler emits with -c for tests/, examples/ and tests/link/parts (-O0 and -O2)
#   3. a few real `copihue -c` object files, read from disk
#   4. on Linux, with gcc, ld and readelf: link a native object with a C program and run it, link native
#      and nasm objects together, and read the objects with readelf and objdump. Elsewhere this is skipped
#      with a notice; with CMP_REQUIRE=1 (the CI sets it) a missing tool or a different system is an error.
# What is compared is in tests/x64/test_obj.c.
set -u
cd "$(dirname "$0")/../.."
command -v nasm >/dev/null || { echo "obj: nasm not found" >&2; exit 1; }
W=build/native/objtest
mkdir -p "$W"
"${CC:-cc}" -O2 -Wall -Wextra -o "$W/test_obj" tests/x64/test_obj.c compiler/native/elf.c compiler/native/asm.c compiler/native/x64.c compiler/native/x64_line.c || exit 1
make copihue >/dev/null || exit 1
if [ -z "${OBJ_CORPUS:-}" ]; then
  rm -rf "$W/corpus"
  CORPUS_FLAGS=-c CORPUS_PARTS=1 tools/native/corpus.sh "$W/corpus" ./copihue || exit 1
  OBJ_CORPUS=$W/corpus
fi
rc=0
echo "== cases";  "$W/test_obj" "$W" cases tests/x64/obj_cases.asm || rc=1
echo "== reject"; "$W/test_obj" "$W" reject || rc=1
echo "== corpus (-c -S text of tests/, examples/ and tests/link/parts)"
"$W/test_obj" "$W" files "$OBJ_CORPUS"/O0/*.asm "$OBJ_CORPUS"/O2/*.asm || rc=1
echo "== copihue -c (files)"
for src in tests/link/parts/mod_add.lap tests/link/parts/mod_main.lap tests/link/parts/mod_mem.lap tests/link/parts/gen_max.lap tests/link/parts/str_def.lap tests/hello.lap; do
  [ -f "$src" ] || continue
  for o in 0 2; do
    ./copihue -O$o -c -S -o "$W/p.asm" "$src" >/dev/null 2>&1 || { echo "FAIL $src: -c -S"; rc=1; continue; }
    ./copihue -O$o -c -o "$W/p.o" "$src" >/dev/null 2>"$W/p.err" || { echo "FAIL $src -O$o -c:"; cat "$W/p.err"; rc=1; continue; }
    if grep -q 'falling back' "$W/p.err"; then echo "FAIL $src -O$o -c: fell back to nasm:"; cat "$W/p.err"; rc=1; continue; fi
    "$W/test_obj" "$W" obj "$W/p.o" "$W/p.asm" | grep -v '^nasm:' | sed "s|^|$src -O$o: |" | tail -1
    "$W/test_obj" "$W" obj "$W/p.o" "$W/p.asm" >/dev/null || rc=1
  done
done

echo "== link and run (Linux)"
skip=
if [ "$(uname -s)" != Linux ]; then skip="this is $(uname -s), not Linux: an ELF object cannot be linked or run here"
else
  for t in gcc ld readelf objdump; do command -v $t >/dev/null || skip="$t not found"; done
fi
if [ -n "$skip" ]; then
  if [ "${CMP_REQUIRE:-0}" = 1 ]; then echo "obj: FAIL: link and run needed, but $skip"; rc=1
  else echo "obj: SKIPPED link, run and readelf: $skip (the CI runs them with CMP_REQUIRE=1)"; fi
else
  L=$W/link; rm -rf "$L"; mkdir -p "$L"
  for o in 0 2; do
    # A native object called from C, and native objects next to nasm ones.
    ./copihue -O$o -c -o "$L/add.o" tests/link/parts/mod_add.lap || { echo "FAIL add.o -O$o"; rc=1; continue; }
    if ! gcc -no-pie -o "$L/cprog" tests/link/c_main.c "$L/add.o" 2>"$L/err"; then echo "FAIL gcc: native add.o with a C main (-O$o):"; cat "$L/err"; rc=1
    elif [ "$("$L/cprog")" != "42 7" ]; then echo "FAIL C main calling native add.o (-O$o): got '$("$L/cprog")', expected '42 7'"; rc=1
    else echo "  C main + native add.o (-O$o): 42 7"; fi
    ./copihue -O$o --nasm -c -o "$L/add_nasm.o" tests/link/parts/mod_add.lap >/dev/null 2>&1 || { echo "FAIL --nasm -c"; rc=1; continue; }
    ./copihue -O$o -c -o "$L/main.o" tests/link/parts/mod_main.lap || { echo "FAIL main.o -O$o"; rc=1; continue; }
    ./copihue -O$o --nasm -c -o "$L/main_nasm.o" tests/link/parts/mod_main.lap >/dev/null 2>&1 || { echo "FAIL --nasm -c main"; rc=1; continue; }
    for pair in "add.o main.o" "add_nasm.o main.o" "add.o main_nasm.o" "add_nasm.o main_nasm.o"; do
      set -- $pair
      if ! ld -o "$L/prog" "$L/$1" "$L/$2" 2>"$L/err"; then echo "FAIL ld $pair (-O$o):"; cat "$L/err"; rc=1
      elif [ "$("$L/prog")" != "42" ]; then echo "FAIL $pair (-O$o): got '$("$L/prog")', expected 42"; rc=1
      else echo "  ld $pair (-O$o): 42"; fi
    done
    for f in add.o main.o; do
      readelf -hSsrW "$L/$f" >"$L/readelf.txt" 2>&1 || { echo "FAIL readelf $f (-O$o)"; cat "$L/readelf.txt"; rc=1; }
      if grep -qi -e warning -e error "$L/readelf.txt"; then echo "FAIL readelf $f (-O$o) printed a warning:"; grep -i -e warning -e error "$L/readelf.txt"; rc=1; fi
      grep -q 'REL (Relocatable file)' "$L/readelf.txt" || { echo "FAIL readelf $f: not a relocatable file"; rc=1; }
      objdump -dr "$L/$f" >"$L/objdump.txt" 2>&1 || { echo "FAIL objdump $f (-O$o)"; cat "$L/objdump.txt"; rc=1; }
    done
  done
fi
[ $rc = 0 ] && echo "obj: OK" || echo "obj: FAIL"
exit $rc
