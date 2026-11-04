// ELF64 executable writer for the native back end (docs/DESIGN_NATIVE.md, M4).
//
// Takes what asm.c returns for a hosted program (section bytes, relocations,
// symbol table) and writes a static ET_EXEC for x86-64 Linux: no section
// headers, no dynamic linking. Relocations are not kept; each field is patched
// with the final address (2.5).
#ifndef COPIHUE_ELF_H
#define COPIHUE_ELF_H

#include <stddef.h>
#include <stdint.h>

#include "asm.h"

#define ELF_BASE      0x400000ULL   // virtual address of the file's first byte
#define ELF_TEXT_OFF  0x1000        // file offset of .text (and so its address 0x401000)
#define ELF_PAGE      0x1000

// Where everything goes. Segment 1 (R+X) maps file offset 0 at ELF_BASE:
// headers, .text, then .rodata if the program has one. Segment 2 (R+W) holds
// .data and, after it, .bss (p_memsz > p_filesz). It is absent (nphdr == 1)
// when the program has neither.
typedef struct {
    uint64_t base[ASM_NSEC];    // virtual address of each section
    uint64_t off[ASM_NSEC];     // file offset (.bss: none, 0)
    uint64_t entry;             // address of _start
    int nphdr;
    uint64_t s1_filesz;         // segment 1: offset 0, address ELF_BASE, filesz == memsz
    uint64_t s2_off, s2_va, s2_filesz, s2_memsz;
    uint64_t file_size;
} ElfLayout;

// Computes the layout. Returns 0, or -1 with the reason in err.
int elf_layout(const AsmOut* a, ElfLayout* lay, char* err, size_t errsz);

// Builds the whole file in memory (malloc'd; the caller frees *img).
int elf_build(const AsmOut* a, uint8_t** img, size_t* n, ElfLayout* lay, char* err, size_t errsz);

// Writes the executable to `path` with mode 0755.
int elf_write_exec(const AsmOut* a, const char* path, char* err, size_t errsz);

#endif
