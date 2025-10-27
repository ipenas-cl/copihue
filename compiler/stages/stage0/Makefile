# Makefile para Tempo Bootstrap Stage 0

NASM = nasm
NASM_FLAGS = -f elf64
LD = ld

all: tempo0 test

tempo0: bootstrap.o
	$(LD) -o tempo0 bootstrap.o

bootstrap.o: bootstrap.s
	$(NASM) $(NASM_FLAGS) bootstrap.s -o bootstrap.o

test: tempo0
	./tempo0 hello.tempo hello.s
	$(NASM) $(NASM_FLAGS) hello.s -o hello.o
	$(LD) -o hello hello.o
	./hello

clean:
	rm -f *.o tempo0 hello hello.s

.PHONY: all test clean