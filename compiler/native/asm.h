// One-and-a-half-pass assembler for the NASM text copihue emits in hosted mode.
//
// Second piece of the native back end (docs/DESIGN_NATIVE.md, M2). It reads a
// whole program, resolves labels and relaxes jumps (rel8 when it fits, rel32
// when not, as nasm does), and returns the bytes of .text plus the relocations
// that remain. .data and .bss are only recognised (labels, section names), not
// encoded: that is M3. Not used by copihue yet.
#ifndef COPIHUE_ASM_H
#define COPIHUE_ASM_H

#include <stddef.h>
#include <stdint.h>

#include "x64.h"

enum { ASM_SEC_TEXT, ASM_SEC_DATA, ASM_SEC_BSS };

// A relocation left in .text. When the target is a label of .text, sym is
// ".text" and addend includes the label's offset (nasm does the same, `global`
// or not). When the target is in .data or .bss, sym is the label, sec says
// which section, and addend does not include the label's offset (not known
// before M3).
typedef struct {
    int off;            // byte offset in .text of the field
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

typedef struct {
    uint8_t* text;
    size_t ntext;
    AsmReloc* rel;
    size_t nrel;
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

#endif
