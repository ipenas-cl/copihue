# Copihue (formerly Chronos) - build, test and install the compiler.
#   make                 build ./copihuec
#   make test            regression suite (tests/run_tests.sh)
#   make net-test        HTTP server/client integration test (needs curl)
#   make install         install to $(PREFIX)/bin (default ~/.local)
PREFIX ?= $(HOME)/.local
CC     ?= gcc
CFLAGS ?= -O2 -Wall

copihuec: compiler/bootstrap-c/copihuec.c
	$(CC) $(CFLAGS) -o $@ $<

test:
	tests/run_tests.sh

net-test:
	tests/net_test.sh

install: copihuec
	install -d $(PREFIX)/bin
	install -m 755 copihuec $(PREFIX)/bin/copihuec

uninstall:
	rm -f $(PREFIX)/bin/copihuec

clean:
	rm -f copihuec a.out output.asm output.o

.PHONY: test net-test install uninstall clean
