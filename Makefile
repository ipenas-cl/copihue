# Copihue (formerly Chronos) - build, test and install the compiler.
#   make                 build ./copihue
#   make test            regression suite (tests/run_tests.sh)
#   make net-test        HTTP server/client integration test (needs curl)
#   make native-test     x86-64 encoder, assembler and ELF writer (compiler/native, M1-M5) against nasm
#   make native-asm-test only the assembler part (labels, jumps, call, data sections) of native-test
#   make native-elf-test only the ELF writer part (structure of the executables --native writes)
#   make native-run-test the regression suite with --native (needs Linux to run the executables)
#   make native-text-test .text of --native against nasm + ld (objcopy; needs Linux binutils, skips without them)
#   make native-readelf-test the ELF files read by readelf and objdump (skips without binutils)
#   make bench           timed comparison in bench/ (not part of make test)
#   make install         install to $(PREFIX)/bin (default ~/.local)
PREFIX ?= $(HOME)/.local
CC     ?= gcc
CFLAGS ?= -O2 -Wall

copihue: compiler/bootstrap-c/copihue.c compiler/native/x64.c compiler/native/x64_line.c compiler/native/x64.h compiler/native/asm.c compiler/native/asm.h compiler/native/elf.c compiler/native/elf.h
	$(CC) $(CFLAGS) -o $@ $<

test:
	tests/run_tests.sh

net-test:
	tests/net_test.sh

native-test:
	tests/x64/run.sh

native-elf-test:
	tests/x64/run_elf.sh

native-run-test:
	tests/run_tests.sh --native

native-text-test:
	tests/x64/run_text.sh

native-readelf-test:
	tests/x64/run_readelf.sh

native-asm-test:
	tests/x64/run_asm.sh

native-inventory: copihue
	tools/native/inventory.sh

bench: copihue
	bench/run.sh

install: copihue
	install -d $(PREFIX)/bin
	install -m 755 copihue $(PREFIX)/bin/copihue

uninstall:
	rm -f $(PREFIX)/bin/copihue

clean:
	rm -f copihue a.out output.asm output.o

.PHONY: test net-test native-test native-asm-test native-elf-test native-run-test native-text-test native-readelf-test native-inventory bench install uninstall clean
