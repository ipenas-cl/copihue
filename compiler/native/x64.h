// Direct x86-64 instruction encoder (no nasm, no ld).
//
// This is the first slice of the native back end described in
// docs/DESIGN_NATIVE.md. It is not wired into copihue.c yet. It parses the
// NASM text the code generator emits (one instruction per line) into an XInsn
// and encodes that XInsn into machine code bytes plus, when the instruction
// names a symbol, one relocation record.
#ifndef COPIHUE_X64_H
#define COPIHUE_X64_H

#include <stddef.h>
#include <stdint.h>

typedef enum { X_NONE, X_REG, X_IMM, X_MEM } XKind;
typedef enum { RC_GPR, RC_XMM } XRegClass;

typedef struct {
    XKind kind;
    // X_REG
    int reg;            // 0..15
    int rclass;         // XRegClass
    int hi8;            // ah, ch, dh, bh
    // X_REG: register width in bytes. X_MEM: size keyword (0 = none).
    int size;
    // X_MEM
    int base, index;    // -1 when absent
    int scale;          // 1, 2, 4, 8
    int64_t disp;
    int rip;            // [rel sym]: RIP-relative, needs has_sym
    // X_IMM and X_MEM: an unresolved symbol, plus the constant added to it.
    // For X_IMM without a symbol the value is in imm.
    int has_sym;
    char sym[96];
    int64_t imm;
    int sym_neg;        // symbol was subtracted (cannot be relocated)
    int shortk, neark;  // "short" / "near" keyword on a jump target
} XOp;

typedef struct {
    char mnem[16];
    int lock;           // lock prefix
    int rep;            // 0, 0xF3 (rep, repe) or 0xF2 (repne)
    int nops;
    XOp op[3];
} XInsn;

// ELF x86-64 relocation numbers.
enum { XR_NONE = 0, XR_64 = 1, XR_PC32 = 2, XR_PLT32 = 4, XR_32 = 10, XR_32S = 11 };

typedef struct {
    int off;            // byte offset of the field inside the instruction
    int type;           // XR_*
    char sym[96];
    int64_t addend;
} XReloc;

typedef struct {
    uint8_t b[24];
    int n;
    int has_rel;
    XReloc rel;
} XEnc;

// A name -> value table for `name equ N` constants. The parser folds a
// known name into the immediate or displacement. Returns 1 and sets *v when
// the name is known.
typedef int (*XConstFn)(const char* name, int64_t* v, void* ctx);
void x64_set_consts(XConstFn fn, void* ctx);

// Parse one line of NASM text holding one instruction (no label, no comment
// needed but a trailing `; comment` is skipped). Returns 0 on success; on
// failure writes a short reason into err.
int x64_parse(const char* line, XInsn* in, char* err, size_t errsz);

// One line of generated assembly, classified.
enum { XL_BLANK, XL_COMMENT, XL_DIRECTIVE, XL_EQU, XL_INSN, XL_LABEL };
typedef struct {
    int kind;           // XL_*
    int has_label;      // a `name:` came first on the line (XL_LABEL: only that)
    char label[128];
    char dir[32];       // XL_DIRECTIVE: the directive; for `section` "section .text"
    char name[128];     // XL_EQU: the constant
    int64_t val;        // XL_EQU: its value
    char* text;         // XL_INSN: the instruction text (inside the caller's buffer)
} XLine;

// Classify one line. The buffer is modified (comment and newline removed).
void x64_classify(char* line, XLine* out);

// Encode. Returns the byte count, or 0 when this encoder does not cover the
// instruction yet (reason in *why, a static string, when why is not NULL).
int x64_encode(const XInsn* in, XEnc* out, const char** why);

// Branch with a known displacement. `rel` is measured from the end of the
// instruction. `short_form` picks rel8 (jmp, jcc) over rel32. Returns the
// byte count, or 0 when the mnemonic is not a branch or rel8 does not fit.
int x64_encode_branch(const char* mnem, int short_form, int32_t rel, XEnc* out);

// Size of a branch in bytes: 2 for the short form, else 5 (jmp, call) or 6 (jcc).
int x64_branch_size(const char* mnem, int short_form);

// Shape key of a parsed instruction, for the inventory: "mov r64,m64".
void x64_shape(const XInsn* in, char* out, size_t outsz);

#endif
