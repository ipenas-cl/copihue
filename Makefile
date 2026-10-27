# Copihue (formerly Chronos) - build, test and install the compiler.
#   make                 build ./copihue
#   make test            regression suite (tests/run_tests.sh)
#   make net-test        HTTP server/client integration test (needs curl)
#   make bench           timed comparison in bench/ (not part of make test)
#   make install         install to $(PREFIX)/bin (default ~/.local)
PREFIX ?= $(HOME)/.local
CC     ?= gcc
CFLAGS ?= -O2 -Wall

copihue: compiler/bootstrap-c/copihue.c
	$(CC) $(CFLAGS) -o $@ $<

test:
	tests/run_tests.sh

net-test:
	tests/net_test.sh

native-test:
	tests/x64/run.sh

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

.PHONY: test net-test native-test native-inventory bench install uninstall clean
