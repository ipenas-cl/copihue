// One-and-a-half-pass assembler for the NASM text copihue emits in hosted mode.
//
// Second and third pieces of the native back end (docs/DESIGN_NATIVE.md, M2 and
// M3). It reads a whole program, resolves labels and relaxes jumps (rel8 when
// it fits, rel32 when not, as nasm does), lays out .data, .rodata and .bss, and
// returns the bytes of every section plus the relocations that remain, with the
// same symbols and addends as `nasm -f elf64`. Also returns the symbol table
// (which labels exist, which are `global`, their final offsets); elf.c (M4)
// uses it to find `_start` and to patch relocations with final addresses.
#ifndef COPIHUE_ASM_H
#define COPIHUE_ASM_H

#include <stddef.h>
#include <stdint.h>

#include "x64.h"

enum { ASM_SEC_TEXT, ASM_SEC_DATA, ASM_SEC_BSS, ASM_SEC_RODATA, ASM_NSEC };

// A relocation left in a section (the field lives in `in`). The target is a
// section symbol (".text", ".data", ".rodata", ".bss", as nasm does for every
// label defined in the file, `global` or not) and addend includes the label's
// offset in that section; `sec` says which. `sym` is the section name.
typedef struct {
    int in;             // ASM_SEC_* holding the field
    int off;            // byte offset in that section of the field
    int type;           // XR_*
    char sym[128];
    int64_t addend;
    int sec;            // ASM_SEC_* of the target
} AsmReloc;

// One branch to a label, in .text order (for diagnostics).
typedef struct {
    int line;           // source line
    long addr;          // offset in .text
    int islong;
    int is_call;
} AsmBranch;

// One section of the output. bytes is NULL for .bss (size only); a
// section the program never names has size 0 and present = 0.
typedef struct {
    uint8_t* bytes;
    size_t size;
    int present;        // the program has a `section` line for it (or it is .text)
    int align;          // section alignment: nasm's default (.text 16, others 4) raised by the largest `align N`
    AsmReloc* rel;
    size_t nrel;
} AsmSection;

// A label of the program (every label, `global` or not; local labels are
// `parent.child`). `off` is the final offset in its section, after jump
// relaxation. Sorted by section, then offset, then name.
typedef struct {
    char* name;
    int sec;            // ASM_SEC_*
    long off;
    int global;         // named by a `global` line
    int line;           // source line of the definition
} AsmSym;

typedef struct {
    AsmSection sec[ASM_NSEC];
    AsmSym* sym;
    size_t nsym;
    AsmBranch* br;
    size_t nbr;
    // Branches to a label, after relaxation.
    long jmp_short, jmp_long, jcc_short, jcc_long, calls;
    long long_fits;     // long jmp/jcc whose final displacement would fit rel8 (grow-only leaves them long)
    long relax_rounds;  // layout passes until nothing grew
} AsmOut;

// Assemble NUL-terminated text. Returns 0 on success. On failure returns -1
// and writes "line N: reason" into err; out holds nothing to free.
int asm_assemble(const char* src, AsmOut* out, char* err, size_t errsz);
void asm_free(AsmOut* out);

// The label `name` (a `global` or not), or NULL.
const AsmSym* asm_find_sym(const AsmOut* out, const char* name);

#endif
