/* CHRONOS v0.17 - COMPILER OPTIMIZATIONS
 * New: Constant folding, strength reduction, peephole optimization
 * Flags: -O0 (none), -O1 (basic), -O2 (all)
 * Previous: ++, --, +=, -=, *=, /=, %=, string literal indexing, %, &&, ||, !
 * Self-hosting ready: All critical features complete
 * Author: Ignacio Peña
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

// ==== NAMING (single place to change when the language is renamed) ====
#define LANG_NAME     "Copihue"
#define LANG_VERSION  "0.32.0"
#define COMPILER_BIN  "copihue"
#define SRC_EXT       ".lap"   /* Lapageria rosea, the copihue */
#define OLD_SRC_EXT   ".cph"   /* v0.18.x extension, still accepted with a warning */
#define OLDER_SRC_EXT ".ch"    /* pre-rename (Chronos) extension, still accepted */

// ==== DIAGNOSTICS ====
const char* g_src_file = "<input>";
const char* g_src_text = NULL;
int g_cur_line = 0, g_cur_col = 0;   // position of the last consumed token
int g_error_count = 0;
static int g_no_semi = 0;  // >0 while parsing a for(...) increment (no ';')
static int g_no_eq = 0;    // deref operand must not swallow '=' (*p = v)
#define MAX_LOOP_DEPTH 64
static int g_loop_break[MAX_LOOP_DEPTH], g_loop_cont[MAX_LOOP_DEPTH], g_loop_depth = 0;
int g_freestanding = 0;              // --freestanding: no OS, no syscalls, no _start
const char* g_flat_org = NULL;       // --flat ADDR: freestanding flat binary loaded at ADDR, entry _start
int g_boot = 0;                      // --boot: flat sector at 0x7C00, padded to 512, signature 0xAA55
int g_reloc = 0;                     // -c: one ELF64 relocatable object (nasm -f elf64), no ld

static void print_source_line(int line, int col) {
    if (!g_src_text || line <= 0) return;
    const char* p = g_src_text;
    for (int l = 1; l < line && *p; p++) if (*p == '\n') l++;
    const char* e = p;
    while (*e && *e != '\n') e++;
    fprintf(stderr, "  %5d | %.*s\n", line, (int)(e - p), p);
    fprintf(stderr, "        | ");
    for (int i = 1; i < col; i++) fputc(p[i - 1] == '\t' ? '\t' : ' ', stderr);
    fprintf(stderr, "^\n");
}

#define MAX_LOADED 64
typedef struct { char* name; char* text; } LoadedSrc;
static LoadedSrc g_loaded[MAX_LOADED];
static int g_nloaded = 0;
static char* g_import_stack[MAX_LOADED];
static int g_import_depth = 0;

static void focus_file(const char* file) {
    if (!file) return;
    for (int i = 0; i < g_nloaded; i++) {
        if (g_loaded[i].name == file) {
            g_src_file = g_loaded[i].name;
            g_src_text = g_loaded[i].text;
            return;
        }
    }
}

// Report an error at line:col in "file:line:col: error: message" form
void diag_error(int line, int col, const char* fmt, ...) {
    va_list ap;
    fprintf(stderr, "%s:%d:%d: error: ", g_src_file, line, col);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    print_source_line(line, col);
    g_error_count++;
}

// Optimization level (0 = none, 1 = basic, 2 = aggressive)
int optimization_level = 0;

// TOKENS
typedef enum {
    T_EOF, T_IDENT, T_NUM, T_STR,
    T_FN, T_LET, T_IF, T_ELSE, T_WHILE, T_FOR, T_RET, T_STRUCT, T_MUT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_LBRACKET, T_RBRACKET,
    T_SEMI, T_COLON, T_COMMA, T_DOT, T_AT, T_AMP,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_MOD,
    T_EQ, T_EQEQ, T_NEQ, T_LT, T_GT, T_LTE, T_GTE, T_ARROW,
    T_AND_AND, T_OR_OR, T_BANG,
    T_PLUSPLUS, T_MINUSMINUS, T_PLUSEQ, T_MINUSEQ, T_STAREQ, T_SLASHEQ, T_MODEQ,
    T_PIPE, T_CARET, T_TILDE, T_SHL, T_SHR
} TokType;

typedef struct { TokType t; char* s; int len; int line; int col; } Tok;
typedef struct { char* src; char* cur; int line; int col; } Lex;

// TYPE SYSTEM

// Type kinds
typedef enum {
    TYPE_BASIC,    // i32, i64, u8, etc.
    TYPE_ARRAY,    // [T; N]
    TYPE_POINTER,  // *T or *mut T
    TYPE_STRUCT    // struct Name { ... }
} TypeKind;

// Type information
typedef struct TypeInfo {
    TypeKind kind;
    int size;          // Size in bytes
    char* name;        // For basic types and structs

    // For arrays
    struct TypeInfo* elem_type;
    int array_size;

    // For pointers
    struct TypeInfo* pointee_type;
    int is_mutable;    // For *mut T
} TypeInfo;

// Struct definitions (kept for compatibility)
typedef struct StructField {
    char* name;
    int offset;
    int size;             // Bytes occupied. An array is N * sizeof(element)
    int array_len;        // 0 if the field is not [T; N]
    int is_signed;        // Narrow signed loads use movsx / movsxd
    char* type_name;      // Element or scalar type (e.g., "i64", "u8", "*Token")
    int is_pointer;       // 1 if the field itself is a pointer, 0 otherwise
    int is_volatile;      // Reloaded/stored on every access; never folded or hoisted
    int align;            // Alignment applied to this field (1, 2, 4, 8, or an explicit align)
} StructField;

typedef struct StructType {
    char* name;
    StructField* fields;
    int field_count;
    int size;             // Laid-out size, including tail padding when not packed
    int align;            // Natural alignment. 0 until the struct is finished. Packed is 1
    int explicit_align;   // align(N) on the struct itself. 0 if the source did not ask
    int is_packed;        // 1: no padding. Hardware records stay contiguous
} StructType;

typedef struct TypeTable {
    StructType* types;
    int count;
} TypeTable;

// AST
typedef enum {
    AST_PROGRAM, AST_FUNCTION, AST_BLOCK, AST_RETURN, AST_LET,
    AST_IF, AST_WHILE, AST_CALL, AST_IDENT, AST_NUMBER,
    AST_BINOP, AST_COMPARE, AST_STRING, AST_ASSIGN,
    AST_ARRAY_LITERAL, AST_INDEX, AST_STRUCT_DEF, AST_STRUCT_LITERAL, AST_FIELD_ACCESS,
    AST_UNARY, AST_DEREF, AST_ADDR_OF, AST_GLOBAL_VAR, AST_ARRAY_ASSIGN, AST_FIELD_ASSIGN,
    AST_LOGICAL,  // && and || operators with short-circuit evaluation
    AST_BREAK, AST_CONTINUE,
    AST_DATA, AST_DATA_ITEM, AST_CAST,
    AST_JMP, AST_MEM_STORE, AST_LABEL
} AstType;

typedef struct AstNode {
    AstType type;
    char* name;
    struct AstNode** children;
    int child_count;
    char* value;
    char* op;
    int offset;
    int array_size;
    char* struct_type;
    int is_pointer;  // For type tracking. Bit 0 = pointer, bit 1 = mut, bit 2 = volatile pointee
    int is_forward_decl;  // For function forward declarations
    int flags;            // AF_PACKED, AF_VOLATILE, AF_INTERRUPT, AF_SYSCALL, AF_NAKED
    int bits;             // 0 = default 64 (no directive); else 16, 32, or 64
    int line, col;        // Source position (for diagnostics)
    const char* file;     // Source file (several files share one program)
    char* section;        // Named section this item is placed in. NULL is the default
    int align;            // align(N) on a struct, a field, or a global. 0 means none
} AstNode;

#define AF_PACKED     1
#define AF_VOLATILE   2
#define AF_INTERRUPT  4
#define AF_SYSCALL    8
#define AF_NAKED      16
#define AF_CARRY      32   // `if carry`: branch on CF, no accumulator load
#define AF_EXTERN     64   // `extern name` or `extern fn`: defined in another -c object
#define AF_JMP_MEM   128   // `jmp [name]`: indirect through memory, naked only

// Symbol table
typedef struct {
    char* name;
    int offset;
    int size;
    char* type_name;
    int is_pointer;
    int is_array;
    int is_volatile;
    int volatile_pointee; // *volatile T: loads/stores through this pointer are volatile
    int array_len;        // Declared element count when is_array (0 if unknown)
    int reg;              // -1 stack; 0..3 callee-saved r12..r15; 4..5 caller-saved r10, r11
    int as_int;           // Pointer value that may also be used as an integer
    char* mem;            // Naked local: label of its slot. NULL means [rbp+offset]
    char* hwreg;          // Naked parameter: register that already holds it. NULL otherwise
    int hwbits;           // Width of hwreg in bits. 0 if hwreg is NULL
} Symbol;

#define MAX_BLOCK_SCOPE 64

typedef struct {
    Symbol* symbols;
    int count;
    int stack_size;
    int scope_base[MAX_BLOCK_SCOPE];
    int scope_depth;
} SymbolTable;

// Global variable table
typedef struct {
    char* name;
    char* type_name;    // Type: "i32", "i64", "u8", etc.
    int size;           // Size in bytes (calculated from type)
    int is_initialized; // 1 = .data, 0 = .bss
    char* init_value;   // For initialized scalar data

    // For arrays: [T; N]
    int is_array;
    int array_count;    // Number of elements
    char* elem_type;    // Element type name
    char** array_init_values;  // Array initialization values
    int array_init_count;      // Number of init values

    // For pointers: *T or *mut T
    int is_pointer;
    int is_mutable;     // For *mut T
    int is_volatile;    // Reloaded/stored on every use; never folded or hoisted
    int is_extern;      // Declared with extern. No dq/resq is emitted for it
    int volatile_pointee; // *volatile T: the pointee access is volatile, not the pointer slot
    char* pointee_type; // Type being pointed to
    char* section;      // Placed in this section, not the default .data or .bss
    int align;          // align(N) on this global. 0 means the type's own alignment
} GlobalVar;

typedef struct {
    GlobalVar* vars;
    int count;
    int capacity;
} GlobalSymbolTable;

// String table
typedef struct {
    char* label;
    char* value;
    int len;
} StringEntry;

typedef struct {
    StringEntry* strings;
    int count;
} StringTable;

// Type specification (temporary structure for parsing)
typedef struct {
    char* base_type;    // i32, i64, etc.
    int is_array;
    int array_count;
    int is_pointer;
    int is_mutable;
    int is_volatile;    // *volatile T: the pointee is a memory-mapped register
} TypeSpec;

typedef struct { Tok* tokens; int pos, count; } Parser;
typedef struct {
    FILE* out;
    int label_count;
    SymbolTable* symtab;
    GlobalSymbolTable* global_symtab;
    StringTable* strtab;
    TypeTable* types;
    char* code_buf;
    int code_len;
    int code_cap;
    AstNode* program;         // Whole program, so calls can resolve functions
    int need_syscall_data;    // Emit __syscall_rsp / __syscall_user_rsp
} Codegen;

// Kind of the function currently being generated (for return/epilogue).
static int g_fn_flags = 0;
static int g_fn_bits = 0;   // 0 or 64: rax. 16 or 32: eax (legacy mode)
// bits 16/32: ebx is the ALU scratch and also a naked lvalue (the INT 15h
// continuation token). Every binary operator and every comparison used as a
// value push/pops ebx, not only an if/while. A shift also push/pops ecx
// (`shl`/`shr`/`sar` need cl). 64-bit rbx stays scratch.
// bits 16/32 unsigned 32-bit compare. 0 everywhere else, so setl/jl stay.
static int g_cmp_unsigned = 0;
static char g_fn_name[128];
static AstNode* g_cur_fn = NULL;   // Function whose body is being generated
// A naked local is a labeled slot in the image. A stack frame at the first
// instruction would use the BIOS stack, and `ss = 0` would then point bp at
// the wrong segment. Arrays and structs stay rejected.
#define MAX_NAKED_SLOTS 64
static char* g_naked_slots[MAX_NAKED_SLOTS];
static int g_naked_slot_n = 0;
static int g_naked_slot_seq = 0;
static int g_naked_slot_bytes = 0;

static int legacy_bits(void) { return g_fn_bits == 16 || g_fn_bits == 32; }

// Named machine registers. A real local or global of the same name wins.
// 64-bit names are rejected in bits 16 and bits 32: nasm has no encoding
// there. The accumulator (ax/eax/rax) and the scratch (bx/ebx/rbx) are not
// in this table as parameter aliases; those are separate names below.
typedef struct {
    const char* name;
    int bits;
    int seg;
    int high;        // ah: the byte is not the low byte of the accumulator
    int legacy_only; // bits 16 and bits 32. 64-bit code keeps using these as scratch or as the frame
} MachReg;

static const MachReg* mach_reg(const char* n) {
    static const MachReg regs[] = {
        {"ds", 16, 1, 0}, {"es", 16, 1, 0}, {"ss", 16, 1, 0},
        {"fs", 16, 1, 0}, {"gs", 16, 1, 0},
        {"al", 8, 0, 0}, {"ah", 8, 0, 1}, {"dl", 8, 0, 0},
        {"ax", 16, 0, 0}, {"sp", 16, 0, 0}, {"si", 16, 0, 0}, {"di", 16, 0, 0},
        // bx is the scratch of a binary operator. bp is the frame pointer of
        // a normal function. Both are naked lvalues in bits 16 and bits 32
        // only: a 64-bit naked function still evaluates through rbx.
        {"bx", 16, 0, 0, 1}, {"bp", 16, 0, 0, 1},
        {"eax", 32, 0, 0}, {"ecx", 32, 0, 0}, {"edx", 32, 0, 0}, {"esp", 32, 0, 0},
        {"ebx", 32, 0, 0, 1}, {"ebp", 32, 0, 0, 1}, {"esi", 32, 0, 0, 1}, {"edi", 32, 0, 0, 1},
        {"rax", 64, 0, 0}, {"rbx", 64, 0, 0}, {"rsp", 64, 0, 0}, {"rbp", 64, 0, 0},
        // rdi/rsi/rdx are also the first naked parameters. A bare assignment
        // overwrites that parameter. They exist so ring-3 code can fill
        // syscall() without a move the compiler inserts.
        {"rdi", 64, 0, 0}, {"rsi", 64, 0, 0}, {"rdx", 64, 0, 0},
    };
    if (!n) return NULL;
    for (unsigned i = 0; i < sizeof regs / sizeof regs[0]; i++)
        if (!strcmp(regs[i].name, n)) return &regs[i];
    return NULL;
}

// 1 if this naked function may name the register. Diagnoses and returns 0
// when the name is a register the current function must not use.
static int mach_here(AstNode* at, const MachReg* r) {
    if (!r) return 0;
    if (!(g_fn_flags & AF_NAKED)) {
        diag_error(at->line, at->col, "register '%s' is only available in a naked function", r->name);
        return 0;
    }
    if (r->bits == 64 && legacy_bits()) {
        diag_error(at->line, at->col, "register '%s' is not available in bits %d", r->name, g_fn_bits);
        return 0;
    }
    if (r->legacy_only && !legacy_bits()) {
        diag_error(at->line, at->col, "register '%s' is not available in bits %d",
                   r->name, g_fn_bits ? g_fn_bits : 64);
        return 0;
    }
    return 1;
}

// Usable as a naked operand in this function. No diagnostic.
static int mach_ok(const MachReg* r) {
    if (!r || !(g_fn_flags & AF_NAKED)) return 0;
    if (r->bits == 64 && legacy_bits()) return 0;
    if (r->legacy_only && !legacy_bits()) return 0;
    return 1;
}

// ==== MEMORY HELPERS ====
void* safe_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return new_ptr;
}

// ==== TYPE TABLE ====
int type_size(const char* type_name);

TypeTable* typetab_new() {
    TypeTable* tt = calloc(1, sizeof(TypeTable));
    return tt;
}

StructType* typetab_lookup(TypeTable* tt, char* name) {
    for (int i = 0; i < tt->count; i++) {
        if (!strcmp(tt->types[i].name, name)) {
            return &tt->types[i];
        }
    }
    return NULL;
}

void typetab_add(TypeTable* tt, char* name) {
    tt->count++;
    tt->types = safe_realloc(tt->types, sizeof(StructType) * tt->count);
    tt->types[tt->count - 1].name = strdup(name);
    tt->types[tt->count - 1].fields = NULL;
    tt->types[tt->count - 1].field_count = 0;
    tt->types[tt->count - 1].size = 0;
    tt->types[tt->count - 1].align = 0;
    tt->types[tt->count - 1].explicit_align = 0;
    tt->types[tt->count - 1].is_packed = 0;
}

// Natural alignment of a scalar: its own width. A pointer is 8. A struct is
// the strictest of its fields. Packed structs report 1 so an outer struct
// does not insert padding in front of a hardware record.
static int align_up(int v, int a) {
    if (a <= 1) return v;
    return (v + a - 1) & ~(a - 1);
}

static int prim_width(const char* name) {
    if (!name) return 0;
    if (!strcmp(name, "i8") || !strcmp(name, "u8")) return 1;
    if (!strcmp(name, "i16") || !strcmp(name, "u16")) return 2;
    if (!strcmp(name, "i32") || !strcmp(name, "u32")) return 4;
    if (!strcmp(name, "i64") || !strcmp(name, "u64")) return 8;
    return 0;
}

// "[T; N]" as spelled by the parser, including one level of nesting.
// The count is the last "; " before the closing bracket.
static int parse_one_array(const char* s, char* inner, int inner_n, int* count) {
    int len, semi;
    if (!s || s[0] != '[') return 0;
    len = (int)strlen(s);
    if (len < 5 || s[len - 1] != ']') return 0;
    semi = -1;
    for (int i = len - 2; i >= 1; i--) {
        if (s[i] == ';') { semi = i; break; }
    }
    if (semi < 2 || s[semi + 1] != ' ') return 0;
    *count = atoi(s + semi + 2);
    if (semi - 1 <= 0 || semi - 1 >= inner_n) return 0;
    memcpy(inner, s + 1, semi - 1);
    inner[semi - 1] = 0;
    return 1;
}

// Byte size of a type, or -1 when it cannot be laid out. align_out receives
// the natural alignment. A struct that is still being defined has align 0
// and is rejected, so a field of its own type is an error instead of a
// zero-size hole. An unknown name is also -1: guessing 8 would be the old
// silent wrong size.
static int type_layout(TypeTable* tt, const char* name, int is_pointer, int* align_out, int depth) {
    int w, n, es, al;
    char inner[256];
    if (depth > 16) return -1;
    if (is_pointer || (name && name[0] == '*')) {
        if (align_out) *align_out = 8;
        return 8;
    }
    w = prim_width(name);
    if (w) {
        if (align_out) *align_out = w;
        return w;
    }
    if (name && name[0] == '[') {
        if (!parse_one_array(name, inner, (int)sizeof inner, &n) || n <= 0) return -1;
        al = 1;
        es = type_layout(tt, inner, 0, &al, depth + 1);
        if (es <= 0) return -1;
        if (n > 2147483647 / es) return -1;
        if (align_out) *align_out = al;
        return es * n;
    }
    if (name && tt) {
        StructType* st = typetab_lookup(tt, (char*)name);
        if (st && st->align > 0) {
            if (align_out) *align_out = st->align;
            return st->size;
        }
    }
    return -1;
}

// array_len > 0 is [T; N]. array_len < 0 is an array whose count is not a
// positive integer. 0 is a scalar or a pointer field.
void typetab_add_field(TypeTable* tt, char* struct_name, char* field_name, char* field_type,
                       int is_pointer, int is_volatile, int array_len, int req_align,
                       int line, int col) {
    StructType* st = typetab_lookup(tt, struct_name);
    if (!st) return;

    int al = 1;
    int fsize = -1;
    int stored_len = 0;
    char shown[256];
    if (array_len > 0) {
        int elem = type_layout(tt, field_type, is_pointer, &al, 0);
        stored_len = array_len;
        if (elem <= 0 || array_len > 2147483647 / elem) fsize = -1;
        else fsize = elem * array_len;
        snprintf(shown, sizeof shown, "[%s; %d]", field_type ? field_type : "?", array_len);
    } else if (array_len < 0) {
        snprintf(shown, sizeof shown, "[%s; %d]", field_type ? field_type : "?", 0);
        fsize = -1;
    } else {
        fsize = type_layout(tt, field_type, is_pointer, &al, 0);
        if (is_pointer && field_type && field_type[0] != '*')
            snprintf(shown, sizeof shown, "*%s", field_type);
        else
            snprintf(shown, sizeof shown, "%s", field_type ? field_type : "?");
    }
    if (fsize < 0) {
        diag_error(line, col, "cannot lay out field '%s' of type '%s'", field_name, shown);
        fsize = 0;
        al = 1;
        stored_len = 0;
    }
    // Packed records stay contiguous unless the field asks for align(N).
    // An IDT gate is 16 bytes and rsp0 of a TSS stays at offset 4.
    // Natural alignment would move both. align(64) on one field still pads
    // that field, packed or not, so two counters can sit on distinct lines.
    if (req_align > 0) al = req_align;
    else if (st->is_packed) al = 1;
    st->size = align_up(st->size, al);
    if (al > st->align) st->align = al;

    st->field_count++;
    st->fields = safe_realloc(st->fields, sizeof(StructField) * st->field_count);
    StructField* f = &st->fields[st->field_count - 1];
    memset(f, 0, sizeof *f);
    f->name = strdup(field_name);
    f->offset = st->size;
    f->size = fsize;
    f->array_len = stored_len;
    f->type_name = field_type ? strdup(field_type) : NULL;
    f->is_pointer = is_pointer;
    f->is_volatile = is_volatile;
    f->is_signed = (field_type && field_type[0] == 'i' && !is_pointer);
    f->align = al;
    st->size += fsize;
}

// Round an ordinary struct up to its strictest field alignment. A packed
// struct is not rounded: its size is the sum of the field widths.
static void typetab_finish(StructType* st) {
    if (!st) return;
    if (st->explicit_align > st->align) st->align = st->explicit_align;
    // A packed record with no align(N) stays contiguous: size is the sum of
    // the fields and the alignment is 1. align(N) on the struct, or on a
    // field, still rounds the size so an array of them keeps that alignment.
    if (st->is_packed && st->align <= 1) {
        st->align = 1;
        return;
    }
    if (st->align < 1) st->align = 1;
    st->size = align_up(st->size, st->align);
}

int typetab_field_offset(TypeTable* tt, char* struct_name, char* field_name) {
    StructType* st = typetab_lookup(tt, struct_name);
    if (!st) return -1;

    for (int i = 0; i < st->field_count; i++) {
        if (!strcmp(st->fields[i].name, field_name)) {
            return st->fields[i].offset;
        }
    }
    return -1;
}

StructField* typetab_field(TypeTable* tt, char* struct_name, const char* field_name) {
    StructType* st = typetab_lookup(tt, struct_name);
    if (!st || !field_name) return NULL;
    for (int i = 0; i < st->field_count; i++) {
        if (!strcmp(st->fields[i].name, field_name)) return &st->fields[i];
    }
    return NULL;
}

// Get the type of a field in a struct
char* typetab_field_type(TypeTable* tt, char* struct_name, char* field_name) {
    StructType* st = typetab_lookup(tt, struct_name);
    if (!st) return NULL;

    for (int i = 0; i < st->field_count; i++) {
        if (!strcmp(st->fields[i].name, field_name)) {
            return st->fields[i].type_name;
        }
    }
    return NULL;
}

// ==== STRING TABLE ====
StringTable* strtab_new() {
    StringTable* st = calloc(1, sizeof(StringTable));
    return st;
}

char* strtab_add(StringTable* st, char* value, int len) {
    st->count++;
    st->strings = safe_realloc(st->strings, sizeof(StringEntry) * st->count);

    char* label = malloc(32);
    snprintf(label, 32, "str_%d", st->count - 1);

    st->strings[st->count - 1].label = label;
    st->strings[st->count - 1].value = strndup(value, len);
    st->strings[st->count - 1].len = len;

    return label;
}

// Hosted -O2. The first four names are callee-saved and survive a call.
// r10 and r11 are caller-saved: only a local whose live range contains no
// call goes there. malloc uses r10 as a syscall argument, so a local in r10
// must not be live across that call. rbx stays scratch.
#define MAX_REG_CALLEE 4
#define MAX_REG_LOCAL 6
static const char *g_reg_local_name[MAX_REG_LOCAL] = {
    "r12", "r13", "r14", "r15", "r10", "r11"
};
static char g_reg_local_var[MAX_REG_LOCAL][96];
static int g_reg_local_n = 0;
static int g_reg_callee_n = 0;

static void symbol_assign_reg(Symbol *s) {
    s->reg = -1;
    if (!s->name || g_reg_local_n <= 0) return;
    if (s->is_array || s->is_pointer || s->is_volatile || s->size != 1) return;
    for (int i = 0; i < g_reg_local_n; i++) {
        if (!strcmp(s->name, g_reg_local_var[i])) { s->reg = i; return; }
    }
}

static const char *local_reg(Symbol *s) {
    if (!s || s->reg < 0 || s->reg >= g_reg_local_n) return NULL;
    return g_reg_local_name[s->reg];
}

static int o2_hosted(void) {
    // bits 16/32 keep the eax/ebx stream. These opts are 64-bit hosted only.
    return optimization_level >= 2 && !g_freestanding && !legacy_bits();
}

// x86 scale is only 1, 2, 4 or 8. A larger power of two is shl at hosted
// -O2 (emit_scale_reg). Anything else stays imul.
static int scale_ok(int elem) {
    return elem == 1 || elem == 2 || elem == 4 || elem == 8;
}

void emit(Codegen* cg, const char* fmt, ...);

// When an index load is the RHS of `reg = a[i]`, the final scaled load
// writes that register. Cleared before the index expression itself runs,
// so a nested a[b[i]] still loads the inner element into rax.
static const char* g_index_dest;
static int g_index_hit;

// Cold copies of the hosted bounds trap, emitted after the function's ret
// so the in-range path falls through instead of jumping over the trap.
static char* g_cold;
static int g_cold_len;
static int g_cold_cap;

// Set while the fall-through of a compare means this register is >= 0.
static const char* g_nonneg_reg;

// The value just loaded for a compare is still in rax. The next identical
// index load in the taken body reuses it. Compiler-time only.
static int g_fwd_ok;
static int g_fwd_hold;
static char g_fwd_arr[96];
static char g_fwd_idx[96];

// A callee-saved register that is not a local, saved in the prologue, used
// to hold a value across a call so the caller does not push it.
static const char* g_expr_tmp;

// Push/pop frame for a hosted -O2 function that never addresses rbp.
static int g_slim;
static int g_slim_pad;
static int g_slim_nsave;
static const char* g_slim_save[5];

// Inside a proven `while (i < lim)` the index `i` needs no bounds check.
static const char* g_proven_idx;
static long long g_proven_lim;
static AstNode* g_prev_stmt;
static AstNode* g_prev2_stmt;
static int g_strength_busy;
// The while whose body is being emitted, and the statement just before it.
static AstNode* g_enclosing_while;
static AstNode* g_enclosing_prev;

// Argument registers (rdi, rsi, r8, r9) must map to their own low names.
// Falling back to eax/ax/al reads a different register: lea rdi, [r12-1]
// followed by movsxd rdi, eax keeps the old eax and the call never shrinks.
static const char* reg_dword(const char* reg) {
    if (!strcmp(reg, "rax")) return "eax";
    if (!strcmp(reg, "rbx")) return "ebx";
    if (!strcmp(reg, "rcx")) return "ecx";
    if (!strcmp(reg, "rdx")) return "edx";
    if (!strcmp(reg, "rsi")) return "esi";
    if (!strcmp(reg, "rdi")) return "edi";
    if (!strcmp(reg, "r8")) return "r8d";
    if (!strcmp(reg, "r9")) return "r9d";
    if (!strcmp(reg, "r10")) return "r10d";
    if (!strcmp(reg, "r11")) return "r11d";
    if (!strcmp(reg, "r12")) return "r12d";
    if (!strcmp(reg, "r13")) return "r13d";
    if (!strcmp(reg, "r14")) return "r14d";
    if (!strcmp(reg, "r15")) return "r15d";
    return "eax";
}

static const char* reg_word(const char* reg) {
    if (!strcmp(reg, "rax")) return "ax";
    if (!strcmp(reg, "rbx")) return "bx";
    if (!strcmp(reg, "rcx")) return "cx";
    if (!strcmp(reg, "rdx")) return "dx";
    if (!strcmp(reg, "rsi")) return "si";
    if (!strcmp(reg, "rdi")) return "di";
    if (!strcmp(reg, "r8")) return "r8w";
    if (!strcmp(reg, "r9")) return "r9w";
    if (!strcmp(reg, "r10")) return "r10w";
    if (!strcmp(reg, "r11")) return "r11w";
    if (!strcmp(reg, "r12")) return "r12w";
    if (!strcmp(reg, "r13")) return "r13w";
    if (!strcmp(reg, "r14")) return "r14w";
    if (!strcmp(reg, "r15")) return "r15w";
    return "ax";
}

static void emit_scaled_load(Codegen* cg, const char* mem, int elem, int is_struct, const char* dest) {
    if (!dest) dest = "rax";
    if (is_struct) emit(cg, "    lea %s, [%s]\n", dest, mem);
    else if (elem == 1) emit(cg, "    movzx %s, byte [%s]\n", dest, mem);
    else if (elem == 2) emit(cg, "    movzx %s, word [%s]\n", dest, mem);
    else if (elem == 4) emit(cg, "    mov %s, [%s]\n", reg_dword(dest), mem);
    else emit(cg, "    mov %s, [%s]\n", dest, mem);
}

static void emit_scaled_store(Codegen* cg, const char* mem, int elem, const char* src) {
    if (elem == 1) emit(cg, "    mov byte [%s], %s\n", mem, src[0] == 'r' && src[1] == 'a' ? "al" : "bl");
    else if (elem == 2) emit(cg, "    mov word [%s], %s\n", mem, !strcmp(src, "rax") ? "ax" : "bx");
    else if (elem == 4) emit(cg, "    mov dword [%s], %s\n", mem, !strcmp(src, "rax") ? "eax" : "ebx");
    else emit(cg, "    mov qword [%s], %s\n", mem, src);
}

// ==== SYMBOL TABLE ====
SymbolTable* symtab_new() {
    SymbolTable* st = calloc(1, sizeof(SymbolTable));
    st->stack_size = 0;
    return st;
}

int symtab_add(SymbolTable* st, char* name, int size) {
    st->count++;
    st->symbols = safe_realloc(st->symbols, sizeof(Symbol) * st->count);
    int bytes = size * 8;
    st->stack_size += bytes;
    st->symbols[st->count - 1].name = strdup(name);
    st->symbols[st->count - 1].offset = -st->stack_size;
    st->symbols[st->count - 1].size = size;
    st->symbols[st->count - 1].type_name = NULL;
    st->symbols[st->count - 1].is_pointer = 0;
    st->symbols[st->count - 1].is_array = 0;
    st->symbols[st->count - 1].is_volatile = 0;
    st->symbols[st->count - 1].volatile_pointee = 0;
    st->symbols[st->count - 1].array_len = 0;
    st->symbols[st->count - 1].as_int = 0;
    st->symbols[st->count - 1].mem = NULL;
    st->symbols[st->count - 1].hwreg = NULL;
    st->symbols[st->count - 1].hwbits = 0;
    symbol_assign_reg(&st->symbols[st->count - 1]);
    return -st->stack_size;
}

int symtab_add_struct(SymbolTable* st, char* name, char* type_name, int size_bytes) {
    st->count++;
    st->symbols = safe_realloc(st->symbols, sizeof(Symbol) * st->count);
    st->stack_size += size_bytes;
    st->symbols[st->count - 1].name = strdup(name);
    st->symbols[st->count - 1].offset = -st->stack_size;
    st->symbols[st->count - 1].size = size_bytes;
    st->symbols[st->count - 1].type_name = strdup(type_name);
    st->symbols[st->count - 1].is_pointer = 0;
    st->symbols[st->count - 1].is_array = 0;
    st->symbols[st->count - 1].is_volatile = 0;
    st->symbols[st->count - 1].volatile_pointee = 0;
    st->symbols[st->count - 1].array_len = 0;
    st->symbols[st->count - 1].as_int = 0;
    st->symbols[st->count - 1].mem = NULL;
    st->symbols[st->count - 1].hwreg = NULL;
    st->symbols[st->count - 1].hwbits = 0;
    symbol_assign_reg(&st->symbols[st->count - 1]);
    return -st->stack_size;
}

int symtab_add_pointer(SymbolTable* st, char* name) {
    st->count++;
    st->symbols = safe_realloc(st->symbols, sizeof(Symbol) * st->count);
    st->stack_size += 8;  // Pointer is 8 bytes
    st->symbols[st->count - 1].name = strdup(name);
    st->symbols[st->count - 1].offset = -st->stack_size;
    st->symbols[st->count - 1].size = 8;
    st->symbols[st->count - 1].type_name = NULL;
    st->symbols[st->count - 1].is_pointer = 1;
    st->symbols[st->count - 1].is_array = 0;
    st->symbols[st->count - 1].is_volatile = 0;
    st->symbols[st->count - 1].volatile_pointee = 0;
    st->symbols[st->count - 1].array_len = 0;
    st->symbols[st->count - 1].as_int = 0;
    st->symbols[st->count - 1].mem = NULL;
    st->symbols[st->count - 1].hwreg = NULL;
    st->symbols[st->count - 1].hwbits = 0;
    symbol_assign_reg(&st->symbols[st->count - 1]);
    return -st->stack_size;
}

// A naked parameter occupies no stack. The register already holds the value.
static void symtab_add_hw(SymbolTable* st, char* name, const char* hw, int bits, int is_ptr, const char* type_name) {
    st->count++;
    st->symbols = safe_realloc(st->symbols, sizeof(Symbol) * st->count);
    Symbol* s = &st->symbols[st->count - 1];
    memset(s, 0, sizeof *s);
    s->name = name ? strdup(name) : NULL;
    s->reg = -1;
    s->hwreg = hw ? strdup(hw) : NULL;
    s->hwbits = bits;
    s->is_pointer = is_ptr;
    s->size = 1;
    if (type_name) {
        if (is_ptr) {
            char* t = malloc(strlen(type_name) + 2);
            sprintf(t, "*%s", type_name);
            s->type_name = t;
        } else {
            s->type_name = strdup(type_name);
        }
    }
}

// A block pushes the current count. Leaving it hides every name declared
// inside, without giving those stack slots back: a later `let` of the same
// name must not reuse the earlier slot, or a use would read the other one.
static void symtab_enter(SymbolTable* st) {
    if (!st || st->scope_depth >= MAX_BLOCK_SCOPE) return;
    st->scope_base[st->scope_depth++] = st->count;
}

static void symtab_leave(SymbolTable* st) {
    if (!st || st->scope_depth <= 0) return;
    st->scope_depth--;
    st->count = st->scope_base[st->scope_depth];
}

static int symtab_redeclared(SymbolTable* st, const char* name) {
    if (!st || !name || st->scope_depth <= 0) return 0;
    int base = st->scope_base[st->scope_depth - 1];
    for (int i = base; i < st->count; i++) {
        if (st->symbols[i].name && !strcmp(st->symbols[i].name, name)) return 1;
    }
    return 0;
}

Symbol* symtab_lookup_symbol(SymbolTable* st, char* name) {
    if (!st) return NULL;
    for (int i = st->count - 1; i >= 0; i--) {
        if (st->symbols[i].name && !strcmp(st->symbols[i].name, name)) {
            return &st->symbols[i];
        }
    }
    return NULL;
}

int symtab_lookup(SymbolTable* st, char* name) {
    Symbol* sym = symtab_lookup_symbol(st, name);
    return sym ? sym->offset : 0;
}

// ==== TYPE HELPERS ====
int type_size(const char* type_name) {
    if (!type_name) return 8;  // Default to 8 bytes
    if (!strcmp(type_name, "i8") || !strcmp(type_name, "u8")) return 1;
    if (!strcmp(type_name, "i16") || !strcmp(type_name, "u16")) return 2;
    if (!strcmp(type_name, "i32") || !strcmp(type_name, "u32")) return 4;
    if (!strcmp(type_name, "i64") || !strcmp(type_name, "u64")) return 8;
    return 8;  // Default
}

const char* type_asm_directive(const char* type_name) {
    int size = type_size(type_name);
    if (size == 1) return "db";
    if (size == 2) return "dw";
    if (size == 4) return "dd";
    return "dq";  // 8 bytes
}

// ==== GLOBAL SYMBOL TABLE ====
void global_symtab_init(GlobalSymbolTable* gst) {
    gst->capacity = 64;
    gst->count = 0;
    gst->vars = malloc(sizeof(GlobalVar) * gst->capacity);
}

void global_symtab_add_full(GlobalSymbolTable* gst, char* name, char* type_name, int size,
                            int is_initialized, char* init_value,
                            int is_array, int array_count, int is_pointer, int is_mutable) {
    if (gst->count >= gst->capacity) {
        gst->capacity *= 2;
        gst->vars = realloc(gst->vars, sizeof(GlobalVar) * gst->capacity);
    }

    GlobalVar* gv = &gst->vars[gst->count++];
    gv->name = strdup(name);
    gv->type_name = type_name ? strdup(type_name) : strdup("i64");
    gv->is_initialized = is_initialized;
    gv->init_value = init_value ? strdup(init_value) : NULL;

    // Array info
    gv->is_array = is_array;
    gv->array_count = array_count;
    gv->elem_type = is_array ? strdup(type_name) : NULL;
    gv->array_init_values = NULL;  // Set later if initialized
    gv->array_init_count = 0;

    // Pointer info
    gv->is_pointer = is_pointer;
    gv->is_mutable = is_mutable;
    gv->is_volatile = 0;
    gv->is_extern = 0;
    gv->volatile_pointee = 0;
    gv->section = NULL;
    gv->align = 0;
    gv->pointee_type = is_pointer ? strdup(type_name) : NULL;

    // Calculate size
    if (is_array) {
        gv->size = type_size(type_name) * array_count;
    } else if (is_pointer) {
        gv->size = 8;  // Pointers are always 8 bytes on x64
    } else {
        gv->size = size > 0 ? size : type_size(gv->type_name);
    }
}

GlobalVar* global_symtab_lookup(GlobalSymbolTable* gst, char* name) {
    for (int i = 0; i < gst->count; i++) {
        if (!strcmp(gst->vars[i].name, name)) {
            return &gst->vars[i];
        }
    }
    return NULL;
}

// ==== LEXER ====
void lex_init(Lex* l, char* s) {
    l->src = l->cur = s;
    l->line = 1;
    l->col = 1;
}

char peek(Lex* l) { return *l->cur; }
char peek_next(Lex* l) { return l->cur[1]; }

char adv(Lex* l) {
    char c = *l->cur++;
    if (c == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

void skip(Lex* l) {
    for (;;) {
        char c = peek(l);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') adv(l);
        else if (c == '/' && peek_next(l) == '/') {
            while (peek(l) != '\n' && peek(l)) adv(l);
        } else break;
    }
}

TokType kw(char* s, int len) {
    if (len == 2 && !memcmp(s, "fn", 2)) return T_FN;
    if (len == 3 && !memcmp(s, "let", 3)) return T_LET;
    if (len == 2 && !memcmp(s, "if", 2)) return T_IF;
    if (len == 4 && !memcmp(s, "else", 4)) return T_ELSE;
    if (len == 3 && !memcmp(s, "for", 3)) return T_FOR;
    if (len == 3 && !memcmp(s, "mut", 3)) return T_MUT;
    if (len == 5 && !memcmp(s, "while", 5)) return T_WHILE;
    if (len == 6 && !memcmp(s, "return", 6)) return T_RET;
    if (len == 6 && !memcmp(s, "struct", 6)) return T_STRUCT;
    return T_IDENT;
}

Tok lex_tok(Lex* l) {
    skip(l);
    char* st = l->cur;
    int tok_line = l->line;
    int tok_col = l->col;
    char c = adv(l);

    if (!c) return (Tok){T_EOF, st, 0, tok_line, tok_col};
    if (isalpha(c) || c == '_') {
        while (isalnum(peek(l)) || peek(l) == '_') adv(l);
        return (Tok){kw(st, l->cur - st), st, l->cur - st, tok_line, tok_col};
    }
    if (isdigit(c)) {
        if (c == '0' && (peek(l) == 'x' || peek(l) == 'X' || peek(l) == 'b' || peek(l) == 'B')) {
            adv(l);
            while (isxdigit(peek(l)) || peek(l) == '_') adv(l);
        } else {
            while (isdigit(peek(l)) || peek(l) == '_') adv(l);
        }
        return (Tok){T_NUM, st, l->cur - st, tok_line, tok_col};
    }
    if (c == '\'') {
        // Character literal: 'a', '\n', '\t', '\0', '\\', '\''
        if (peek(l) == '\\') adv(l);
        adv(l);
        if (peek(l) != '\'') {
            fprintf(stderr, "%s:%d:%d: error: unterminated character literal\n", g_src_file, tok_line, tok_col); print_source_line(tok_line, tok_col);
            exit(1);
        }
        adv(l);
        return (Tok){T_NUM, st, l->cur - st, tok_line, tok_col};
    }
    if (c == '"') {
        while (peek(l) && peek(l) != '"') {
            if (peek(l) == '\\' && peek_next(l)) adv(l);  // Skip escape sequence
            adv(l);
        }
        if (peek(l) != '"') {
            fprintf(stderr, "%s:%d:%d: error: unterminated string literal\n", g_src_file, tok_line, tok_col); print_source_line(tok_line, tok_col);
            exit(1);
        }
        adv(l);  // Consume closing quote
        return (Tok){T_STR, st, l->cur - st, tok_line, tok_col};
    }

    if (c == '(') return (Tok){T_LPAREN, st, 1, tok_line, tok_col};
    if (c == ')') return (Tok){T_RPAREN, st, 1, tok_line, tok_col};
    if (c == '{') return (Tok){T_LBRACE, st, 1, tok_line, tok_col};
    if (c == '}') return (Tok){T_RBRACE, st, 1, tok_line, tok_col};
    if (c == '[') return (Tok){T_LBRACKET, st, 1, tok_line, tok_col};
    if (c == ']') return (Tok){T_RBRACKET, st, 1, tok_line, tok_col};
    if (c == ';') return (Tok){T_SEMI, st, 1, tok_line, tok_col};
    if (c == ':') return (Tok){T_COLON, st, 1, tok_line, tok_col};
    if (c == ',') return (Tok){T_COMMA, st, 1, tok_line, tok_col};
    if (c == '.') return (Tok){T_DOT, st, 1, tok_line, tok_col};
    if (c == '@') return (Tok){T_AT, st, 1, tok_line, tok_col};
    if (c == '&' && peek(l) == '&') { adv(l); return (Tok){T_AND_AND, st, 2, tok_line, tok_col}; }
    if (c == '&') return (Tok){T_AMP, st, 1, tok_line, tok_col};
    if (c == '|' && peek(l) == '|') { adv(l); return (Tok){T_OR_OR, st, 2, tok_line, tok_col}; }
    if (c == '|') return (Tok){T_PIPE, st, 1, tok_line, tok_col};
    if (c == '^') return (Tok){T_CARET, st, 1, tok_line, tok_col};
    if (c == '~') return (Tok){T_TILDE, st, 1, tok_line, tok_col};
    if (c == '<' && peek(l) == '<') { adv(l); return (Tok){T_SHL, st, 2, tok_line, tok_col}; }
    if (c == '>' && peek(l) == '>') { adv(l); return (Tok){T_SHR, st, 2, tok_line, tok_col}; }
    if (c == '+' && peek(l) == '+') { adv(l); return (Tok){T_PLUSPLUS, st, 2, tok_line, tok_col}; }
    if (c == '+' && peek(l) == '=') { adv(l); return (Tok){T_PLUSEQ, st, 2, tok_line, tok_col}; }
    if (c == '+') return (Tok){T_PLUS, st, 1, tok_line, tok_col};
    if (c == '-' && peek(l) == '-') { adv(l); return (Tok){T_MINUSMINUS, st, 2, tok_line, tok_col}; }
    if (c == '-' && peek(l) == '=') { adv(l); return (Tok){T_MINUSEQ, st, 2, tok_line, tok_col}; }
    if (c == '*' && peek(l) == '=') { adv(l); return (Tok){T_STAREQ, st, 2, tok_line, tok_col}; }
    if (c == '*') return (Tok){T_STAR, st, 1, tok_line, tok_col};
    if (c == '/' && peek(l) == '=') { adv(l); return (Tok){T_SLASHEQ, st, 2, tok_line, tok_col}; }
    if (c == '/') return (Tok){T_SLASH, st, 1, tok_line, tok_col};
    if (c == '%' && peek(l) == '=') { adv(l); return (Tok){T_MODEQ, st, 2, tok_line, tok_col}; }
    if (c == '%') return (Tok){T_MOD, st, 1, tok_line, tok_col};
    if (c == '=' && peek(l) == '=') { adv(l); return (Tok){T_EQEQ, st, 2, tok_line, tok_col}; }
    if (c == '=') return (Tok){T_EQ, st, 1, tok_line, tok_col};
    if (c == '!' && peek(l) == '=') { adv(l); return (Tok){T_NEQ, st, 2, tok_line, tok_col}; }
    if (c == '!') return (Tok){T_BANG, st, 1, tok_line, tok_col};
    if (c == '<' && peek(l) == '=') { adv(l); return (Tok){T_LTE, st, 2, tok_line, tok_col}; }
    if (c == '<') return (Tok){T_LT, st, 1, tok_line, tok_col};
    if (c == '>' && peek(l) == '=') { adv(l); return (Tok){T_GTE, st, 2, tok_line, tok_col}; }
    if (c == '>') return (Tok){T_GT, st, 1, tok_line, tok_col};
    if (c == '-' && peek(l) == '>') { adv(l); return (Tok){T_ARROW, st, 2, tok_line, tok_col}; }
    if (c == '-') return (Tok){T_MINUS, st, 1, tok_line, tok_col};

    fprintf(stderr, "%s:%d:%d: error: unexpected character '%c'\n", g_src_file, tok_line, tok_col, c); print_source_line(tok_line, tok_col);
    exit(1);
}

Tok* tokenize(char* src, int* count) {
    Lex l; lex_init(&l, src);
    int cap = 2000;  // Initial capacity
    Tok* toks = malloc(sizeof(Tok) * cap);
    if (!toks) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    *count = 0;
    do {
        if (*count >= cap) {
            // Grow array dynamically
            cap *= 2;
            Tok* new_toks = safe_realloc(toks, sizeof(Tok) * cap);
            if (!new_toks) {
                fprintf(stderr, "Out of memory\n");
                free(toks);
                exit(1);
            }
            toks = new_toks;
        }
        toks[(*count)++] = lex_tok(&l);
    } while (toks[*count - 1].t != T_EOF);
    return toks;
}

// ==== PARSER ====
AstNode* ast_new(AstType type) {
    AstNode* n = calloc(1, sizeof(AstNode));
    n->type = type;
    n->line = g_cur_line;
    n->col = g_cur_col;
    n->file = g_src_file;
    return n;
}

void ast_add(AstNode* p, AstNode* c) {
    p->child_count++;
    p->children = safe_realloc(p->children, sizeof(AstNode*) * p->child_count);
    p->children[p->child_count - 1] = c;
}

// Token type to string for error messages
const char* tok_name(TokType t) {
    switch(t) {
        case T_EOF: return "end of file";
        case T_IDENT: return "identifier";
        case T_NUM: return "number";
        case T_STR: return "string";
        case T_FN: return "'fn'";
        case T_LET: return "'let'";
        case T_IF: return "'if'";
        case T_ELSE: return "'else'";
        case T_FOR: return "'for'";
        case T_WHILE: return "'while'";
        case T_RET: return "'return'";
        case T_STRUCT: return "'struct'";
        case T_LPAREN: return "'('";
        case T_RPAREN: return "')'";
        case T_LBRACE: return "'{'";
        case T_RBRACE: return "'}'";
        case T_LBRACKET: return "'['";
        case T_RBRACKET: return "']'";
        case T_SEMI: return "';'";
        case T_COLON: return "':'";
        case T_COMMA: return "','";
        case T_AT: return "'@'";
        case T_ARROW: return "'->'";
        default: return "token";
    }
}

Tok peek_tok(Parser* p) { return p->tokens[p->pos]; }
Tok advance_tok(Parser* p) {
    Tok t = p->tokens[p->pos++];
    g_cur_line = t.line; g_cur_col = t.col;
    return t;
}
int check_tok(Parser* p, TokType t) { return peek_tok(p).t == t; }
int match_tok(Parser* p, TokType t) { if (check_tok(p, t)) { advance_tok(p); return 1; } return 0; }

static int peek_word(Parser* p, const char* s);
static int peek_tok_at(Parser* p, int ahead, TokType t);

void expect(Parser* p, TokType t) {
    if (!match_tok(p, t)) {
        Tok got = peek_tok(p);
        if (t == T_SEMI && p->pos > 0) {
            // Point just after the previous token: that is where ';' is missing
            Tok prev = p->tokens[p->pos - 1];
            diag_error(prev.line, prev.col + prev.len, "expected ';' after this, found '%.*s'",
                       got.len, got.s);
        } else {
            diag_error(got.line, got.col, "expected %s, found '%.*s'", tok_name(t), got.len, got.s);
        }
        exit(1);
    }
}

// ==== OPTIMIZATION HELPERS ====

// Check if both operands are compile-time constants
int can_fold_constants(AstNode* left, AstNode* right) {
    if (!left || !right) return 0;
    return (left->type == AST_NUMBER && right->type == AST_NUMBER);
}

// Fold binary operation at compile time
AstNode* fold_binary_op(AstNode* left, AstNode* right, char* op) {
    if (!can_fold_constants(left, right)) return NULL;

    long left_val = atol(left->value);
    long right_val = atol(right->value);
    long result = 0;

    if (op[0] == '+') result = left_val + right_val;
    else if (op[0] == '-') result = left_val - right_val;
    else if (op[0] == '*') result = left_val * right_val;
    else if (op[0] == '/' && right_val != 0) result = left_val / right_val;
    else if (op[0] == '%' && right_val != 0) result = left_val % right_val;
    else return NULL;  // Don't fold division by zero

    AstNode* folded = ast_new(AST_NUMBER);
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    folded->value = strdup(buf);

    return folded;
}

// Check if a number is a power of 2
int is_power_of_2(long n) {
    return n > 0 && (n & (n - 1)) == 0;
}

// Get log2 of a power of 2
int get_log2(long n) {
    int log = 0;
    while (n > 1) {
        n >>= 1;
        log++;
    }
    return log;
}

// Scale an index that is already in reg. Hosted -O2 uses shl for a power of
// two (sal is the same opcode). -O0, freestanding, and bits 16/32 keep imul.
static void emit_scale_reg(Codegen* cg, const char* reg, int stride) {
    int sh;
    if (stride <= 1 || !reg) return;
    if (o2_hosted() && is_power_of_2((long)stride)) {
        sh = get_log2((long)stride);
        if (sh > 0 && sh <= 63) {
            emit(cg, "    shl %s, %d\n", reg, sh);
            return;
        }
    }
    emit(cg, "    imul %s, %d\n", reg, stride);
}

AstNode* parse_expr(Parser* p);
AstNode* parse_stmt(Parser* p);
TypeSpec parse_type(Parser* p);
static int peek_word(Parser* p, const char* s);
static int at_type_start(Parser* p);
static void error_expected_type(Parser* p);

// Turn 42, 1_000, 0xFF, 0b1010, 'A', '\n' into a decimal string
char* number_literal_to_decimal(Tok t) {
    char buf[64];
    long long v = 0;
    if (t.s[0] == '\'') {
        char c = t.s[1];
        if (c == '\\') {
            switch (t.s[2]) {
                case 'n': v = 10; break; case 't': v = 9; break; case 'r': v = 13; break;
                case '0': v = 0; break; case 'b': v = 8; break; default: v = (unsigned char)t.s[2];
            }
        } else v = (unsigned char)c;
    } else {
        char digits[64]; int k = 0;
        for (int i = 0; i < t.len && k < 63; i++) if (t.s[i] != '_') digits[k++] = t.s[i];
        digits[k] = 0;
        if (k > 2 && digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B'))
            v = (long long)strtoull(digits + 2, NULL, 2);
        else
            v = (long long)strtoull(digits, NULL, 0);
    }
    snprintf(buf, sizeof(buf), "%lld", v);
    return strdup(buf);
}

// Decode escape sequences in a string literal body: \n \t \r \0 \\ \" \'
static char* decode_string_literal(const char* s, int len) {
    char* out = malloc(len + 1);
    int o = 0;
    for (int i = 0; i < len; i++) {
        if (s[i] == '\\' && i + 1 < len) {
            char e = s[++i];
            switch (e) {
                case 'n': out[o++] = '\n'; break;
                case 't': out[o++] = '\t'; break;
                case 'r': out[o++] = '\r'; break;
                case '0': out[o++] = '\0'; break;
                case '\\': out[o++] = '\\'; break;
                case '"': out[o++] = '"'; break;
                case '\'': out[o++] = '\''; break;
                default:
                    diag_error(g_cur_line, g_cur_col, "unknown escape sequence '\\%c' in string", e);
                    out[o++] = e;
            }
        } else out[o++] = s[i];
    }
    out[o] = '\0';
    return out;
}

AstNode* parse_primary(Parser* p) {
    if (check_tok(p, T_NUM)) {
        Tok t = advance_tok(p);
        AstNode* n = ast_new(AST_NUMBER);
        n->value = number_literal_to_decimal(t);
        return n;
    }
    if (check_tok(p, T_STR)) {
        Tok t = advance_tok(p);
        AstNode* n = ast_new(AST_STRING);
        n->value = decode_string_literal(t.s + 1, t.len - 2);
        return n;
    }
    if (check_tok(p, T_LBRACKET)) {
        // Array literal
        advance_tok(p);
        AstNode* arr = ast_new(AST_ARRAY_LITERAL);
        while (!check_tok(p, T_RBRACKET)) {
            ast_add(arr, parse_expr(p));
            if (!check_tok(p, T_RBRACKET)) expect(p, T_COMMA);
        }
        expect(p, T_RBRACKET);
        arr->array_size = arr->child_count;
        return arr;
    }
    if (check_tok(p, T_IDENT)) {
        Tok t = advance_tok(p);

        // Struct literal
        if (check_tok(p, T_LBRACE)) {
            advance_tok(p);
            AstNode* struct_lit = ast_new(AST_STRUCT_LITERAL);
            struct_lit->struct_type = strndup(t.s, t.len);

            while (!check_tok(p, T_RBRACE)) {
                Tok field_name = advance_tok(p);
                expect(p, T_COLON);
                AstNode* field_val = parse_expr(p);

                AstNode* field = ast_new(AST_IDENT);
                field->name = strndup(field_name.s, field_name.len);
                ast_add(field, field_val);
                ast_add(struct_lit, field);

                if (!check_tok(p, T_RBRACE)) expect(p, T_COMMA);
            }
            expect(p, T_RBRACE);
            return struct_lit;
        }

        // Function call
        if (check_tok(p, T_LPAREN)) {
            AstNode* call = ast_new(AST_CALL);
            call->name = strndup(t.s, t.len);
            advance_tok(p);
            while (!check_tok(p, T_RPAREN)) {
                ast_add(call, parse_expr(p));
                if (!check_tok(p, T_RPAREN)) expect(p, T_COMMA);
            }
            expect(p, T_RPAREN);
            return call;
        }

        // Assignment. Suppressed while parsing a dereference operand so
        // `*p = v` is a store through p, not `*(p = v)`.
        if (!g_no_eq && check_tok(p, T_EQ)) {
            advance_tok(p);
            AstNode* assign = ast_new(AST_ASSIGN);
            assign->name = strndup(t.s, t.len);
            ast_add(assign, parse_expr(p));
            return assign;
        }

        // Identifier
        AstNode* n = ast_new(AST_IDENT);
        n->name = strndup(t.s, t.len);
        return n;
    }
    if (match_tok(p, T_LPAREN)) {
        AstNode* n = parse_expr(p);
        expect(p, T_RPAREN);
        return n;
    }
    Tok err = peek_tok(p);
    if (err.t == T_EOF) diag_error(err.line, err.col, "unexpected end of file, expected an expression");
    else diag_error(err.line, err.col, "expected an expression, found '%.*s'", err.len, err.s);
    exit(1);
}

// Forward declarations
AstNode* parse_postfix(Parser* p);

// Parse unary operators: &x, *ptr
AstNode* parse_unary(Parser* p) {
    if (check_tok(p, T_MINUS)) {
        // Unary minus: -expr
        advance_tok(p);
        AstNode* neg = ast_new(AST_UNARY);
        neg->op = strdup("-");
        ast_add(neg, parse_unary(p));  // Allow chaining
        return neg;
    }
    if (check_tok(p, T_BANG)) {
        // Logical NOT: !x
        advance_tok(p);
        AstNode* not_node = ast_new(AST_UNARY);
        not_node->op = strdup("!");
        ast_add(not_node, parse_unary(p));  // Allow chaining (!(!x))
        return not_node;
    }
    if (check_tok(p, T_TILDE)) {
        // Bitwise NOT: ~x
        advance_tok(p);
        AstNode* not_node = ast_new(AST_UNARY);
        not_node->op = strdup("~");
        ast_add(not_node, parse_unary(p));
        return not_node;
    }
    if (check_tok(p, T_AMP)) {
        // Address-of: &variable
        advance_tok(p);
        AstNode* addr_of = ast_new(AST_ADDR_OF);
        ast_add(addr_of, parse_unary(p));  // Allow chaining
        return addr_of;
    }
    if (check_tok(p, T_STAR)) {
        // Dereference: *ptr (need to distinguish from multiplication)
        // Look ahead: if next is identifier/lparen, it's dereference
        int saved_pos = p->pos;
        advance_tok(p);
        if (check_tok(p, T_IDENT) || check_tok(p, T_LPAREN) ||
            check_tok(p, T_STAR) || check_tok(p, T_AMP)) {
            // It's dereference
            AstNode* deref = ast_new(AST_DEREF);
            int saved_eq = g_no_eq;
            g_no_eq = 1;
            ast_add(deref, parse_unary(p));
            g_no_eq = saved_eq;
            return deref;
        } else {
            // It's multiplication, backtrack
            p->pos = saved_pos;
            return parse_primary(p);
        }
    }
    return parse_primary(p);
}

// Parse postfix base (handles unary in postfix context)
AstNode* parse_postfix_base(Parser* p) {
    if (check_tok(p, T_AMP)) {
        // Address-of: &variable or &array[index] or &struct.field
        advance_tok(p);
        AstNode* addr_of = ast_new(AST_ADDR_OF);
        ast_add(addr_of, parse_postfix(p));  // Recursive to handle postfix ops
        return addr_of;
    }
    if (check_tok(p, T_STAR)) {
        // Dereference: *ptr
        int saved_pos = p->pos;
        advance_tok(p);
        if (check_tok(p, T_IDENT) || check_tok(p, T_LPAREN) ||
            check_tok(p, T_STAR) || check_tok(p, T_AMP)) {
            AstNode* deref = ast_new(AST_DEREF);
            int saved_eq = g_no_eq;
            g_no_eq = 1;
            ast_add(deref, parse_postfix(p));  // Recursive to handle postfix ops
            g_no_eq = saved_eq;
            return deref;
        } else {
            p->pos = saved_pos;
            return parse_primary(p);
        }
    }
    return parse_unary(p);
}

AstNode* parse_postfix(Parser* p) {
    AstNode* left = parse_postfix_base(p);
    while (1) {
        if (check_tok(p, T_LBRACKET)) {
            // Array indexing
            advance_tok(p);
            AstNode* index_node = ast_new(AST_INDEX);
            ast_add(index_node, left);
            ast_add(index_node, parse_expr(p));
            expect(p, T_RBRACKET);
            left = index_node;
        } else if (check_tok(p, T_ARROW)) {
            // Arrow operator: ptr->field
            advance_tok(p);
            Tok field = advance_tok(p);
            // Desugar: ptr->field becomes (*ptr).field
            AstNode* deref = ast_new(AST_DEREF);
            ast_add(deref, left);
            AstNode* field_node = ast_new(AST_FIELD_ACCESS);
            field_node->name = strndup(field.s, field.len);
            ast_add(field_node, deref);
            left = field_node;
        } else if (check_tok(p, T_DOT)) {
            // Field access
            advance_tok(p);
            Tok field = advance_tok(p);
            AstNode* field_node = ast_new(AST_FIELD_ACCESS);
            field_node->name = strndup(field.s, field.len);
            ast_add(field_node, left);
            left = field_node;
        } else {
            break;
        }
    }
    return left;
}

// `as` is a contextual postfix word, not a keyword (a variable may be named as).
// It binds looser than unary `&`/`*` and postfix `[]` `.` `->`, and tighter
// than `*` `/` `+`, so `&e as *u8` is `(&e) as *u8` and `1 + 2 as u8` is `1 + (2 as u8)`.
AstNode* parse_cast(Parser* p) {
    AstNode* left = parse_postfix(p);
    while (peek_word(p, "as")) {
        advance_tok(p);
        if (!at_type_start(p)) error_expected_type(p);
        TypeSpec spec = parse_type(p);
        AstNode* cast = ast_new(AST_CAST);
        cast->value = spec.base_type;
        cast->is_pointer = spec.is_pointer | (spec.is_mutable << 1) | (spec.is_volatile << 2);
        cast->array_size = spec.array_count;
        if (spec.is_array) cast->struct_type = strdup("__array__");
        ast_add(cast, left);
        left = cast;
    }
    return left;
}

AstNode* parse_multiplicative(Parser* p) {
    AstNode* left = parse_cast(p);
    while (check_tok(p, T_STAR) || check_tok(p, T_SLASH) || check_tok(p, T_MOD)) {
        Tok op = advance_tok(p);
        AstNode* right = parse_cast(p);

        // Constant folding optimization (O1+)
        if (optimization_level >= 1) {
            char op_str[2] = {op.s[0], '\0'};
            AstNode* folded = fold_binary_op(left, right, op_str);
            if (folded) {
                left = folded;
                continue;
            }
        }

        AstNode* binop = ast_new(AST_BINOP);
        binop->op = strndup(op.s, op.len);
        ast_add(binop, left);
        ast_add(binop, right);
        left = binop;
    }
    return left;
}

AstNode* parse_additive(Parser* p) {
    AstNode* left = parse_multiplicative(p);
    while (check_tok(p, T_PLUS) || check_tok(p, T_MINUS)) {
        Tok op = advance_tok(p);
        AstNode* right = parse_multiplicative(p);

        // Constant folding optimization (O1+)
        if (optimization_level >= 1) {
            char op_str[2] = {op.s[0], '\0'};
            AstNode* folded = fold_binary_op(left, right, op_str);
            if (folded) {
                left = folded;
                continue;
            }
        }

        AstNode* binop = ast_new(AST_BINOP);
        binop->op = strndup(op.s, op.len);
        ast_add(binop, left);
        ast_add(binop, right);
        left = binop;
    }
    return left;
}

AstNode* make_binop(Tok op, AstNode* l, AstNode* r) {
    AstNode* b = ast_new(AST_BINOP);
    b->op = strndup(op.s, op.len);
    ast_add(b, l); ast_add(b, r);
    return b;
}
AstNode* parse_shift(Parser* p) {
    AstNode* left = parse_additive(p);
    while (check_tok(p, T_SHL) || check_tok(p, T_SHR)) {
        Tok op = advance_tok(p);
        left = make_binop(op, left, parse_additive(p));
    }
    return left;
}
AstNode* parse_bitand(Parser* p) {
    AstNode* left = parse_shift(p);
    while (check_tok(p, T_AMP)) {
        Tok op = advance_tok(p);
        left = make_binop(op, left, parse_shift(p));
    }
    return left;
}
AstNode* parse_bitxor(Parser* p) {
    AstNode* left = parse_bitand(p);
    while (check_tok(p, T_CARET)) {
        Tok op = advance_tok(p);
        left = make_binop(op, left, parse_bitand(p));
    }
    return left;
}
AstNode* parse_bitor(Parser* p) {
    AstNode* left = parse_bitxor(p);
    while (check_tok(p, T_PIPE)) {
        Tok op = advance_tok(p);
        left = make_binop(op, left, parse_bitxor(p));
    }
    return left;
}

AstNode* parse_comparison(Parser* p) {
    AstNode* left = parse_bitor(p);
    while (check_tok(p, T_EQEQ) || check_tok(p, T_NEQ) ||
           check_tok(p, T_LT) || check_tok(p, T_GT) ||
           check_tok(p, T_LTE) || check_tok(p, T_GTE)) {
        Tok op = advance_tok(p);
        AstNode* right = parse_bitor(p);
        AstNode* cmp = ast_new(AST_COMPARE);
        cmp->op = strndup(op.s, op.len);
        ast_add(cmp, left);
        ast_add(cmp, right);
        left = cmp;
    }
    return left;
}

AstNode* parse_logical_and(Parser* p) {
    AstNode* left = parse_comparison(p);
    while (check_tok(p, T_AND_AND)) {
        advance_tok(p);  // consume &&
        AstNode* right = parse_comparison(p);
        AstNode* logical = ast_new(AST_LOGICAL);
        logical->op = strdup("&&");
        ast_add(logical, left);
        ast_add(logical, right);
        left = logical;
    }
    return left;
}

AstNode* parse_logical_or(Parser* p) {
    AstNode* left = parse_logical_and(p);
    while (check_tok(p, T_OR_OR)) {
        advance_tok(p);  // consume ||
        AstNode* right = parse_logical_and(p);
        AstNode* logical = ast_new(AST_LOGICAL);
        logical->op = strdup("||");
        ast_add(logical, left);
        ast_add(logical, right);
        left = logical;
    }
    return left;
}

AstNode* parse_expr(Parser* p) {
    return parse_logical_or(p);
}

AstNode* parse_block(Parser* p);
static int parse_align_prefix(Parser* p);

static void end_simple_stmt(Parser* p) { if (!g_no_semi) expect(p, T_SEMI); }

AstNode* parse_stmt(Parser* p) {
    if (peek_word(p, "align") && peek_tok_at(p, 1, T_LPAREN)) {
        Tok kw = peek_tok(p);
        diag_error(kw.line, kw.col, "align() applies to a struct, a field, or a global");
        parse_align_prefix(p);
    }
    if (check_tok(p, T_LBRACE)) return parse_block(p);
    if (match_tok(p, T_RET)) {
        AstNode* ret = ast_new(AST_RETURN);
        if (!check_tok(p, T_SEMI)) ast_add(ret, parse_expr(p));
        expect(p, T_SEMI);
        return ret;
    }
    if (match_tok(p, T_LET)) {
        int is_volatile = 0;
        if (peek_word(p, "volatile")) {
            advance_tok(p);
            is_volatile = 1;
        }
        Tok name = advance_tok(p);
        AstNode* let = ast_new(AST_LET);
        let->name = strndup(name.s, name.len);
        if (is_volatile) let->flags |= AF_VOLATILE;

        // Optional type annotation: let x: i32 or let arr: [i32; 10] or let ptr: *i32 or let p: Point
        if (match_tok(p, T_COLON)) {
            TypeSpec spec = parse_type(p);

            // Store type info in AST node (similar to global vars)
            let->value = spec.base_type;          // Base type name
            let->array_size = spec.array_count;   // Array count (0 if not array)
            let->is_pointer = spec.is_pointer | (spec.is_mutable << 1) | (spec.is_volatile << 2);

            // For arrays, mark as array in struct_type field
            if (spec.is_array) {
                let->struct_type = strdup("__array__");  // Marker for array
            } else if (!spec.is_pointer && spec.base_type) {
                // Check if it's a known struct type (not a primitive)
                // We'll store the type name and codegen will check if it's a struct
                let->struct_type = strdup(spec.base_type);  // Store potential struct name
            }
        }

        if (match_tok(p, T_EQ)) ast_add(let, parse_expr(p));
        expect(p, T_SEMI);
        return let;
    }
    if (match_tok(p, T_IF)) {
        AstNode* ifnode = ast_new(AST_IF);
        // `if carry { }` reads CF (setc / jnc). `if (carry)` stays a normal call.
        if (peek_word(p, "carry") && !peek_tok_at(p, 1, T_LPAREN)) {
            advance_tok(p);
            ifnode->flags |= AF_CARRY;
            ast_add(ifnode, parse_block(p));
            if (match_tok(p, T_ELSE)) {
                if (check_tok(p, T_IF)) {
                    AstNode* else_block = ast_new(AST_BLOCK);
                    ast_add(else_block, parse_stmt(p));
                    ast_add(ifnode, else_block);
                } else {
                    ast_add(ifnode, parse_block(p));
                }
            }
            return ifnode;
        }
        expect(p, T_LPAREN);
        ast_add(ifnode, parse_expr(p));
        expect(p, T_RPAREN);
        ast_add(ifnode, parse_block(p));
        if (match_tok(p, T_ELSE)) {
            if (check_tok(p, T_IF)) {
                // else if: the nested if becomes the only statement of the else block
                AstNode* else_block = ast_new(AST_BLOCK);
                ast_add(else_block, parse_stmt(p));
                ast_add(ifnode, else_block);
            } else {
                ast_add(ifnode, parse_block(p));
            }
        }
        return ifnode;
    }
    if (match_tok(p, T_WHILE)) {
        AstNode* whilenode = ast_new(AST_WHILE);
        expect(p, T_LPAREN);
        ast_add(whilenode, parse_expr(p));
        expect(p, T_RPAREN);
        ast_add(whilenode, parse_block(p));
        return whilenode;
    }
    if (match_tok(p, T_FOR)) {
        // for (init; cond; inc) body → { init; while (cond) { body; inc; } }
        expect(p, T_LPAREN);

        // Parse init statement (usually let i = 0)
        AstNode* init = parse_stmt(p);

        // Parse condition (i < 10)
        AstNode* cond = parse_expr(p);
        expect(p, T_SEMI);

        // Parse increment: any simple statement without ';' (i++, i += 2, i = i + 1)
        g_no_semi++;
        AstNode* inc = parse_stmt(p);
        g_no_semi--;
        expect(p, T_RPAREN);

        // Parse body
        AstNode* body = parse_block(p);

        // Desugar to: { init; while (cond) { body; inc; } }
        AstNode* block = ast_new(AST_BLOCK);
        ast_add(block, init);

        AstNode* whilenode = ast_new(AST_WHILE);
        ast_add(whilenode, cond);

        // children: cond, body, increment (run at the 'continue' point)
        ast_add(whilenode, body);
        AstNode* inc_block = ast_new(AST_BLOCK);
        ast_add(inc_block, inc);
        ast_add(whilenode, inc_block);
        ast_add(block, whilenode);

        return block;
    }
    // `label name;` is a code address inside a function. `label()` stays a call.
    if (peek_word(p, "label") && peek_tok_at(p, 1, T_IDENT) && peek_tok_at(p, 2, T_SEMI)) {
        Tok kw = advance_tok(p);
        Tok name = advance_tok(p);
        expect(p, T_SEMI);
        AstNode* lab = ast_new(AST_LABEL);
        lab->name = strndup(name.s, name.len);
        lab->line = kw.line;
        lab->col = kw.col;
        return lab;
    }
    if (check_tok(p, T_IDENT)) {
        Tok t = peek_tok(p);
        int is_break = (t.len == 5 && !memcmp(t.s, "break", 5));
        int is_cont = (t.len == 8 && !memcmp(t.s, "continue", 8));
        if (is_break || is_cont) {
            advance_tok(p);
            AstNode* jn = ast_new(is_break ? AST_BREAK : AST_CONTINUE);
            end_simple_stmt(p);
            return jn;
        }
        // `jmp 0x8200`, `jmp other`, `jmp reg`, and `jmp [name]`.
        // `jmp(...)` stays a call.
        if (t.len == 3 && !memcmp(t.s, "jmp", 3) &&
            (peek_tok_at(p, 1, T_NUM) ||
             peek_tok_at(p, 1, T_LBRACKET) ||
             (peek_tok_at(p, 1, T_IDENT) && !peek_tok_at(p, 2, T_LPAREN) && !peek_tok_at(p, 2, T_EQ)))) {
            advance_tok(p);
            AstNode* jp = ast_new(AST_JMP);
            if (check_tok(p, T_LBRACKET)) {
                advance_tok(p);
                jp->flags |= AF_JMP_MEM;
                if (!check_tok(p, T_IDENT)) {
                    Tok bad = peek_tok(p);
                    diag_error(bad.line, bad.col, "jmp [name] takes a variable or a data label");
                } else {
                    Tok id = advance_tok(p);
                    jp->name = strndup(id.s, id.len);
                }
                expect(p, T_RBRACKET);
            } else if (check_tok(p, T_NUM)) {
                Tok num = advance_tok(p);
                jp->value = strndup(num.s, num.len);
            } else if (check_tok(p, T_IDENT)) {
                Tok id = advance_tok(p);
                jp->name = strndup(id.s, id.len);
            } else {
                Tok bad = peek_tok(p);
                diag_error(bad.line, bad.col, "jmp takes an integer address or a function name");
            }
            end_simple_stmt(p);
            return jp;
        }
    }
    AstNode* expr = parse_expr(p);

    // Check for increment/decrement operators: x++ or x--
    if (expr && expr->type == AST_IDENT) {
        if (check_tok(p, T_PLUSPLUS)) {
            advance_tok(p);  // Consume '++'
            // Desugar: x++ => x = x + 1
            AstNode* assign = ast_new(AST_ASSIGN);
            assign->name = strdup(expr->name);
            AstNode* add = ast_new(AST_BINOP);
            add->op = strdup("+");
            AstNode* ident = ast_new(AST_IDENT);
            ident->name = strdup(expr->name);
            ast_add(add, ident);
            AstNode* one = ast_new(AST_NUMBER);
            one->value = strdup("1");
            ast_add(add, one);
            ast_add(assign, add);
            end_simple_stmt(p);
            return assign;
        }
        if (check_tok(p, T_MINUSMINUS)) {
            advance_tok(p);  // Consume '--'
            // Desugar: x-- => x = x - 1
            AstNode* assign = ast_new(AST_ASSIGN);
            assign->name = strdup(expr->name);
            AstNode* sub = ast_new(AST_BINOP);
            sub->op = strdup("-");
            AstNode* ident = ast_new(AST_IDENT);
            ident->name = strdup(expr->name);
            ast_add(sub, ident);
            AstNode* one = ast_new(AST_NUMBER);
            one->value = strdup("1");
            ast_add(sub, one);
            ast_add(assign, sub);
            end_simple_stmt(p);
            return assign;
        }
        // Check for compound assignment operators: +=, -=, *=, /=, %=
        if (check_tok(p, T_PLUSEQ) || check_tok(p, T_MINUSEQ) ||
            check_tok(p, T_STAREQ) || check_tok(p, T_SLASHEQ) || check_tok(p, T_MODEQ)) {

            char* op = NULL;
            if (check_tok(p, T_PLUSEQ)) { op = "+"; }
            else if (check_tok(p, T_MINUSEQ)) { op = "-"; }
            else if (check_tok(p, T_STAREQ)) { op = "*"; }
            else if (check_tok(p, T_SLASHEQ)) { op = "/"; }
            else if (check_tok(p, T_MODEQ)) { op = "%"; }

            advance_tok(p);  // Consume compound assignment operator

            // Desugar: x += expr => x = x + expr
            AstNode* assign = ast_new(AST_ASSIGN);
            assign->name = strdup(expr->name);
            AstNode* binop = ast_new(AST_BINOP);
            binop->op = strdup(op);
            AstNode* ident = ast_new(AST_IDENT);
            ident->name = strdup(expr->name);
            ast_add(binop, ident);
            ast_add(binop, parse_expr(p));  // Right-hand side
            ast_add(assign, binop);
            end_simple_stmt(p);
            return assign;
        }
    }

    // Check for array assignment: array[index] = value
    if (expr && expr->type == AST_INDEX && check_tok(p, T_EQ)) {
        advance_tok(p);  // Consume '='
        AstNode* array_assign = ast_new(AST_ARRAY_ASSIGN);
        // children[0] = array base (from INDEX)
        // children[1] = index expression (from INDEX)
        // children[2] = value expression
        ast_add(array_assign, expr->children[0]);  // array
        ast_add(array_assign, expr->children[1]);  // index
        ast_add(array_assign, parse_expr(p));      // value
        end_simple_stmt(p);
        return array_assign;
    }

    // *p = value, for a named pointer. Same store as p[0] = value, so a
    // *volatile pointee keeps its width. (Historical *p reads of a non-volatile
    // pointer stay 8 bytes; this write uses the pointee width.)
    if (expr && expr->type == AST_DEREF && expr->child_count == 1 &&
        expr->children[0] && expr->children[0]->type == AST_IDENT && check_tok(p, T_EQ)) {
        advance_tok(p);
        AstNode* array_assign = ast_new(AST_ARRAY_ASSIGN);
        AstNode* zero = ast_new(AST_NUMBER);
        zero->value = strdup("0");
        ast_add(array_assign, expr->children[0]);
        ast_add(array_assign, zero);
        ast_add(array_assign, parse_expr(p));
        end_simple_stmt(p);
        return array_assign;
    }

    // *(addr) = value. addr may be a cast (`0x1000 as *u32`) or `rbp + 136`.
    if (expr && expr->type == AST_DEREF && expr->child_count == 1 && check_tok(p, T_EQ)) {
        advance_tok(p);
        AstNode* st = ast_new(AST_MEM_STORE);
        ast_add(st, expr);
        ast_add(st, parse_expr(p));
        end_simple_stmt(p);
        return st;
    }

    // Check for field assignment: struct.field = value
    if (expr && expr->type == AST_FIELD_ACCESS && check_tok(p, T_EQ)) {
        advance_tok(p);  // Consume '='
        AstNode* field_assign = ast_new(AST_FIELD_ASSIGN);
        // name = field name
        // children[0] = struct object
        // children[1] = value expression
        field_assign->name = strdup(expr->name);   // Field name
        ast_add(field_assign, expr->children[0]);  // Struct object
        ast_add(field_assign, parse_expr(p));      // Value expression
        end_simple_stmt(p);
        return field_assign;
    }

    end_simple_stmt(p);
    return expr;
}

AstNode* parse_block(Parser* p) {
    expect(p, T_LBRACE);
    AstNode* block = ast_new(AST_BLOCK);
    while (!check_tok(p, T_RBRACE) && !check_tok(p, T_EOF)) {
        ast_add(block, parse_stmt(p));
    }
    expect(p, T_RBRACE);
    return block;
}

static int peek_word(Parser* p, const char* s) {
    Tok t = peek_tok(p);
    int n = (int)strlen(s);
    return t.t == T_IDENT && t.len == n && !memcmp(t.s, s, n);
}

static int peek_word_at(Parser* p, int ahead, const char* s) {
    if (p->pos + ahead >= p->count) return 0;
    Tok t = p->tokens[p->pos + ahead];
    int n = (int)strlen(s);
    return t.t == T_IDENT && t.len == n && !memcmp(t.s, s, n);
}

static int peek_tok_at(Parser* p, int ahead, TokType t) {
    return p->pos + ahead < p->count && p->tokens[p->pos + ahead].t == t;
}

static int tok_u64(Tok t, unsigned long long* out);

// align(N) is a power of two from 1 to 4096. Returns 0 when the source
// has no align clause. A bad N is diagnosed and also returns 0.
static int parse_align_prefix(Parser* p) {
    unsigned long long v = 0;
    Tok num;
    if (!(peek_word(p, "align") && peek_tok_at(p, 1, T_LPAREN))) return 0;
    advance_tok(p);
    expect(p, T_LPAREN);
    num = peek_tok(p);
    if (!tok_u64(num, &v) || v < 1 || v > 4096 || (v & (v - 1)) != 0) {
        diag_error(num.line, num.col, "align() takes a power of two from 1 to 4096");
        if (num.t != T_RPAREN && num.t != T_EOF && num.t != T_SEMI) advance_tok(p);
        v = 0;
    } else {
        advance_tok(p);
    }
    expect(p, T_RPAREN);
    return (int)v;
}

AstNode* parse_struct_def(Parser* p) {
    expect(p, T_STRUCT);
    Tok name = advance_tok(p);
    AstNode* struct_def = ast_new(AST_STRUCT_DEF);
    struct_def->name = strndup(name.s, name.len);

    expect(p, T_LBRACE);
    while (!check_tok(p, T_RBRACE)) {
        // `volatile name: T` mirrors `let volatile`. A field literally named
        // volatile is still `volatile: T` (the next token is ':', not a name).
        int field_vol = 0;
        int field_align = parse_align_prefix(p);
        if (peek_word(p, "volatile") && peek_tok_at(p, 1, T_IDENT)) {
            advance_tok(p);
            field_vol = 1;
        }
        Tok field_name = advance_tok(p);
        expect(p, T_COLON);

        // Parse field type (handles i32, *i8, [i32; 10], etc.)
        TypeSpec field_type = parse_type(p);

        AstNode* field = ast_new(AST_IDENT);
        field->name = strndup(field_name.s, field_name.len);
        field->value = field_type.base_type;      // Store type name
        field->is_pointer = field_type.is_pointer;  // Store if it's a pointer
        field->array_size = field_type.array_count;
        // The count has to survive into the type table. Dropping it is what
        // made [u8; 16] occupy one byte and the compiler still exit 0.
        if (field_type.is_array) field->struct_type = strdup("__array__");
        if (field_vol) field->flags |= AF_VOLATILE;
        field->align = field_align;
        ast_add(struct_def, field);

        if (!check_tok(p, T_RBRACE)) expect(p, T_COMMA);
    }
    expect(p, T_RBRACE);

    return struct_def;
}

static int at_type_start(Parser* p) {
    return check_tok(p, T_STAR) || check_tok(p, T_LBRACKET) || check_tok(p, T_IDENT);
}

static void error_expected_type(Parser* p) {
    Tok got = peek_tok(p);
    if (got.t == T_EOF)
        diag_error(got.line, got.col, "expected a type, found end of file");
    else
        diag_error(got.line, got.col, "expected a type, found '%.*s'", got.len, got.s);
    exit(1);
}

// A nested pointer or array, spelled back out so one TypeSpec level can hold it.
// A plain `*u8` does not use this: its base stays `u8`.
static char* type_spec_name(TypeSpec s) {
    char buf[256];
    if (s.is_array) {
        snprintf(buf, sizeof(buf), "[%s; %d]", s.base_type ? s.base_type : "i64", s.array_count);
        return strdup(buf);
    }
    if (s.is_pointer) {
        snprintf(buf, sizeof(buf), "*%s%s%s",
                 s.is_volatile ? "volatile " : "",
                 s.is_mutable ? "mut " : "",
                 s.base_type ? s.base_type : "");
        return strdup(buf);
    }
    return s.base_type ? strdup(s.base_type) : strdup("");
}

static char* pointee_name(TypeSpec inner) {
    if (inner.is_pointer || inner.is_array) return type_spec_name(inner);
    return inner.base_type;
}

// Parse type specification: i32, [i32; 10], *i32, *mut i32, **u8, Point (struct)
TypeSpec parse_type(Parser* p) {
    TypeSpec spec = {0};

    // Check for pointer: *T, *mut T, *volatile T, *volatile mut T, **T
    if (check_tok(p, T_STAR)) {
        advance_tok(p);
        spec.is_pointer = 1;

        if (peek_word(p, "volatile")) {
            advance_tok(p);
            spec.is_volatile = 1;
        }
        // Check for mut
        if (check_tok(p, T_MUT)) {
            advance_tok(p);
            spec.is_mutable = 1;
        }
        if (peek_word(p, "volatile")) {
            advance_tok(p);
            spec.is_volatile = 1;
        }

        if (!at_type_start(p)) error_expected_type(p);
        spec.base_type = pointee_name(parse_type(p));
        return spec;
    }

    // Check for array: [T; N], including [*u8; 4]
    if (check_tok(p, T_LBRACKET)) {
        advance_tok(p);
        spec.is_array = 1;

        if (!at_type_start(p)) error_expected_type(p);
        spec.base_type = pointee_name(parse_type(p));

        expect(p, T_SEMI);

        // Get array count
        Tok count = advance_tok(p);
        spec.array_count = atoi(count.s);

        expect(p, T_RBRACKET);
        return spec;
    }

    // Basic type or struct name (i32, i64, Point, Vector, etc.)
    if (!check_tok(p, T_IDENT)) error_expected_type(p);
    Tok type = advance_tok(p);
    spec.base_type = strndup(type.s, type.len);
    return spec;
}

AstNode* parse_global_var(Parser* p) {
    expect(p, T_LET);
    int is_volatile = 0;
    if (peek_word(p, "volatile")) {
        advance_tok(p);
        is_volatile = 1;
    }
    Tok name = advance_tok(p);

    AstNode* global_var = ast_new(AST_GLOBAL_VAR);
    global_var->name = strndup(name.s, name.len);
    if (is_volatile) global_var->flags |= AF_VOLATILE;

    // Optional type annotation: let x: i32 = ... or let arr: [i32; 10];
    if (match_tok(p, T_COLON)) {
        TypeSpec spec = parse_type(p);

        // Store type info in AST node (creative use of fields)
        global_var->value = spec.base_type;          // Base type
        global_var->array_size = spec.array_count;   // Array count
        global_var->is_pointer = spec.is_pointer | (spec.is_mutable << 1) | (spec.is_volatile << 2);
        // Note: is_pointer bit 0 = is_pointer, bit 1 = is_mutable, bit 2 = volatile pointee

        // For arrays, mark as array in struct_type field
        if (spec.is_array) {
            global_var->struct_type = strdup("__array__");  // Marker for array
        }
    }

    // Initialization value (optional for arrays)
    if (match_tok(p, T_EQ)) {
        ast_add(global_var, parse_expr(p));
    }

    expect(p, T_SEMI);
    return global_var;
}

AstNode* parse_func(Parser* p) {
    expect(p, T_FN);
    Tok name = advance_tok(p);
    AstNode* func = ast_new(AST_FUNCTION);
    func->name = strndup(name.s, name.len);

    expect(p, T_LPAREN);
    while (!check_tok(p, T_RPAREN)) {
        Tok param = advance_tok(p);
        AstNode* par = ast_new(AST_IDENT);
        par->name = strndup(param.s, param.len);
        ast_add(func, par);
        if (match_tok(p, T_COLON)) {
            // *T, *mut T, *volatile T, *volatile mut T, *mut volatile T
            if (check_tok(p, T_STAR)) {
                TypeSpec spec = parse_type(p);
                par->is_pointer = spec.is_pointer | (spec.is_mutable << 1) | (spec.is_volatile << 2);
                par->value = spec.base_type;
            } else {
                Tok type_tok = advance_tok(p);  // Get type
                par->value = strndup(type_tok.s, type_tok.len);  // Save type name
            }
        }
        if (!check_tok(p, T_RPAREN)) expect(p, T_COMMA);
    }
    expect(p, T_RPAREN);

    // The return type used to be a single token, so `-> *u8` ate `*` and then
    // rejected `u8`. An integer or pointer return is checked against `return`.
    // The value still comes back in rax.
    if (match_tok(p, T_ARROW)) {
        TypeSpec spec = parse_type(p);
        func->is_pointer = spec.is_pointer | (spec.is_mutable << 1) | (spec.is_volatile << 2);
        func->array_size = spec.array_count;
        if (spec.is_pointer || spec.is_array) func->value = type_spec_name(spec);
        else func->value = spec.base_type;
        if (spec.is_array) func->struct_type = strdup("__array__");
    }

    // Check if this is a forward declaration (ends with ;) or full definition (has body)
    if (check_tok(p, T_SEMI)) {
        // Forward declaration
        expect(p, T_SEMI);
        func->is_forward_decl = 1;
        // Add empty block as placeholder
        ast_add(func, ast_new(AST_BLOCK));
    } else {
        // Full function definition
        ast_add(func, parse_block(p));
    }
    return func;
}

static char* append_byte(char* buf, int* n, int cap, unsigned c, int* first) {
    char num[16];
    int k = snprintf(num, sizeof(num), "%s%u", *first ? "" : ", ", c);
    if (*n + k >= cap) return buf;
    memcpy(buf + *n, num, k);
    *n += k;
    buf[*n] = '\0';
    *first = 0;
    return buf;
}

// db/dw/dd/dq operands. A string expands to its bytes. Numbers pass through to NASM.
static char* parse_data_ops(Parser* p) {
    char buf[4096];
    int n = 0;
    int first = 1;
    buf[0] = '\0';
    if (check_tok(p, T_SEMI) || check_tok(p, T_RBRACE) || check_tok(p, T_EOF)) {
        diag_error(peek_tok(p).line, peek_tok(p).col, "data directive needs a value");
        return strdup("0");
    }
    while (!check_tok(p, T_SEMI) && !check_tok(p, T_RBRACE) && !check_tok(p, T_EOF)) {
        if (check_tok(p, T_STR)) {
            Tok s = advance_tok(p);
            const char* v = s.s;
            if (s.len < 2 || v[0] != '"') {
                diag_error(s.line, s.col, "malformed string in data block");
            } else {
                for (int i = 1; i < s.len - 1; i++) {
                    unsigned char c = (unsigned char)v[i];
                    if (c == '\\' && i + 1 < s.len - 1) {
                        char e = v[++i];
                        if (e == 'n') c = '\n';
                        else if (e == 't') c = '\t';
                        else if (e == 'r') c = '\r';
                        else if (e == '0') c = 0;
                        else c = (unsigned char)e;
                    }
                    append_byte(buf, &n, (int)sizeof(buf), c, &first);
                }
            }
        } else if (check_tok(p, T_NUM) || check_tok(p, T_IDENT)) {
            // `dd gdt` and `dq next + 3`. NASM evaluates the label. `*` is not
            // an operand separator, so `1 * 2` still fails at the semicolon.
            if (!first) {
                if (n + 2 < (int)sizeof(buf)) {
                    memcpy(buf + n, ", ", 2);
                    n += 2;
                    buf[n] = '\0';
                }
            }
            for (;;) {
                Tok t = advance_tok(p);
                if (n + t.len < (int)sizeof(buf)) {
                    memcpy(buf + n, t.s, t.len);
                    n += t.len;
                    buf[n] = '\0';
                }
                if (!check_tok(p, T_PLUS) && !check_tok(p, T_MINUS)) break;
                Tok op = advance_tok(p);
                const char* sep = op.t == T_MINUS ? " - " : " + ";
                if (n + 3 < (int)sizeof(buf)) {
                    memcpy(buf + n, sep, 3);
                    n += 3;
                    buf[n] = '\0';
                }
                if (!check_tok(p, T_NUM) && !check_tok(p, T_IDENT)) {
                    Tok bad = peek_tok(p);
                    diag_error(bad.line, bad.col, "expected a number or a label in a data block");
                    break;
                }
            }
            first = 0;
        } else {
            Tok bad = peek_tok(p);
            diag_error(bad.line, bad.col, "expected a number, a string, or a label in a data block");
            break;
        }
        if (check_tok(p, T_COMMA)) advance_tok(p);
        else break;
    }
    return strdup(buf[0] ? buf : "0");
}

static AstNode* parse_data(Parser* p) {
    advance_tok(p); // data
    Tok name = advance_tok(p);
    AstNode* node = ast_new(AST_DATA);
    node->name = strndup(name.s, name.len);
    expect(p, T_LBRACE);
    while (!check_tok(p, T_RBRACE) && !check_tok(p, T_EOF)) {
        if (peek_word(p, "align")) {
            advance_tok(p);
            Tok num = advance_tok(p);
            if (num.t != T_NUM) diag_error(num.line, num.col, "align needs an integer");
            AstNode* it = ast_new(AST_DATA_ITEM);
            it->name = strdup("align");
            it->value = strndup(num.s, num.len);
            ast_add(node, it);
            expect(p, T_SEMI);
            continue;
        }
        if (peek_word(p, "times")) {
            advance_tok(p);
            Tok num = advance_tok(p);
            if (num.t != T_NUM) diag_error(num.line, num.col, "times needs an integer count");
            Tok dir = advance_tok(p);
            int ok = dir.t == T_IDENT && (dir.len == 2 || dir.len == 2) &&
                     (!memcmp(dir.s, "db", 2) || !memcmp(dir.s, "dw", 2) ||
                      !memcmp(dir.s, "dd", 2) || !memcmp(dir.s, "dq", 2));
            if (!ok) diag_error(dir.line, dir.col, "times needs db, dw, dd, or dq");
            char* ops = parse_data_ops(p);
            AstNode* it = ast_new(AST_DATA_ITEM);
            it->name = strdup("times");
            char line[4200];
            snprintf(line, sizeof(line), "%.*s %.*s %s", num.len, num.s, dir.len, dir.s, ops);
            it->value = strdup(line);
            free(ops);
            ast_add(node, it);
            expect(p, T_SEMI);
            continue;
        }
        if (check_tok(p, T_IDENT) && peek_tok_at(p, 1, T_COLON)) {
            Tok lab = advance_tok(p);
            advance_tok(p);
            AstNode* it = ast_new(AST_DATA_ITEM);
            it->name = strdup("label");
            it->value = strndup(lab.s, lab.len);
            ast_add(node, it);
            continue;
        }
        if (peek_word(p, "db") || peek_word(p, "dw") || peek_word(p, "dd") || peek_word(p, "dq")) {
            Tok dir = advance_tok(p);
            char* ops = parse_data_ops(p);
            AstNode* it = ast_new(AST_DATA_ITEM);
            it->name = strndup(dir.s, dir.len);
            it->value = ops;
            ast_add(node, it);
            expect(p, T_SEMI);
            continue;
        }
        Tok bad = peek_tok(p);
        diag_error(bad.line, bad.col, "expected db, dw, dd, dq, times, align, or a label");
        break;
    }
    expect(p, T_RBRACE);
    return node;
}

// `bits 16`, `bits 32`, `bits 64`, and `naked`, in either order, in front of fn.
static AstNode* parse_bits_naked_fn(Parser* p) {
    int bits = 0, naked = 0, saw_bits = 0;
    while (peek_word(p, "bits") || peek_word(p, "naked")) {
        if (peek_word(p, "bits")) {
            Tok b = advance_tok(p);
            Tok num = advance_tok(p);
            int v = (num.t == T_NUM) ? atoi(num.s) : 0;
            if (v != 16 && v != 32 && v != 64)
                diag_error(b.line, b.col, "bits must be 16, 32, or 64");
            if (saw_bits) diag_error(b.line, b.col, "bits was already set on this function");
            bits = v;
            saw_bits = 1;
        } else {
            Tok nk = advance_tok(p);
            if (naked) diag_error(nk.line, nk.col, "naked was already set on this function");
            naked = 1;
        }
    }
    if (!check_tok(p, T_FN)) {
        Tok bad = peek_tok(p);
        diag_error(bad.line, bad.col, "expected 'fn' after bits or naked");
        return ast_new(AST_FUNCTION);
    }
    AstNode* fn = parse_func(p);
    if (naked) fn->flags |= AF_NAKED;
    fn->bits = bits;
    return fn;
}

typedef struct {
    char* name;
    const char* file;
    int line, col;
    int forward;
    const char* kind;
} DeclSite;
static DeclSite* g_decls = NULL;
static int g_ndecls = 0, g_decl_cap = 0;

static void note_top(AstNode* n) {
    if (!n || !n->name) return;
    const char* what = NULL;
    int fwd = 0;
    if (n->type == AST_FUNCTION) { what = "function"; fwd = n->is_forward_decl; }
    else if (n->type == AST_GLOBAL_VAR && (n->flags & AF_EXTERN)) what = "extern";
    else if (n->type == AST_GLOBAL_VAR) what = "global";
    else if (n->type == AST_STRUCT_DEF) what = "struct";
    else if (n->type == AST_DATA) what = "data";
    else return;
    for (int i = 0; i < g_ndecls; i++) {
        if (strcmp(g_decls[i].name, n->name)) continue;
        if (fwd) return;
        if (g_decls[i].forward) {
            g_decls[i].forward = 0;
            g_decls[i].file = g_src_file;
            g_decls[i].line = n->line;
            g_decls[i].col = n->col;
            return;
        }
        // `extern scratch` names a qword. `data scratch` is its bytes.
        // Either order is one symbol, not two definitions.
        if (g_decls[i].kind &&
            ((!strcmp(what, "extern") && !strcmp(g_decls[i].kind, "data")) ||
             (!strcmp(what, "data") && !strcmp(g_decls[i].kind, "extern"))))
            return;
        diag_error(n->line, n->col, "%s '%s' is already defined at %s:%d:%d",
                   what, n->name, g_decls[i].file, g_decls[i].line, g_decls[i].col);
        return;
    }
    if (g_ndecls >= g_decl_cap) {
        g_decl_cap = g_decl_cap ? g_decl_cap * 2 : 64;
        g_decls = safe_realloc(g_decls, sizeof(DeclSite) * g_decl_cap);
    }
    g_decls[g_ndecls].name = strdup(n->name);
    g_decls[g_ndecls].file = g_src_file;
    g_decls[g_ndecls].line = n->line;
    g_decls[g_ndecls].col = n->col;
    g_decls[g_ndecls].forward = fwd;
    g_decls[g_ndecls].kind = what;
    g_ndecls++;
}

// A qword whose storage Copihue does not allocate. `data name` in this file,
// or a symbol the compiler emits (`__syscall_rsp`), is the definition.
static char* g_cur_section;
static int g_in_section;

#define MAX_NAMED_SECTIONS 32
typedef struct {
    char* name;
    int has_addr;
    unsigned long long addr;
    int explicit_sec;   // The source wrote `section "name"`
    int need_start;
    int need_end;
    int started;
    int ended;
    char sym[80];
} NamedSection;
static NamedSection g_named_secs[MAX_NAMED_SECTIONS];
static int g_nnamed;
static const char* g_emit_section;
static int g_bss_tail; // Code emitted into .bss after the file's .bss block

static void sections_reset(void) {
    g_nnamed = 0;
    g_cur_section = NULL;
    g_in_section = 0;
    g_emit_section = NULL;
    g_bss_tail = 0;
}

static int section_name_ok(const char* s) {
    if (!s || !s[0]) return 0;
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_' || s[0] == '.')) return 0;
    for (const char* p = s + 1; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '.')) return 0;
    return 1;
}

static void section_sym_base(const char* name, char* out, int n) {
    if (!strcmp(name, ".bss")) { snprintf(out, n, "__bss"); return; }
    if (!strcmp(name, ".text")) { snprintf(out, n, "__text"); return; }
    if (!strcmp(name, ".data")) { snprintf(out, n, "__data"); return; }
    char buf[64];
    int k = 0;
    for (const char* p = name; *p && k < 60; p++) buf[k++] = (*p == '.') ? '_' : *p;
    buf[k] = 0;
    snprintf(out, n, "__sec_%s", buf);
}

static NamedSection* section_find(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < g_nnamed; i++)
        if (!strcmp(g_named_secs[i].name, name)) return &g_named_secs[i];
    return NULL;
}

// The first `@` fixes the load address. Reopening the name appends to it.
static NamedSection* section_note(const char* name, int has_addr, unsigned long long addr, int line, int col) {
    NamedSection* s = section_find(name);
    if (s) {
        if (has_addr && s->has_addr && s->addr != addr)
            diag_error(line, col, "section '%s' already has load address 0x%llx", name, s->addr);
        else if (has_addr && !s->has_addr) { s->has_addr = 1; s->addr = addr; }
        s->explicit_sec = 1;
        s->need_start = 1;
        s->need_end = 1;
        return s;
    }
    if (g_nnamed >= MAX_NAMED_SECTIONS) {
        diag_error(line, col, "too many sections");
        return NULL;
    }
    s = &g_named_secs[g_nnamed++];
    memset(s, 0, sizeof *s);
    s->name = strdup(name);
    s->has_addr = has_addr;
    s->addr = addr;
    s->explicit_sec = 1;
    s->need_start = 1;
    s->need_end = 1;
    section_sym_base(name, s->sym, sizeof s->sym);
    for (int i = 0; i < g_nnamed - 1; i++) {
        if (!strcmp(g_named_secs[i].sym, s->sym))
            diag_error(line, col, "section '%s' uses the same bounds symbol as '%s'", name, g_named_secs[i].name);
    }
    return s;
}

static NamedSection* section_ensure(const char* name) {
    NamedSection* s = section_find(name);
    if (s) return s;
    if (g_nnamed >= MAX_NAMED_SECTIONS) return NULL;
    s = &g_named_secs[g_nnamed++];
    memset(s, 0, sizeof *s);
    s->name = strdup(name);
    section_sym_base(name, s->sym, sizeof s->sym);
    return s;
}

static int tok_u64(Tok t, unsigned long long* out) {
    if (t.t != T_NUM || t.s[0] == '\'' || t.len <= 0) return 0;
    char digits[80];
    int k = 0;
    for (int i = 0; i < t.len && k < 79; i++) if (t.s[i] != '_') digits[k++] = t.s[i];
    digits[k] = 0;
    if (!k) return 0;
    errno = 0;
    char* end = NULL;
    unsigned long long v = (k > 2 && digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B'))
        ? strtoull(digits + 2, &end, 2) : strtoull(digits, &end, 0);
    if (errno || !end || *end) return 0;
    *out = v;
    return 1;
}

static void add_top(AstNode* prog, AstNode* n) {
    if (!n) return;
    if (g_cur_section) n->section = g_cur_section;
    note_top(n);
    ast_add(prog, n);
}

static void parse_extern(Parser* p, AstNode* prog) {
    advance_tok(p);
    Tok name = advance_tok(p);
    if (name.t != T_IDENT) {
        diag_error(name.line, name.col, "extern needs a name");
        return;
    }
    AstNode* g = ast_new(AST_GLOBAL_VAR);
    g->name = strndup(name.s, name.len);
    g->value = strdup("i64");
    g->flags |= AF_EXTERN;
    expect(p, T_SEMI);
    add_top(prog, g);
}

// `extern fn add(a: i64) -> i64;` and `extern bits 32 naked fn putc(c: i64);`.
// The declaration is a forward function so calls keep their arity check.
// The body is supplied by another `-c` object. Without `-c` there is no
// second object, so the declaration is an error.
static void parse_extern_fn(Parser* p, AstNode* prog) {
    Tok kw = advance_tok(p);
    if (!g_reloc)
        diag_error(kw.line, kw.col, "extern fn is only available with -c");
    AstNode* fn;
    if (peek_word(p, "bits") || peek_word(p, "naked"))
        fn = parse_bits_naked_fn(p);
    else if (check_tok(p, T_FN))
        fn = parse_func(p);
    else {
        Tok bad = peek_tok(p);
        diag_error(bad.line, bad.col, "extern fn needs a function declaration");
        return;
    }
    if (fn && !fn->is_forward_decl)
        diag_error(fn->line, fn->col, "extern fn has no body");
    if (fn) fn->flags |= AF_EXTERN;
    add_top(prog, fn);
}

static void parse_into(Parser* p, AstNode* prog);

static char* path_dir(const char* path) {
    const char* slash = strrchr(path, '/');
    if (!slash) return strdup(".");
    if (slash == path) return strdup("/");
    size_t n = (size_t)(slash - path);
    char* d = malloc(n + 1);
    memcpy(d, path, n);
    d[n] = '\0';
    return d;
}

static char* path_join(const char* dir, const char* rel) {
    if (rel[0] == '/') return strdup(rel);
    size_t nd = strlen(dir), nr = strlen(rel);
    char* s = malloc(nd + nr + 2);
    memcpy(s, dir, nd);
    s[nd] = '/';
    memcpy(s + nd + 1, rel, nr + 1);
    return s;
}

static int import_active(const char* path) {
    for (int i = 0; i < g_import_depth; i++)
        if (!strcmp(g_import_stack[i], path)) return 1;
    return 0;
}

static int import_done(const char* path) {
    for (int i = 0; i < g_nloaded; i++)
        if (!strcmp(g_loaded[i].name, path)) return 1;
    return 0;
}

static int parse_file_into(AstNode* prog, const char* path, int from_import, int line, int col) {
    if (import_active(path)) {
        diag_error(line, col, "import of '%s' is a cycle", path);
        return 0;
    }
    if (import_done(path)) return 1;
    FILE* f = fopen(path, "r");
    if (!f) {
        if (from_import) diag_error(line, col, "cannot open import '%s'", path);
        else fprintf(stderr, COMPILER_BIN ": error: cannot open '%s'\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) sz = 0;
    char* src = malloc((size_t)sz + 1);
    size_t got = fread(src, 1, (size_t)sz, f);
    src[got] = '\0';
    fclose(f);
    if (g_nloaded >= MAX_LOADED) {
        diag_error(line, col, "too many source files (maximum %d)", MAX_LOADED);
        free(src);
        return 0;
    }
    g_loaded[g_nloaded].name = strdup(path);
    g_loaded[g_nloaded].text = src;
    const char* saved_file = g_src_file;
    const char* saved_text = g_src_text;
    g_src_file = g_loaded[g_nloaded].name;
    g_src_text = src;
    g_import_stack[g_import_depth++] = g_loaded[g_nloaded].name;
    g_nloaded++;
    int count = 0;
    Tok* toks = tokenize(src, &count);
    Parser parser = {toks, 0, count};
    parse_into(&parser, prog);
    g_import_depth--;
    g_src_file = saved_file;
    g_src_text = saved_text;
    return 1;
}

static void parse_import(Parser* p, AstNode* prog) {
    Tok kw = advance_tok(p);
    Tok str = advance_tok(p);
    if (str.t != T_STR || str.len < 2) {
        diag_error(kw.line, kw.col, "import needs a string path");
        return;
    }
    char* rel = decode_string_literal(str.s + 1, str.len - 2);
    char* dir = path_dir(g_src_file);
    char* full = path_join(dir, rel);
    expect(p, T_SEMI);
    parse_file_into(prog, full, 1, str.line, str.col);
    free(rel);
    free(dir);
    free(full);
}

static void parse_top_item(Parser* p, AstNode* prog);

// `section "name" { ... }` and `section "name" @ addr { ... }`.
// `@` is NASM `vstart`: labels in the section resolve to that load address.
// The bytes stay packed in the image. A hole (`start=`) is not emitted,
// because an address like 0x100000 would otherwise write a megabyte of zeros.
static void parse_section(Parser* p, AstNode* prog) {
    Tok kw = advance_tok(p);
    if (!g_freestanding)
        diag_error(kw.line, kw.col, "section is only available with --freestanding, --flat, or --boot");
    if (g_in_section)
        diag_error(kw.line, kw.col, "a section cannot contain another section");
    if (!check_tok(p, T_STR)) {
        diag_error(kw.line, kw.col, "section takes a string name");
        return;
    }
    Tok str = advance_tok(p);
    char* name = decode_string_literal(str.s + 1, str.len - 2);
    if (!section_name_ok(name))
        diag_error(str.line, str.col, "section name '%s' is not a NASM section name", name ? name : "");
    int has_addr = 0;
    unsigned long long addr = 0;
    if (match_tok(p, T_AT)) {
        Tok num = peek_tok(p);
        if (!tok_u64(num, &addr))
            diag_error(num.line, num.col, "section load address must be a non-negative integer");
        else
            advance_tok(p);
        has_addr = 1;
    }
    // nasm -f elf64 warns and ignores vstart. -c must not emit it, and must
    // not pretend the address was applied. --flat and --boot are nasm -f bin.
    if (has_addr && g_reloc) {
        diag_error(kw.line, kw.col,
                   "section '@' is a load address for --flat and --boot; -c does not emit vstart");
        has_addr = 0;
    }
    NamedSection* sec = name && section_name_ok(name) ? section_note(name, has_addr, addr, kw.line, kw.col) : NULL;
    if (!check_tok(p, T_LBRACE)) {
        diag_error(peek_tok(p).line, peek_tok(p).col, "section body must be a block");
        free(name);
        return;
    }
    expect(p, T_LBRACE);
    char* saved = g_cur_section;
    int saved_in = g_in_section;
    g_cur_section = sec ? sec->name : saved;
    g_in_section = 1;
    while (!check_tok(p, T_RBRACE) && !check_tok(p, T_EOF)) {
        if (peek_word(p, "import") && peek_tok_at(p, 1, T_STR)) {
            diag_error(peek_tok(p).line, peek_tok(p).col, "import is not allowed inside a section");
            advance_tok(p);
            advance_tok(p);
            if (check_tok(p, T_SEMI)) advance_tok(p);
            continue;
        }
        parse_top_item(p, prog);
    }
    expect(p, T_RBRACE);
    g_cur_section = saved;
    g_in_section = saved_in;
    free(name);
}

static void parse_top_item(Parser* p, AstNode* prog) {
    int item_align = 0;
    // Contextual words, so `syscall(...)` and a variable named `packed` still parse.
    if (peek_word(p, "align") && peek_tok_at(p, 1, T_LPAREN))
        item_align = parse_align_prefix(p);
    if (peek_word(p, "section") && peek_tok_at(p, 1, T_STR)) {
        parse_section(p, prog);
    } else if (peek_word(p, "import") && peek_tok_at(p, 1, T_STR)) {
        parse_import(p, prog);
    } else if (peek_word(p, "packed") && peek_tok_at(p, 1, T_STRUCT)) {
        advance_tok(p);
        AstNode* sd = parse_struct_def(p);
        sd->flags |= AF_PACKED;
        sd->align = item_align;
        add_top(prog, sd);
    } else if (peek_word(p, "interrupt") && peek_tok_at(p, 1, T_FN)) {
        advance_tok(p);
        AstNode* fn = parse_func(p);
        fn->flags |= AF_INTERRUPT;
        add_top(prog, fn);
    } else if (peek_word(p, "syscall") && peek_tok_at(p, 1, T_FN)) {
        advance_tok(p);
        AstNode* fn = parse_func(p);
        fn->flags |= AF_SYSCALL;
        add_top(prog, fn);
    } else if (peek_word(p, "data") && peek_tok_at(p, 1, T_IDENT)) {
        add_top(prog, parse_data(p));
    } else if (peek_word(p, "extern") &&
               (peek_tok_at(p, 1, T_FN) || peek_word_at(p, 1, "bits") || peek_word_at(p, 1, "naked"))) {
        parse_extern_fn(p, prog);
    } else if (peek_word(p, "extern") && peek_tok_at(p, 1, T_IDENT)) {
        parse_extern(p, prog);
    } else if (peek_word(p, "bits") || peek_word(p, "naked")) {
        add_top(prog, parse_bits_naked_fn(p));
    } else if (check_tok(p, T_STRUCT)) {
        AstNode* sd = parse_struct_def(p);
        sd->align = item_align;
        add_top(prog, sd);
    } else if (check_tok(p, T_LET)) {
        AstNode* gv = parse_global_var(p);
        gv->align = item_align;
        add_top(prog, gv);
    } else {
        if (item_align)
            diag_error(peek_tok(p).line, peek_tok(p).col, "align() applies to a struct or a global");
        add_top(prog, parse_func(p));
    }
}

static void parse_into(Parser* p, AstNode* prog) {
    while (!check_tok(p, T_EOF)) parse_top_item(p, prog);
}

// ==== CODEGEN ====
void emit(Codegen* cg, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    while (cg->code_len + needed + 1 > cg->code_cap) {
        cg->code_cap = cg->code_cap ? cg->code_cap * 2 : 4096;
        cg->code_buf = safe_realloc(cg->code_buf, cg->code_cap);
    }

    vsnprintf(cg->code_buf + cg->code_len, needed + 1, fmt, args);
    cg->code_len += needed;

    va_end(args);
}

int new_label(Codegen* cg) { return cg->label_count++; }

static void cold_emit(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) needed = 0;
    while (g_cold_len + needed + 1 > g_cold_cap) {
        g_cold_cap = g_cold_cap ? g_cold_cap * 2 : 4096;
        g_cold = safe_realloc(g_cold, g_cold_cap);
    }
    vsnprintf(g_cold + g_cold_len, needed + 1, fmt, args);
    g_cold_len += needed;
    va_end(args);
}

static void cold_flush(Codegen* cg) {
    if (g_cold && g_cold_len > 0) emit(cg, "%s", g_cold);
    g_cold_len = 0;
}

// In-range path falls through. The trap is appended and emitted after ret.
static void emit_bounds_hot(Codegen* cg, const char* reg, int count, int skip_neg) {
    int lab = new_label(cg);
    if (!skip_neg) {
        emit(cg, "    test %s, %s\n", reg, reg);
        emit(cg, "    js .Lbounds_error_%d\n", lab);
    }
    emit(cg, "    cmp %s, %d\n", reg, count);
    emit(cg, "    jge .Lbounds_error_%d\n", lab);
    cold_emit(".Lbounds_error_%d:\n", lab);
    char* err_msg = strtab_add(cg->strtab, "Array bounds error\\n", 19);
    cold_emit("    mov rsi, %s\n", err_msg);
    cold_emit("    mov rdx, 19\n");
    cold_emit("    mov rdi, 2\n");
    cold_emit("    mov rax, 1\n");
    cold_emit("    syscall\n");
    cold_emit("    mov rdi, 1\n");
    cold_emit("    mov rax, 60\n");
    cold_emit("    syscall\n");
}

void gen_expr(Codegen* cg, AstNode* n);

// Load or store `size` bytes at `mem` (a NASM address without brackets).
// size >= 8 keeps the historical `mov rax, [mem]` / `mov [mem], rax` text.
// vol appends "  ; volatile" so the access is visible in the assembly and is
// not a candidate for folding. An empty mark reproduces the historical line.
static void emit_sized_load_mark(Codegen* cg, const char* mem, int size, int is_signed, int vol) {
    const char* m = vol ? "  ; volatile" : "";
    if (size <= 1)
        emit(cg, is_signed ? "    movsx rax, byte [%s]%s\n" : "    movzx rax, byte [%s]%s\n", mem, m);
    else if (size == 2)
        emit(cg, is_signed ? "    movsx rax, word [%s]%s\n" : "    movzx rax, word [%s]%s\n", mem, m);
    else if (size == 4)
        emit(cg, is_signed ? "    movsxd rax, dword [%s]%s\n" : "    mov eax, [%s]%s\n", mem, m);
    else
        emit(cg, "    mov rax, [%s]%s\n", mem, m);
}

static void emit_sized_store_mark(Codegen* cg, const char* mem, int size, int from_rcx, int vol) {
    const char* m = vol ? "  ; volatile" : "";
    const char* r8  = from_rcx ? "cl"  : "al";
    const char* r16 = from_rcx ? "cx"  : "ax";
    const char* r32 = from_rcx ? "ecx" : "eax";
    const char* r64 = from_rcx ? "rcx" : "rax";
    if (size <= 1) emit(cg, "    mov byte [%s], %s%s\n", mem, r8, m);
    else if (size == 2) emit(cg, "    mov word [%s], %s%s\n", mem, r16, m);
    else if (size == 4) emit(cg, "    mov dword [%s], %s%s\n", mem, r32, m);
    else emit(cg, "    mov [%s], %s%s\n", mem, r64, m);
}

// Scalar field geometry, packed or not. Every field is stored at its real
// width: u8 is one byte, u32 is four, a pointer is eight. An array field is
// not a scalar (the index path addresses it) and returns 0. A size of 8
// still emits `mov rax, [mem]`, the same text an all-i64 struct already had.
static int field_mem_info(Codegen* cg, const char* stype, const char* field,
                          int* off, int* size, int* is_signed, int* vol) {
    if (vol) *vol = 0;
    if (!stype || !field || !cg->types) return 0;
    StructField* f = typetab_field(cg->types, (char*)stype, field);
    if (!f || f->array_len > 0 || f->size <= 0) return 0;
    *off = f->offset;
    *size = f->size;
    *is_signed = f->is_signed;
    if (vol) *vol = f->is_volatile;
    return 1;
}

static void mem_base_disp(char* buf, int n, const char* base, int disp) {
    if (disp == 0) snprintf(buf, n, "%s", base);
    else snprintf(buf, n, "%s%+d", base, disp);
}

static AstNode* find_func(Codegen* cg, const char* name) {
    AstNode* fwd = NULL;
    if (!cg->program || !name) return NULL;
    for (int i = 0; i < cg->program->child_count; i++) {
        AstNode* fn = cg->program->children[i];
        if (fn->type != AST_FUNCTION || !fn->name || strcmp(fn->name, name)) continue;
        if (!fn->is_forward_decl) return fn;
        fwd = fn;
    }
    return fwd;
}

// ==== TYPES (0.31.0) ====
// Assignments, call arguments, and `return` distinguish integers from pointers.
// A wider integer does not narrow: `as` is that conversion. Same width (i64
// and u64, i8 and u8) and a narrower source still match. `mut` is not part of
// the type. `volatile` is: `*volatile T` is not `*T`. Adding volatile is
// allowed; dropping it is an error. An integer literal may initialize a
// pointer (a physical address or null). Pointer arithmetic and malloc() yield
// an address, which converts to an integer and, for malloc, to any pointer.
// A narrow integer operator truncates to that width; i64 and u64 stay 64-bit.

static char* array_elem_type_name(Codegen* cg, AstNode* arr);
static char* obj_record_type(Codegen* cg, AstNode* obj);
static StructField* access_field(Codegen* cg, AstNode* fa);
static int sizeof_node(Codegen* cg, AstNode* n);
static int gen_embedded_array(Codegen* cg, AstNode* field, AstNode* index, AstNode* value);

typedef enum { VK_UNK = 0, VK_INT, VK_PTR, VK_ADDR, VK_STRUCT, VK_MEM } VKind;

typedef struct {
    VKind kind;
    int bits;
    int sg;
    int lit;
    int addr_int;
    long long litv;
    char disp[160];
} VTy;

static int int_name(const char* s, int* bits, int* sg) {
    if (!s) return 0;
    if (!strcmp(s, "i8"))  { *bits = 8;  *sg = 1; return 1; }
    if (!strcmp(s, "u8"))  { *bits = 8;  *sg = 0; return 1; }
    if (!strcmp(s, "i16")) { *bits = 16; *sg = 1; return 1; }
    if (!strcmp(s, "u16")) { *bits = 16; *sg = 0; return 1; }
    if (!strcmp(s, "i32")) { *bits = 32; *sg = 1; return 1; }
    if (!strcmp(s, "u32")) { *bits = 32; *sg = 0; return 1; }
    if (!strcmp(s, "i64")) { *bits = 64; *sg = 1; return 1; }
    if (!strcmp(s, "u64")) { *bits = 64; *sg = 0; return 1; }
    return 0;
}

static int lit_fits(int bits, int sg, long long v) {
    if (bits >= 64) return sg ? 1 : v >= 0;
    if (sg) {
        long long max = (1LL << (bits - 1)) - 1;
        long long min = -max - 1;
        return v >= min && v <= max;
    }
    if (v < 0) return 0;
    unsigned long long max = (bits >= 32) ? 4294967295ULL : ((1ULL << bits) - 1ULL);
    return (unsigned long long)v <= max;
}

static void canon_type(const char* in, char* out, int n) {
    int o = 0;
    if (!in) { if (n > 0) out[0] = 0; return; }
    for (int i = 0; in[i] && o + 1 < n; ) {
        if (in[i] == '*') {
            int vol = 0;
            out[o++] = '*';
            i++;
            for (;;) {
                if (!strncmp(in + i, "volatile ", 9)) { vol = 1; i += 9; }
                else if (!strncmp(in + i, "mut ", 4)) i += 4;
                else break;
            }
            // `*volatile mut T` and `*mut volatile T` are `*volatile T`.
            if (vol && o + 9 < n) {
                memcpy(out + o, "volatile ", 9);
                o += 9;
            }
        } else out[o++] = in[i++];
    }
    out[o] = 0;
}

// Pointee spelling, skipping `volatile` and `mut` that belong to this star.
static const char* after_star(const char* disp) {
    if (!disp || disp[0] != '*') return disp ? disp : "";
    disp++;
    for (;;) {
        if (!strncmp(disp, "volatile ", 9)) disp += 9;
        else if (!strncmp(disp, "mut ", 4)) disp += 4;
        else break;
    }
    return disp;
}

static VTy ty_unk(void) {
    VTy t; memset(&t, 0, sizeof t);
    t.kind = VK_UNK;
    snprintf(t.disp, sizeof t.disp, "untyped");
    return t;
}

static VTy ty_i64(void) {
    VTy t; memset(&t, 0, sizeof t);
    t.kind = VK_INT; t.bits = 64; t.sg = 1;
    snprintf(t.disp, sizeof t.disp, "i64");
    return t;
}

static VTy ty_lit(long long v) {
    VTy t = ty_i64();
    t.lit = 1;
    t.litv = v;
    return t;
}

static VTy ty_addr(void) {
    VTy t; memset(&t, 0, sizeof t);
    t.kind = VK_ADDR; t.addr_int = 1;
    snprintf(t.disp, sizeof t.disp, "address");
    return t;
}

static VTy ty_mem(void) {
    VTy t; memset(&t, 0, sizeof t);
    t.kind = VK_MEM;
    snprintf(t.disp, sizeof t.disp, "address");
    return t;
}

static VTy ty_from_spelled(Codegen* cg, const char* spelled) {
    VTy t; memset(&t, 0, sizeof t);
    char canon[160];
    canon_type(spelled ? spelled : "", canon, (int)sizeof canon);
    if (!canon[0]) return ty_unk();
    if (canon[0] == '*') {
        t.kind = VK_PTR;
        snprintf(t.disp, sizeof t.disp, "%s", canon);
        return t;
    }
    if (canon[0] == '[') return ty_unk();
    int bits = 64, sg = 1;
    if (int_name(canon, &bits, &sg)) {
        t.kind = VK_INT; t.bits = bits; t.sg = sg;
        snprintf(t.disp, sizeof t.disp, "%s", canon);
        return t;
    }
    if (cg && cg->types && typetab_lookup(cg->types, canon)) {
        t.kind = VK_STRUCT;
        snprintf(t.disp, sizeof t.disp, "%s", canon);
        return t;
    }
    t.kind = VK_UNK;
    snprintf(t.disp, sizeof t.disp, "%s", canon);
    return t;
}

static VTy with_volatile_ptr(VTy p) {
    char raw[220];
    VTy n;
    if (p.kind != VK_PTR || p.disp[0] != '*' || !strncmp(p.disp + 1, "volatile ", 9))
        return p;
    snprintf(raw, sizeof raw, "*volatile %s", p.disp + 1);
    n = ty_from_spelled(NULL, raw);
    n.addr_int = p.addr_int;
    return n;
}

// full=1: function return stores the complete spelling (`*u8`, `i64`, `[u8; 4]`).
// full=0: a declaration stores the pointee in `base` and the pointer bits apart,
// so `**u8` is base `*u8` with the pointer bit set.
static VTy ty_decl(Codegen* cg, int full, int ptr_bits, const char* base) {
    if (full) return ty_from_spelled(cg, base);
    if (ptr_bits & 1) {
        char raw[220];
        snprintf(raw, sizeof raw, "*%s%s%s",
                 (ptr_bits & 4) ? "volatile " : "",
                 (ptr_bits & 2) ? "mut " : "",
                 base ? base : "");
        return ty_from_spelled(cg, raw);
    }
    return ty_from_spelled(cg, base);
}

static VTy sym_vty(Codegen* cg, Symbol* s) {
    if (!s) return ty_unk();
    if (s->type_name && !strcmp(s->type_name, "__addr__")) return ty_addr();
    if (s->is_array && s->type_name) {
        char raw[200];
        snprintf(raw, sizeof raw, "*%s", s->type_name);
        return ty_from_spelled(cg, raw);
    }
    if (s->is_pointer || (s->type_name && s->type_name[0] == '*')) {
        VTy p;
        if (s->type_name && s->type_name[0] == '*') p = ty_from_spelled(cg, s->type_name);
        else {
            char raw[200];
            snprintf(raw, sizeof raw, "*%s", s->type_name ? s->type_name : "u8");
            p = ty_from_spelled(cg, raw);
        }
        p.addr_int = s->as_int;
        if (s->volatile_pointee) p = with_volatile_ptr(p);
        return p;
    }
    if (s->type_name) return ty_from_spelled(cg, s->type_name);
    return ty_unk();
}

static VTy global_vty(Codegen* cg, GlobalVar* g) {
    if (!g) return ty_unk();
    if (g->is_array && g->elem_type) {
        char raw[200];
        snprintf(raw, sizeof raw, "*%s", g->elem_type);
        return ty_from_spelled(cg, raw);
    }
    if (g->is_pointer) {
        char raw[200];
        const char* base = g->pointee_type ? g->pointee_type : g->type_name;
        snprintf(raw, sizeof raw, "*%s", base ? base : "");
        {
            VTy p = ty_from_spelled(cg, raw);
            if (g->volatile_pointee) p = with_volatile_ptr(p);
            return p;
        }
    }
    if (g->type_name) return ty_from_spelled(cg, g->type_name);
    return ty_unk();
}

static void bind_vty(Symbol* s, VTy t) {
    if (!s) return;
    if (t.kind == VK_ADDR) {
        s->type_name = strdup("__addr__");
        s->is_pointer = 0;
        s->as_int = 1;
        return;
    }
    if (t.kind == VK_PTR) {
        s->is_pointer = 1;
        s->type_name = strdup(t.disp);
        s->as_int = t.addr_int;
        return;
    }
    if (t.kind == VK_INT) {
        s->type_name = strdup(t.disp[0] ? t.disp : "i64");
        s->is_pointer = 0;
        s->as_int = 0;
        return;
    }
    if (t.kind == VK_STRUCT && t.disp[0]) {
        s->type_name = strdup(t.disp);
        s->is_pointer = 0;
        s->as_int = 0;
    }
}

static VTy ty_pointer_to(VTy elem) {
    if (elem.kind == VK_UNK || elem.kind == VK_ADDR || !elem.disp[0]) return ty_addr();
    char raw[200];
    snprintf(raw, sizeof raw, "*%s", elem.disp);
    return ty_from_spelled(NULL, raw);
}

static VTy expr_ty(Codegen* cg, AstNode* n);
static VTy ty_field(Codegen* cg, AstNode* n);
static int is_atomic_rmw(const char* f);
static int atomic_pointee(Codegen* cg, AstNode* p, int* bits, int* sg);

static VTy ty_indexed(Codegen* cg, AstNode* base) {
    if (base && base->type == AST_IDENT) {
        char* et = array_elem_type_name(cg, base);
        if (et) return ty_from_spelled(cg, et);
    }
    VTy b = expr_ty(cg, base);
    if (b.kind == VK_PTR && b.disp[0] == '*') return ty_from_spelled(cg, after_star(b.disp));
    return ty_unk();
}

static VTy ty_addr_of(Codegen* cg, AstNode* n) {
    if (!n || !n->child_count || !n->children[0]) return ty_unk();
    AstNode* c = n->children[0];
    if (c->type == AST_IDENT && c->name) {
        Symbol* s = (cg && cg->symtab) ? symtab_lookup_symbol(cg->symtab, c->name) : NULL;
        GlobalVar* g = (!s && cg && cg->global_symtab) ? global_symtab_lookup(cg->global_symtab, c->name) : NULL;
        if (!s && !g && find_func(cg, c->name)) return ty_i64();
        if (s) {
            if (s->is_pointer || (s->type_name && s->type_name[0] == '*')) return sym_vty(cg, s);
            if (s->type_name) {
                char raw[200];
                snprintf(raw, sizeof raw, "*%s%s", s->is_volatile ? "volatile " : "", s->type_name);
                return ty_from_spelled(cg, raw);
            }
            return ty_addr();
        }
        if (g) {
            if (g->is_pointer) return global_vty(cg, g);
            if (g->is_array && g->elem_type) {
                char raw[200];
                snprintf(raw, sizeof raw, "*%s", g->elem_type);
                return ty_from_spelled(cg, raw);
            }
            if (g->type_name) {
                char raw[200];
                snprintf(raw, sizeof raw, "*%s%s",
                         (g->is_volatile && !g->is_array) ? "volatile " : "", g->type_name);
                return ty_from_spelled(cg, raw);
            }
        }
    }
    if (c->type == AST_FIELD_ACCESS) {
        StructField* f = access_field(cg, c);
        VTy inner = ty_field(cg, c);
        if (f && f->is_volatile && inner.disp[0] && inner.disp[0] != '*') {
            char raw[220];
            snprintf(raw, sizeof raw, "*volatile %s", inner.disp);
            return ty_from_spelled(cg, raw);
        }
        return ty_pointer_to(inner);
    }
    return ty_pointer_to(expr_ty(cg, c));
}

static VTy ty_field(Codegen* cg, AstNode* n) {
    if (!n || !n->child_count || !n->name) return ty_unk();
    char* rec = obj_record_type(cg, n->children[0]);
    if (!rec) return ty_unk();
    char* ft = typetab_field_type(cg->types, rec, n->name);
    if (!ft) return ty_unk();
    return ty_from_spelled(cg, ft);
}

static int builtin_ret_ty(const char* f, VTy* out) {
    if (!f) return 0;
    if (!strcmp(f, "malloc")) { *out = ty_addr(); return 1; }
    if (!strcmp(f, "bss_start") || !strcmp(f, "bss_end")) {
        *out = ty_from_spelled(NULL, "*u8");
        return 1;
    }
    if (!strcmp(f, "strlen") || !strcmp(f, "strcmp") || !strcmp(f, "strcpy") ||
        !strcmp(f, "open") || !strcmp(f, "read") || !strcmp(f, "write") || !strcmp(f, "close") ||
        !strcmp(f, "inb") || !strcmp(f, "inw") || !strcmp(f, "inl") ||
        !strcmp(f, "outb") || !strcmp(f, "outw") || !strcmp(f, "outl") ||
        !strcmp(f, "rdmsr") || !strcmp(f, "wrmsr") || !strcmp(f, "ltr") ||
        !strcmp(f, "read_cr0") || !strcmp(f, "read_cr2") || !strcmp(f, "read_cr3") || !strcmp(f, "read_cr4") ||
        !strcmp(f, "write_cr0") || !strcmp(f, "write_cr3") || !strcmp(f, "write_cr4") ||
        !strcmp(f, "print_int") || !strcmp(f, "print") || !strcmp(f, "println") ||
        !strcmp(f, "exit") || !strcmp(f, "free") || !strcmp(f, "syscall") || !strcmp(f, "syscall6") ||
        !strcmp(f, "lidt") || !strcmp(f, "lgdt") || !strcmp(f, "invlpg") ||
        !strcmp(f, "hlt") || !strcmp(f, "cli") || !strcmp(f, "sti") || !strcmp(f, "pause") ||
        !strcmp(f, "swapgs") || !strcmp(f, "iretq") || !strcmp(f, "sysretq") ||
        !strcmp(f, "set_syscall_stack") || !strcmp(f, "ljmp") || !strcmp(f, "int") || !strcmp(f, "asm") ||
        !strcmp(f, "lodsb") || !strcmp(f, "stosb") || !strcmp(f, "stosd") ||
        !strcmp(f, "pushfd") || !strcmp(f, "popfd") || !strcmp(f, "push") || !strcmp(f, "pop") ||
        !strcmp(f, "stc") || !strcmp(f, "clc") || !strcmp(f, "cld") || !strcmp(f, "std") || !strcmp(f, "ud2") ||
        !strcmp(f, "sizeof") || !strcmp(f, "offsetof") ||
        !strcmp(f, "atomic_load_acquire") || !strcmp(f, "atomic_load_seq_cst") ||
        !strcmp(f, "atomic_store_release") || !strcmp(f, "atomic_store_seq_cst") ||
        !strcmp(f, "fetch_add") || !strcmp(f, "atomic_add") ||
        !strcmp(f, "swap") || !strcmp(f, "compare_exchange") || !strcmp(f, "fence") ||
        !strcmp(f, "cpu_pause") || !strcmp(f, "thread_spawn") || !strcmp(f, "thread_join") ||
        !strcmp(f, "sched_setaffinity") ||
        !strcmp(f, "rep_stosb") || !strcmp(f, "rep_stosw") || !strcmp(f, "rep_stosd") || !strcmp(f, "rep_stosq") ||
        !strcmp(f, "rep_movsb") || !strcmp(f, "rep_movsw") || !strcmp(f, "rep_movsd") || !strcmp(f, "rep_movsq") ||
        !strcmp(f, "save_context") || !strcmp(f, "restore_context") || !strcmp(f, "label_addr") ||
        !strcmp(f, "section_start") || !strcmp(f, "section_end")) {
        *out = ty_i64();
        return 1;
    }
    return 0;
}

static VTy ty_named_int(int bits, int sg) {
    const char* name = "i64";
    if (bits <= 8) name = sg ? "i8" : "u8";
    else if (bits <= 16) name = sg ? "i16" : "u16";
    else if (bits <= 32) name = sg ? "i32" : "u32";
    return ty_from_spelled(NULL, name);
}

// Width is the wider typed operand. Signed only when both are signed.
// A bare literal adopts the other side when it fits; if it does not, or both
// sides are literals, the operation stays i64 (folding and literal errors).
// A shift uses the left operand only. 64-bit results stay i64.
static VTy ty_int_result(VTy L, VTy R, int shift) {
    int bits, sg;
    if (shift) {
        if (L.kind == VK_INT && !L.lit && L.bits < 64) return ty_named_int(L.bits, L.sg);
        return ty_i64();
    }
    if (L.kind != VK_INT || R.kind != VK_INT) return ty_i64();
    if (L.lit && !R.lit && lit_fits(R.bits, R.sg, L.litv)) {
        L.bits = R.bits;
        L.sg = R.sg;
        L.lit = 0;
    } else if (R.lit && !L.lit && lit_fits(L.bits, L.sg, R.litv)) {
        R.bits = L.bits;
        R.sg = L.sg;
        R.lit = 0;
    }
    if (L.lit || R.lit) return ty_i64();
    bits = L.bits > R.bits ? L.bits : R.bits;
    sg = (L.sg && R.sg) ? 1 : 0;
    if (bits >= 64) return ty_i64();
    return ty_named_int(bits, sg);
}

static VTy ty_binop(Codegen* cg, AstNode* n) {
    VTy L = (n->child_count > 0) ? expr_ty(cg, n->children[0]) : ty_unk();
    VTy R = (n->child_count > 1) ? expr_ty(cg, n->children[1]) : ty_unk();
    int plus = n->op && n->op[0] == '+' && !n->op[1];
    int minus = n->op && n->op[0] == '-' && !n->op[1];
    int shift = n->op && (!strcmp(n->op, "<<") || !strcmp(n->op, ">>"));
    if ((plus || minus) && (L.kind == VK_PTR || L.kind == VK_ADDR) && R.kind == VK_INT) {
        if (L.kind == VK_ADDR) return ty_addr();
        L.addr_int = 1;
        return L;
    }
    if (plus && L.kind == VK_INT && (R.kind == VK_PTR || R.kind == VK_ADDR)) {
        if (R.kind == VK_ADDR) return ty_addr();
        R.addr_int = 1;
        return R;
    }
    if (minus && (L.kind == VK_PTR || L.kind == VK_ADDR) && (R.kind == VK_PTR || R.kind == VK_ADDR))
        return ty_i64();
    return ty_int_result(L, R, shift);
}

static VTy expr_ty(Codegen* cg, AstNode* n) {
    if (!n) return ty_unk();
    if (n->type == AST_NUMBER) return ty_lit(n->value ? atoll(n->value) : 0);
    if (n->type == AST_STRING) return ty_from_spelled(NULL, "*u8");
    if (n->type == AST_CAST) {
        if (n->struct_type && !strcmp(n->struct_type, "__array__")) return ty_unk();
        return ty_decl(cg, 0, n->is_pointer, n->value);
    }
    if (n->type == AST_IDENT && n->name) {
        Symbol* s = (cg && cg->symtab) ? symtab_lookup_symbol(cg->symtab, n->name) : NULL;
        if (s) return sym_vty(cg, s);
        GlobalVar* g = (cg && cg->global_symtab) ? global_symtab_lookup(cg->global_symtab, n->name) : NULL;
        if (g) return global_vty(cg, g);
        {
            if (mach_ok(mach_reg(n->name)))
                return ty_i64();
        }
        return ty_unk();
    }
    if (n->type == AST_ADDR_OF) return ty_addr_of(cg, n);
    if (n->type == AST_DEREF && n->child_count > 0) {
        VTy p = expr_ty(cg, n->children[0]);
        if (p.kind == VK_PTR && p.disp[0] == '*') return ty_from_spelled(cg, after_star(p.disp));
        return ty_unk();
    }
    if (n->type == AST_INDEX && n->child_count > 0) return ty_indexed(cg, n->children[0]);
    if (n->type == AST_FIELD_ACCESS) return ty_field(cg, n);
    if (n->type == AST_UNARY && n->child_count > 0) {
        if (n->op && (n->op[0] == '-' || n->op[0] == '~') && !n->op[1]) {
            VTy c = expr_ty(cg, n->children[0]);
            if (n->op[0] == '-' && c.kind == VK_INT && c.lit) return ty_lit(-c.litv);
            if (c.kind == VK_INT && !c.lit && c.bits < 64) return ty_named_int(c.bits, c.sg);
        }
        return ty_i64();
    }
    if (n->type == AST_BINOP) return ty_binop(cg, n);
    if (n->type == AST_COMPARE || n->type == AST_LOGICAL) return ty_i64();
    if (n->type == AST_CALL && n->name) {
        VTy b;
        if (is_atomic_rmw(n->name) && n->child_count > 0) {
            int ab = 0, asg = 0;
            if (atomic_pointee(cg, n->children[0], &ab, &asg)) return ty_named_int(ab, asg);
        }
        if (builtin_ret_ty(n->name, &b)) return b;
        Symbol* loc = (cg && cg->symtab) ? symtab_lookup_symbol(cg->symtab, n->name) : NULL;
        GlobalVar* gv = (!loc && cg && cg->global_symtab) ? global_symtab_lookup(cg->global_symtab, n->name) : NULL;
        AstNode* fn = cg ? find_func(cg, n->name) : NULL;
        int indirect = (loc || (gv && !fn)) ? 1 : 0;
        if (indirect || !fn) return ty_unk();
        return ty_decl(cg, 1, fn->is_pointer, fn->value);
    }
    if (n->type == AST_STRUCT_LITERAL && n->struct_type) return ty_from_spelled(cg, n->struct_type);
    if (n->type == AST_ASSIGN && n->child_count > 0) return expr_ty(cg, n->children[0]);
    return ty_unk();
}

static int ptr_compat(const char* a, const char* b);

static void mismatch(AstNode* at, const char* ctx, const char* exp, const char* got) {
    if (!at) return;
    if (ctx && ctx[0])
        diag_error(at->line, at->col, "%s: expected %s, found %s", ctx, exp, got);
    else
        diag_error(at->line, at->col, "expected %s, found %s", exp, got);
}

static void check_types(AstNode* at, const char* ctx, VTy dst, VTy src) {
    if (!at) return;
    if (dst.kind == VK_UNK || src.kind == VK_UNK) return;
    if (dst.kind == VK_STRUCT || src.kind == VK_STRUCT) return;
    if (src.kind == VK_ADDR) return;
    if (dst.kind == VK_MEM) return;
    if (dst.kind == VK_INT && src.kind == VK_INT) {
        if (src.lit) {
            if (!lit_fits(dst.bits, dst.sg, src.litv)) {
                if (ctx && ctx[0])
                    diag_error(at->line, at->col, "%s: expected %s, found integer literal %lld",
                               ctx, dst.disp, src.litv);
                else
                    diag_error(at->line, at->col, "expected %s, found integer literal %lld",
                               dst.disp, src.litv);
            }
            return;
        }
        // A wider integer does not narrow by itself. `as` is that conversion.
        // Same width (i64 and u64, i8 and u8) and a narrower source still match.
        if (src.bits > dst.bits) {
            mismatch(at, ctx, dst.disp, src.disp);
            return;
        }
        return;
    }
    if (dst.kind == VK_INT && src.kind == VK_PTR && src.addr_int) return;
    if (dst.kind == VK_INT && src.kind == VK_PTR) { mismatch(at, ctx, dst.disp, src.disp); return; }
    if (dst.kind == VK_PTR && src.kind == VK_INT && src.lit) return;
    if (dst.kind == VK_PTR && src.kind == VK_INT) { mismatch(at, ctx, dst.disp, src.disp); return; }
    if (dst.kind == VK_PTR && src.kind == VK_PTR) {
        if (!ptr_compat(dst.disp, src.disp)) mismatch(at, ctx, dst.disp, src.disp);
        return;
    }
}

static void ptr_split(const char* t, int* vol, const char** pointee) {
    *vol = 0;
    *pointee = t ? t : "";
    if (!t || t[0] != '*') return;
    t++;
    if (!strncmp(t, "volatile ", 9)) { *vol = 1; t += 9; }
    *pointee = t;
}

static int pointee_eq(const char* a, const char* b) {
    if (!a || !b) return 0;
    if (!strcmp(a, b)) return 1;
    if ((!strcmp(a, "i8") && !strcmp(b, "u8")) || (!strcmp(a, "u8") && !strcmp(b, "i8")))
        return 1;
    return 0;
}

// *i8 and *u8 are the same byte pointer, including when both are volatile.
// Dropping volatile is an error. Adding it (`*u8` into `*volatile u8`) is
// allowed, so an array still initializes a volatile pointer.
static int ptr_compat(const char* dst, const char* src) {
    int dv = 0, sv = 0;
    const char* dp = "";
    const char* sp = "";
    if (!dst || !src) return 0;
    ptr_split(dst, &dv, &dp);
    ptr_split(src, &sv, &sp);
    if (!pointee_eq(dp, sp)) return 0;
    if (sv && !dv) return 0;
    return 1;
}

// Unsigned 32-bit compare in bits 16/32. A zero-extended u8/u16 still
// compares correctly with signed jl, because bit 31 is clear. u32 does not.
static int cmp_legacy_unsigned(Codegen* cg, AstNode* n) {
    VTy L, R, t;
    if (!legacy_bits() || !n || n->child_count < 2) return 0;
    L = expr_ty(cg, n->children[0]);
    R = expr_ty(cg, n->children[1]);
    t = ty_int_result(L, R, 0);
    return t.kind == VK_INT && !t.lit && !t.sg && t.bits >= 32 && t.bits < 64;
}

static void check_arg(Codegen* cg, AstNode* arg, int idx, const char* fname, VTy dst) {
    char ctx[128];
    if (!arg) return;
    snprintf(ctx, sizeof ctx, "argument %d of '%s'", idx + 1, fname ? fname : "(call)");
    check_types(arg, ctx, dst, expr_ty(cg, arg));
}

static void check_user_args(Codegen* cg, AstNode* n, AstNode* fn) {
    int argc = n->child_count;
    int want = fn->child_count - 1;
    int narg = argc < want ? argc : want;
    for (int i = 0; i < narg; i++) {
        AstNode* par = fn->children[i];
        if (!par) continue;
        VTy dst = ty_decl(cg, 0, par->is_pointer, par->value);
        check_arg(cg, n->children[i], i, n->name, dst);
    }
}

static int is_atomic_rmw(const char* f) {
    return f && (!strcmp(f, "atomic_load_acquire") || !strcmp(f, "atomic_load_seq_cst") ||
                 !strcmp(f, "fetch_add") || !strcmp(f, "atomic_add") ||
                 !strcmp(f, "swap") || !strcmp(f, "compare_exchange"));
}

static int is_atomic_store(const char* f) {
    return f && (!strcmp(f, "atomic_store_release") || !strcmp(f, "atomic_store_seq_cst"));
}

// *volatile u32/i32 or *volatile u64/i64. Other pointees, and a pointer that
// is not volatile, are rejected: a plain mov is not an atomic protocol.
static int atomic_pointee(Codegen* cg, AstNode* p, int* bits, int* sg) {
    VTy t;
    const char* po;
    if (!p || !bits || !sg) return 0;
    t = expr_ty(cg, p);
    if (t.kind != VK_PTR || strncmp(t.disp, "*volatile ", 10)) return 0;
    po = after_star(t.disp);
    if (!int_name(po, bits, sg)) return 0;
    if (*bits != 32 && *bits != 64) return 0;
    return 1;
}

static void atomic_ptr_error(Codegen* cg, AstNode* n, AstNode* p) {
    VTy t = p ? expr_ty(cg, p) : ty_unk();
    const char* got = (t.disp[0]) ? t.disp : "untyped";
    diag_error(n->line, n->col, "%s() takes a *volatile u32 or *volatile u64, found %s",
               n->name ? n->name : "atomic", got);
}

static void check_builtin_args(Codegen* cg, AstNode* n) {
    const char* f;
    int c;
    if (!n || !n->name) return;
    f = n->name;
    c = n->child_count;
    if (!strcmp(f, "print") || !strcmp(f, "println") || !strcmp(f, "strlen")) {
        if (c >= 1) check_arg(cg, n->children[0], 0, f, ty_from_spelled(cg, "*u8"));
        return;
    }
    if (!strcmp(f, "strcmp") || !strcmp(f, "strcpy")) {
        if (c >= 2) {
            check_arg(cg, n->children[0], 0, f, ty_from_spelled(cg, "*u8"));
            check_arg(cg, n->children[1], 1, f, ty_from_spelled(cg, "*u8"));
        }
        return;
    }
    if (!strcmp(f, "open")) {
        if (c >= 2) {
            check_arg(cg, n->children[0], 0, f, ty_from_spelled(cg, "*u8"));
            check_arg(cg, n->children[1], 1, f, ty_i64());
            if (c >= 3) check_arg(cg, n->children[2], 2, f, ty_i64());
        }
        return;
    }
    if (!strcmp(f, "read") || !strcmp(f, "write")) {
        if (c >= 3) {
            check_arg(cg, n->children[0], 0, f, ty_i64());
            check_arg(cg, n->children[1], 1, f, ty_mem());
            check_arg(cg, n->children[2], 2, f, ty_i64());
        }
        return;
    }
    if (!strcmp(f, "free") || !strcmp(f, "lidt") || !strcmp(f, "lgdt") || !strcmp(f, "invlpg")) {
        if (c == 1) check_arg(cg, n->children[0], 0, f, ty_mem());
        return;
    }
    if (!strcmp(f, "print_int") || !strcmp(f, "exit") || !strcmp(f, "malloc") ||
        !strcmp(f, "close") || !strcmp(f, "inb") || !strcmp(f, "inw") || !strcmp(f, "inl") ||
        !strcmp(f, "rdmsr") || !strcmp(f, "ltr") || !strcmp(f, "set_syscall_stack") ||
        !strcmp(f, "write_cr0") || !strcmp(f, "write_cr3") || !strcmp(f, "write_cr4")) {
        if ((!strcmp(f, "print_int") || !strcmp(f, "exit") || !strcmp(f, "malloc") || !strcmp(f, "close")) ? c >= 1 : c == 1)
            check_arg(cg, n->children[0], 0, f, ty_i64());
        return;
    }
    if (!strcmp(f, "outb") || !strcmp(f, "outw") || !strcmp(f, "outl") || !strcmp(f, "wrmsr")) {
        if (c == 2) {
            check_arg(cg, n->children[0], 0, f, ty_i64());
            check_arg(cg, n->children[1], 1, f, ty_i64());
        }
        return;
    }
    // syscall registers are raw values: an integer or any pointer.
    if (!strcmp(f, "syscall") || !strcmp(f, "syscall6")) {
        for (int i = 0; i < c && i < 7; i++) check_arg(cg, n->children[i], i, f, ty_mem());
        return;
    }
    if (!strcmp(f, "save_context") || !strcmp(f, "restore_context")) {
        if (c == 1) check_arg(cg, n->children[0], 0, f, ty_mem());
        return;
    }
    if (is_atomic_rmw(f) || is_atomic_store(f)) {
        int need = is_atomic_store(f) ? 2 : 1;
        int bits = 0, sg = 0;
        if (!strcmp(f, "fetch_add") || !strcmp(f, "atomic_add") || !strcmp(f, "swap")) need = 2;
        if (!strcmp(f, "compare_exchange")) need = 3;
        if (c != need) {
            diag_error(n->line, n->col, "%s() takes %d argument%s", f, need, need == 1 ? "" : "s");
            return;
        }
        if (!atomic_pointee(cg, n->children[0], &bits, &sg)) atomic_ptr_error(cg, n, n->children[0]);
        for (int i = 1; i < c; i++) check_arg(cg, n->children[i], i, f, ty_i64());
        return;
    }
    if (!strcmp(f, "fence") || !strcmp(f, "cpu_pause")) {
        if (c != 0) diag_error(n->line, n->col, "%s() takes no arguments", f);
        return;
    }
    if (!strcmp(f, "thread_spawn")) {
        if (c != 3) {
            diag_error(n->line, n->col, "thread_spawn() takes a function, a stack top, and a tid pointer");
            return;
        }
        check_arg(cg, n->children[0], 0, f, ty_mem());
        check_arg(cg, n->children[1], 1, f, ty_mem());
        check_arg(cg, n->children[2], 2, f, ty_mem());
        return;
    }
    if (!strcmp(f, "thread_join")) {
        if (c != 1) diag_error(n->line, n->col, "thread_join() takes the tid pointer");
        else check_arg(cg, n->children[0], 0, f, ty_mem());
        return;
    }
    if (!strcmp(f, "sched_setaffinity")) {
        if (c != 1) diag_error(n->line, n->col, "sched_setaffinity() takes a cpu number");
        else check_arg(cg, n->children[0], 0, f, ty_i64());
        return;
    }
}

static void emit_width_trunc(Codegen* cg, int bits, int sg) {
    if (bits >= 64) return;
    // bits 16/32 already hold the value in eax. movsxd/mov rax are rejected there.
    const char* d = legacy_bits() ? "eax" : "rax";
    if (sg) {
        if (bits <= 8) emit(cg, "    movsx %s, al\n", d);
        else if (bits <= 16) emit(cg, "    movsx %s, ax\n", d);
        else if (!legacy_bits()) emit(cg, "    movsxd rax, eax\n");
    } else {
        if (bits <= 8) emit(cg, "    movzx %s, al\n", d);
        else if (bits <= 16) emit(cg, "    movzx %s, ax\n", d);
        else if (!legacy_bits()) emit(cg, "    mov eax, eax\n");
    }
}

static void emit_reg_trunc(Codegen* cg, const char* reg, int bits, int sg);

static void trunc_rax_node(Codegen* cg, AstNode* n) {
    VTy t;
    if (!n) return;
    if (n->type == AST_BINOP) t = ty_binop(cg, n);
    else if (n->type == AST_UNARY && n->op && (n->op[0] == '-' || n->op[0] == '~') && !n->op[1])
        t = expr_ty(cg, n);
    else return;
    if (t.kind == VK_INT && !t.lit && t.bits < 64) emit_width_trunc(cg, t.bits, t.sg);
}

static int is_numeral(AstNode* n, long long* v) {
    if (!n) return 0;
    if (n->type == AST_NUMBER && n->value) { *v = atoll(n->value); return 1; }
    if (n->type == AST_UNARY && n->op && n->op[0] == '-' && n->child_count == 1 &&
        n->children[0] && n->children[0]->type == AST_NUMBER && n->children[0]->value) {
        *v = -atoll(n->children[0]->value);
        return 1;
    }
    return 0;
}

// A store of `e as T` into a slot of width T already keeps the low bits.
// Drop that cast so the store text stays the instruction it was without `as`.
static AstNode* peel_store_cast(AstNode* e, int bits) {
    int sb = 64, ss = 1;
    if (bits >= 64 || bits <= 0) return e;
    if (!e || e->type != AST_CAST || (e->is_pointer & 1) || e->child_count < 1) return e;
    if (!e->value || !int_name(e->value, &sb, &ss)) return e;
    if (sb != bits) return e;
    return e->children[0];
}

static void emit_store_trunc(Codegen* cg, const char* tname, AstNode* rhs) {
    int bits = 64, sg = 1;
    long long v = 0;
    VTy s;
    if (!int_name(tname, &bits, &sg) || bits >= 64) return;
    if (is_numeral(rhs, &v) && lit_fits(bits, sg, v)) return;
    // The operator already truncated to this type. A second mask would be a
    // new instruction in front of the same store.
    if (rhs && (rhs->type == AST_BINOP || rhs->type == AST_CAST ||
                (rhs->type == AST_UNARY && rhs->op &&
                 (rhs->op[0] == '-' || rhs->op[0] == '~') && !rhs->op[1]))) {
        s = expr_ty(cg, rhs);
        if (s.kind == VK_INT && !s.lit && s.bits == bits && s.sg == sg) return;
    }
    emit_width_trunc(cg, bits, sg);
}

static void trunc_rcx_named(Codegen* cg, const char* tname) {
    int bits = 64, sg = 1;
    if (!tname || tname[0] == '*') return;
    if (!int_name(tname, &bits, &sg) || bits >= 64) return;
    emit(cg, "    mov rax, rcx\n");
    emit_width_trunc(cg, bits, sg);
    emit(cg, "    mov rcx, rax\n");
}

static void trunc_rax_field(Codegen* cg, const char* stype, const char* field) {
    char* ft;
    int bits = 64, sg = 1;
    if (!stype || !field || !cg->types) return;
    ft = typetab_field_type(cg->types, (char*)stype, (char*)field);
    if (ft && int_name(ft, &bits, &sg)) emit_width_trunc(cg, bits, sg);
}


static int is_struct_value_arg(Codegen* cg, AstNode* a) {
    if (!a || a->type != AST_IDENT) return 0;
    Symbol* asym = symtab_lookup_symbol(cg->symtab, a->name);
    return asym && !asym->is_pointer && asym->type_name && asym->type_name[0] != '*' &&
           typetab_lookup(cg->types, asym->type_name) != NULL;
}

// Evaluate one call argument into rax. A struct value is passed as its address;
// the callee copies it.
static void gen_one_arg(Codegen* cg, AstNode* a) {
    if (is_struct_value_arg(cg, a)) {
        Symbol* asym = symtab_lookup_symbol(cg->symtab, a->name);
        emit(cg, "    lea rax, [rbp%d]\n", asym->offset);
    } else {
        gen_expr(cg, a);
    }
}

static void gen_load_callee(Codegen* cg, const char* name) {
    Symbol* loc = name ? symtab_lookup_symbol(cg->symtab, (char*)name) : NULL;
    if (loc) {
        if (loc->is_array) emit(cg, "    lea rbx, [rbp%d]\n", loc->offset);
        else if (local_reg(loc)) emit(cg, "    mov rbx, %s\n", local_reg(loc));
        else emit(cg, "    mov rbx, [rbp%d]\n", loc->offset);
        return;
    }
    GlobalVar* gv = name ? global_symtab_lookup(cg->global_symtab, (char*)name) : NULL;
    if (gv) {
        if (gv->is_array) emit(cg, "    lea rbx, [%s]\n", gv->name);
        else emit(cg, "    mov rbx, [%s]\n", gv->name);
        return;
    }
    emit(cg, "    xor rbx, rbx\n");
}

static int can_place(Codegen* cg, AstNode* n, const char* dest);
static void place_in(Codegen* cg, AstNode* n, const char* dest);

static int node_has_call(AstNode* n) {
    if (!n) return 0;
    if (n->type == AST_CALL) return 1;
    for (int i = 0; i < n->child_count; i++)
        if (node_has_call(n->children[i])) return 1;
    return 0;
}

// How many callee-saved temps are live at once to hold a value across a call.
static int expr_tmp_depth(AstNode* n) {
    if (!n) return 0;
    if (n->type == AST_BINOP && n->child_count == 2) {
        int ld = expr_tmp_depth(n->children[0]);
        int rd = expr_tmp_depth(n->children[1]);
        if (node_has_call(n->children[1])) {
            int need = 1 + rd;
            return ld > need ? ld : need;
        }
        return ld > rd ? ld : rd;
    }
    int m = 0;
    for (int i = 0; i < n->child_count; i++) {
        int d = expr_tmp_depth(n->children[i]);
        if (d > m) m = d;
    }
    return m;
}

// Hosted -O2: each argument can be placed straight into its register.
// -O0 and freestanding keep the push/pop sequence below.
static int args_all_direct(Codegen* cg, AstNode* n, int argc) {
    static const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    if (!o2_hosted() || argc > 6) return 0;
    for (int i = 0; i < argc; i++) {
        if (is_struct_value_arg(cg, n->children[i])) return 0;
        if (!can_place(cg, n->children[i], regs[i])) return 0;
    }
    return 1;
}

// User-function call. Six or fewer register arguments keep the historical
// push/pop sequence at -O0 and in freestanding mode. Hosted -O2 places a
// simple argument in rdi..r9 directly. The syscall instruction and
// syscall()/syscall6() are not this path. Further arguments follow the
// System V stack convention (7th at [rbp+16]).
//
// bits 16 and bits 32 cannot `push rax` / `pop rdi`: that encoding does not
// exist, and a 64-bit `ret` would pop a return address the 16-bit `call`
// never pushed. Those functions are naked, so the callee already reads
// eax/ecx/edx/esi or ax/cx/dx/si. The caller evaluates each argument into
// the accumulator, pushes that width, and pops into those registers.
// Both sides must be the same bits mode. A near call is relative; in bits
// 16 the displacement is 16 bits, so the callee has to land within ±32KiB
// or nasm rejects the object. Indirect calls stay 64-bit: the name is what
// tells us the callee's mode.
static void gen_user_call(Codegen* cg, AstNode* n, int indirect) {
    if (legacy_bits()) {
        int mode = g_fn_bits;
        int argc = n->child_count;
        if (indirect) {
            diag_error(n->line, n->col, "an indirect call is not available in bits %d", mode);
            return;
        }
        if (argc > 4) {
            diag_error(n->line, n->col, "a bits %d call takes at most 4 register arguments", mode);
            return;
        }
        {
            AstNode* fn = find_func(cg, n->name);
            if (!fn) {
                diag_error(n->line, n->col, "unknown function '%s'", n->name ? n->name : "(call)");
            } else {
                int target = fn->bits ? fn->bits : 64;
                if (target != mode) {
                    if (target == 64)
                        diag_error(n->line, n->col, "a bits %d call cannot target a 64-bit function", mode);
                    else
                        diag_error(n->line, n->col,
                                   "a bits %d call cannot target a bits %d function", mode, target);
                } else {
                    int want = fn->child_count - 1;
                    if (argc != want)
                        diag_error(n->line, n->col, "function '%s' takes %d argument%s, not %d",
                                   n->name, want, want == 1 ? "" : "s", argc);
                    else
                        check_user_args(cg, n, fn);
                }
            }
        }
        {
            static const char* p16[] = {"ax", "cx", "dx", "si"};
            static const char* p32[] = {"eax", "ecx", "edx", "esi"};
            const char** pregs = mode == 16 ? p16 : p32;
            const char* acc = mode == 16 ? "ax" : "eax";
            for (int i = 0; i < argc; i++) {
                gen_one_arg(cg, n->children[i]);
                emit(cg, "    push %s\n", acc);
            }
            for (int i = argc - 1; i >= 0; i--)
                emit(cg, "    pop %s\n", pregs[i]);
            if (n->name) emit(cg, "    call %s\n", n->name);
        }
        return;
    }
    const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    int argc = n->child_count;
    if (argc > 16) {
        diag_error(n->line, n->col, "call has %d arguments; the maximum is 16", argc);
        argc = 16;
    }
    if (!indirect) {
        AstNode* fn = find_func(cg, n->name);
        if (!fn) {
            diag_error(n->line, n->col, "unknown function '%s'", n->name ? n->name : "(call)");
        } else {
            int want = fn->child_count - 1;
            if (argc != want) {
                diag_error(n->line, n->col, "function '%s' takes %d argument%s, not %d",
                           n->name, want, want == 1 ? "" : "s", argc);
            } else {
                check_user_args(cg, n, fn);
            }
        }
    }
    if (!indirect && argc <= 6 && args_all_direct(cg, n, argc)) {
        for (int i = 0; i < argc; i++)
            place_in(cg, n->children[i], regs[i]);
        emit(cg, "    call %s\n", n->name);
        return;
    }
    if (!indirect && argc <= 6) {
        for (int i = 0; i < argc; i++) {
            gen_one_arg(cg, n->children[i]);
            emit(cg, "    push rax\n");
        }
        for (int i = argc - 1; i >= 0; i--)
            emit(cg, "    pop %s\n", regs[i]);
        emit(cg, "    call %s\n", n->name);
        return;
    }
    if (argc <= 6) {
        for (int i = 0; i < argc; i++) {
            gen_one_arg(cg, n->children[i]);
            emit(cg, "    push rax\n");
        }
        gen_load_callee(cg, n->name);
        for (int i = argc - 1; i >= 0; i--)
            emit(cg, "    pop %s\n", regs[i]);
        emit(cg, "    call rbx\n");
        return;
    }
    emit(cg, "    sub rsp, %d\n", argc * 8);
    for (int i = 0; i < argc; i++) {
        gen_one_arg(cg, n->children[i]);
        emit(cg, "    mov [rsp+%d], rax\n", i * 8);
    }
    int nreg = argc < 6 ? argc : 6;
    for (int i = 0; i < nreg; i++)
        emit(cg, "    mov %s, [rsp+%d]\n", regs[i], i * 8);
    int stack_args = argc - 6;
    for (int i = 6; i < argc; i++) {
        emit(cg, "    mov rax, [rsp+%d]\n", i * 8);
        emit(cg, "    mov [rsp+%d], rax\n", (i - 6) * 8);
    }
    emit(cg, "    add rsp, %d\n", 6 * 8);
    if (indirect) {
        gen_load_callee(cg, n->name);
        emit(cg, "    call rbx\n");
    } else {
        emit(cg, "    call %s\n", n->name);
    }
    emit(cg, "    add rsp, %d\n", stack_args * 8);
}

// Helper: Generate builtin function calls
static const char* hosted_only_builtins[] = {
    "print", "println", "print_int", "malloc", "free", "open", "read", "write",
    "close", "exit", "syscall", "thread_spawn", "thread_join", "sched_setaffinity",
    NULL
};

// Evaluate up to two call arguments into rdx (first) and rax (second)
static void gen_two_args(Codegen* cg, AstNode* n) {
    gen_expr(cg, n->children[0]);
    if (legacy_bits()) {
        emit(cg, "    push eax\n");
        gen_expr(cg, n->children[1]);
        emit(cg, "    pop edx\n");
    } else {
        emit(cg, "    push rax\n");
        gen_expr(cg, n->children[1]);
        emit(cg, "    pop rdx\n");
    }
}

static const char* addr_data_label(Codegen* cg, AstNode* e);

// push/pop accept a wider set than the naked lvalues. Saving rax does not
// assign it, so a normal function may push it. 0 = not a register.
// 1 = segment, 2 = 16-bit, 3 = 32-bit, 4 = 64-bit.
static int stack_reg_class(const char* n) {
    if (!n) return 0;
    static const char* seg[] = {"ds", "es", "ss", "fs", "gs", NULL};
    static const char* r16[] = {"ax", "bx", "cx", "dx", "si", "di", "bp", "sp", NULL};
    static const char* r32[] = {"eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp", NULL};
    static const char* r64[] = {
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", NULL
    };
    for (int i = 0; seg[i]; i++) if (!strcmp(n, seg[i])) return 1;
    for (int i = 0; r16[i]; i++) if (!strcmp(n, r16[i])) return 2;
    for (int i = 0; r32[i]; i++) if (!strcmp(n, r32[i])) return 3;
    for (int i = 0; r64[i]; i++) if (!strcmp(n, r64[i])) return 4;
    return 0;
}

// bits 64 pushes 64-bit registers and segments. bits 16 and bits 32 push
// 16-bit and 32-bit registers (pushfd is popped into eax in real mode)
// and segments. `push rax` has no encoding in bits 16.
static int stack_reg_here(int cls) {
    if (cls == 1) return 1;
    if (cls == 4) return !legacy_bits();
    if (cls == 2 || cls == 3) return legacy_bits();
    return 0;
}

static int rep_insn(const char* f, const char** mnem, int* wide) {
    static const char* tab[] = {
        "rep_stosb", "rep stosb", "rep_stosw", "rep stosw",
        "rep_stosd", "rep stosd", "rep_stosq", "rep stosq",
        "rep_movsb", "rep movsb", "rep_movsw", "rep movsw",
        "rep_movsd", "rep movsd", "rep_movsq", "rep movsq",
    };
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0]; i += 2) {
        if (!strcmp(f, tab[i])) {
            *mnem = tab[i + 1];
            *wide = tab[i + 1][strlen(tab[i + 1]) - 1] == 'q';
            return 1;
        }
    }
    return 0;
}

static int is_data_name(Codegen* cg, const char* name);

// A `label name;` statement anywhere in the program.
static int ast_has_label(AstNode* n, const char* name) {
    if (!n || !name) return 0;
    if (n->type == AST_LABEL && n->name && !strcmp(n->name, name)) return 1;
    for (int i = 0; i < n->child_count; i++)
        if (ast_has_label(n->children[i], name)) return 1;
    return 0;
}

static int is_code_label(Codegen* cg, const char* name) {
    return cg && cg->program && ast_has_label(cg->program, name);
}

// Address of a context in r11. r11 is caller-saved and is not one of the
// registers the context holds, so the lea cannot clobber what we are about
// to save. Only a bare name or &name: a general expression would run through
// the accumulator and might use rbx.
static int emit_ctx_r11(Codegen* cg, AstNode* e) {
    int take_addr = 0;
    AstNode* id = e;
    if (e && e->type == AST_ADDR_OF && e->child_count == 1 && e->children[0] &&
        e->children[0]->type == AST_IDENT) {
        take_addr = 1;
        id = e->children[0];
    } else if (!e || e->type != AST_IDENT || !e->name) {
        return 0;
    }
    if (!id->name) return 0;
    Symbol* s = cg->symtab ? symtab_lookup_symbol(cg->symtab, id->name) : NULL;
    if (s) {
        if (s->hwreg) return 0;
        if (take_addr) {
            if (s->mem) emit(cg, "    lea r11, [%s]\n", s->mem);
            else emit(cg, "    lea r11, [rbp%d]\n", s->offset);
            return 1;
        }
        if (s->is_pointer || (s->type_name && s->type_name[0] == '*')) {
            if (s->mem) emit(cg, "    mov r11, [%s]\n", s->mem);
            else emit(cg, "    mov r11, [rbp%d]\n", s->offset);
            return 1;
        }
        return 0;
    }
    GlobalVar* g = cg->global_symtab ? global_symtab_lookup(cg->global_symtab, id->name) : NULL;
    if (g) {
        if (take_addr) {
            emit(cg, "    lea r11, [%s]\n", g->name);
            return 1;
        }
        if (g->is_pointer) {
            emit(cg, "    mov r11, [%s]\n", g->name);
            return 1;
        }
        return 0;
    }
    if (take_addr && is_data_name(cg, id->name)) {
        emit(cg, "    lea r11, [%s]\n", id->name);
        return 1;
    }
    return 0;
}

static int gen_save_context(Codegen* cg, AstNode* n) {
    if (n->child_count != 1) {
        diag_error(n->line, n->col, "save_context() takes the address of a context");
        return 1;
    }
    if (legacy_bits()) {
        diag_error(n->line, n->col, "save_context() is a 64-bit context switch");
        return 1;
    }
    if (!emit_ctx_r11(cg, n->children[0])) {
        diag_error(n->line, n->col, "save_context() takes the address of a context");
        return 1;
    }
    // Layout, one qword each: rip, rsp, rbp, rbx, r12, r13, r14, r15.
    // The resume address is the instruction after this builtin, not a label
    // the user has to name. rsp and rbp are the live frame, so a later
    // restore_context lands in this function with the epilogue still valid.
    int resume = new_label(cg);
    int done = new_label(cg);
    emit(cg, "    mov [r11+8], rsp\n");
    emit(cg, "    mov [r11+16], rbp\n");
    emit(cg, "    mov [r11+24], rbx\n");
    emit(cg, "    mov [r11+32], r12\n");
    emit(cg, "    mov [r11+40], r13\n");
    emit(cg, "    mov [r11+48], r14\n");
    emit(cg, "    mov [r11+56], r15\n");
    emit(cg, "    lea rax, [.L%d]\n", resume);
    emit(cg, "    mov [r11], rax\n");
    emit(cg, "    xor eax, eax\n");
    emit(cg, "    jmp .L%d\n", done);
    emit(cg, ".L%d:\n", resume);
    emit(cg, "    mov eax, 1\n");
    emit(cg, ".L%d:\n", done);
    return 1;
}

static int gen_restore_context(Codegen* cg, AstNode* n) {
    if (n->child_count != 1) {
        diag_error(n->line, n->col, "restore_context() takes the address of a context");
        return 1;
    }
    if (legacy_bits()) {
        diag_error(n->line, n->col, "restore_context() is a 64-bit context switch");
        return 1;
    }
    if (!emit_ctx_r11(cg, n->children[0])) {
        diag_error(n->line, n->col, "restore_context() takes the address of a context");
        return 1;
    }
    // Read every field through r11 before rsp changes. The jump does not return.
    emit(cg, "    mov rax, [r11]\n");
    emit(cg, "    mov rbx, [r11+24]\n");
    emit(cg, "    mov r12, [r11+32]\n");
    emit(cg, "    mov r13, [r11+40]\n");
    emit(cg, "    mov r14, [r11+48]\n");
    emit(cg, "    mov r15, [r11+56]\n");
    emit(cg, "    mov rbp, [r11+16]\n");
    emit(cg, "    mov rsp, [r11+8]\n");
    emit(cg, "    jmp rax\n");
    return 1;
}

static int gen_label_addr(Codegen* cg, AstNode* n) {
    if (n->child_count != 1 || !n->children[0] || n->children[0]->type != AST_IDENT ||
        !n->children[0]->name) {
        diag_error(n->line, n->col, "label_addr() takes a label name");
        return 1;
    }
    if (legacy_bits()) {
        diag_error(n->line, n->col, "label_addr() is a 64-bit address");
        return 1;
    }
    const char* name = n->children[0]->name;
    if (!is_code_label(cg, name) && !find_func(cg, name) && !is_data_name(cg, name))
        diag_error(n->line, n->col, "unknown label '%s'", name);
    emit(cg, "    lea rax, [%s]\n", name);
    return 1;
}

// x86-64 atomics. A load is a mov (acquire on x86). seq_cst load adds mfence.
// A release store is a mov. A seq_cst store is xchg, which locks by itself.
// fetch_add / atomic_add are lock xadd. swap is xchg. compare_exchange is
// lock cmpxchg and returns the old value, not a boolean. The pointer is in
// r11 and the addend or the desired value is in r10. Both are caller-saved,
// and a local lives there only when no call crosses its range.
static int gen_atomic_builtin(Codegen* cg, AstNode* n) {
    const char* f = n->name;
    int bits = 0, sg = 0;
    int store = is_atomic_store(f);
    int rmw = is_atomic_rmw(f);
    const char* width;
    const char* acc;
    const char* accd;
    if (!store && !rmw && strcmp(f, "fence") && strcmp(f, "cpu_pause")) return 0;
    if (legacy_bits()) {
        diag_error(n->line, n->col, "%s() is a 64-bit operation", f);
        return 1;
    }
    if (!strcmp(f, "fence")) {
        emit(cg, "    mfence\n");
        emit(cg, "    xor eax, eax\n");
        return 1;
    }
    if (!strcmp(f, "cpu_pause")) {
        emit(cg, "    pause\n");
        emit(cg, "    xor eax, eax\n");
        return 1;
    }
    if (n->child_count < 1 || !atomic_pointee(cg, n->children[0], &bits, &sg)) return 1;
    width = (bits == 32) ? "dword" : "qword";
    acc = (bits == 32) ? "eax" : "rax";
    accd = (bits == 32) ? "r10d" : "r10";
    gen_expr(cg, n->children[0]);
    emit(cg, "    push rax\n");
    if (!strcmp(f, "atomic_load_acquire") || !strcmp(f, "atomic_load_seq_cst")) {
        emit(cg, "    pop r11\n");
        emit(cg, "    mov %s, %s [r11]\n", acc, width);
        if (!strcmp(f, "atomic_load_seq_cst")) emit(cg, "    mfence\n");
        return 1;
    }
    if (store) {
        if (n->child_count >= 2) gen_expr(cg, n->children[1]);
        emit(cg, "    pop r11\n");
        if (!strcmp(f, "atomic_store_seq_cst")) emit(cg, "    xchg %s [r11], %s\n", width, acc);
        else emit(cg, "    mov %s [r11], %s\n", width, acc);
        emit(cg, "    xor eax, eax\n");
        return 1;
    }
    if (!strcmp(f, "compare_exchange")) {
        if (n->child_count >= 2) {
            gen_expr(cg, n->children[1]);
            emit(cg, "    push rax\n");
        }
        if (n->child_count >= 3) gen_expr(cg, n->children[2]);
        emit(cg, "    mov %s, %s\n", accd, acc);
        emit(cg, "    pop %s\n", acc);
        emit(cg, "    pop r11\n");
        emit(cg, "    lock cmpxchg %s [r11], %s\n", width, accd);
        return 1;
    }
    if (n->child_count >= 2) gen_expr(cg, n->children[1]);
    emit(cg, "    pop r11\n");
    if (!strcmp(f, "swap")) {
        emit(cg, "    xchg %s [r11], %s\n", width, acc);
        return 1;
    }
    if (!strcmp(f, "atomic_add")) emit(cg, "    mov %s, %s\n", accd, acc);
    emit(cg, "    lock xadd %s [r11], %s\n", width, acc);
    if (!strcmp(f, "atomic_add")) emit(cg, "    add %s, %s\n", acc, accd);
    return 1;
}

// Builtins that work everywhere, including --freestanding (kernel) code.
// Returns 1 if handled.
static int gen_core_builtin(Codegen* cg, AstNode* n) {
    const char* f = n->name;
    if (gen_atomic_builtin(cg, n)) return 1;
    if (!strcmp(f, "sizeof")) {
        if (n->child_count != 1) {
            diag_error(n->line, n->col, "sizeof() takes 1 argument");
            return 1;
        }
        int sz = sizeof_node(cg, n->children[0]);
        if (sz < 0) {
            diag_error(n->line, n->col, "sizeof() has no size for this type");
            return 1;
        }
        if (legacy_bits()) emit(cg, "    mov eax, %d\n", sz);
        else emit(cg, "    mov rax, %d\n", sz);
        return 1;
    }
    if (!strcmp(f, "offsetof")) {
        if (n->child_count != 2 || !n->children[0] || n->children[0]->type != AST_IDENT ||
            !n->children[0]->name || !n->children[1] || n->children[1]->type != AST_IDENT ||
            !n->children[1]->name) {
            diag_error(n->line, n->col, "offsetof(Struct, field) takes a struct name and a field name");
            return 1;
        }
        if (!typetab_lookup(cg->types, n->children[0]->name)) {
            diag_error(n->line, n->col, "offsetof() unknown struct '%s'", n->children[0]->name);
            return 1;
        }
        int off = typetab_field_offset(cg->types, n->children[0]->name, n->children[1]->name);
        if (off < 0) {
            diag_error(n->line, n->col, "struct '%s' has no field '%s'",
                       n->children[0]->name, n->children[1]->name);
            return 1;
        }
        if (legacy_bits()) emit(cg, "    mov eax, %d\n", off);
        else emit(cg, "    mov rax, %d\n", off);
        return 1;
    }
    if (!strcmp(f, "outb") || !strcmp(f, "outw") || !strcmp(f, "outl")) {
        if (n->child_count != 2) { diag_error(n->line, n->col, "%s(port, value) takes 2 arguments", f); return 1; }
        gen_two_args(cg, n);
        emit(cg, "    out dx, %s\n", f[3] == 'b' ? "al" : f[3] == 'w' ? "ax" : "eax");
        return 1;
    }
    if (!strcmp(f, "inb") || !strcmp(f, "inw") || !strcmp(f, "inl")) {
        if (n->child_count != 1) { diag_error(n->line, n->col, "%s(port) takes 1 argument", f); return 1; }
        gen_expr(cg, n->children[0]);
        if (legacy_bits()) emit(cg, "    mov edx, eax\n    xor eax, eax\n");
        else emit(cg, "    mov rdx, rax\n    xor rax, rax\n");
        emit(cg, "    in %s, dx\n", f[2] == 'b' ? "al" : f[2] == 'w' ? "ax" : "eax");
        return 1;
    }
    if (!strcmp(f, "hlt") || !strcmp(f, "cli") || !strcmp(f, "sti") || !strcmp(f, "pause") ||
        !strcmp(f, "swapgs") || !strcmp(f, "lodsb") || !strcmp(f, "stosb") || !strcmp(f, "stosd") ||
        !strcmp(f, "stc") || !strcmp(f, "clc") || !strcmp(f, "cld") || !strcmp(f, "std") ||
        !strcmp(f, "ud2")) {
        if (n->child_count != 0) {
            diag_error(n->line, n->col, "%s() takes no arguments", f);
            return 1;
        }
        emit(cg, "    %s\n", f);
        return 1;
    }
    if (!strcmp(f, "pushfd") || !strcmp(f, "popfd")) {
        if (n->child_count != 0) {
            diag_error(n->line, n->col, "%s() takes no arguments", f);
            return 1;
        }
        // bits 64 has no 32-bit push of EFLAGS. The mnemonic is pushfq.
        emit(cg, "    %s%s\n", f[0] == 'p' && f[1] == 'u' ? "pushf" : "popf",
             legacy_bits() ? "d" : "q");
        return 1;
    }
    {
        const char* mnem = NULL;
        int wide = 0;
        if (rep_insn(f, &mnem, &wide)) {
            if (n->child_count != 0) {
                diag_error(n->line, n->col, "%s() takes no arguments", f);
                return 1;
            }
            if (wide && legacy_bits()) {
                diag_error(n->line, n->col, "%s() is not available in bits %d", f, g_fn_bits);
                return 1;
            }
            emit(cg, "    %s\n", mnem);
            return 1;
        }
    }
    if (!strcmp(f, "push")) {
        if (n->child_count != 1) {
            diag_error(n->line, n->col, "push() takes 1 argument");
            return 1;
        }
        AstNode* a = n->children[0];
        if (a && a->type == AST_IDENT && a->name) {
            int cls = stack_reg_class(a->name);
            if (cls) {
                if (!stack_reg_here(cls))
                    diag_error(a->line, a->col, "register '%s' cannot be pushed in bits %d",
                               a->name, g_fn_bits ? g_fn_bits : 64);
                else emit(cg, "    push %s\n", a->name);
                return 1;
            }
        }
        if (a && a->type == AST_NUMBER && a->value) {
            emit(cg, "    push %s\n", a->value);
            return 1;
        }
        if (a) gen_expr(cg, a);
        emit(cg, "    push %s\n", legacy_bits() ? "eax" : "rax");
        return 1;
    }
    if (!strcmp(f, "pop")) {
        if (n->child_count != 1 || !n->children[0] || n->children[0]->type != AST_IDENT ||
            !n->children[0]->name || !stack_reg_class(n->children[0]->name)) {
            diag_error(n->line, n->col, "pop() takes a register name");
            return 1;
        }
        const char* rn = n->children[0]->name;
        if (!stack_reg_here(stack_reg_class(rn)))
            diag_error(n->children[0]->line, n->children[0]->col,
                       "register '%s' cannot be popped in bits %d", rn, g_fn_bits ? g_fn_bits : 64);
        else emit(cg, "    pop %s\n", rn);
        return 1;
    }
    if (!strcmp(f, "lidt") || !strcmp(f, "lgdt") || !strcmp(f, "invlpg")) {
        if (n->child_count != 1) { diag_error(n->line, n->col, "%s(ptr) takes 1 argument", f); return 1; }
        // `lgdt [gdtr]` is the 16-bit form. `lgdt [rax]` is not a 16-bit address.
        const char* lab = addr_data_label(cg, n->children[0]);
        if (lab && legacy_bits()) {
            emit(cg, "    %s [%s]\n", f, lab);
            return 1;
        }
        gen_expr(cg, n->children[0]);
        if (legacy_bits()) {
            if (g_fn_bits == 16) emit(cg, "    mov bx, ax\n    %s [bx]\n", f);
            else emit(cg, "    mov ebx, eax\n    %s [ebx]\n", f);
        } else {
            emit(cg, "    %s [rax]\n", f);
        }
        return 1;
    }
    if (!strcmp(f, "rdmsr")) {
        if (n->child_count != 1) { diag_error(n->line, n->col, "rdmsr(msr) takes 1 argument"); return 1; }
        gen_expr(cg, n->children[0]);
        if (legacy_bits()) {
            // Leave the high half in edx. A later wrmsr of this value keeps it.
            // `shl rdx, 32` is not encodable in bits 16 or bits 32.
            emit(cg, "    mov ecx, eax\n    rdmsr\n");
        } else {
            emit(cg, "    mov rcx, rax\n    rdmsr\n    shl rdx, 32\n    or rax, rdx\n");
        }
        return 1;
    }
    if (!strcmp(f, "wrmsr")) {
        if (n->child_count != 2) { diag_error(n->line, n->col, "wrmsr(msr, value) takes 2 arguments"); return 1; }
        if (legacy_bits()) {
            // edx:eax is the value. Zero the high half first so a plain integer
            // writes edx = 0; rdmsr inside the value expression overwrites edx
            // with the MSR's real high half, and the OR below preserves it.
            emit(cg, "    xor edx, edx\n");
            gen_expr(cg, n->children[1]);
            emit(cg, "    push eax\n    push edx\n");
            gen_expr(cg, n->children[0]);
            emit(cg, "    mov ecx, eax\n    pop edx\n    pop eax\n    wrmsr\n");
        } else {
            gen_two_args(cg, n);                 // rdx = msr, rax = value
            emit(cg, "    mov rcx, rdx\n    mov rdx, rax\n    shr rdx, 32\n    wrmsr\n");
        }
        return 1;
    }
    // ltr takes a selector value (the low 16 bits), not a pointer. lidt/lgdt
    // are the ones that load a memory operand.
    if (!strcmp(f, "ltr")) {
        if (n->child_count != 1) {
            diag_error(n->line, n->col, "ltr(selector) takes 1 argument");
            return 1;
        }
        if (!g_freestanding) {
            diag_error(n->line, n->col,
                       "ltr() is only available with --freestanding, --flat, or --boot");
            return 1;
        }
        gen_expr(cg, n->children[0]);
        emit(cg, "    ltr ax\n");
        return 1;
    }
    if (!strcmp(f, "iretq") || !strcmp(f, "sysretq")) {
        if (n->child_count != 0) {
            diag_error(n->line, n->col, "%s() takes no arguments", f);
            return 1;
        }
        if (!g_freestanding) {
            diag_error(n->line, n->col,
                       "%s() is only available with --freestanding, --flat, or --boot", f);
            return 1;
        }
        // NASM has no `sysretq` mnemonic. `o64 sysret` is REX.W SYSRET
        // (48 0F 07): RIP from rcx, RFLAGS from r11, ring 3.
        if (!strcmp(f, "sysretq")) emit(cg, "    o64 sysret\n");
        else emit(cg, "    iretq\n");
        return 1;
    }
    // CR0/CR2/CR3/CR4. Reads leave the register in rax (an i64 expression).
    // Writes take one value and leave that value in rax. Ring 0 only, so
    // hosted programs get a compile error instead of a faulting instruction.
    if (!strcmp(f, "read_cr0") || !strcmp(f, "read_cr2") || !strcmp(f, "read_cr3") ||
        !strcmp(f, "read_cr4")) {
        if (!g_freestanding) {
            diag_error(n->line, n->col, "%s() is only available with --freestanding or --flat", f);
            return 1;
        }
        if (n->child_count != 0) { diag_error(n->line, n->col, "%s() takes no arguments", f); return 1; }
        // CR moves are r32 outside long mode. `mov rax, cr0` does not assemble
        // in bits 16 or bits 32; `mov ax, cr0` is not an encoding either.
        if (legacy_bits()) emit(cg, "    mov eax, cr%c\n", f[7]);
        else emit(cg, "    mov rax, cr%c\n", f[7]);
        return 1;
    }
    if (!strcmp(f, "write_cr0") || !strcmp(f, "write_cr3") || !strcmp(f, "write_cr4")) {
        if (n->child_count != 1) { diag_error(n->line, n->col, "%s(value) takes 1 argument", f); return 1; }
        gen_expr(cg, n->children[0]);   // also diagnoses a nested read_cr* in hosted mode
        if (!g_freestanding) {
            diag_error(n->line, n->col, "%s() is only available with --freestanding or --flat", f);
            return 1;
        }
        if (legacy_bits()) emit(cg, "    mov cr%c, eax\n", f[8]);
        else emit(cg, "    mov cr%c, rax\n", f[8]);
        return 1;
    }
    if (!strcmp(f, "set_syscall_stack")) {
        if (n->child_count != 1) { diag_error(n->line, n->col, "set_syscall_stack(top) takes 1 argument"); return 1; }
        gen_expr(cg, n->children[0]);
        emit(cg, "    mov [__syscall_rsp], rax\n");
        cg->need_syscall_data = 1;
        return 1;
    }
    if (!strcmp(f, "bss_start") || !strcmp(f, "bss_end")) {
        if (!g_flat_org) { diag_error(n->line, n->col, "%s() is only available with --flat", f); return 1; }
        if (n->child_count != 0) { diag_error(n->line, n->col, "%s() takes no arguments", f); return 1; }
        emit(cg, "    mov rax, __%s\n", f);
        return 1;
    }
    if (!strcmp(f, "section_start") || !strcmp(f, "section_end")) {
        int is_end = !strcmp(f, "section_end");
        if (!g_freestanding) {
            diag_error(n->line, n->col, "%s() is only available with --freestanding, --flat, or --boot", f);
            return 1;
        }
        if (n->child_count != 1 || !n->children[0] || n->children[0]->type != AST_STRING) {
            diag_error(n->line, n->col, "%s() takes a section name", f);
            return 1;
        }
        const char* sn = n->children[0]->value;
        int standard = sn && (!strcmp(sn, ".text") || !strcmp(sn, ".data") || !strcmp(sn, ".bss"));
        NamedSection* sec = sn ? section_find(sn) : NULL;
        if (!sn || (!sec && !standard)) {
            diag_error(n->line, n->col, "unknown section '%s'", sn ? sn : "");
            return 1;
        }
        if (!sec) sec = section_ensure(sn);
        if (sec) {
            sec->need_start = 1;
            sec->need_end = 1;
        }
        if (g_fn_bits == 16 && sec && sec->has_addr && sec->addr > 0xFFFFULL) {
            diag_error(n->line, n->col, "section '%s' does not fit in bits 16", sn);
            return 1;
        }
        if (sec) {
            const char* which = is_end ? "end" : "start";
            if (g_fn_bits == 16) emit(cg, "    mov ax, %s_%s\n", sec->sym, which);
            else if (g_fn_bits == 32) emit(cg, "    mov eax, %s_%s\n", sec->sym, which);
            else emit(cg, "    mov rax, %s_%s\n", sec->sym, which);
        }
        return 1;
    }
    if (!strcmp(f, "ljmp")) {
        if (!g_freestanding) {
            diag_error(n->line, n->col, "ljmp() is only available with --freestanding, --flat, or --boot");
            return 1;
        }
        if (n->child_count != 2 || !n->children[0] || n->children[0]->type != AST_NUMBER ||
            !n->children[1] || n->children[1]->type != AST_IDENT) {
            diag_error(n->line, n->col, "ljmp(selector, label) takes an integer selector and a label name");
            return 1;
        }
        emit(cg, "    jmp %s:%s\n", n->children[0]->value, n->children[1]->name);
        return 1;
    }
    if (!strcmp(f, "far_jump")) {
        // bits 64 has no `jmp sel:label`. Push the selector and the address
        // of the next instruction, then retfq reloads CS and falls through.
        // The two pushes are popped by retfq, so a frame above them stays.
        if (!g_freestanding) {
            diag_error(n->line, n->col, "far_jump() is only available with --freestanding, --flat, or --boot");
            return 1;
        }
        if (legacy_bits()) {
            diag_error(n->line, n->col, "far_jump() is a 64-bit far return; use ljmp(selector, label) in bits 16 or 32");
            return 1;
        }
        if (n->child_count != 1 || !n->children[0] || n->children[0]->type != AST_NUMBER) {
            diag_error(n->line, n->col, "far_jump(selector) takes one integer literal");
            return 1;
        }
        int lab = new_label(cg);
        emit(cg, "    push %s\n", n->children[0]->value);
        emit(cg, "    lea rax, [.L%d]\n", lab);
        emit(cg, "    push rax\n");
        emit(cg, "    retfq\n");
        emit(cg, ".L%d:\n", lab);
        return 1;
    }
    if (!strcmp(f, "retfq")) {
        if (n->child_count != 0) { diag_error(n->line, n->col, "retfq() takes no arguments"); return 1; }
        if (legacy_bits()) emit(cg, "    retf\n");
        else emit(cg, "    retfq\n");
        return 1;
    }
    if (!strcmp(f, "syscall") && g_freestanding) {
        // The instruction, not the hosted Linux helper. No arguments leaves
        // the registers exactly as the caller set them (one `syscall` line,
        // so existing programs stay byte-identical). With arguments the
        // compiler loads rax, then rdi, rsi, rdx, r10, r8, r9, and the
        // result stays in rax. Up to six arguments after the number.
        if (legacy_bits()) {
            diag_error(n->line, n->col, "syscall() is a 64-bit instruction");
            return 1;
        }
        if (n->child_count > 7) {
            diag_error(n->line, n->col, "syscall() takes at most 6 arguments");
            return 1;
        }
        if (n->child_count == 0) {
            emit(cg, "    syscall\n");
            return 1;
        }
        {
            const char* sregs[] = {"rdi", "rsi", "rdx", "r10", "r8", "r9"};
            int narg = n->child_count - 1;
            if (narg > 6) narg = 6;
            for (int i = 0; i < narg; i++) {
                gen_expr(cg, n->children[i + 1]);
                emit(cg, "    push rax\n");
            }
            for (int i = narg - 1; i >= 0; i--)
                emit(cg, "    pop %s\n", sregs[i]);
            gen_expr(cg, n->children[0]);
            emit(cg, "    syscall\n");
        }
        return 1;
    }
    if (!strcmp(f, "int")) {
        if (!g_freestanding) {
            diag_error(n->line, n->col, "int() is only available with --freestanding, --flat, or --boot");
            return 1;
        }
        if (n->child_count != 1 || !n->children[0] || n->children[0]->type != AST_NUMBER) {
            diag_error(n->line, n->col, "int(n) takes one integer literal");
            return 1;
        }
        emit(cg, "    int %s\n", n->children[0]->value);
        return 1;
    }
    if (!strcmp(f, "int_carry")) {
        if (n->child_count != 0) { diag_error(n->line, n->col, "int_carry() takes no arguments"); return 1; }
        // CF as 0 or 1. No instruction between the flag and setc.
        if (legacy_bits()) emit(cg, "    setc al\n    movzx eax, al\n");
        else emit(cg, "    setc al\n    movzx rax, al\n");
        return 1;
    }
    if (!strcmp(f, "cpuid") || !strcmp(f, "cpuid_edx")) {
        if (n->child_count != 1) {
            diag_error(n->line, n->col, "%s(leaf) takes 1 argument", f);
            return 1;
        }
        gen_expr(cg, n->children[0]);
        emit(cg, "    cpuid\n");
        // cpuid writes edx. cpuid() returns eax (already the accumulator in
        // legacy mode; in 64-bit, eax is zero-extended into rax). cpuid_edx()
        // returns that edx so a long-mode feature bit is visible.
        if (!strcmp(f, "cpuid_edx")) {
            if (legacy_bits()) emit(cg, "    mov eax, edx\n");
            else emit(cg, "    mov rax, rdx\n");
        }
        return 1;
    }
    if (!strcmp(f, "asm")) {
        // asm("instruction; instruction") - raw instructions, ';' separates lines
        if (n->child_count != 1 || n->children[0]->type != AST_STRING) {
            diag_error(n->line, n->col, "asm() takes one string literal");
            return 1;
        }
        const char* src = n->children[0]->value;
        emit(cg, "    ; inline asm\n    ");
        for (const char* c = src; *c; c++) {
            if (*c == ';') emit(cg, "\n    ");
            else emit(cg, "%c", *c);
        }
        emit(cg, "\n");
        return 1;
    }
    if (!strcmp(f, "save_context")) return gen_save_context(cg, n);
    if (!strcmp(f, "restore_context")) return gen_restore_context(cg, n);
    if (!strcmp(f, "label_addr")) return gen_label_addr(cg, n);
    return 0;
}

// Evaluate the first `count` arguments left to right, then load them into
// `regs`. Evaluating everything first means a call inside a later argument
// (e.g. write(fd, s, strlen(s))) cannot clobber an earlier one.
static void gen_args_to_regs(Codegen* cg, AstNode* n, const char** regs, int count) {
    for (int i = 0; i < count; i++) {
        gen_expr(cg, n->children[i]);
        emit(cg, "    push rax\n");
    }
    for (int i = count - 1; i >= 0; i--) emit(cg, "    pop %s\n", regs[i]);
}

// print/println argument: a string literal (length known at compile time)
// or any expression yielding a pointer to a NUL-terminated string.
static void gen_print_arg(Codegen* cg, AstNode* arg) {
    gen_expr(cg, arg);
    if (arg->type == AST_STRING) {
        emit(cg, "    mov rsi, rax\n    mov rdx, rbx\n");
    } else {
        emit(cg, "    push rax\n    mov rdi, rax\n    call __strlen\n");
        emit(cg, "    mov rdx, rax\n    pop rsi\n");
    }
    emit(cg, "    mov rdi, 1\n    mov rax, 1\n    syscall\n");
}

void gen_builtin_call(Codegen* cg, AstNode* n) {
    check_builtin_args(cg, n);
    if (gen_core_builtin(cg, n)) return;
    if (g_freestanding) {
        for (int i = 0; hosted_only_builtins[i]; i++) {
            if (!strcmp(n->name, hosted_only_builtins[i])) {
                diag_error(n->line, n->col,
                           "'%s' needs the hosted runtime (Linux syscalls) and is not available with --freestanding",
                           n->name);
                return;
            }
        }
    }
    if (!strcmp(n->name, "print")) {
        if (n->child_count > 0) {
            gen_print_arg(cg, n->children[0]);
        }
    } else if (!strcmp(n->name, "println")) {
        if (n->child_count > 0) {
            gen_print_arg(cg, n->children[0]);
        }
        // Newline from a temporary stack slot (never overlaps locals)
        emit(cg, "    push 10\n    mov rsi, rsp\n");
        emit(cg, "    mov rdi, 1\n    mov rdx, 1\n    mov rax, 1\n    syscall\n");
        emit(cg, "    add rsp, 8\n");
    } else if (!strcmp(n->name, "print_int")) {
        if (n->child_count > 0) {
            gen_expr(cg, n->children[0]);
            emit(cg, "    call __print_int\n");
        }
    } else if (!strcmp(n->name, "exit")) {
        if (n->child_count > 0) {
            gen_expr(cg, n->children[0]);
            emit(cg, "    mov rdi, rax\n");
        } else {
            emit(cg, "    xor rdi, rdi\n");
        }
        emit(cg, "    mov rax, 60\n    syscall\n");
    } else if (!strcmp(n->name, "strcmp")) {
        if (n->child_count >= 2) {
            gen_expr(cg, n->children[0]);
            emit(cg, "    push rax\n");
            gen_expr(cg, n->children[1]);
            emit(cg, "    mov rsi, rax\n");
            emit(cg, "    pop rdi\n");
            emit(cg, "    call __strcmp\n");
        }
    } else if (!strcmp(n->name, "strcpy")) {
        if (n->child_count >= 2) {
            gen_expr(cg, n->children[0]);
            emit(cg, "    push rax\n");
            gen_expr(cg, n->children[1]);
            emit(cg, "    mov rsi, rax\n");
            emit(cg, "    pop rdi\n");
            emit(cg, "    call __strcpy\n");
        }
    } else if (!strcmp(n->name, "strlen")) {
        if (n->child_count >= 1) {
            gen_expr(cg, n->children[0]);
            emit(cg, "    mov rdi, rax\n");
            emit(cg, "    call __strlen\n");
        }
    } else if (!strcmp(n->name, "open")) {
        // open(filename, flags, mode) -> fd
        // syscall 2: rax=2, rdi=filename, rsi=flags, rdx=mode
        if (n->child_count >= 2) {
            static const char* r3[] = {"rdi", "rsi", "rdx"};
            if (n->child_count >= 3) {
                gen_args_to_regs(cg, n, r3, 3);  // filename, flags, mode
            } else {
                gen_args_to_regs(cg, n, r3, 2);  // filename, flags
                emit(cg, "    mov rdx, 420\n");  // Default permissions: 0644 octal
            }
            emit(cg, "    mov rax, 2\n");  // sys_open
            emit(cg, "    syscall\n");
        }
    } else if (!strcmp(n->name, "read")) {
        // read(fd, buffer, count) -> bytes_read
        // syscall 0: rax=0, rdi=fd, rsi=buffer, rdx=count
        if (n->child_count >= 3) {
            static const char* r3[] = {"rdi", "rsi", "rdx"};
            gen_args_to_regs(cg, n, r3, 3);  // fd, buffer, count
            emit(cg, "    mov rax, 0\n");  // sys_read
            emit(cg, "    syscall\n");
        }
    } else if (!strcmp(n->name, "write")) {
        // write(fd, buffer, count) -> bytes_written
        // syscall 1: rax=1, rdi=fd, rsi=buffer, rdx=count
        if (n->child_count >= 3) {
            static const char* r3[] = {"rdi", "rsi", "rdx"};
            gen_args_to_regs(cg, n, r3, 3);  // fd, buffer, count
            emit(cg, "    mov rax, 1\n");  // sys_write
            emit(cg, "    syscall\n");
        }
    } else if (!strcmp(n->name, "close")) {
        // close(fd) -> status
        // syscall 3: rax=3, rdi=fd
        if (n->child_count >= 1) {
            gen_expr(cg, n->children[0]);  // fd
            emit(cg, "    mov rdi, rax\n");
            emit(cg, "    mov rax, 3\n");  // sys_close
            emit(cg, "    syscall\n");
        }
    } else if (!strcmp(n->name, "malloc")) {
        // malloc(size) -> pointer
        // Uses mmap syscall (9) with size tracking header
        // Layout: [8 bytes size][allocated memory]
        // Returns pointer to allocated memory (after header)
        if (n->child_count >= 1) {
            gen_expr(cg, n->children[0]);  // size requested by user
            // rbx, not r12: hosted -O2 keeps locals in r12-r15 across this call.
            emit(cg, "    mov rbx, rax\n");  // Save original size in rbx
            emit(cg, "    add rax, 8\n");    // Add 8 bytes for size header
            emit(cg, "    mov rsi, rax\n");  // length = size + 8
            emit(cg, "    xor rdi, rdi\n");  // addr = 0 (let kernel choose)
            emit(cg, "    mov rdx, 3\n");    // prot = PROT_READ | PROT_WRITE
            emit(cg, "    mov r10, 34\n");   // flags = MAP_PRIVATE | MAP_ANONYMOUS (0x22)
            emit(cg, "    mov r8, -1\n");    // fd = -1
            emit(cg, "    xor r9, r9\n");    // offset = 0
            emit(cg, "    mov rax, 9\n");    // sys_mmap
            emit(cg, "    syscall\n");
            // Check if mmap failed (returns -1)
            emit(cg, "    cmp rax, -1\n");
            emit(cg, "    je .Lmalloc_failed_%d\n", new_label(cg));
            // Store size in header
            emit(cg, "    mov [rax], rbx\n");  // Store original size in first 8 bytes
            emit(cg, "    add rax, 8\n");      // Return pointer after header
            emit(cg, ".Lmalloc_failed_%d:\n", cg->label_count - 1);
            // Returns pointer in rax (ptr+8, or -1 on error which stays -1)
        }
    } else if (!strcmp(n->name, "free")) {
        // free(ptr) -> status
        // Uses munmap syscall (11): munmap(addr, length)
        // Reads size from header at (ptr - 8)
        if (n->child_count >= 1) {
            gen_expr(cg, n->children[0]);  // ptr (user pointer)
            emit(cg, "    ; free(ptr) - read size from header and munmap\n");
            emit(cg, "    test rax, rax\n");  // Check if ptr is NULL
            emit(cg, "    jz .Lfree_null_%d\n", new_label(cg));
            emit(cg, "    mov rdi, rax\n");   // Save user pointer
            emit(cg, "    sub rdi, 8\n");     // rdi = actual allocation start (header)
            emit(cg, "    mov rsi, [rdi]\n"); // Load size from header
            emit(cg, "    add rsi, 8\n");     // Add header size to get total allocation
            emit(cg, "    mov rax, 11\n");    // sys_munmap
            emit(cg, "    syscall\n");
            // Returns 0 on success, -1 on error
            emit(cg, "    jmp .Lfree_done_%d\n", cg->label_count - 1);
            emit(cg, ".Lfree_null_%d:\n", cg->label_count - 1);
            emit(cg, "    xor rax, rax\n");   // free(NULL) returns 0
            emit(cg, ".Lfree_done_%d:\n", cg->label_count - 1);
        }
    } else if (!strcmp(n->name, "thread_spawn")) {
        // clone(2) flags: VM|FS|FILES|SIGHAND|THREAD|SYSVSEM|PARENT_SETTID|CHILD_CLEARTID.
        // The child starts at the next instruction with rsp = stack top, aligned
        // down to 16 so `call` enters the function at rsp ≡ 8 (mod 16). It calls
        // the function and exits that thread (syscall 60), which clears the tid
        // word. The parent returns the tid. r12 is restored only on the parent
        // path: the push lives on the parent stack, not the child's.
        int parent = new_label(cg);
        if (legacy_bits()) {
            diag_error(n->line, n->col, "thread_spawn() is a 64-bit operation");
        } else if (n->child_count == 3) {
            emit(cg, "    push r12\n");
            gen_expr(cg, n->children[0]);
            emit(cg, "    push rax\n");
            gen_expr(cg, n->children[1]);
            emit(cg, "    push rax\n");
            gen_expr(cg, n->children[2]);
            emit(cg, "    mov r10, rax\n");
            emit(cg, "    mov rdx, rax\n");
            emit(cg, "    pop rsi\n");
            emit(cg, "    pop r12\n");
            emit(cg, "    and rsi, -16\n");
            emit(cg, "    mov edi, 0x350F00\n");
            emit(cg, "    xor r8, r8\n");
            emit(cg, "    mov eax, 56\n");
            emit(cg, "    syscall\n");
            emit(cg, "    test rax, rax\n");
            emit(cg, "    jnz .L%d\n", parent);
            emit(cg, "    call r12\n");
            emit(cg, "    xor edi, edi\n");
            emit(cg, "    mov eax, 60\n");
            emit(cg, "    syscall\n");
            emit(cg, "    hlt\n");
            emit(cg, ".L%d:\n", parent);
            emit(cg, "    pop r12\n");
        }
    } else if (!strcmp(n->name, "thread_join")) {
        // Wait until the kernel stores 0 in the tid word (CHILD_CLEARTID) and
        // wakes the futex. FUTEX_WAIT sleeps only while the word still holds
        // the value we just read, so a clear between the load and the syscall
        // comes back and the loop sees 0.
        int loop = new_label(cg);
        int done = new_label(cg);
        if (legacy_bits()) {
            diag_error(n->line, n->col, "thread_join() is a 64-bit operation");
        } else         if (n->child_count == 1) {
            // rbx, not r11: syscall clobbers rcx and r11. rbx is scratch
            // in 64-bit code and is not a local register.
            gen_expr(cg, n->children[0]);
            emit(cg, "    mov rbx, rax\n");
            emit(cg, ".L%d:\n", loop);
            emit(cg, "    mov eax, dword [rbx]\n");
            emit(cg, "    test eax, eax\n");
            emit(cg, "    jz .L%d\n", done);
            emit(cg, "    mov edx, eax\n");
            emit(cg, "    mov rdi, rbx\n");
            emit(cg, "    xor esi, esi\n");
            emit(cg, "    xor r10, r10\n");
            emit(cg, "    xor r8, r8\n");
            emit(cg, "    xor r9, r9\n");
            emit(cg, "    mov eax, 202\n");
            emit(cg, "    syscall\n");
            emit(cg, "    jmp .L%d\n", loop);
            emit(cg, ".L%d:\n", done);
            emit(cg, "    xor eax, eax\n");
        }
    } else if (!strcmp(n->name, "sched_setaffinity")) {
        // sched_setaffinity(0, 128, mask). cpu >= 1024 returns -1 and does
        // not call the kernel. The mask is 16 zero qwords on the stack.
        int bad = new_label(cg);
        int out = new_label(cg);
        if (legacy_bits()) {
            diag_error(n->line, n->col, "sched_setaffinity() is a 64-bit operation");
        } else if (n->child_count == 1) {
            gen_expr(cg, n->children[0]);
            emit(cg, "    cmp rax, 1024\n");
            emit(cg, "    jae .L%d\n", bad);
            emit(cg, "    mov r11, rax\n");
            emit(cg, "    sub rsp, 128\n");
            emit(cg, "    xor eax, eax\n");
            emit(cg, "    mov ecx, 16\n");
            emit(cg, "    mov rdi, rsp\n");
            emit(cg, "    cld\n");
            emit(cg, "    rep stosq\n");
            emit(cg, "    mov rcx, r11\n");
            emit(cg, "    and rcx, 7\n");
            emit(cg, "    shr r11, 3\n");
            emit(cg, "    mov eax, 1\n");
            emit(cg, "    shl eax, cl\n");
            emit(cg, "    mov byte [rsp+r11], al\n");
            emit(cg, "    xor edi, edi\n");
            emit(cg, "    mov esi, 128\n");
            emit(cg, "    mov rdx, rsp\n");
            emit(cg, "    mov eax, 203\n");
            emit(cg, "    syscall\n");
            emit(cg, "    add rsp, 128\n");
            emit(cg, "    jmp .L%d\n", out);
            emit(cg, ".L%d:\n", bad);
            emit(cg, "    mov rax, -1\n");
            emit(cg, ".L%d:\n", out);
        }
    } else if (!strcmp(n->name, "syscall") || !strcmp(n->name, "syscall6")) {
        // Generic syscall: syscall6(num, arg1, arg2, arg3, arg4, arg5, arg6)
        // This allows Copihue code to make ANY syscall directly
        // rax=num, rdi=arg1, rsi=arg2, rdx=arg3, r10=arg4, r8=arg5, r9=arg6

        if (n->child_count >= 1) {
            // First, evaluate all arguments and push them to stack
            // We need to do this in reverse order to pop them correctly
            const char* regs[] = {"rdi", "rsi", "rdx", "r10", "r8", "r9"};

            // Push all arguments
            for (int i = 1; i < n->child_count && i <= 6; i++) {
                gen_expr(cg, n->children[i]);
                emit(cg, "    push rax\n");
            }

            // Pop arguments into registers (reverse order)
            for (int i = (n->child_count - 1 < 6 ? n->child_count - 1 : 6); i >= 1; i--) {
                emit(cg, "    pop %s\n", regs[i-1]);
            }

            // Syscall number in rax
            gen_expr(cg, n->children[0]);  // syscall number
            emit(cg, "    syscall\n");
            // Result already in rax
        }
    } else {
        // Regular function call, or a call through a function pointer variable.
        Symbol* loc = n->name ? symtab_lookup_symbol(cg->symtab, n->name) : NULL;
        GlobalVar* gv = (!loc && n->name) ? global_symtab_lookup(cg->global_symtab, n->name) : NULL;
        AstNode* fn = n->name ? find_func(cg, n->name) : NULL;
        int indirect = (loc || (gv && !fn)) ? 1 : 0;
        gen_user_call(cg, n, indirect);
    }
}

// A `data` block name, or a label written inside one (`mark:`).
static int is_data_name(Codegen* cg, const char* name) {
    if (!cg || !cg->program || !name) return 0;
    AstNode* prog = cg->program;
    for (int i = 0; i < prog->child_count; i++) {
        AstNode* d = prog->children[i];
        if (!d || d->type != AST_DATA) continue;
        if (d->name && !strcmp(d->name, name)) return 1;
        for (int j = 0; j < d->child_count; j++) {
            AstNode* it = d->children[j];
            if (it && it->name && !strcmp(it->name, "label") &&
                it->value && !strcmp(it->value, name))
                return 1;
        }
    }
    return 0;
}

// `&gdtr` or a bare `gdtr` when that name is a data label. NULL otherwise.
static const char* addr_data_label(Codegen* cg, AstNode* e) {
    if (!e) return NULL;
    if (e->type == AST_IDENT && is_data_name(cg, e->name)) return e->name;
    if (e->type == AST_ADDR_OF && e->child_count > 0 && e->children[0] &&
        e->children[0]->type == AST_IDENT && is_data_name(cg, e->children[0]->name))
        return e->children[0]->name;
    return NULL;
}

// Helper: Generate address-of operator (&)
void gen_addr_of(Codegen* cg, AstNode* n) {
    if (!n->children || n->child_count == 0) return;
    AstNode* var = n->children[0];
    if (!var) return;

    if (var->type == AST_IDENT) {
        Symbol* hw = var->name ? symtab_lookup_symbol(cg->symtab, var->name) : NULL;
        if (hw && hw->hwreg) {
            diag_error(var->line, var->col, "naked parameter '%s' has no address", var->name);
            return;
        }
        int off = symtab_lookup(cg->symtab, var->name);
        if (off) {
            // Check if this is already a pointer - if so, just load the value
            Symbol* sym = symtab_lookup_symbol(cg->symtab, var->name);
            if (sym && sym->is_pointer) {
                // Variable is already a pointer - just load it
                if (sym->mem && legacy_bits()) emit(cg, "    mov eax, [%s]\n", sym->mem);
                else if (sym->mem) emit(cg, "    mov rax, [%s]\n", sym->mem);
                else emit(cg, "    mov rax, [rbp%d]\n", off);
            } else if (sym && sym->mem && legacy_bits()) {
                emit(cg, "    mov eax, %s\n", sym->mem);
            } else if (sym && sym->mem) {
                emit(cg, "    lea rax, [%s]\n", sym->mem);
            } else {
                // Regular variable - take its address
                emit(cg, "    lea rax, [rbp%d]\n", off);
            }
        } else {
            // Global variable: take its static address
            GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, var->name);
            if (gvar) {
                if (gvar->is_pointer) emit(cg, "    mov rax, [%s]\n", gvar->name);
                else emit(cg, "    lea rax, [%s]\n", gvar->name);
            } else if (find_func(cg, var->name)) {
                // Address of a function, for an IDT gate or a syscall table.
                emit(cg, "    lea rax, [%s]\n", var->name);
            } else if (is_data_name(cg, var->name)) {
                // `data gdtr` is a label, not a `let`. bits 16/32 cannot `lea rax`.
                if (legacy_bits()) emit(cg, "    mov eax, %s\n", var->name);
                else emit(cg, "    lea rax, [%s]\n", var->name);
            } else {
                diag_error(var->line, var->col, "use of undeclared variable '%s'", var->name);
            }
        }
    } else if (var->type == AST_INDEX) {
        // &array[index] - compute address of element
        if (!var->children || var->child_count < 2) return;
        AstNode* arr = var->children[0];
        if (!arr || !arr->name) return;

        Symbol* sym = symtab_lookup_symbol(cg->symtab, arr->name);
        if (!sym) {
            // &global_array[index]
            GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, arr->name);
            if (gvar && gvar->is_array) {
                int elem_size = 1;
                if (gvar->elem_type) {
                    if (!strcmp(gvar->elem_type, "i32") || !strcmp(gvar->elem_type, "u32")) elem_size = 4;
                    else if (!strcmp(gvar->elem_type, "i64") || !strcmp(gvar->elem_type, "u64")) elem_size = 8;
                    else if ((!strcmp(gvar->elem_type, "i16") || !strcmp(gvar->elem_type, "u16"))) elem_size = 2;
                    else {
                        StructType* est = typetab_lookup(cg->types, gvar->elem_type);
                        if (est && est->size > 0) elem_size = est->size;
                    }
                }
                gen_expr(cg, var->children[1]);  // Index expression
                if (elem_size > 1) emit_scale_reg(cg, "rax", elem_size);
                emit(cg, "    lea rbx, [%s]\n", gvar->name);
                emit(cg, "    add rax, rbx\n");
            }
            return;
        }
        if (sym) {
            gen_expr(cg, var->children[1]);  // Index expression

            // Bounds checking for address-of. Primitive arrays keep the
            // historical count (slot count / 8). A struct array uses the
            // declared length so &table[0] is valid for a short table.
            if (sym->size > 0 && !g_freestanding) {
                int array_count = sym->size / 8;
                if (sym->is_array && sym->type_name && sym->array_len > 0) {
                    StructType* est = typetab_lookup(cg->types, sym->type_name);
                    if (est && est->size > 0) array_count = sym->array_len;
                }
                int ok_label = new_label(cg);

                emit(cg, "    test rax, rax\n");
                emit(cg, "    js .Lbounds_error_%d\n", ok_label);
                emit(cg, "    cmp rax, %d\n", array_count);
                emit(cg, "    jge .Lbounds_error_%d\n", ok_label);
                emit(cg, "    jmp .Lbounds_ok_%d\n", ok_label);

                emit(cg, ".Lbounds_error_%d:\n", ok_label);
                char* err_msg = strtab_add(cg->strtab, "Array bounds error\\n", 19);
                emit(cg, "    mov rsi, %s\n", err_msg);
                emit(cg, "    mov rdx, 19\n");
                emit(cg, "    mov rdi, 2\n");
                emit(cg, "    mov rax, 1\n");
                emit(cg, "    syscall\n");
                emit(cg, "    mov rdi, 1\n");
                emit(cg, "    mov rax, 60\n");
                emit(cg, "    syscall\n");

                emit(cg, ".Lbounds_ok_%d:\n", ok_label);
            }

            // Address: rbp + offset + index * stride. Primitive arrays stay
            // at stride 8 (the historical layout). Struct arrays use the
            // struct's real size so packed records stay contiguous.
            int stride = 8;
            if (sym->is_array && sym->type_name) {
                StructType* est = typetab_lookup(cg->types, sym->type_name);
                if (est && est->size > 0) stride = est->size;
            }
            emit_scale_reg(cg, "rax", stride);
            emit(cg, "    lea rbx, [rbp%d]\n", sym->offset);
            emit(cg, "    add rax, rbx\n");
        }
    }
}

// Helper: Generate array indexing with bounds checking
// Indexing x[i] requires x to be an array or a pointer. A plain scalar
// (e.g. a parameter declared `buf: i32`) would otherwise index the stack slot.
static void check_indexable(Codegen* cg, AstNode* arr) {
    if (!arr || arr->type != AST_IDENT) return;
    Symbol* sym = symtab_lookup_symbol(cg->symtab, arr->name);
    if (!sym || sym->is_pointer || sym->is_array) return;
    if (sym->type_name && sym->type_name[0] == '*') return;
    if (sym->type_name && typetab_lookup(cg->types, sym->type_name)) return;
    diag_error(arr->line, arr->col,
               "cannot index '%s': it is not an array or a pointer (declare it as e.g. '%s: *i8')",
               arr->name, arr->name);
}

void gen_array_index(Codegen* cg, AstNode* n) {
    const char* want = g_index_dest;
    g_index_dest = NULL;
    AstNode* arr = n->children[0];
    AstNode* ixn = (n->child_count > 1) ? n->children[1] : NULL;
    if (g_fwd_ok && arr && arr->type == AST_IDENT && arr->name && ixn &&
        ixn->type == AST_IDENT && ixn->name &&
        !strcmp(arr->name, g_fwd_arr) && !strcmp(ixn->name, g_fwd_idx)) {
        g_fwd_ok = 0;
        if (want && strcmp(want, "rax") != 0) emit(cg, "    mov %s, rax\n", want);
        if (want) g_index_hit = 1;
        return;
    }
    g_fwd_ok = 0;
    check_indexable(cg, arr);

    // Handle field access indexing: lex.source[i], or e.name[i] for [T; N]
    if (arr->type == AST_FIELD_ACCESS) {
        if (gen_embedded_array(cg, arr, n->children[1], NULL)) return;
        AstNode* obj = arr->children[0];
        char* field_name = arr->name;

        Symbol* sym = symtab_lookup_symbol(cg->symtab, obj->name);
        if (sym && sym->type_name) {
            // Get struct type (remove pointer if present)
            char* struct_type = sym->type_name;
            int is_pointer = 0;
            if (struct_type[0] == '*') {
                struct_type = struct_type + 1;
                is_pointer = 1;
            }

            int field_off = typetab_field_offset(cg->types, struct_type, field_name);
            if (field_off >= 0) {
                // Get field type to determine element size
                char* field_type = typetab_field_type(cg->types, struct_type, field_name);
                int elem_size = 1;  // Default to i8
                int is_struct_elem = 0;

                if (field_type) {
                    char* base_type = field_type;
                    if (base_type[0] == '*') base_type++;  // Skip '*' to get element type

                    if (!strcmp(base_type, "i8") || !strcmp(base_type, "u8")) {
                        elem_size = 1;
                    } else if ((!strcmp(base_type, "i16") || !strcmp(base_type, "u16"))) {
                        elem_size = 2;
                    } else if (!strcmp(base_type, "i32") || !strcmp(base_type, "u32")) {
                        elem_size = 4;
                    } else if (!strcmp(base_type, "i64") || !strcmp(base_type, "u64")) {
                        elem_size = 8;
                    } else if (base_type[0] == '*') {
                        elem_size = 8;
                    } else {
                        // Not a primitive type - check if it's a struct
                        StructType* st = typetab_lookup(cg->types, base_type);
                        if (st) {
                            elem_size = st->size;
                            is_struct_elem = 1;
                        }
                    }
                }

                // Generate index expression first
                gen_expr(cg, n->children[1]);  // Index in rax

                // Scale index by element size
                if (elem_size > 1) {
                    emit_scale_reg(cg, "rax", elem_size);
                }

                emit(cg, "    push rax\n");     // Save scaled index

                // Load the pointer from struct field
                if (is_pointer) {
                    emit(cg, "    mov rbx, [rbp%d]\n", sym->offset);  // Load struct pointer
                    if (field_off == 0) {
                        emit(cg, "    mov rbx, [rbx]\n");  // Load field (pointer)
                    } else {
                        emit(cg, "    mov rbx, [rbx+%d]\n", field_off);  // Load field (pointer)
                    }
                } else {
                    emit(cg, "    mov rbx, [rbp%d]\n", sym->offset + field_off);  // Load field (pointer)
                }

                emit(cg, "    pop rax\n");      // Restore scaled index
                emit(cg, "    add rbx, rax\n"); // Add scaled index to pointer

                // For structs, return address; for primitives, load value
                if (is_struct_elem) {
                    emit(cg, "    mov rax, rbx\n");  // Return address of struct element
                } else {
                    // Load element based on size
                    if (elem_size == 1) {
                        emit(cg, "    movzx rax, byte [rbx]\n");
                    } else if (elem_size == 2) {
                        emit(cg, "    movzx rax, word [rbx]\n");
                    } else if (elem_size == 4) {
                        emit(cg, "    mov eax, [rbx]\n");
                    } else {
                        emit(cg, "    mov rax, [rbx]\n");
                    }
                }
                return;
            }
        }
    }

    // Handle string literal indexing: "Hello"[0]
    if (arr->type == AST_STRING) {
        char* label = strtab_add(cg->strtab, arr->value, strlen(arr->value));
        int str_len = strlen(arr->value);

        gen_expr(cg, n->children[1]);  // Index in rax

        // Bounds checking
        int ok_label = new_label(cg);
        emit(cg, "    test rax, rax\n");
        emit(cg, "    js .Lbounds_error_%d\n", ok_label);
        emit(cg, "    cmp rax, %d\n", str_len);
        emit(cg, "    jge .Lbounds_error_%d\n", ok_label);
        emit(cg, "    jmp .Lbounds_ok_%d\n", ok_label);

        emit(cg, ".Lbounds_error_%d:\n", ok_label);
        char* err_msg = strtab_add(cg->strtab, "Array bounds error\\n", 19);
        emit(cg, "    mov rsi, %s\n", err_msg);
        emit(cg, "    mov rdx, 19\n");
        emit(cg, "    mov rdi, 2\n");
        emit(cg, "    mov rax, 1\n");
        emit(cg, "    syscall\n");
        emit(cg, "    mov rdi, 1\n");
        emit(cg, "    mov rax, 60\n");
        emit(cg, "    syscall\n");

        emit(cg, ".Lbounds_ok_%d:\n", ok_label);

        // Load byte from string: string_label + index
        emit(cg, "    lea rbx, [%s]\n", label);
        emit(cg, "    add rbx, rax\n");
        emit(cg, "    movzx rax, byte [rbx]\n");
        return;
    }

    Symbol* sym = symtab_lookup_symbol(cg->symtab, arr->name);

    // Try local array/pointer first
    if (sym) {
        // Hosted -O2: a scalar index that already lives in a register is the
        // SIB index. The pointer path still wants the index in rax.
        const char* idxreg = NULL;
        if (!sym->is_pointer && o2_hosted() && n->children[1] &&
            n->children[1]->type == AST_IDENT) {
            int el = 8;
            if (sym->type_name) {
                if (!strcmp(sym->type_name, "i8") || !strcmp(sym->type_name, "u8")) el = 1;
                else if (!strcmp(sym->type_name, "i16") || !strcmp(sym->type_name, "u16")) el = 2;
                else if (!strcmp(sym->type_name, "i32") || !strcmp(sym->type_name, "u32")) el = 4;
                else if (!strcmp(sym->type_name, "i64") || !strcmp(sym->type_name, "u64")) el = 8;
                else {
                    StructType* st = typetab_lookup(cg->types, sym->type_name);
                    if (st) el = st->size;
                }
            }
            if (scale_ok(el)) {
                Symbol* isym = symtab_lookup_symbol(cg->symtab, n->children[1]->name);
                idxreg = local_reg(isym);
            }
        }
        if (!idxreg) gen_expr(cg, n->children[1]);  // Index in rax

        // Check if this is a pointer parameter (not an array)
        if (sym->is_pointer) {
            // This is a pointer - no bounds checking, just index through it
            // Determine element size from type_name (e.g., "*i8", "*i64", "*Point")
            int elem_size = 1;  // Default to i8
            int is_struct = 0;

            if (sym->type_name) {
                char* base_type = sym->type_name;
                if (base_type[0] == '*') base_type++;  // Skip '*'

                if (!strcmp(base_type, "i8") || !strcmp(base_type, "u8")) {
                    elem_size = 1;
                } else if ((!strcmp(base_type, "i16") || !strcmp(base_type, "u16"))) {
                    elem_size = 2;
                } else if (!strcmp(base_type, "i32") || !strcmp(base_type, "u32")) {
                    elem_size = 4;
                } else if (!strcmp(base_type, "i64") || !strcmp(base_type, "u64")) {
                    elem_size = 8;
                } else if (base_type[0] == '*') {
                    elem_size = 8;
                } else {
                    // Not a primitive type - check if it's a struct
                    StructType* st = typetab_lookup(cg->types, base_type);
                    if (st) {
                        elem_size = st->size;
                        is_struct = 1;
                    }
                }
            }

            emit(cg, "    mov rbx, [rbp%d]\n", sym->offset);  // Load pointer

            // Scale index by element size
            if (elem_size > 1) {
                emit_scale_reg(cg, "rax", elem_size);
            }

            emit(cg, "    add rbx, rax\n");  // Add scaled index

            // For structs, return address in rax (for field access)
            // For primitives, load the value
            if (is_struct) {
                emit(cg, "    mov rax, rbx\n");  // Return address of struct element
            } else {
                const char* mark = sym->volatile_pointee ? "  ; volatile" : "";
                // Load element based on size
                if (elem_size == 1) {
                    emit(cg, "    movzx rax, byte [rbx]%s\n", mark);
                } else if (elem_size == 2) {
                    emit(cg, "    movzx rax, word [rbx]%s\n", mark);
                } else if (elem_size == 4) {
                    emit(cg, "    mov eax, [rbx]%s\n", mark);
                } else {
                    emit(cg, "    mov rax, [rbx]%s\n", mark);
                }
            }
            return;
        }

        // It's a real array - do bounds checking
        // Determine element size from type_name
        int elem_size = 8;  // Default to 8 bytes (i64)
        int array_count = sym->size;  // size is already the count for typed arrays
        int is_struct = 0;

        if (sym->type_name) {
            if (!strcmp(sym->type_name, "i8") || !strcmp(sym->type_name, "u8")) {
                elem_size = 1;
            } else if ((!strcmp(sym->type_name, "i16") || !strcmp(sym->type_name, "u16"))) {
                elem_size = 2;
            } else if (!strcmp(sym->type_name, "i32") || !strcmp(sym->type_name, "u32")) {
                elem_size = 4;
            } else if (!strcmp(sym->type_name, "i64") || !strcmp(sym->type_name, "u64")) {
                elem_size = 8;
            } else {
                // Not a primitive - check if struct
                StructType* st = typetab_lookup(cg->types, sym->type_name);
                if (st) {
                    elem_size = st->size;
                    is_struct = 1;
                }
            }
        } else {
            // If no type_name, assume old behavior (size is in qwords)
            array_count = sym->size;
        }

        // Bounds checking. idxreg is non-NULL only at hosted -O2; the rax
        // text is what -O0 and freestanding already emit.
        const char* breg = idxreg ? idxreg : "rax";
        int proven = o2_hosted() && g_proven_idx && n->children[1] &&
            n->children[1]->type == AST_IDENT && n->children[1]->name &&
            !strcmp(n->children[1]->name, g_proven_idx) &&
            g_proven_lim >= 0 && (long long)array_count >= g_proven_lim;
        if (!proven && sym->size > 0 && !g_freestanding) {
            if (o2_hosted()) {
                int skip_neg = g_nonneg_reg && !strcmp(g_nonneg_reg, breg);
                emit_bounds_hot(cg, breg, array_count, skip_neg);
            } else {
            int ok_label = new_label(cg);

            emit(cg, "    test %s, %s\n", breg, breg);
            emit(cg, "    js .Lbounds_error_%d\n", ok_label);

            emit(cg, "    cmp %s, %d\n", breg, array_count);
            emit(cg, "    jge .Lbounds_error_%d\n", ok_label);
            emit(cg, "    jmp .Lbounds_ok_%d\n", ok_label);

            emit(cg, ".Lbounds_error_%d:\n", ok_label);
            char* err_msg = strtab_add(cg->strtab, "Array bounds error\\n", 19);
            emit(cg, "    mov rsi, %s\n", err_msg);
            emit(cg, "    mov rdx, 19\n");
            emit(cg, "    mov rdi, 2\n");
            emit(cg, "    mov rax, 1\n");
            emit(cg, "    syscall\n");
            emit(cg, "    mov rdi, 1\n");
            emit(cg, "    mov rax, 60\n");
            emit(cg, "    syscall\n");

            emit(cg, ".Lbounds_ok_%d:\n", ok_label);
            }
        }

        // base + index*scale in one instruction when the element size is a
        // legal SIB scale. Otherwise the historical imul/rbx sequence.
        if (o2_hosted() && scale_ok(elem_size)) {
            char mem[96];
            snprintf(mem, sizeof mem, "rbp%d+%s*%d", sym->offset, breg, elem_size);
            emit_scaled_load(cg, mem, elem_size, is_struct, want ? want : "rax");
            if (want) g_index_hit = 1;
        } else if (o2_hosted() && is_struct) {
            emit_scale_reg(cg, "rax", elem_size);
            emit(cg, "    lea rax, [rbp%d+rax]\n", sym->offset);
        } else {
            if (elem_size > 1) {
                emit_scale_reg(cg, "rax", elem_size);
            }
            emit(cg, "    mov rbx, rax\n");
            if (is_struct) {
                emit(cg, "    lea rax, [rbp%d+rbx]\n", sym->offset);
            } else if (elem_size == 1) {
                emit(cg, "    movzx rax, byte [rbp%d+rbx]\n", sym->offset);
            } else if (elem_size == 2) {
                emit(cg, "    movzx rax, word [rbp%d+rbx]\n", sym->offset);
            } else if (elem_size == 4) {
                emit(cg, "    mov eax, [rbp%d+rbx]\n", sym->offset);
            } else {
                emit(cg, "    mov rax, [rbp%d+rbx]\n", sym->offset);
            }
        }
    } else {
        // Try global array
        GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, arr->name);
        if (gvar && gvar->is_array) {
            // For i8 arrays, element size is 1 byte
            // For i32/i64, element size is 4/8 bytes
            int elem_size = 1;  // Default to byte for i8
            int is_struct_elem = 0;
            if (gvar->elem_type) {
                if ((!strcmp(gvar->elem_type, "i32") || !strcmp(gvar->elem_type, "u32"))) elem_size = 4;
                else if ((!strcmp(gvar->elem_type, "i64") || !strcmp(gvar->elem_type, "u64"))) elem_size = 8;
                else if ((!strcmp(gvar->elem_type, "i16") || !strcmp(gvar->elem_type, "u16"))) elem_size = 2;
                else {
                    StructType* est = typetab_lookup(cg->types, gvar->elem_type);
                    if (est && est->size > 0) { elem_size = est->size; is_struct_elem = 1; }
                }
            }
            const char* idxreg = NULL;
            if (o2_hosted() && scale_ok(elem_size) && n->children[1] &&
                n->children[1]->type == AST_IDENT) {
                Symbol* isym = symtab_lookup_symbol(cg->symtab, n->children[1]->name);
                idxreg = local_reg(isym);
            }
            if (!idxreg) gen_expr(cg, n->children[1]);  // Index in rax

            if (o2_hosted() && scale_ok(elem_size)) {
                char mem[128];
                snprintf(mem, sizeof mem, "%s+%s*%d", gvar->name, idxreg ? idxreg : "rax", elem_size);
                emit_scaled_load(cg, mem, elem_size, is_struct_elem, want ? want : "rax");
                if (want) g_index_hit = 1;
            } else if (o2_hosted() && is_struct_elem) {
                emit_scale_reg(cg, "rax", elem_size);
                emit(cg, "    lea rax, [%s+rax]\n", gvar->name);
            } else {
                if (elem_size > 1) {
                    emit_scale_reg(cg, "rax", elem_size);
                }
                emit(cg, "    lea rbx, [%s]\n", gvar->name);
                emit(cg, "    add rbx, rax\n");
                if (is_struct_elem) {
                    emit(cg, "    mov rax, rbx\n");
                } else if (elem_size == 1) {
                    emit(cg, "    movzx rax, byte [rbx]\n");
                } else if (elem_size == 2) {
                    emit(cg, "    movzx rax, word [rbx]\n");
                } else if (elem_size == 4) {
                    emit(cg, "    mov eax, [rbx]\n");
                } else {
                    emit(cg, "    mov rax, [rbx]\n");
                }
            }
        } else if (gvar && gvar->is_pointer) {
            gen_expr(cg, n->children[1]);
            const char* base = gvar->pointee_type;
            int elem_size = 1;
            int is_struct_elem = 0;
            if (base) {
                if (!strcmp(base, "i8") || !strcmp(base, "u8")) elem_size = 1;
                else if (!strcmp(base, "i16") || !strcmp(base, "u16")) elem_size = 2;
                else if (!strcmp(base, "i32") || !strcmp(base, "u32")) elem_size = 4;
                else if (!strcmp(base, "i64") || !strcmp(base, "u64")) elem_size = 8;
                else if (base[0] == '*') elem_size = 8;
                else {
                    StructType* est = typetab_lookup(cg->types, (char*)base);
                    if (est && est->size > 0) { elem_size = est->size; is_struct_elem = 1; }
                }
            }
            emit(cg, "    mov rbx, [%s]\n", gvar->name);
            if (elem_size > 1) emit_scale_reg(cg, "rax", elem_size);
            emit(cg, "    add rbx, rax\n");
            if (is_struct_elem) {
                emit(cg, "    mov rax, rbx\n");
            } else {
                const char* mark = gvar->volatile_pointee ? "  ; volatile" : "";
                if (elem_size == 1) emit(cg, "    movzx rax, byte [rbx]%s\n", mark);
                else if (elem_size == 2) emit(cg, "    movzx rax, word [rbx]%s\n", mark);
                else if (elem_size == 4) emit(cg, "    mov eax, [rbx]%s\n", mark);
                else emit(cg, "    mov rax, [rbx]%s\n", mark);
            }
        } else {
            emit(cg, "    mov rax, 0  ; array not found\n");
        }
    }
}

// Store a struct literal into the frame at [rbp+base]. Nested struct
// literals are stored in place; struct-typed variables are copied.
static void gen_struct_literal_at(Codegen* cg, AstNode* n, int base) {
    for (int i = 0; i < n->child_count; i++) {
        AstNode* field_assign = n->children[i];
        char* field_name = field_assign->name;
        AstNode* field_value = field_assign->children[0];
        int field_off = typetab_field_offset(cg->types, n->struct_type, field_name);
        if (field_off < 0) {
            diag_error(field_assign->line, field_assign->col,
                       "struct '%s' has no field named '%s'", n->struct_type, field_name);
            continue;
        }
        char* ftype = typetab_field_type(cg->types, n->struct_type, field_name);
        StructType* fst = (ftype && ftype[0] != '*') ? typetab_lookup(cg->types, ftype) : NULL;
        if (field_value->type == AST_STRUCT_LITERAL) {
            gen_struct_literal_at(cg, field_value, base + field_off);
        } else if (fst && field_value->type == AST_IDENT) {
            Symbol* src = symtab_lookup_symbol(cg->symtab, field_value->name);
            if (src) {
                for (int b = 0; b < fst->size; b += 8) {
                    emit(cg, "    mov rax, [rbp%+d]\n", src->offset + b);
                    emit(cg, "    mov [rbp%+d], rax\n", base + field_off + b);
                }
            }
        } else {
            gen_expr(cg, field_value);
            int psz = 8, psg = 0, poff = field_off, pvol = 0;
            if (field_mem_info(cg, n->struct_type, field_name, &poff, &psz, &psg, &pvol)) {
                char mem[64];
                snprintf(mem, sizeof(mem), "rbp%+d", base + poff);
                emit_sized_store_mark(cg, mem, psz, 0, pvol);
            } else {
                emit(cg, "    mov [rbp%+d], rax\n", base + field_off);
            }
        }
    }
}

// Offset of `field` in the struct that `ptr` (an identifier of type *Struct)
// points to, for ptr->field. Reports an error and returns -1 if unresolved.
static int deref_field_offset(Codegen* cg, AstNode* ptr, const char* field, AstNode* at) {
    const char* type = NULL;
    if (ptr && ptr->type == AST_IDENT && ptr->name) {
        Symbol* sym = symtab_lookup_symbol(cg->symtab, ptr->name);
        if (sym && sym->type_name) type = sym->type_name;
    }
    if (!type) {
        diag_error(at->line, at->col, "'->%s' needs a variable of pointer-to-struct type on the left", field);
        return -1;
    }
    if (type[0] == '*') type++;
    int off = typetab_field_offset(cg->types, (char*)type, (char*)field);
    if (off < 0) diag_error(at->line, at->col, "struct '%s' has no field '%s'", type, field);
    return off;
}

// ---- struct values (copy) and volatile pointees ----

static int is_record(Codegen* cg, const char* t) {
    return t && typetab_lookup(cg->types, (char*)t) != NULL;
}

static char* strip_star(char* t) {
    if (t && t[0] == '*') return t + 1;
    return t;
}

static char* array_elem_type_name(Codegen* cg, AstNode* arr) {
    if (!arr || arr->type != AST_IDENT || !arr->name) return NULL;
    Symbol* s = symtab_lookup_symbol(cg->symtab, arr->name);
    if (s) {
        if (s->is_array && s->type_name) return s->type_name;
        if (s->is_pointer && s->type_name) return strip_star(s->type_name);
    }
    GlobalVar* g = global_symtab_lookup(cg->global_symtab, arr->name);
    if (!g) return NULL;
    if (g->is_array && g->elem_type) return g->elem_type;
    if (g->is_pointer && g->pointee_type) return g->pointee_type;
    return NULL;
}

static char* obj_record_type(Codegen* cg, AstNode* obj);

static char* expr_struct_type(Codegen* cg, AstNode* n) {
    if (!n) return NULL;
    if (n->type == AST_IDENT && n->name) {
        Symbol* s = symtab_lookup_symbol(cg->symtab, n->name);
        if (s && !s->is_pointer && !s->is_array && is_record(cg, s->type_name))
            return s->type_name;
        GlobalVar* g = global_symtab_lookup(cg->global_symtab, n->name);
        if (g && !g->is_pointer && !g->is_array && is_record(cg, g->type_name))
            return g->type_name;
        return NULL;
    }
    if (n->type == AST_FIELD_ACCESS && n->child_count == 1 && n->name) {
        char* rec = obj_record_type(cg, n->children[0]);
        if (!rec) return NULL;
        char* ft = typetab_field_type(cg->types, rec, n->name);
        if (ft && ft[0] != '*' && is_record(cg, ft)) return ft;
        return NULL;
    }
    if (n->type == AST_INDEX && n->child_count >= 1) {
        char* et = array_elem_type_name(cg, n->children[0]);
        if (et && et[0] != '*' && is_record(cg, et)) return et;
        return NULL;
    }
    return NULL;
}

static char* obj_record_type(Codegen* cg, AstNode* obj) {
    if (!obj) return NULL;
    if (obj->type == AST_IDENT && obj->name) {
        Symbol* s = symtab_lookup_symbol(cg->symtab, obj->name);
        if (s && s->type_name && !s->is_array) {
            char* t = s->is_pointer ? strip_star(s->type_name) : s->type_name;
            if (is_record(cg, t)) return t;
        }
        GlobalVar* g = global_symtab_lookup(cg->global_symtab, obj->name);
        if (g && !g->is_array && !g->is_pointer && is_record(cg, g->type_name)) return g->type_name;
        if (g && g->is_pointer && is_record(cg, g->pointee_type)) return g->pointee_type;
        return NULL;
    }
    if (obj->type == AST_DEREF && obj->child_count == 1)
        return obj_record_type(cg, obj->children[0]);
    if (obj->type == AST_INDEX && obj->child_count >= 1) {
        char* et = array_elem_type_name(cg, obj->children[0]);
        if (et && is_record(cg, et)) return et;
        return NULL;
    }
    if (obj->type == AST_FIELD_ACCESS && obj->child_count == 1 && obj->name) {
        char* rec = obj_record_type(cg, obj->children[0]);
        if (!rec) return NULL;
        char* ft = typetab_field_type(cg->types, rec, obj->name);
        if (ft && ft[0] != '*' && is_record(cg, ft)) return ft;
        return NULL;
    }
    return NULL;
}

static int gen_struct_addr(Codegen* cg, AstNode* n);

// Address of `obj.field` (the field itself, which may be an embedded struct).
static int gen_field_addr(Codegen* cg, AstNode* obj, const char* field) {
    char* rec = obj_record_type(cg, obj);
    if (!rec || !field) return 0;
    int off = typetab_field_offset(cg->types, rec, (char*)field);
    if (off < 0) return 0;
    if (obj->type == AST_IDENT && obj->name) {
        Symbol* s = symtab_lookup_symbol(cg->symtab, obj->name);
        if (s && !s->is_pointer && !s->is_array) {
            emit(cg, "    lea rax, [rbp%d]\n", s->offset + off);
            return 1;
        }
        if (s && s->is_pointer) {
            emit(cg, "    mov rax, [rbp%d]\n", s->offset);
            if (off) emit(cg, "    add rax, %d\n", off);
            return 1;
        }
        GlobalVar* g = global_symtab_lookup(cg->global_symtab, obj->name);
        if (g && !g->is_pointer && !g->is_array) {
            if (off) emit(cg, "    lea rax, [%s+%d]\n", g->name, off);
            else emit(cg, "    lea rax, [%s]\n", g->name);
            return 1;
        }
        if (g && g->is_pointer) {
            emit(cg, "    mov rax, [%s]\n", g->name);
            if (off) emit(cg, "    add rax, %d\n", off);
            return 1;
        }
    }
    if (obj->type == AST_DEREF && obj->child_count == 1) {
        gen_expr(cg, obj->children[0]);
        if (off) emit(cg, "    add rax, %d\n", off);
        return 1;
    }
    if (obj->type == AST_INDEX || obj->type == AST_FIELD_ACCESS) {
        if (!gen_struct_addr(cg, obj)) return 0;
        if (off) emit(cg, "    add rax, %d\n", off);
        return 1;
    }
    return 0;
}

// Address of a struct value: a variable, an embedded field, or an array element.
static int gen_struct_addr(Codegen* cg, AstNode* n) {
    if (!n || !expr_struct_type(cg, n)) return 0;
    if (n->type == AST_IDENT && n->name) {
        Symbol* s = symtab_lookup_symbol(cg->symtab, n->name);
        if (s && !s->is_pointer && !s->is_array) {
            emit(cg, "    lea rax, [rbp%d]\n", s->offset);
            return 1;
        }
        GlobalVar* g = global_symtab_lookup(cg->global_symtab, n->name);
        if (g && !g->is_pointer && !g->is_array) {
            emit(cg, "    lea rax, [%s]\n", g->name);
            return 1;
        }
        return 0;
    }
    if (n->type == AST_INDEX) {
        gen_array_index(cg, n);  // struct elements leave their address in rax
        return 1;
    }
    if (n->type == AST_FIELD_ACCESS && n->child_count == 1)
        return gen_field_addr(cg, n->children[0], n->name);
    return 0;
}

static StructField* access_field(Codegen* cg, AstNode* fa) {
    char* rec;
    if (!cg || !fa || fa->type != AST_FIELD_ACCESS || !fa->name || fa->child_count < 1)
        return NULL;
    rec = obj_record_type(cg, fa->children[0]);
    if (!rec) return NULL;
    return typetab_field(cg->types, rec, fa->name);
}

// Size of a type name, or -1 when this compiler cannot lay it out.
static int known_type_size(Codegen* cg, const char* name, int is_pointer) {
    int al = 1;
    StructType* st;
    if (is_pointer || (name && name[0] == '*'))
        return type_layout(cg ? cg->types : NULL, name, 1, &al, 0);
    if (!name) return -1;
    if (prim_width(name) || name[0] == '[')
        return type_layout(cg ? cg->types : NULL, name, 0, &al, 0);
    st = (cg && cg->types) ? typetab_lookup(cg->types, (char*)name) : NULL;
    if (st && st->align > 0) return st->size;
    return -1;
}

// sizeof(T), sizeof(value), or sizeof(obj.field). -1 if the size is not known.
// A scalar local still occupies an 8-byte slot; this reports the type's width.
static int sizeof_node(Codegen* cg, AstNode* n) {
    if (!n || !cg) return -1;
    if (n->type == AST_FIELD_ACCESS) {
        StructField* f = access_field(cg, n);
        if (!f) return -1;
        return f->size;
    }
    if (n->type != AST_IDENT || !n->name) return -1;
    {
        Symbol* s = cg->symtab ? symtab_lookup_symbol(cg->symtab, n->name) : NULL;
        if (s) {
            if (s->is_array && s->array_len > 0 && s->type_name) {
                int es = known_type_size(cg, s->type_name, 0);
                if (es <= 0 || s->array_len > 2147483647 / es) return -1;
                return es * s->array_len;
            }
            if (s->is_pointer || (s->type_name && s->type_name[0] == '*')) return 8;
            if (s->type_name) return known_type_size(cg, s->type_name, 0);
            return -1;
        }
    }
    {
        GlobalVar* g = cg->global_symtab ? global_symtab_lookup(cg->global_symtab, n->name) : NULL;
        if (g) {
            if (g->is_array && g->array_count > 0 && g->elem_type) {
                int es = known_type_size(cg, g->elem_type, 0);
                if (es <= 0 || g->array_count > 2147483647 / es) return -1;
                return es * g->array_count;
            }
            if (g->is_pointer) return 8;
            if (g->type_name) return known_type_size(cg, g->type_name, 0);
            return -1;
        }
    }
    return known_type_size(cg, n->name, 0);
}

static void emit_elem_load_rax(Codegen* cg, int elem, int sg) {
    if (elem <= 1)
        emit(cg, sg ? "    movsx rax, byte [rax]\n" : "    movzx rax, byte [rax]\n");
    else if (elem == 2)
        emit(cg, sg ? "    movsx rax, word [rax]\n" : "    movzx rax, word [rax]\n");
    else if (elem == 4)
        emit(cg, sg ? "    movsxd rax, dword [rax]\n" : "    mov eax, [rax]\n");
    else
        emit(cg, "    mov rax, [rax]\n");
}

static void emit_elem_store_rcx(Codegen* cg, int elem) {
    if (elem <= 1) emit(cg, "    mov byte [rcx], al\n");
    else if (elem == 2) emit(cg, "    mov word [rcx], ax\n");
    else if (elem == 4) emit(cg, "    mov dword [rcx], eax\n");
    else emit(cg, "    mov [rcx], rax\n");
}

// Index an embedded [T; N] field. value == NULL loads the element (a struct
// element leaves its address in rax). Returns 1 when this was an array field,
// including when the layout or the address cannot be honored.
static int gen_embedded_array(Codegen* cg, AstNode* field, AstNode* index, AstNode* value) {
    StructField* f = access_field(cg, field);
    int elem, is_struct;
    if (!f || f->array_len <= 0) return 0;
    if (f->size <= 0 || f->size % f->array_len != 0) {
        diag_error(field->line, field->col, "cannot lay out field '%s'", field->name ? field->name : "?");
        return 1;
    }
    elem = f->size / f->array_len;
    is_struct = 0;
    if (f->type_name && f->type_name[0] != '*' && !prim_width(f->type_name) &&
        typetab_lookup(cg->types, f->type_name))
        is_struct = 1;
    if (value && is_struct) {
        diag_error(field->line, field->col, "cannot assign a whole struct element of field '%s'",
                   field->name ? field->name : "?");
        return 1;
    }
    if (value) {
        gen_expr(cg, peel_store_cast(value, elem * 8));
        emit(cg, "    push rax\n");
    }
    if (index) gen_expr(cg, index);
    else emit(cg, "    xor rax, rax\n");
    emit_scale_reg(cg, "rax", elem);
    emit(cg, "    push rax\n");
    if (!field->children || !gen_field_addr(cg, field->children[0], field->name)) {
        emit(cg, "    pop rax\n");
        if (value) emit(cg, "    pop rax\n");
        diag_error(field->line, field->col, "cannot index field '%s'", field->name ? field->name : "?");
        return 1;
    }
    emit(cg, "    pop rbx\n");
    emit(cg, "    add rax, rbx\n");
    if (value) {
        emit(cg, "    mov rcx, rax\n    pop rax\n");
        emit_elem_store_rcx(cg, elem);
    } else if (!is_struct) {
        emit_elem_load_rax(cg, elem, f->is_signed);
    }
    return 1;
}

// Copy exactly `sz` bytes. A short tail is copied a byte at a time so a packed
// record does not spill into the next array element or field.
static void emit_copy_mem(Codegen* cg, const char* dst_reg, const char* src_reg, int sz) {
    int f = 0;
    for (; f + 8 <= sz; f += 8) {
        emit(cg, "    mov rax, [%s+%d]\n", src_reg, f);
        emit(cg, "    mov [%s+%d], rax\n", dst_reg, f);
    }
    for (; f < sz; f++) {
        emit(cg, "    mov al, [%s+%d]\n", src_reg, f);
        emit(cg, "    mov [%s+%d], al\n", dst_reg, f);
    }
}

// Returns 1 if `dst` is a struct value (copied, or diagnosed). 0 if it is not.
static int gen_struct_assign(Codegen* cg, AstNode* dst, AstNode* src) {
    char* dt = expr_struct_type(cg, dst);
    if (!dt) return 0;
    char* st = src ? expr_struct_type(cg, src) : NULL;
    if (!st || strcmp(dt, st) != 0) {
        diag_error(dst->line, dst->col, "cannot assign struct '%s' to struct '%s'",
                   st ? st : "(non-struct)", dt);
        return 1;
    }
    StructType* sty = typetab_lookup(cg->types, dt);
    int sz = sty && sty->size > 0 ? sty->size : 0;
    if (!gen_struct_addr(cg, src)) {
        diag_error(src->line, src->col, "cannot copy this struct value");
        return 1;
    }
    emit(cg, "    push rax\n");
    if (!gen_struct_addr(cg, dst)) {
        emit(cg, "    pop rax\n");
        diag_error(dst->line, dst->col, "cannot copy this struct value");
        return 1;
    }
    emit(cg, "    mov rcx, rax\n");
    emit(cg, "    pop rbx\n");
    emit_copy_mem(cg, "rcx", "rbx", sz);
    return 1;
}

// *volatile T: width and signedness of one pointee access. 0 if not volatile.
static int volatile_pointee_info(Codegen* cg, AstNode* p, int* sz, int* sg) {
    if (!p || p->type != AST_IDENT || !p->name) return 0;
    const char* tn = NULL;
    int vp = 0;
    Symbol* s = symtab_lookup_symbol(cg->symtab, p->name);
    if (s && s->is_pointer && s->volatile_pointee) {
        vp = 1;
        tn = s->type_name ? strip_star(s->type_name) : NULL;
    } else if (!s) {
        GlobalVar* g = global_symtab_lookup(cg->global_symtab, p->name);
        if (g && g->is_pointer && g->volatile_pointee) {
            vp = 1;
            tn = g->pointee_type;
        }
    }
    if (!vp || !tn) return 0;
    StructType* rec = typetab_lookup(cg->types, (char*)tn);
    if (rec && rec->size > 8) { *sz = 8; *sg = 0; return 1; }
    if (rec && rec->size > 0) { *sz = rec->size; *sg = 0; return 1; }
    *sz = type_size(tn);
    *sg = (tn[0] == 'i');
    return 1;
}

// A value that can be computed into rbx/rcx without touching rax or the stack.
// The right child of a binary op has to be a leaf: two registers, no third.
static int rbx_leaf(Codegen* cg, AstNode* n) {
    if (!n) return 0;
    if (n->type == AST_NUMBER) return 1;
    if (n->type == AST_UNARY && n->op && n->child_count == 1 &&
        (n->op[0] == '-' || n->op[0] == '!' || n->op[0] == '~'))
        return rbx_leaf(cg, n->children[0]);
    if (n->type != AST_IDENT || !n->name) return 0;
    Symbol* sym = symtab_lookup_symbol(cg->symtab, n->name);
    if (sym) {
        if (sym->hwreg) return 1;
        if (sym->is_array) return 0;
        if (sym->size > 1 && !sym->is_pointer) return 0;
        return 1;
    }
    GlobalVar* gv = global_symtab_lookup(cg->global_symtab, n->name);
    return gv && !gv->is_array;
}

static int can_emit_rbx(Codegen* cg, AstNode* n) {
    if (!n) return 0;
    if (rbx_leaf(cg, n)) return 1;
    if (n->type == AST_UNARY && n->op && n->child_count == 1 &&
        (n->op[0] == '-' || n->op[0] == '!' || n->op[0] == '~'))
        return can_emit_rbx(cg, n->children[0]);
    if (n->type == AST_BINOP && n->op && n->child_count == 2 &&
        n->op[0] != '/' && n->op[0] != '%')
        return can_emit_rbx(cg, n->children[0]) && rbx_leaf(cg, n->children[1]);
    return 0;
}

static void emit_leaf_reg(Codegen* cg, AstNode* n, const char* reg) {
    if (n->type == AST_NUMBER) {
        emit(cg, "    mov %s, %s\n", reg, n->value);
        return;
    }
    if (n->type == AST_UNARY) {
        emit_leaf_reg(cg, n->children[0], reg);
        if (n->op[0] == '-') emit(cg, "    neg %s\n", reg);
        else if (n->op[0] == '~') emit(cg, "    not %s\n", reg);
        else {
            const char* lo = (reg[0] == 'r' && reg[1] == 'b') ? "bl" : "cl";
            emit(cg, "    test %s, %s\n    setz %s\n    movzx %s, %s\n", reg, reg, lo, reg, lo);
        }
        return;
    }
    Symbol* sym = symtab_lookup_symbol(cg->symtab, n->name);
    if (sym) {
        if (sym->hwreg) {
            if (sym->hwbits >= 64) emit(cg, "    mov %s, %s\n", reg, sym->hwreg);
            else if (sym->hwbits == 32) {
                const char* d = !strcmp(reg, "rbx") ? "ebx" : "ecx";
                emit(cg, "    mov %s, %s\n", d, sym->hwreg);
            } else emit(cg, "    movzx %s, %s\n", reg, sym->hwreg);
        } else if (local_reg(sym)) emit(cg, "    mov %s, %s\n", reg, local_reg(sym));
        else emit(cg, "    mov %s, [rbp%d]\n", reg, sym->offset);
        return;
    }
    GlobalVar* gv = global_symtab_lookup(cg->global_symtab, n->name);
    if (gv && gv->is_volatile) emit(cg, "    mov %s, [%s]  ; volatile\n", reg, gv->name);
    else if (gv) emit(cg, "    mov %s, [%s]\n", reg, gv->name);
    else emit(cg, "    xor %s, %s\n", reg, reg);
}

static void emit_rbx_binop(Codegen* cg, const char* op) {
    if (!strcmp(op, "<<")) emit(cg, "    shl rbx, cl\n");
    else if (!strcmp(op, ">>")) emit(cg, "    sar rbx, cl\n");
    else if (op[0] == '&') emit(cg, "    and rbx, rcx\n");
    else if (op[0] == '|') emit(cg, "    or rbx, rcx\n");
    else if (op[0] == '^') emit(cg, "    xor rbx, rcx\n");
    else if (op[0] == '+') emit(cg, "    add rbx, rcx\n");
    else if (op[0] == '-') emit(cg, "    sub rbx, rcx\n");
    else if (op[0] == '*') emit(cg, "    imul rbx, rcx\n");
}

// Result in rbx. Does not read or write rax.
static void emit_rbx(Codegen* cg, AstNode* n) {
    if (n->type == AST_BINOP) {
        emit_rbx(cg, n->children[0]);
        emit_leaf_reg(cg, n->children[1], "rcx");
        emit_rbx_binop(cg, n->op);
        {
            VTy t = ty_binop(cg, n);
            if (t.kind == VK_INT && !t.lit && t.bits < 64)
                emit_reg_trunc(cg, "rbx", t.bits, t.sg);
        }
        return;
    }
    if (n->type == AST_UNARY) {
        emit_rbx(cg, n->children[0]);
        if (n->op[0] == '-') emit(cg, "    neg rbx\n");
        else if (n->op[0] == '~') emit(cg, "    not rbx\n");
        else emit(cg, "    test rbx, rbx\n    setz bl\n    movzx rbx, bl\n");
        if ((n->op[0] == '-' || n->op[0] == '~') && !n->op[1]) {
            VTy t = expr_ty(cg, n);
            if (t.kind == VK_INT && !t.lit && t.bits < 64)
                emit_reg_trunc(cg, "rbx", t.bits, t.sg);
        }
        return;
    }
    emit_leaf_reg(cg, n, "rbx");
}

// eax/ebx form for bits 16 and bits 32. edx is not touched by + - & | ^ *,
// so an rdmsr high half survives `rdmsr(m) | imm`. The caller push/pops ebx.
// u32 / and % zero edx for `div`. u32 >> is logical.
static void emit_binop_legacy(Codegen* cg, const char* op, int u32) {
    // A shift count lives in cl. ecx is also a named register (the E820
    // buffer size), so put it back after shl/shr/sar, in a condition and outside.
    if (!strcmp(op, "<<") || !strcmp(op, ">>")) {
        const char* ins = "shl";
        if (op[0] == '>') ins = u32 ? "shr" : "sar";
        emit(cg, "    push ecx\n");
        emit(cg, "    mov ecx, ebx\n    %s eax, cl\n", ins);
        emit(cg, "    pop ecx\n");
    } else if (op[0] == '&') emit(cg, "    and eax, ebx\n");
    else if (op[0] == '|') emit(cg, "    or eax, ebx\n");
    else if (op[0] == '^') emit(cg, "    xor eax, ebx\n");
    else if (op[0] == '+') emit(cg, "    add eax, ebx\n");
    else if (op[0] == '-') emit(cg, "    sub eax, ebx\n");
    else if (op[0] == '*') emit(cg, "    imul eax, ebx\n");
    else if (op[0] == '/' || op[0] == '%') {
        int skip_label = new_label(cg);
        emit(cg, "    test ebx, ebx\n");
        emit(cg, "    jnz .L%d\n", skip_label);
        emit(cg, "    xor eax, eax\n");
        emit(cg, "    jmp .L%d_end\n", skip_label);
        emit(cg, ".L%d:\n", skip_label);
        if (u32) emit(cg, "    xor edx, edx\n    div ebx\n");
        else emit(cg, "    cdq\n    idiv ebx\n");
        if (op[0] == '%') emit(cg, "    mov eax, edx\n");
        emit(cg, ".L%d_end:\n", skip_label);
    }
}

// Hosted -O2. A binary op whose operands are registers, immediates, or
// scalar memory can land directly in `dest` (add r15, r12) instead of
// moving through rax and rbx. / and % stay on the rax/rdx path: idiv needs
// them, and so does the signed power-of-two bias. can_place is decided
// before anything is emitted.
typedef struct {
    int kind;          // 1 register, 2 memory, 3 imm32
    const char* reg;
    char mem[96];
    int vol;
    long long imm;
} OpLeaf;

static int parse_i64(const char* s, long long* out) {
    if (!s || !s[0]) return 0;
    errno = 0;
    char* end = NULL;
    long long v = strtoll(s, &end, 10);
    if (errno == ERANGE || end == s || (end && *end)) return 0;
    *out = v;
    return 1;
}

static int fits_imm32_val(long long v) {
    return v >= -2147483648LL && v <= 2147483647LL;
}

static int scalar_leaf(Codegen* cg, AstNode* n, OpLeaf* L) {
    memset(L, 0, sizeof *L);
    if (!n) return 0;
    if (n->type == AST_NUMBER) {
        long long v;
        if (!parse_i64(n->value, &v) || !fits_imm32_val(v)) return 0;
        L->kind = 3;
        L->imm = v;
        return 1;
    }
    if (n->type != AST_IDENT || !n->name) return 0;
    Symbol* sym = symtab_lookup_symbol(cg->symtab, n->name);
    if (sym) {
        if (sym->hwreg) return 0;
        if (sym->is_array) return 0;
        if (sym->size > 1 && !sym->is_pointer) return 0;
        const char* r = local_reg(sym);
        if (r) { L->kind = 1; L->reg = r; return 1; }
        L->kind = 2;
        snprintf(L->mem, sizeof L->mem, "rbp%d", sym->offset);
        return 1;
    }
    GlobalVar* gv = global_symtab_lookup(cg->global_symtab, n->name);
    if (!gv || gv->is_array) return 0;
    L->kind = 2;
    snprintf(L->mem, sizeof L->mem, "%s", gv->name);
    L->vol = gv->is_volatile;
    return 1;
}

static int ident_is_dest(Codegen* cg, AstNode* n, const char* dest) {
    OpLeaf L;
    if (!scalar_leaf(cg, n, &L) || L.kind != 1) return 0;
    return strcmp(L.reg, dest) == 0;
}

static int alu_op(const char* op) {
    if (!op || !op[0]) return 0;
    if (!strcmp(op, "<<") || !strcmp(op, ">>")) return 1;
    char c = op[0];
    return c == '+' || c == '-' || c == '*' || c == '&' || c == '|' || c == '^';
}

static int is_shift_op(const char* op) {
    return op && (!strcmp(op, "<<") || !strcmp(op, ">>"));
}

static int is_comm_op(const char* op) {
    if (!op) return 0;
    char c = op[0];
    return c == '+' || c == '*' || c == '&' || c == '|' || c == '^';
}

static int pow2_shift_amt(AstNode* n, int* sh) {
    if (!n || n->type != AST_NUMBER || !n->value) return 0;
    long rv = atol(n->value);
    if (rv > 1 && is_power_of_2(rv)) { *sh = get_log2(rv); return 1; }
    return 0;
}

static const char* reg_low(const char* reg) {
    if (!strcmp(reg, "rax")) return "al";
    if (!strcmp(reg, "rbx")) return "bl";
    if (!strcmp(reg, "rcx")) return "cl";
    if (!strcmp(reg, "rdx")) return "dl";
    if (!strcmp(reg, "rsi")) return "sil";
    if (!strcmp(reg, "rdi")) return "dil";
    if (!strcmp(reg, "r8")) return "r8b";
    if (!strcmp(reg, "r9")) return "r9b";
    if (!strcmp(reg, "r10")) return "r10b";
    if (!strcmp(reg, "r11")) return "r11b";
    if (!strcmp(reg, "r12")) return "r12b";
    if (!strcmp(reg, "r13")) return "r13b";
    if (!strcmp(reg, "r14")) return "r14b";
    if (!strcmp(reg, "r15")) return "r15b";
    return "al";
}

static void emit_reg_trunc(Codegen* cg, const char* reg, int bits, int sg) {
    const char* lo;
    const char* w;
    const char* d;
    if (!reg || bits >= 64) return;
    if (!strcmp(reg, "rax") || !strcmp(reg, "eax")) {
        emit_width_trunc(cg, bits, sg);
        return;
    }
    lo = reg_low(reg);
    w = reg_word(reg);
    d = reg_dword(reg);
    if (sg) {
        if (bits <= 8) emit(cg, "    movsx %s, %s\n", reg, lo);
        else if (bits <= 16) emit(cg, "    movsx %s, %s\n", reg, w);
        else emit(cg, "    movsxd %s, %s\n", reg, d);
    } else {
        if (bits <= 8) emit(cg, "    movzx %s, %s\n", reg, lo);
        else if (bits <= 16) emit(cg, "    movzx %s, %s\n", reg, w);
        else emit(cg, "    mov %s, %s\n", d, d);
    }
}

static void emit_alu(Codegen* cg, const char* dest, const char* op, const char* src, int is_imm, long long imm) {
    if (!strcmp(op, "<<")) { emit(cg, "    shl %s, %lld\n", dest, imm); return; }
    if (!strcmp(op, ">>")) { emit(cg, "    sar %s, %lld\n", dest, imm); return; }
    if (op[0] == '*') {
        if (is_imm) emit(cg, "    imul %s, %s, %lld\n", dest, dest, imm);
        else emit(cg, "    imul %s, %s\n", dest, src);
        return;
    }
    const char* m = "add";
    if (op[0] == '-') m = "sub";
    else if (op[0] == '&') m = "and";
    else if (op[0] == '|') m = "or";
    else if (op[0] == '^') m = "xor";
    if (is_imm) emit(cg, "    %s %s, %lld\n", m, dest, imm);
    else emit(cg, "    %s %s, %s\n", m, dest, src);
}

static void emit_alu_leaf(Codegen* cg, const char* dest, const char* op, OpLeaf* S) {
    if (S->kind == 3) emit_alu(cg, dest, op, NULL, 1, S->imm);
    else if (S->kind == 1) emit_alu(cg, dest, op, S->reg, 0, 0);
    else {
        char buf[128];
        snprintf(buf, sizeof buf, "[%s]%s", S->mem, S->vol ? "  ; volatile" : "");
        emit_alu(cg, dest, op, buf, 0, 0);
    }
}

static int can_place_k(Codegen* cg, AstNode* n, const char* dest, int allow);
static void place_k(Codegen* cg, AstNode* n, const char* dest, int allow);
static void place_k_raw(Codegen* cg, AstNode* n, const char* dest, int allow);

// rax is the scratch, unless the result itself is rax: then rbx is scratch.
// Neither register holds a local.
static const char* scratch_reg(const char* dest) {
    return strcmp(dest, "rax") == 0 ? "rbx" : "rax";
}

static int can_place(Codegen* cg, AstNode* n, const char* dest) {
    return can_place_k(cg, n, dest, 1);
}

static void place_in(Codegen* cg, AstNode* n, const char* dest) {
    place_k(cg, n, dest, 1);
}

// `left OP right` into dest. A leaf right is applied after left is placed.
// Otherwise right is computed into the scratch first (it may still read dest)
// and left must be placeable without that scratch. allow==0 forbids the scratch.
static int can_ordered_k(Codegen* cg, AstNode* left, AstNode* right, const char* dest, int allow) {
    OpLeaf R;
    if (scalar_leaf(cg, right, &R) && !ident_is_dest(cg, right, dest) &&
        can_place_k(cg, left, dest, allow))
        return 1;
    if (!allow) return 0;
    const char* sc = scratch_reg(dest);
    if (can_place_k(cg, right, sc, 1) && can_place_k(cg, left, dest, 0)) return 1;
    return 0;
}

static int can_place_k(Codegen* cg, AstNode* n, const char* dest, int allow) {
    if (!n || !dest) return 0;
    if (n->type == AST_NUMBER) {
        long long v;
        return parse_i64(n->value, &v);
    }
    if (n->type == AST_IDENT) {
        OpLeaf L;
        return scalar_leaf(cg, n, &L);
    }
    if (n->type == AST_UNARY && n->op && n->child_count == 1 &&
        (n->op[0] == '-' || n->op[0] == '!' || n->op[0] == '~'))
        return can_place_k(cg, n->children[0], dest, allow);
    if (n->type != AST_BINOP || !n->op || n->child_count != 2 || !alu_op(n->op)) return 0;
    AstNode* L = n->children[0];
    AstNode* R = n->children[1];
    if (is_shift_op(n->op)) {
        OpLeaf S;
        if (!scalar_leaf(cg, R, &S) || S.kind != 3 || S.imm < 0 || S.imm > 63) return 0;
        return can_place_k(cg, L, dest, allow);
    }
    int sh = 0;
    if (n->op[0] == '*' && pow2_shift_amt(R, &sh)) return can_place_k(cg, L, dest, allow);
    if (ident_is_dest(cg, L, dest) && ident_is_dest(cg, R, dest)) return 1;
    if (can_ordered_k(cg, L, R, dest, allow)) return 1;
    if (is_comm_op(n->op) && can_ordered_k(cg, R, L, dest, allow)) return 1;
    return 0;
}

static void place_ordered_k(Codegen* cg, const char* op, AstNode* left, AstNode* right,
                            const char* dest, int allow) {
    OpLeaf R;
    if (scalar_leaf(cg, right, &R) && !ident_is_dest(cg, right, dest) &&
        can_place_k(cg, left, dest, allow)) {
        // imul dest, src, imm32 is one instruction. A power of two never
        // reaches here: place_k already turned it into shl.
        OpLeaf Ls;
        // lea rdi, [r12-1] is one instruction. add is shorter when dest is
        // already the source (`i = i + 1`).
        if ((op[0] == '+' || (op[0] == '-' && !op[1])) && R.kind == 3 &&
            scalar_leaf(cg, left, &Ls) && Ls.kind == 1 && strcmp(Ls.reg, dest) != 0) {
            long long d = op[0] == '-' ? -R.imm : R.imm;
            if (d >= -2147483648LL && d <= 2147483647LL) {
                if (d == 0) emit(cg, "    mov %s, %s\n", dest, Ls.reg);
                else emit(cg, "    lea %s, [%s%+lld]\n", dest, Ls.reg, d);
                return;
            }
        }
        if (op[0] == '*' && R.kind == 3 && scalar_leaf(cg, left, &Ls) && Ls.kind == 1) {
            // lea wraps mod 2^64, same as imul, and does not set flags.
            // Nothing after place_k reads the multiply flags. *4 and *8
            // already became shl. *6 is not a scale.
            int scale = 0;
            if (o2_hosted() && R.imm == 3) scale = 2;
            else if (o2_hosted() && R.imm == 5) scale = 4;
            else if (o2_hosted() && R.imm == 9) scale = 8;
            if (scale)
                emit(cg, "    lea %s, [%s+%s*%d]\n", dest, Ls.reg, Ls.reg, scale);
            else
                emit(cg, "    imul %s, %s, %lld\n", dest, Ls.reg, R.imm);
            return;
        }
        place_k(cg, left, dest, allow);
        emit_alu_leaf(cg, dest, op, &R);
        return;
    }
    if (!allow) return;
    const char* sc = scratch_reg(dest);
    if (can_place_k(cg, right, sc, 1) && can_place_k(cg, left, dest, 0)) {
        place_k(cg, right, sc, 1);
        if (!ident_is_dest(cg, left, dest)) place_k(cg, left, dest, 0);
        emit_alu(cg, dest, op, sc, 0, 0);
    }
}

static void place_k_raw(Codegen* cg, AstNode* n, const char* dest, int allow) {
    if (n->type == AST_NUMBER) {
        emit(cg, "    mov %s, %s\n", dest, n->value);
        return;
    }
    if (n->type == AST_IDENT) {
        if (ident_is_dest(cg, n, dest)) return;
        OpLeaf L;
        if (!scalar_leaf(cg, n, &L)) return;
        if (L.kind == 1) emit(cg, "    mov %s, %s\n", dest, L.reg);
        else emit(cg, "    mov %s, [%s]%s\n", dest, L.mem, L.vol ? "  ; volatile" : "");
        return;
    }
    if (n->type == AST_UNARY) {
        place_k(cg, n->children[0], dest, allow);
        if (n->op[0] == '-') emit(cg, "    neg %s\n", dest);
        else if (n->op[0] == '~') emit(cg, "    not %s\n", dest);
        else {
            const char* lo = reg_low(dest);
            emit(cg, "    test %s, %s\n    setz %s\n    movzx %s, %s\n", dest, dest, lo, dest, lo);
        }
        return;
    }
    if (n->type != AST_BINOP) return;
    AstNode* L = n->children[0];
    AstNode* R = n->children[1];
    const char* op = n->op;
    if (is_shift_op(op)) {
        OpLeaf S;
        scalar_leaf(cg, R, &S);
        place_k(cg, L, dest, allow);
        emit(cg, "    %s %s, %lld\n", op[0] == '>' ? "sar" : "shl", dest, S.imm);
        return;
    }
    int sh = 0;
    if (op[0] == '*' && pow2_shift_amt(R, &sh)) {
        place_k(cg, L, dest, allow);
        emit(cg, "    shl %s, %d\n", dest, sh);
        return;
    }
    if (ident_is_dest(cg, L, dest) && ident_is_dest(cg, R, dest)) {
        emit_alu(cg, dest, op, dest, 0, 0);
        return;
    }
    // Swap a commutative op only when that puts a leaf on the right and the
    // written order does not (`a = b + a` -> add a, b). Keep `i & 7` as an
    // immediate; swapping it would move the 7 into the register.
    OpLeaf side;
    int direct_leaf = scalar_leaf(cg, R, &side) && !ident_is_dest(cg, R, dest) &&
        can_place_k(cg, L, dest, allow);
    int swap_leaf = is_comm_op(op) && scalar_leaf(cg, L, &side) && !ident_is_dest(cg, L, dest) &&
        can_place_k(cg, R, dest, allow);
    if (swap_leaf && !direct_leaf) {
        place_ordered_k(cg, op, R, L, dest, allow);
        return;
    }
    if (can_ordered_k(cg, L, R, dest, allow)) {
        place_ordered_k(cg, op, L, R, dest, allow);
        return;
    }
    if (is_comm_op(op) && can_ordered_k(cg, R, L, dest, allow))
        place_ordered_k(cg, op, R, L, dest, allow);
}

static void place_k(Codegen* cg, AstNode* n, const char* dest, int allow) {
    VTy t;
    place_k_raw(cg, n, dest, allow);
    if (!n || !dest) return;
    if (n->type == AST_BINOP) t = ty_binop(cg, n);
    else if (n->type == AST_UNARY && n->op && (n->op[0] == '-' || n->op[0] == '~') && !n->op[1])
        t = expr_ty(cg, n);
    else return;
    if (t.kind == VK_INT && !t.lit && t.bits < 64)
        emit_reg_trunc(cg, dest, t.bits, t.sg);
}

static const char* ucond(const char* s) {
    if (!g_cmp_unsigned || !s) return s;
    if (!strcmp(s, "setl")) return "setb";
    if (!strcmp(s, "setg")) return "seta";
    if (!strcmp(s, "setle")) return "setbe";
    if (!strcmp(s, "setge")) return "setae";
    if (!strcmp(s, "jl")) return "jb";
    if (!strcmp(s, "jg")) return "ja";
    if (!strcmp(s, "jle")) return "jbe";
    if (!strcmp(s, "jge")) return "jae";
    return s;
}

static const char* cmp_setcc(const char* op) {
    if (!op) return NULL;
    if (!strcmp(op, "==")) return "sete";
    if (!strcmp(op, "!=")) return "setne";
    if (!strcmp(op, "<")) return ucond("setl");
    if (!strcmp(op, ">")) return ucond("setg");
    if (!strcmp(op, "<=")) return ucond("setle");
    if (!strcmp(op, ">=")) return ucond("setge");
    return NULL;
}

static const char* cmp_fail_jcc(const char* op) {
    if (!op) return NULL;
    if (!strcmp(op, "==")) return "jne";
    if (!strcmp(op, "!=")) return "je";
    if (!strcmp(op, "<")) return ucond("jge");
    if (!strcmp(op, ">")) return ucond("jle");
    if (!strcmp(op, "<=")) return ucond("jg");
    if (!strcmp(op, ">=")) return ucond("jl");
    return NULL;
}

static void emit_cmp_operand(Codegen* cg, OpLeaf* S) {
    if (S->kind == 3) emit(cg, "%lld", S->imm);
    else if (S->kind == 1) emit(cg, "%s", S->reg);
    else emit(cg, "[%s]%s", S->mem, S->vol ? "  ; volatile" : "");
}

// cmp of two leaves. Emits nothing and returns 0 if the form is not direct.
static int emit_cmp_raw(Codegen* cg, AstNode* n) {
    if (!n || n->type != AST_COMPARE || !cmp_setcc(n->op) || n->child_count != 2) return 0;
    OpLeaf L, R;
    if (!scalar_leaf(cg, n->children[0], &L) || !scalar_leaf(cg, n->children[1], &R)) return 0;
    if (L.kind == 1) {
        emit(cg, "    cmp %s, ", L.reg);
        emit_cmp_operand(cg, &R);
        emit(cg, "\n");
        return 1;
    }
    if (L.kind == 2 && R.kind != 2) {
        // A memory destination has no register to imply the width.
        emit(cg, "    cmp qword [%s], ", L.mem);
        emit_cmp_operand(cg, &R);
        if (L.vol) emit(cg, "  ; volatile");
        emit(cg, "\n");
        return 1;
    }
    if (L.kind == 2 && R.kind == 2) {
        emit(cg, "    mov rax, [%s]%s\n", L.mem, L.vol ? "  ; volatile" : "");
        emit(cg, "    cmp rax, ");
        emit_cmp_operand(cg, &R);
        emit(cg, "\n");
        return 1;
    }
    emit(cg, "    mov rax, %lld\n", L.imm);
    emit(cg, "    cmp rax, ");
    emit_cmp_operand(cg, &R);
    emit(cg, "\n");
    return 1;
}

// A narrow integer must be truncated in rax. The direct register path skips that.
static int sym_narrow_int(Symbol* sym) {
    int bits = 64, sg = 1;
    if (!sym || sym->is_pointer || sym->is_array || !sym->type_name) return 0;
    if (!int_name(sym->type_name, &bits, &sg)) return 0;
    return bits < 64;
}

static int assign_dest_narrow(Codegen* cg, AstNode* n) {
    if (!n || !n->name || !cg->symtab) return 0;
    return sym_narrow_int(symtab_lookup_symbol(cg->symtab, n->name));
}

static void check_assign_types(Codegen* cg, AstNode* n) {
    if (!n || n->child_count < 1 || !n->children[0]) return;
    Symbol* asym = n->name ? symtab_lookup_symbol(cg->symtab, n->name) : NULL;
    GlobalVar* ag = (!asym && n->name) ? global_symtab_lookup(cg->global_symtab, n->name) : NULL;
    VTy dst_ty = asym && !asym->is_array ? sym_vty(cg, asym) : (ag ? global_vty(cg, ag) : ty_unk());
    check_types(n->children[0], "", dst_ty, expr_ty(cg, n->children[0]));
}

// `reg = a[i]` with a scaled index: one load into reg, not mov rax / mov reg.
static int assign_index(Codegen* cg, AstNode* n) {
    if (!o2_hosted() || !n || n->type != AST_ASSIGN || !n->name) return 0;
    if (!n->children || n->child_count < 1 || !n->children[0] || n->children[0]->type != AST_INDEX)
        return 0;
    Symbol* sym = symtab_lookup_symbol(cg->symtab, n->name);
    const char* rr = local_reg(sym);
    if (!rr) return 0;
    check_assign_types(cg, n);
    g_index_dest = rr;
    g_index_hit = 0;
    gen_array_index(cg, n->children[0]);
    g_index_dest = NULL;
    if (!g_index_hit) emit(cg, "    mov %s, rax\n", rr);
    return 1;
}

// Place the RHS into the local's register. 0 if the assignment is not that form.
static int assign_direct(Codegen* cg, AstNode* n) {
    if (!o2_hosted() || !n || n->type != AST_ASSIGN || !n->name) return 0;
    if (!n->children || n->child_count < 1 || !n->children[0]) return 0;
    AstNode dst;
    memset(&dst, 0, sizeof dst);
    dst.type = AST_IDENT;
    dst.name = n->name;
    dst.line = n->line;
    dst.col = n->col;
    if (expr_struct_type(cg, &dst)) return 0;
    Symbol* sym = symtab_lookup_symbol(cg->symtab, n->name);
    const char* rr = local_reg(sym);
    if (!rr || !can_place(cg, n->children[0], rr)) return 0;
    check_assign_types(cg, n);
    place_in(cg, n->children[0], rr);
    return 1;
}

static void emit_binop_rax_rbx(Codegen* cg, const char* op, int bits, int sg) {
    int u32 = legacy_bits() && !sg && bits >= 32 && bits < 64;
    if (legacy_bits()) { emit_binop_legacy(cg, op, u32); return; }
    if (!strcmp(op, "<<")) emit(cg, "    mov rcx, rbx\n    shl rax, cl\n");
    else if (!strcmp(op, ">>")) emit(cg, "    mov rcx, rbx\n    sar rax, cl\n");
    else if (op[0] == '&') emit(cg, "    and rax, rbx\n");
    else if (op[0] == '|') emit(cg, "    or rax, rbx\n");
    else if (op[0] == '^') emit(cg, "    xor rax, rbx\n");
    else if (op[0] == '+') emit(cg, "    add rax, rbx\n");
    else if (op[0] == '-') emit(cg, "    sub rax, rbx\n");
    else if (op[0] == '*') emit(cg, "    imul rax, rbx\n");
    else if (op[0] == '/') {
        int skip_label = new_label(cg);
        emit(cg, "    test rbx, rbx\n");
        emit(cg, "    jnz .L%d\n", skip_label);
        emit(cg, "    ; Division by zero - return 0\n");
        emit(cg, "    xor rax, rax\n");
        emit(cg, "    jmp .L%d_end\n", skip_label);
        emit(cg, ".L%d:\n", skip_label);
        emit(cg, "    cqo\n    idiv rbx\n");
        emit(cg, ".L%d_end:\n", skip_label);
    } else if (op[0] == '%') {
        int skip_label = new_label(cg);
        emit(cg, "    test rbx, rbx\n");
        emit(cg, "    jnz .L%d\n", skip_label);
        emit(cg, "    ; Modulo by zero - return 0\n");
        emit(cg, "    xor rax, rax\n");
        emit(cg, "    jmp .L%d_end\n", skip_label);
        emit(cg, ".L%d:\n", skip_label);
        emit(cg, "    cqo\n    idiv rbx\n");
        emit(cg, "    mov rax, rdx  ; Move remainder to rax\n");
        emit(cg, ".L%d_end:\n", skip_label);
    }
}

static void emit_node_binop(Codegen* cg, AstNode* n) {
    VTy t = ty_binop(cg, n);
    int bits = 64, sg = 1;
    if (t.kind == VK_INT) { bits = t.bits; sg = t.sg; }
    emit_binop_rax_rbx(cg, n->op, bits, sg);
}

// Read a named register into the accumulator. Zero-extends a narrower
// register. Reading rax in 64-bit code leaves the accumulator as it is.
static void emit_reg_read(Codegen* cg, const char* name, int bits) {
    const MachReg* seg = mach_reg(name);
    if (seg && seg->seg) {
        emit(cg, "    mov ax, %s\n", name);
        if (legacy_bits()) emit(cg, "    movzx eax, ax\n");
        else emit(cg, "    movzx rax, ax\n");
        return;
    }
    if (bits <= 8 || bits == 16) {
        if (legacy_bits()) emit(cg, "    movzx eax, %s\n", name);
        else emit(cg, "    movzx rax, %s\n", name);
    } else if (bits == 32) {
        if (strcmp(name, "eax")) emit(cg, "    mov eax, %s\n", name);
    } else if (strcmp(name, "rax")) {
        emit(cg, "    mov rax, %s\n", name);
    }
}

static void emit_reg_write_acc(Codegen* cg, const char* name, int bits) {
    if (bits <= 8) {
        if (strcmp(name, "al")) emit(cg, "    mov %s, al\n", name);
    } else if (bits == 16) {
        if (strcmp(name, "ax")) emit(cg, "    mov %s, ax\n", name);
    } else if (bits == 32) {
        if (strcmp(name, "eax")) emit(cg, "    mov %s, eax\n", name);
    } else if (strcmp(name, "rax")) {
        emit(cg, "    mov %s, rax\n", name);
    }
}

// An immediate writes the register directly, so `ah = 0x0E` does not clear
// al. Any other right-hand side is computed in the accumulator first, and
// that does clear the other bytes of eax/rax.
static void emit_reg_write(Codegen* cg, const char* name, int bits, int seg, AstNode* rhs) {
    if (seg) {
        if (rhs) gen_expr(cg, rhs);
        emit(cg, "    mov %s, ax\n", name);
        return;
    }
    if (rhs && rhs->type == AST_NUMBER && rhs->value) {
        emit(cg, "    mov %s, %s\n", name, rhs->value);
        return;
    }
    if (rhs && rhs->type == AST_IDENT && rhs->name &&
        (is_data_name(cg, rhs->name) || find_func(cg, rhs->name))) {
        emit(cg, "    mov %s, %s\n", name, rhs->name);
        return;
    }
    if (rhs) gen_expr(cg, rhs);
    if (bits == 8) {
        const MachReg* r = mach_reg(name);
        if (r && r->high) emit(cg, "    mov %s, al\n", name);
        else emit_reg_write_acc(cg, name, bits);
        return;
    }
    emit_reg_write_acc(cg, name, bits);
}

static int cast_pointee_size(AstNode* e, int* sz) {
    if (!e || e->type != AST_CAST || !(e->is_pointer & 1)) return 0;
    int bits = 64, sg = 1;
    if (!e->value || !int_name(e->value, &bits, &sg)) bits = 64;
    *sz = bits / 8;
    if (*sz < 1) *sz = 1;
    return 1;
}

// A nasm memory operand that does not need the accumulator: a number,
// a naked register, or `reg+disp` / `reg-disp`.
static int mem_operand(AstNode* addr, char* buf, int n) {
    AstNode* e = addr;
    if (e && e->type == AST_CAST && e->child_count > 0) e = e->children[0];
    if (!e) return 0;
    if (e->type == AST_NUMBER && e->value) {
        snprintf(buf, n, "%s", e->value);
        return 1;
    }
    if (e->type == AST_IDENT && e->name && mach_ok(mach_reg(e->name))) {
        snprintf(buf, n, "%s", e->name);
        return 1;
    }
    if (e->type == AST_BINOP && e->op && e->child_count == 2 &&
        (e->op[0] == '+' || e->op[0] == '-') && !e->op[1]) {
        AstNode* a = e->children[0];
        AstNode* b = e->children[1];
        if (a && a->type == AST_IDENT && a->name && mach_ok(mach_reg(a->name)) &&
            b && b->type == AST_NUMBER && b->value) {
            snprintf(buf, n, "%s%s%s", a->name, e->op, b->value);
            return 1;
        }
    }
    return 0;
}

static int emit_imm_store(Codegen* cg, AstNode* value, const char* mem, int el);

static void emit_acc_store(Codegen* cg, const char* mem, int size, int line, int col) {
    if (size >= 8 && legacy_bits()) {
        diag_error(line, col, "an 8-byte store is not available in bits %d", g_fn_bits);
        return;
    }
    if (size <= 1) emit(cg, "    mov byte [%s], al\n", mem);
    else if (size == 2) emit(cg, "    mov word [%s], ax\n", mem);
    else if (size == 4) emit(cg, "    mov dword [%s], eax\n", mem);
    else emit(cg, "    mov [%s], rax\n", mem);
}

static void emit_acc_load(Codegen* cg, const char* mem, int size, int line, int col) {
    if (size >= 8 && legacy_bits()) {
        diag_error(line, col, "an 8-byte load is not available in bits %d", g_fn_bits);
        return;
    }
    if (size <= 1) {
        if (legacy_bits()) emit(cg, "    movzx eax, byte [%s]\n", mem);
        else emit(cg, "    movzx rax, byte [%s]\n", mem);
    } else if (size == 2) {
        if (legacy_bits()) emit(cg, "    movzx eax, word [%s]\n", mem);
        else emit(cg, "    movzx rax, word [%s]\n", mem);
    } else if (size == 4) {
        emit(cg, "    mov eax, [%s]\n", mem);
    } else {
        emit(cg, "    mov rax, [%s]\n", mem);
    }
}

static void gen_mem_store(Codegen* cg, AstNode* n) {
    AstNode* deref = n->children[0];
    AstNode* val = n->child_count > 1 ? n->children[1] : NULL;
    AstNode* addr = (deref && deref->child_count > 0) ? deref->children[0] : NULL;
    int sz = legacy_bits() ? 4 : 8;
    if (cast_pointee_size(addr, &sz)) {
        // The cast type is the pointer (`*u8`). The store is checked against
        // the pointee, so `*(0x1000 as *u8) = 300` is the usual literal error.
        VTy dst = ty_from_spelled(cg, addr->value);
        if (val) check_types(val, "", dst, expr_ty(cg, val));
    }
    char mem[80];
    if (addr && mem_operand(addr, mem, sizeof mem)) {
        // `*(addr) = 0` must not `mov rax, 0`. The syscall result lives in rax
        // until the next expression that needs the accumulator.
        if (!val || !emit_imm_store(cg, val, mem, sz)) {
            if (val) gen_expr(cg, peel_store_cast(val, sz * 8));
            emit_acc_store(cg, mem, sz, n->line, n->col);
        }
        return;
    }
    if (addr) gen_expr(cg, addr);
    if (legacy_bits()) emit(cg, "    push eax\n");
    else emit(cg, "    push rax\n");
    if (val) gen_expr(cg, peel_store_cast(val, sz * 8));
    if (legacy_bits()) emit(cg, "    pop ebx\n");
    else emit(cg, "    pop rbx\n");
    emit_acc_store(cg, legacy_bits() ? "ebx" : "rbx", sz, n->line, n->col);
}

// ident, ident+imm, or ident-imm already in a register. add is the
// constant added to the register. Bare idents return 0 so the existing
// path keeps its text.
static int parse_index_disp(Codegen* cg, AstNode* ix, const char** reg, long long* add) {
    if (!ix || !reg || !add) return 0;
    if (ix->type != AST_BINOP || !ix->op || ix->child_count != 2) return 0;
    if (!(ix->op[0] == '+' || (ix->op[0] == '-' && !ix->op[1])) || ix->op[1]) return 0;
    AstNode* a = ix->children[0];
    AstNode* b = ix->children[1];
    AstNode* id = NULL;
    AstNode* num = NULL;
    int neg = ix->op[0] == '-';
    if (a && a->type == AST_IDENT && b && b->type == AST_NUMBER) { id = a; num = b; }
    else if (!neg && b && b->type == AST_IDENT && a && a->type == AST_NUMBER) { id = b; num = a; }
    else return 0;
    long long v = 0;
    if (!parse_i64(num->value, &v)) return 0;
    Symbol* s = symtab_lookup_symbol(cg->symtab, id->name);
    const char* r = local_reg(s);
    if (!r) return 0;
    *reg = r;
    *add = neg ? -v : v;
    if (*add < -4194304 || *add > 4194304) return 0;
    return *add != 0;
}

// `mov byte/word/dword/qword [mem], imm` when the immediate is exactly the
// value the store would truncate into that width. A qword immediate is the
// sign-extended imm32 form, so a larger constant stays on the rax path.
static int imm_store_enc(int elem, long long v, long long* enc) {
    if (elem == 1) {
        if (v < -128 || v > 255) return 0;
    } else if (elem == 2) {
        if (v < -32768 || v > 65535) return 0;
    } else if (elem == 4 || elem == 8) {
        if (!fits_imm32_val(v)) return 0;
    } else return 0;
    *enc = v;
    return 1;
}

static int emit_imm_store(Codegen* cg, AstNode* value, const char* mem, int el) {
    long long v, enc;
    // bits 16/32 have no 8-byte store. Leave that to emit_acc_store so the
    // error stays `an 8-byte store is not available in bits 16`.
    if (el >= 8 && legacy_bits()) return 0;
    if (!value || value->type != AST_NUMBER || !parse_i64(value->value, &v)) return 0;
    if (!imm_store_enc(el, v, &enc)) return 0;
    if (el == 1) emit(cg, "    mov byte [%s], %lld\n", mem, enc);
    else if (el == 2) emit(cg, "    mov word [%s], %lld\n", mem, enc);
    else if (el == 4) emit(cg, "    mov dword [%s], %lld\n", mem, enc);
    else emit(cg, "    mov qword [%s], %lld\n", mem, enc);
    return 1;
}

static int try_emit_byte_add(Codegen* cg, AstNode* value, const char* dst_mem,
                             const char* dst_name, const char* idx_name, const char* ireg);

void gen_expr(Codegen* cg, AstNode* n) {
    if (!n) return;  // Null safety guard
    // An index load may reuse the value a compare just left in rax.
    // An array store generates that load as its value expression.
    if (n->type != AST_INDEX && n->type != AST_ARRAY_ASSIGN) g_fwd_ok = 0;
    if (n->file) focus_file(n->file);
    if (n->type == AST_NUMBER) {
        if (legacy_bits()) emit(cg, "    mov eax, %s\n", n->value);
        else emit(cg, "    mov rax, %s\n", n->value);
    } else if (n->type == AST_STRING) {
        char* label = strtab_add(cg->strtab, n->value, strlen(n->value));
        if (legacy_bits()) emit(cg, "    mov eax, %s\n", label);
        else {
            emit(cg, "    mov rax, %s\n", label);
            emit(cg, "    mov rbx, %d\n", (int)strlen(n->value));
        }
    } else if (n->type == AST_IDENT) {
        // Try local variable first
        Symbol* sym0 = n->name ? symtab_lookup_symbol(cg->symtab, n->name) : NULL;
        if (sym0 && sym0->hwreg) {
            emit_reg_read(cg, sym0->hwreg, sym0->hwbits);
            return;
        }
        int off = symtab_lookup(cg->symtab, n->name);
        if (off) {
            // Check if this is an array - if so, load address not value
            Symbol* sym = sym0;
            if (sym && sym->size > 1 && sym->type_name && !sym->is_pointer) {
                // This is an array - use lea to get address
                emit(cg, "    lea rax, [rbp%d]\n", off);
            } else if (local_reg(sym)) {
                emit(cg, "    mov rax, %s\n", local_reg(sym));
            } else if (sym && sym->mem && legacy_bits()) {
                emit(cg, "    mov eax, [%s]\n", sym->mem);
            } else if (sym && sym->mem) {
                emit(cg, "    mov rax, [%s]\n", sym->mem);
            } else {
                // Regular variable or pointer - load value
                emit(cg, "    mov rax, [rbp%d]\n", off);
            }
        } else {
            // Try global variable
            GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, n->name);
            if (gvar) {
                // For arrays, load address; for scalars, load value
                if (gvar->is_array) {
                    emit(cg, "    lea rax, [%s]\n", gvar->name);
                } else if (gvar->is_volatile) {
                    // Volatile: this load must stay a memory operand. Do not
                    // fold, CSE, or hoist it (a tick counter or MMIO register
                    // changes behind the compiler's back).
                    emit(cg, "    mov rax, [%s]  ; volatile\n", gvar->name);
                } else {
                    emit(cg, "    mov rax, [%s]\n", gvar->name);
                }
            } else {
                const MachReg* mr = mach_reg(n->name);
                if (mr) {
                    if (mach_here(n, mr)) emit_reg_read(cg, mr->name, mr->seg ? 16 : mr->bits);
                    else if (legacy_bits()) emit(cg, "    xor eax, eax\n");
                    else emit(cg, "    xor rax, rax\n");
                } else if (is_data_name(cg, n->name) || find_func(cg, n->name)) {
                    if (legacy_bits()) emit(cg, "    mov eax, %s\n", n->name);
                    else emit(cg, "    mov rax, %s\n", n->name);
                } else {
                    diag_error(n->line, n->col, "use of undeclared variable '%s'", n->name);
                    emit(cg, "    mov rax, 0  ; unknown var %s\n", n->name);
                }
            }
        }
    } else if (n->type == AST_ADDR_OF) {
        gen_addr_of(cg, n);
    } else if (n->type == AST_DEREF) {
        // Dereference: *ptr. A non-volatile pointer stays a 64-bit load.
        // *volatile T loads the pointee's real width and is never folded.
        // `*(rbp + 136)` and `*(0x1000 as *u32)` are one memory operand.
        int vsz = 8, vsg = 0;
        int vpo = n->child_count > 0 && volatile_pointee_info(cg, n->children[0], &vsz, &vsg);
        char mem[80];
        if (!vpo && n->child_count > 0 && mem_operand(n->children[0], mem, sizeof mem)) {
            int sz = legacy_bits() ? 4 : 8;
            cast_pointee_size(n->children[0], &sz);
            emit_acc_load(cg, mem, sz, n->line, n->col);
        } else {
            gen_expr(cg, n->children[0]);
            if (vpo) emit_sized_load_mark(cg, "rax", vsz, vsg, 1);
            else if (legacy_bits()) emit(cg, "    mov eax, [eax]\n");
            else emit(cg, "    mov rax, [rax]\n");
        }
    } else if (n->type == AST_UNARY) {
        // Unary operators: '-' (negation) and '!' (logical NOT)
        if (n->children && n->child_count > 0) {
            gen_expr(cg, n->children[0]);
            if (legacy_bits()) {
                if (n->op && n->op[0] == '-') emit(cg, "    neg eax\n");
                else if (n->op && n->op[0] == '!') {
                    emit(cg, "    test eax, eax\n");
                    emit(cg, "    setz al\n");
                    emit(cg, "    movzx eax, al\n");
                } else if (n->op && n->op[0] == '~') emit(cg, "    not eax\n");
            } else if (n->op && n->op[0] == '-') {
                emit(cg, "    neg rax\n");  // Two's complement negation
            } else if (n->op && n->op[0] == '!') {
                // Logical NOT: convert non-zero to 0, zero to 1
                emit(cg, "    test rax, rax\n");
                emit(cg, "    setz al  ; Set al to 1 if rax was 0\n");
                emit(cg, "    movzx rax, al\n");
            } else if (n->op && n->op[0] == '~') {
                emit(cg, "    not rax\n");
            }
            trunc_rax_node(cg, n);
        }
    } else if (n->type == AST_CAST) {
        // Pointer and 64-bit casts are a type change only. A narrower integer
        // cast keeps the low bits (signed: sign-extend, unsigned: zero-extend).
        if (n->child_count > 0) gen_expr(cg, n->children[0]);
        if (!(n->is_pointer & 1)) {
            int bits = 64, sg = 1;
            if (int_name(n->value, &bits, &sg)) emit_width_trunc(cg, bits, sg);
        }
    } else if (n->type == AST_ASSIGN) {
        // A naked parameter is the register itself. A machine-register name
        // is the same, but only in a naked function, and a real variable wins.
        Symbol* hw = n->name ? symtab_lookup_symbol(cg->symtab, n->name) : NULL;
        GlobalVar* hwg = (!hw && n->name) ? global_symtab_lookup(cg->global_symtab, n->name) : NULL;
        if (hw && hw->hwreg) {
            emit_reg_write(cg, hw->hwreg, hw->hwbits, 0, n->child_count > 0 ? n->children[0] : NULL);
            return;
        }
        if (!hw && !hwg && n->name) {
            const MachReg* mr = mach_reg(n->name);
            if (mr) {
                if (mach_here(n, mr))
                    emit_reg_write(cg, mr->name, mr->seg ? 16 : mr->bits, mr->seg,
                                   n->child_count > 0 ? n->children[0] : NULL);
                return;
            }
        }
        if (n->name) {
            AstNode dst;
            memset(&dst, 0, sizeof(dst));
            dst.type = AST_IDENT;
            dst.name = n->name;
            dst.line = n->line;
            dst.col = n->col;
            if (expr_struct_type(cg, &dst)) {
                gen_struct_assign(cg, &dst, n->children[0]);
                return;
            }
        }
        if (!assign_dest_narrow(cg, n) &&
            (assign_direct(cg, n) || assign_index(cg, n))) {
            Symbol* sym = n->name ? symtab_lookup_symbol(cg->symtab, n->name) : NULL;
            const char* rr = local_reg(sym);
            if (rr && strcmp(rr, "rax") != 0) emit(cg, "    mov rax, %s\n", rr);
            return;
        }
        check_assign_types(cg, n);
        gen_expr(cg, n->children[0]);
        // Try local variable first
        int off = symtab_lookup(cg->symtab, n->name);
        if (off) {
            Symbol* sym = symtab_lookup_symbol(cg->symtab, n->name);
            if (sym && !sym->is_pointer && !sym->is_array)
                emit_store_trunc(cg, sym->type_name, n->children[0]);
            if (sym && local_reg(sym)) emit(cg, "    mov %s, rax\n", local_reg(sym));
            else if (sym && sym->mem && legacy_bits()) emit(cg, "    mov [%s], eax\n", sym->mem);
            else if (sym && sym->mem) emit(cg, "    mov [%s], rax\n", sym->mem);
            else emit(cg, "    mov [rbp%d], rax\n", off);
        } else {
            // Try global variable
            GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, n->name);
            if (gvar) {
                if (!gvar->is_pointer && !gvar->is_array)
                    emit_store_trunc(cg, gvar->type_name, n->children[0]);
                if (gvar->is_volatile) emit(cg, "    mov [%s], rax  ; volatile\n", gvar->name);
                else emit(cg, "    mov [%s], rax\n", gvar->name);
            } else if (n->name) {
                diag_error(n->line, n->col, "assignment to undeclared variable '%s'", n->name);
            }
        }
    } else if (n->type == AST_BINOP) {
        if (o2_hosted() && can_place(cg, n, "rax")) {
            place_in(cg, n, "rax");
            return;
        }
        // Strength reduction optimization (O2+): Check if right operand is power of 2
        AstNode* right = n->children[1];
        int use_shift = 0;
        long right_val = 0;
        int shift_amount = 0;

        if (optimization_level >= 2 && !legacy_bits() && right && right->type == AST_NUMBER) {
            right_val = atol(right->value);
            if (right_val > 1 && is_power_of_2(right_val)) {
                use_shift = 1;
                shift_amount = get_log2(right_val);
            }
        }

        gen_expr(cg, n->children[0]);

        if (use_shift && n->op[0] == '*') {
            // Multiplication by power of 2: use left shift
            emit(cg, "    ; Optimized: x * %ld => x << %d\n", right_val, shift_amount);
            emit(cg, "    shl rax, %d\n", shift_amount);
        } else if (use_shift && n->op[0] == '/') {
            // Division by power of 2: use arithmetic right shift
            // Signed: bias negative dividends so the result truncates toward zero
            emit(cg, "    ; Optimized: x / %ld => (x + bias) >> %d\n", right_val, shift_amount);
            emit(cg, "    mov rdx, rax\n    sar rdx, 63\n    shr rdx, %d\n", 64 - shift_amount);
            emit(cg, "    add rax, rdx\n    sar rax, %d\n", shift_amount);
        } else if (use_shift && n->op[0] == '%') {
            // Modulo by power of 2: use AND mask
            long mask = right_val - 1;
            emit(cg, "    ; Optimized: x %% %ld => x & %ld\n", right_val, mask);
            // Signed remainder keeps the dividend's sign: ((x + bias) & mask) - bias
            emit(cg, "    mov rdx, rax\n    sar rdx, 63\n    shr rdx, %d\n", 64 - shift_amount);
            emit(cg, "    add rax, rdx\n    and rax, %ld\n    sub rax, rdx\n", mask);
        } else {
            // Hosted -O2: a simple right-hand side lands in rbx. No push/pop.
            // bits 16/32 keep edx (the high half of an MSR) across the operator,
            // and put ebx back: `di = di + 24` must not leave 24 in ebx.
            if (legacy_bits()) {
                emit(cg, "    push ebx\n");
                emit(cg, "    push eax\n    push edx\n");
                gen_expr(cg, n->children[1]);
                emit(cg, "    mov ebx, eax\n    pop edx\n    pop eax\n");
            } else if (optimization_level >= 2 && !g_freestanding && can_emit_rbx(cg, right))
                emit_rbx(cg, right);
            else if (o2_hosted() && g_expr_tmp && node_has_call(right) && expr_tmp_depth(right) == 0) {
                // Left stays in a callee-saved register across the call.
                // Both calls keep the stack alignment the prologue established.
                emit(cg, "    mov %s, rax\n", g_expr_tmp);
                gen_expr(cg, right);
                if (n->op[0] == '+' || n->op[0] == '*' || n->op[0] == '&' ||
                    n->op[0] == '|' || n->op[0] == '^')
                    emit_alu(cg, "rax", n->op, g_expr_tmp, 0, 0);
                else if (n->op[0] == '-' && !n->op[1]) {
                    emit(cg, "    sub %s, rax\n", g_expr_tmp);
                    emit(cg, "    mov rax, %s\n", g_expr_tmp);
                } else {
                    emit(cg, "    mov rbx, rax\n");
                    emit(cg, "    mov rax, %s\n", g_expr_tmp);
                    emit_node_binop(cg, n);
                }
                trunc_rax_node(cg, n);
                return;
            } else {
                emit(cg, "    push rax\n");
                gen_expr(cg, n->children[1]);
                emit(cg, "    mov rbx, rax\n    pop rax\n");
            }
            emit_node_binop(cg, n);
            if (legacy_bits()) emit(cg, "    pop ebx\n");
        }
        trunc_rax_node(cg, n);
    } else if (n->type == AST_COMPARE) {
        int uns = cmp_legacy_unsigned(cg, n);
        const char* scc;
        if (o2_hosted() && emit_cmp_raw(cg, n)) {
            g_cmp_unsigned = uns;
            scc = cmp_setcc(n->op);
            g_cmp_unsigned = 0;
            if (scc) emit(cg, "    %s al\n", scc);
            emit(cg, "    movzx rax, al\n");
            return;
        }
        gen_expr(cg, n->children[0]);
        if (legacy_bits()) {
            // A comparison used as a value (`let x = a == b`) writes ebx for
            // `cmp eax, ebx`. Pop after cmp: pop does not change the flags,
            // so sete still sees the compare, and a named ebx is intact.
            emit(cg, "    push ebx\n    push eax\n    push edx\n");
            gen_expr(cg, n->children[1]);
            emit(cg, "    mov ebx, eax\n    pop edx\n    pop eax\n");
            emit(cg, "    cmp eax, ebx\n");
            emit(cg, "    pop ebx\n");
        } else if (optimization_level >= 2 && !g_freestanding && can_emit_rbx(cg, n->children[1])) {
            emit_rbx(cg, n->children[1]);
            emit(cg, "    cmp rax, rbx\n");
        } else {
            emit(cg, "    push rax\n");
            gen_expr(cg, n->children[1]);
            emit(cg, "    mov rbx, rax\n    pop rax\n");
            emit(cg, "    cmp rax, rbx\n");
        }
        g_cmp_unsigned = uns;
        scc = cmp_setcc(n->op);
        g_cmp_unsigned = 0;
        if (scc) emit(cg, "    %s al\n", scc);
        if (legacy_bits()) emit(cg, "    movzx eax, al\n");
        else emit(cg, "    movzx rax, al\n");
    } else if (n->type == AST_LOGICAL) {
        if (!strcmp(n->op, "&&")) {
            // Short-circuit AND: if left is false (0), don't evaluate right
            int false_label = new_label(cg);
            int end_label = new_label(cg);

            gen_expr(cg, n->children[0]);  // Evaluate left
            if (legacy_bits()) emit(cg, "    test eax, eax\n");
            else emit(cg, "    test rax, rax\n");
            emit(cg, "    jz .L%d  ; Jump if left is false\n", false_label);

            gen_expr(cg, n->children[1]);  // Evaluate right
            if (legacy_bits()) emit(cg, "    test eax, eax\n");
            else emit(cg, "    test rax, rax\n");
            emit(cg, "    jz .L%d  ; Jump if right is false\n", false_label);

            if (legacy_bits()) emit(cg, "    mov eax, 1\n");
            else emit(cg, "    mov rax, 1  ; Both true\n");
            emit(cg, "    jmp .L%d\n", end_label);

            emit(cg, ".L%d:\n", false_label);
            if (legacy_bits()) emit(cg, "    xor eax, eax\n");
            else emit(cg, "    xor rax, rax  ; Result is false\n");
            emit(cg, ".L%d:\n", end_label);
        } else if (!strcmp(n->op, "||")) {
            // Short-circuit OR: if left is true (non-zero), don't evaluate right
            int true_label = new_label(cg);
            int end_label = new_label(cg);

            gen_expr(cg, n->children[0]);  // Evaluate left
            if (legacy_bits()) emit(cg, "    test eax, eax\n");
            else emit(cg, "    test rax, rax\n");
            emit(cg, "    jnz .L%d  ; Jump if left is true\n", true_label);

            gen_expr(cg, n->children[1]);  // Evaluate right
            if (legacy_bits()) emit(cg, "    test eax, eax\n");
            else emit(cg, "    test rax, rax\n");
            emit(cg, "    jnz .L%d  ; Jump if right is true\n", true_label);

            if (legacy_bits()) emit(cg, "    xor eax, eax\n");
            else emit(cg, "    xor rax, rax  ; Both false\n");
            emit(cg, "    jmp .L%d\n", end_label);

            emit(cg, ".L%d:\n", true_label);
            if (legacy_bits()) emit(cg, "    mov eax, 1\n");
            else emit(cg, "    mov rax, 1  ; Result is true\n");
            emit(cg, ".L%d:\n", end_label);
        }
    } else if (n->type == AST_CALL) {
        gen_builtin_call(cg, n);
    } else if (n->type == AST_ARRAY_LITERAL) {
        emit(cg, "    ; array literal\n");
        if (cg->symtab && cg->symtab->count > 0) {
            Symbol* sym = &cg->symtab->symbols[cg->symtab->count - 1];
            for (int i = 0; i < n->child_count; i++) {
                gen_expr(cg, n->children[i]);
                int elem_off = sym->offset + (i * 8);
                emit(cg, "    mov [rbp%d], rax\n", elem_off);
            }
            emit(cg, "    lea rax, [rbp%d]\n", sym->offset);
        }
    } else if (n->type == AST_INDEX) {
        gen_array_index(cg, n);
    } else if (n->type == AST_STRUCT_LITERAL) {
        emit(cg, "    ; struct literal %s\n", n->struct_type);
        if (cg->symtab && cg->symtab->count > 0) {
            Symbol* sym = &cg->symtab->symbols[cg->symtab->count - 1];
            gen_struct_literal_at(cg, n, sym->offset);
            emit(cg, "    lea rax, [rbp%d]\n", sym->offset);
        }
    } else if (n->type == AST_FIELD_ACCESS) {
        AstNode* obj = n->children[0];
        char* field_name = n->name;

        // [T; N] decays to the address of its first element. Loading it as a
        // scalar would read past the element and into the next field.
        {
            StructField* af = access_field(cg, n);
            if (af && af->array_len > 0) {
                if (!gen_field_addr(cg, obj, field_name))
                    diag_error(n->line, n->col, "cannot take the address of field '%s'", field_name);
                return;
            }
        }

        // Check if object is a dereference (pointer access)
        if (obj->type == AST_DEREF) {
            // ptr->field case (already desugared)
            AstNode* ptr = obj->children[0];
            int field_off = deref_field_offset(cg, ptr, field_name, n);
            if (field_off >= 0) {
                const char* stype = NULL;
                if (ptr && ptr->type == AST_IDENT && ptr->name) {
                    Symbol* psym = symtab_lookup_symbol(cg->symtab, ptr->name);
                    if (psym && psym->type_name)
                        stype = psym->type_name[0] == '*' ? psym->type_name + 1 : psym->type_name;
                }
                int psz = 8, psg = 0, poff = field_off, pvol = 0;
                int packed = field_mem_info(cg, stype, field_name, &poff, &psz, &psg, &pvol);
                gen_expr(cg, ptr);                                  // pointer value
                if (packed) {
                    char mem[64];
                    mem_base_disp(mem, 64, "rax", poff);
                    emit_sized_load_mark(cg, mem, psz, psg, pvol);
                } else {
                    emit(cg, "    mov rax, [rax+%d]\n", field_off);   // load field
                }
            }
        } else if (obj->type == AST_IDENT) {
            // Regular struct field access from identifier
            Symbol* sym = symtab_lookup_symbol(cg->symtab, obj->name);
            if (sym && sym->type_name) {
                // Get the actual struct type name (remove pointer if present)
                char* struct_type = sym->type_name;
                int is_pointer = 0;
                if (struct_type[0] == '*') {
                    struct_type = struct_type + 1;  // Skip the '*'
                    is_pointer = 1;
                }

                int field_off = typetab_field_offset(cg->types, struct_type, field_name);
                if (field_off >= 0) {
                    int psz = 8, psg = 0, poff = field_off, pvol = 0;
                    int packed = field_mem_info(cg, struct_type, field_name, &poff, &psz, &psg, &pvol);
                    if (is_pointer) {
                        // For pointers: load pointer, then load field
                        emit(cg, "    mov rax, [rbp%d]\n", sym->offset);  // Load pointer
                        if (packed) {
                            char mem[64];
                            mem_base_disp(mem, 64, "rax", poff);
                            emit_sized_load_mark(cg, mem, psz, psg, pvol);
                        } else if (field_off == 0) {
                            emit(cg, "    mov rax, [rax]\n");  // Load field at offset 0
                        } else {
                            emit(cg, "    mov rax, [rax+%d]\n", field_off);  // Load field
                        }
                    } else if (packed) {
                        char mem[64];
                        snprintf(mem, sizeof(mem), "rbp%+d", sym->offset + poff);
                        emit_sized_load_mark(cg, mem, psz, psg, pvol);
                    } else {
                        // For direct structs: load directly
                        emit(cg, "    mov rax, [rbp%d]\n", sym->offset + field_off);
                    }
                }
            } else {
                // Global struct field: g.field
                GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, obj->name);
                StructType* gst = (gvar && gvar->type_name && !gvar->is_array && !gvar->is_pointer)
                    ? typetab_lookup(cg->types, gvar->type_name) : NULL;
                if (gst) {
                    int field_off = typetab_field_offset(cg->types, gvar->type_name, field_name);
                    if (field_off >= 0) {
                        int psz = 8, psg = 0, poff = field_off, pvol = 0;
                        int packed = field_mem_info(cg, gvar->type_name, field_name, &poff, &psz, &psg, &pvol);
                        char mem[96];
                        mem_base_disp(mem, sizeof(mem), gvar->name, packed ? poff : field_off);
                        if (packed) emit_sized_load_mark(cg, mem, psz, psg, pvol);
                        else emit(cg, "    mov rax, [%s]\n", mem);
                    }
                }
            }
        } else if (obj->type == AST_FIELD_ACCESS) {
            // Nested scalar: a.p.x. The parent field is itself a struct value.
            char* rec = obj_record_type(cg, obj);
            int field_off = rec ? typetab_field_offset(cg->types, rec, field_name) : -1;
            if (rec && field_off >= 0 && gen_field_addr(cg, obj, field_name)) {
                int psz = 8, psg = 0, poff = 0, pvol = 0;
                if (field_mem_info(cg, rec, field_name, &poff, &psz, &psg, &pvol))
                    emit_sized_load_mark(cg, "rax", psz, psg, pvol);
                else
                    emit(cg, "    mov rax, [rax]\n");
            }
        } else {
            // Field access from expression (e.g., array[index].field)
            // Generate the expression first (should leave struct address in rax)
            gen_expr(cg, obj);

            // Now rax contains the address of the struct
            // We need to know the struct type to get field offset
            // For now, we'll try to infer it from the expression
            // This is a simplified approach - full implementation needs type inference

            // Try to get type from INDEX expression
            if (obj->type == AST_INDEX && obj->children && obj->child_count > 0) {
                AstNode* base = obj->children[0];
                char* struct_type = NULL;

                if (base->type == AST_IDENT) {
                    // Simple case: array[index].field
                    Symbol* base_sym = symtab_lookup_symbol(cg->symtab, base->name);
                    if (base_sym && base_sym->type_name) {
                        struct_type = base_sym->type_name;
                        // Remove pointer prefix if present
                        if (struct_type[0] == '*') struct_type++;
                    }
                    if (!struct_type) {
                        GlobalVar* gv = global_symtab_lookup(cg->global_symtab, base->name);
                        if (gv && gv->is_array && gv->elem_type) struct_type = gv->elem_type;
                    }
                } else if (base->type == AST_FIELD_ACCESS) {
                    // Complex case: struct.field[index].field
                    // E.g., container.tokens[0].value
                    // base is AST_FIELD_ACCESS for "container.tokens"
                    // Now we CAN determine the element type using type tracking!

                    if (base->children && base->child_count > 0) {
                        AstNode* struct_obj = base->children[0];
                        char* field_name_inner = base->name;  // "tokens"

                        if (struct_obj->type == AST_IDENT) {
                            // Look up the struct variable
                            Symbol* struct_sym = symtab_lookup_symbol(cg->symtab, struct_obj->name);
                            if (struct_sym && struct_sym->type_name) {
                                // Get the struct type (e.g., "Container" or "*Container")
                                char* container_type = struct_sym->type_name;
                                if (container_type[0] == '*') container_type++;

                                // Get the type of the field (e.g., "*Token" for tokens field)
                                char* field_type = typetab_field_type(cg->types, container_type, field_name_inner);
                                if (field_type) {
                                    // If it's a pointer type, extract base type
                                    if (field_type[0] == '*') {
                                        struct_type = field_type + 1;  // Skip '*' to get "Token"
                                    } else {
                                        struct_type = field_type;
                                    }
                                }
                            }
                        }
                    }
                }

                if (struct_type) {
                    int field_off = typetab_field_offset(cg->types, struct_type, field_name);
                    if (field_off >= 0) {
                        int psz = 8, psg = 0, poff = field_off, pvol = 0;
                        int packed = field_mem_info(cg, struct_type, field_name, &poff, &psz, &psg, &pvol);
                        if (packed) {
                            char mem[64];
                            mem_base_disp(mem, 64, "rax", poff);
                            emit_sized_load_mark(cg, mem, psz, psg, pvol);
                        } else if (field_off == 0) {
                            emit(cg, "    mov rax, [rax]\n");
                        } else {
                            emit(cg, "    mov rax, [rax+%d]\n", field_off);
                        }
                    }
                }
            }
        }
    } else if (n->type == AST_ARRAY_ASSIGN) {
        // Array assignment: array[index] = value
        // children[0] = array base
        // children[1] = index expression
        // children[2] = value expression

        if (!n->children || n->child_count < 3) return;
        AstNode* arr = n->children[0];
        AstNode* index_expr = n->children[1];
        AstNode* value_expr = n->children[2];
        if (arr && arr->type == AST_FIELD_ACCESS &&
            gen_embedded_array(cg, arr, index_expr, value_expr))
            return;
        check_indexable(cg, arr);

        if (!arr || !arr->name) return;

        // Whole-element copy when the element is a struct. Exact size, so the
        // next element is left alone (packed records included).
        {
            char* et = array_elem_type_name(cg, arr);
            if (et && et[0] != '*' && is_record(cg, et)) {
                AstNode idx;
                AstNode* ik[2];
                memset(&idx, 0, sizeof(idx));
                idx.type = AST_INDEX;
                idx.line = n->line;
                idx.col = n->col;
                ik[0] = arr;
                ik[1] = index_expr;
                idx.children = ik;
                idx.child_count = 2;
                gen_struct_assign(cg, &idx, value_expr);
                return;
            }
            if (et) check_types(value_expr, "", ty_from_spelled(cg, et), expr_ty(cg, value_expr));
        }

        // Hosted -O2: arr[i] = expr with i already in a register is one
        // store, base+index*scale. The value is computed first, so an index
        // that the value also reads is still the index afterwards.
        if (o2_hosted() && index_expr && index_expr->type == AST_IDENT) {
            Symbol* isym = symtab_lookup_symbol(cg->symtab, index_expr->name);
            const char* ireg = local_reg(isym);
            Symbol* asym = symtab_lookup_symbol(cg->symtab, arr->name);
            if (ireg && asym && !asym->is_pointer) {
                int el = 8;
                if (asym->type_name) {
                    if (!strcmp(asym->type_name, "i8") || !strcmp(asym->type_name, "u8")) el = 1;
                    else if (!strcmp(asym->type_name, "i16") || !strcmp(asym->type_name, "u16")) el = 2;
                    else if (!strcmp(asym->type_name, "i32") || !strcmp(asym->type_name, "u32")) el = 4;
                    else if (!strcmp(asym->type_name, "i64") || !strcmp(asym->type_name, "u64")) el = 8;
                }
                if (scale_ok(el)) {
                    char mem[96];
                    snprintf(mem, sizeof mem, "rbp%d+%s*%d", asym->offset, ireg, el);
                    if (el == 1 && try_emit_byte_add(cg, value_expr, mem, arr->name, index_expr->name, ireg))
                        return;
                    if (!emit_imm_store(cg, value_expr, mem, el)) {
                        gen_expr(cg, peel_store_cast(value_expr, el * 8));
                        emit_scaled_store(cg, mem, el, "rax");
                    }
                    return;
                }
            }
            if (ireg) {
                GlobalVar* gv = global_symtab_lookup(cg->global_symtab, arr->name);
                if (gv && gv->is_array) {
                    int el = 1;
                    if (gv->elem_type) {
                        if (!strcmp(gv->elem_type, "i32") || !strcmp(gv->elem_type, "u32")) el = 4;
                        else if (!strcmp(gv->elem_type, "i64") || !strcmp(gv->elem_type, "u64")) el = 8;
                        else if (!strcmp(gv->elem_type, "i16") || !strcmp(gv->elem_type, "u16")) el = 2;
                    }
                    if (scale_ok(el)) {
                        char mem[128];
                        snprintf(mem, sizeof mem, "%s+%s*%d", gv->name, ireg, el);
                        if (el == 1 && try_emit_byte_add(cg, value_expr, mem, arr->name, index_expr->name, ireg))
                            return;
                        if (!emit_imm_store(cg, value_expr, mem, el)) {
                            gen_expr(cg, peel_store_cast(value_expr, el * 8));
                            emit_scaled_store(cg, mem, el, "rax");
                        }
                        return;
                    }
                }
            }
        }

        // a[j+1] is [base+8+j*scale]. The value is computed first. Stores do
        // not bounds-check; the load of the value still does, as before.
        if (o2_hosted() && index_expr) {
            const char* ireg = NULL;
            long long add = 0;
            if (parse_index_disp(cg, index_expr, &ireg, &add)) {
                Symbol* asym = symtab_lookup_symbol(cg->symtab, arr->name);
                if (asym && !asym->is_pointer) {
                    int el = 8;
                    if (asym->type_name) {
                        if (!strcmp(asym->type_name, "i8") || !strcmp(asym->type_name, "u8")) el = 1;
                        else if (!strcmp(asym->type_name, "i16") || !strcmp(asym->type_name, "u16")) el = 2;
                        else if (!strcmp(asym->type_name, "i32") || !strcmp(asym->type_name, "u32")) el = 4;
                        else if (!strcmp(asym->type_name, "i64") || !strcmp(asym->type_name, "u64")) el = 8;
                    }
                    long long off = (long long)asym->offset + add * (long long)el;
                    if (scale_ok(el) && off >= -2147483648LL && off <= 2147483647LL) {
                        char mem[96];
                        snprintf(mem, sizeof mem, "rbp%lld+%s*%d", off, ireg, el);
                        if (!emit_imm_store(cg, value_expr, mem, el)) {
                            gen_expr(cg, peel_store_cast(value_expr, el * 8));
                            emit_scaled_store(cg, mem, el, "rax");
                        }
                        return;
                    }
                }
                GlobalVar* gv = global_symtab_lookup(cg->global_symtab, arr->name);
                if (gv && gv->is_array) {
                    int el = 1;
                    if (gv->elem_type) {
                        if (!strcmp(gv->elem_type, "i32") || !strcmp(gv->elem_type, "u32")) el = 4;
                        else if (!strcmp(gv->elem_type, "i64") || !strcmp(gv->elem_type, "u64")) el = 8;
                        else if (!strcmp(gv->elem_type, "i16") || !strcmp(gv->elem_type, "u16")) el = 2;
                    }
                    long long off = add * (long long)el;
                    if (scale_ok(el) && off >= -2147483648LL && off <= 2147483647LL) {
                        char mem[128];
                        snprintf(mem, sizeof mem, "%s%+lld+%s*%d", gv->name, off, ireg, el);
                        if (!emit_imm_store(cg, value_expr, mem, el)) {
                            gen_expr(cg, peel_store_cast(value_expr, el * 8));
                            emit_scaled_store(cg, mem, el, "rax");
                        }
                        return;
                    }
                }
            }
        }

        // arr[2] = 0 is one immediate store. The index is not in a register,
        // so the ident and reg±imm paths above do not see it.
        if (o2_hosted() && index_expr && index_expr->type == AST_NUMBER) {
            long long ix = 0;
            if (parse_i64(index_expr->value, &ix) && ix >= 0 && ix <= 2147483647LL) {
                Symbol* asym = symtab_lookup_symbol(cg->symtab, arr->name);
                if (asym && asym->is_array && !asym->is_pointer) {
                    int el = 8;
                    if (asym->type_name) {
                        if (!strcmp(asym->type_name, "i8") || !strcmp(asym->type_name, "u8")) el = 1;
                        else if (!strcmp(asym->type_name, "i16") || !strcmp(asym->type_name, "u16")) el = 2;
                        else if (!strcmp(asym->type_name, "i32") || !strcmp(asym->type_name, "u32")) el = 4;
                        else if (!strcmp(asym->type_name, "i64") || !strcmp(asym->type_name, "u64")) el = 8;
                    }
                    long long disp = (long long)asym->offset + ix * (long long)el;
                    if (scale_ok(el) && fits_imm32_val(disp)) {
                        char mem[96];
                        snprintf(mem, sizeof mem, "rbp%lld", disp);
                        if (emit_imm_store(cg, value_expr, mem, el)) return;
                    }
                }
                GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, arr->name);
                if (gvar && gvar->is_array) {
                    int el = 1;
                    if (gvar->elem_type) {
                        if (!strcmp(gvar->elem_type, "i32") || !strcmp(gvar->elem_type, "u32")) el = 4;
                        else if (!strcmp(gvar->elem_type, "i64") || !strcmp(gvar->elem_type, "u64")) el = 8;
                        else if (!strcmp(gvar->elem_type, "i16") || !strcmp(gvar->elem_type, "u16")) el = 2;
                    }
                    long long disp = ix * (long long)el;
                    if (scale_ok(el) && fits_imm32_val(disp)) {
                        char mem[128];
                        if (disp == 0) snprintf(mem, sizeof mem, "%s", gvar->name);
                        else snprintf(mem, sizeof mem, "%s%+lld", gvar->name, disp);
                        if (emit_imm_store(cg, value_expr, mem, el)) return;
                    }
                }
            }
        }

        // Evaluate value expression and save it. A matching `as` is not an
        // extra trunc: the store below writes only this element's width.
        {
            char* et = array_elem_type_name(cg, arr);
            int bits = 64, sg = 1;
            if (!et || !int_name(et, &bits, &sg)) bits = 64;
            gen_expr(cg, peel_store_cast(value_expr, bits));
        }
        emit(cg, "    push rax\n");  // Save value

        // Evaluate index expression
        gen_expr(cg, index_expr);

        // Try local array/pointer first
        Symbol* sym = symtab_lookup_symbol(cg->symtab, arr->name);
        if (sym) {
            // Check if this is a pointer (not an array)
            if (sym->is_pointer) {
                // Pointer assignment: ptr[index] = value
                // Determine element size from type_name
                int elem_size = 8;  // Default to i64
                if (sym->type_name) {
                    char* base_type = sym->type_name;
                    if (base_type[0] == '*') base_type++;  // Skip '*'

                    if (!strcmp(base_type, "i8") || !strcmp(base_type, "u8")) elem_size = 1;
                    else if ((!strcmp(base_type, "i16") || !strcmp(base_type, "u16"))) elem_size = 2;
                    else if (!strcmp(base_type, "i32") || !strcmp(base_type, "u32")) elem_size = 4;
                    else if (!strcmp(base_type, "i64") || !strcmp(base_type, "u64")) elem_size = 8;
                }

                // Scale index by element size
                if (elem_size > 1) {
                    emit_scale_reg(cg, "rax", elem_size);
                }

                // Load pointer and add offset
                emit(cg, "    mov rbx, [rbp%d]\n", sym->offset);  // Load pointer
                emit(cg, "    add rbx, rax\n");  // Add scaled index
                emit(cg, "    pop rax\n");  // Restore value

                // Store with correct width
                {
                    const char* mark = sym->volatile_pointee ? "  ; volatile" : "";
                    if (elem_size == 1) {
                        emit(cg, "    mov byte [rbx], al%s\n", mark);
                    } else if (elem_size == 2) {
                        emit(cg, "    mov word [rbx], ax%s\n", mark);
                    } else if (elem_size == 4) {
                        emit(cg, "    mov dword [rbx], eax%s\n", mark);
                    } else {
                        emit(cg, "    mov qword [rbx], rax%s\n", mark);
                    }
                }
            } else {
                // Local array assignment
                // Determine element size from type_name
                int elem_size = 8;  // Default to 8 bytes (i64)
                if (sym->type_name) {
                    if (!strcmp(sym->type_name, "i8") || !strcmp(sym->type_name, "u8")) elem_size = 1;
                    else if ((!strcmp(sym->type_name, "i16") || !strcmp(sym->type_name, "u16"))) elem_size = 2;
                    else if (!strcmp(sym->type_name, "i32") || !strcmp(sym->type_name, "u32")) elem_size = 4;
                    else if (!strcmp(sym->type_name, "i64") || !strcmp(sym->type_name, "u64")) elem_size = 8;
                }

                if (o2_hosted() && scale_ok(elem_size)) {
                    emit(cg, "    pop rbx\n");
                    char mem[96];
                    snprintf(mem, sizeof mem, "rbp%d+rax*%d", sym->offset, elem_size);
                    emit_scaled_store(cg, mem, elem_size, "rbx");
                } else {
                    if (elem_size > 1) {
                        emit_scale_reg(cg, "rax", elem_size);
                    }
                    emit(cg, "    mov rbx, rax\n");
                    emit(cg, "    pop rax\n");  // Restore value
                    if (elem_size == 1) {
                        emit(cg, "    mov byte [rbp%d+rbx], al\n", sym->offset);
                    } else if (elem_size == 2) {
                        emit(cg, "    mov word [rbp%d+rbx], ax\n", sym->offset);
                    } else if (elem_size == 4) {
                        emit(cg, "    mov dword [rbp%d+rbx], eax\n", sym->offset);
                    } else {
                        emit(cg, "    mov qword [rbp%d+rbx], rax\n", sym->offset);
                    }
                }
            }
        } else {
            // Try global array
            GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, arr->name);
            if (gvar && gvar->is_array) {
                // Calculate element size
                int elem_size = 1;  // i8
                if (gvar->elem_type) {
                    if ((!strcmp(gvar->elem_type, "i32") || !strcmp(gvar->elem_type, "u32"))) elem_size = 4;
                    else if ((!strcmp(gvar->elem_type, "i64") || !strcmp(gvar->elem_type, "u64"))) elem_size = 8;
                    else if ((!strcmp(gvar->elem_type, "i16") || !strcmp(gvar->elem_type, "u16"))) elem_size = 2;
                }

                if (o2_hosted() && scale_ok(elem_size)) {
                    emit(cg, "    pop rbx\n");
                    char mem[128];
                    snprintf(mem, sizeof mem, "%s+rax*%d", gvar->name, elem_size);
                    emit_scaled_store(cg, mem, elem_size, "rbx");
                } else {
                    if (elem_size > 1) {
                        emit_scale_reg(cg, "rax", elem_size);
                    }
                    emit(cg, "    lea rbx, [%s]\n", gvar->name);
                    emit(cg, "    add rbx, rax\n");
                    emit(cg, "    pop rax\n");  // Restore value
                    if (elem_size == 1) {
                        emit(cg, "    mov byte [rbx], al\n");
                    } else if (elem_size == 2) {
                        emit(cg, "    mov word [rbx], ax\n");
                    } else if (elem_size == 4) {
                        emit(cg, "    mov dword [rbx], eax\n");
                    } else {
                        emit(cg, "    mov qword [rbx], rax\n");
                    }
                }
            } else if (gvar && gvar->is_pointer) {
                const char* base = gvar->pointee_type;
                int elem_size = 8;
                if (base) {
                    if (!strcmp(base, "i8") || !strcmp(base, "u8")) elem_size = 1;
                    else if (!strcmp(base, "i16") || !strcmp(base, "u16")) elem_size = 2;
                    else if (!strcmp(base, "i32") || !strcmp(base, "u32")) elem_size = 4;
                    else if (!strcmp(base, "i64") || !strcmp(base, "u64")) elem_size = 8;
                    else {
                        StructType* est = typetab_lookup(cg->types, (char*)base);
                        if (est && est->size > 0) elem_size = est->size;
                    }
                }
                if (elem_size > 1) emit_scale_reg(cg, "rax", elem_size);
                emit(cg, "    mov rbx, [%s]\n", gvar->name);
                emit(cg, "    add rbx, rax\n");
                emit(cg, "    pop rax\n");
                {
                    const char* mark = gvar->volatile_pointee ? "  ; volatile" : "";
                    if (elem_size == 1) emit(cg, "    mov byte [rbx], al%s\n", mark);
                    else if (elem_size == 2) emit(cg, "    mov word [rbx], ax%s\n", mark);
                    else if (elem_size == 4) emit(cg, "    mov dword [rbx], eax%s\n", mark);
                    else emit(cg, "    mov qword [rbx], rax%s\n", mark);
                }
            }
        }
    } else if (n->type == AST_FIELD_ASSIGN) {
        // Field assignment: struct.field = value or array[index].field = value
        // name = field name
        // children[0] = struct object or array index expression
        // children[1] = value expression

        if (!n->children || n->child_count < 2) return;
        AstNode* obj = n->children[0];
        AstNode* value_expr = n->children[1];
        char* field_name = n->name;

        if (!obj || !field_name) return;

        // Copy an embedded struct field (nested records, packed or not).
        {
            char* rec = obj_record_type(cg, obj);
            char* ft = rec ? typetab_field_type(cg->types, rec, field_name) : NULL;
            if (ft && (ft[0] == '*' || !is_record(cg, ft)))
                check_types(value_expr, "", ty_from_spelled(cg, ft), expr_ty(cg, value_expr));
            if (ft && ft[0] != '*' && is_record(cg, ft)) {
                AstNode fld;
                AstNode* fk[1];
                memset(&fld, 0, sizeof(fld));
                fld.type = AST_FIELD_ACCESS;
                fld.name = field_name;
                fld.line = n->line;
                fld.col = n->col;
                fk[0] = obj;
                fld.children = fk;
                fld.child_count = 1;
                gen_struct_assign(cg, &fld, value_expr);
                return;
            }
        }

        // Evaluate value expression. The field store writes `ft`'s width.
        {
            char* rec = obj_record_type(cg, obj);
            char* ft = rec ? typetab_field_type(cg->types, rec, field_name) : NULL;
            int bits = 64, sg = 1;
            if (!ft || !int_name(ft, &bits, &sg)) bits = 64;
            gen_expr(cg, peel_store_cast(value_expr, bits));
        }
        emit(cg, "    push rax\n");  // Save value

        // Check if object is an array index expression (e.g., arr[0])
        if (obj->type == AST_INDEX) {
            // Field assignment to array element: arr[index].field = value
            if (!obj->children || obj->child_count < 2) return;
            AstNode* arr_base = obj->children[0];
            AstNode* index_expr = obj->children[1];

            if (!arr_base || !arr_base->name) return;

            // Evaluate index expression
            gen_expr(cg, index_expr);
            emit(cg, "    push rax\n");  // Save index

            // Look up array in symbol table
            Symbol* sym = symtab_lookup_symbol(cg->symtab, arr_base->name);
            if (sym && sym->type_name) {
                char* type_name = sym->type_name;

                // Determine element size and struct type
                int elem_size = 8;
                char* struct_type = type_name;

                // Check if it's a struct type
                StructType* st = typetab_lookup(cg->types, type_name);
                if (st) {
                    elem_size = st->size;
                    struct_type = type_name;
                }

                // Get field offset
                int field_off = typetab_field_offset(cg->types, struct_type, field_name);
                if (field_off >= 0) {
                    emit(cg, "    pop rbx\n");  // Restore index
                    emit(cg, "    ; Array element field assignment: %s[index].%s (elem_size=%d, offset %d)\n",
                         arr_base->name, field_name, elem_size, field_off);

                    // Calculate array element address
                    if (elem_size > 1) {
                        emit_scale_reg(cg, "rbx", elem_size);
                    }
                    emit(cg, "    lea rcx, [rbp%d+rbx]\n", sym->offset);  // rcx = address of array element

                    // Add field offset
                    if (field_off > 0) {
                        emit(cg, "    add rcx, %d\n", field_off);
                    }

                    emit(cg, "    pop rax\n");  // Restore value
                    {
                        int psz = 8, psg = 0, poff = field_off, pvol = 0;
                        if (field_mem_info(cg, struct_type, field_name, &poff, &psz, &psg, &pvol))
                            emit_sized_store_mark(cg, "rcx", psz, 0, pvol);
                        else {
                            trunc_rax_field(cg, struct_type, field_name);
                            emit(cg, "    mov [rcx], rax\n");  // Store value at field location
                        }
                    }
                }
            } else {
                // Global array of structs: idt[n].field = value
                GlobalVar* gv = global_symtab_lookup(cg->global_symtab, arr_base->name);
                StructType* st = (gv && gv->is_array && gv->elem_type) ? typetab_lookup(cg->types, gv->elem_type) : NULL;
                if (st) {
                    int field_off = typetab_field_offset(cg->types, gv->elem_type, field_name);
                    if (field_off >= 0) {
                        emit(cg, "    pop rbx\n");
                        emit_scale_reg(cg, "rbx", st->size);
                        emit(cg, "    lea rcx, [%s]\n", gv->name);
                        emit(cg, "    add rcx, rbx\n");
                        if (field_off > 0) emit(cg, "    add rcx, %d\n", field_off);
                        emit(cg, "    pop rax\n");
                        int psz = 8, psg = 0, poff = field_off, pvol = 0;
                        if (field_mem_info(cg, gv->elem_type, field_name, &poff, &psz, &psg, &pvol))
                            emit_sized_store_mark(cg, "rcx", psz, 0, pvol);
                        else {
                            trunc_rax_field(cg, gv->elem_type, field_name);
                            emit(cg, "    mov [rcx], rax\n");
                        }
                    } else {
                        emit(cg, "    pop rbx\n    pop rax\n");
                    }
                } else {
                    emit(cg, "    pop rbx\n    pop rax\n");
                }
            }
        } else if (obj->type == AST_DEREF && obj->child_count == 1) {
            // ptr->field = value
            int field_off = deref_field_offset(cg, obj->children[0], field_name, n);
            if (field_off >= 0) {
                const char* stype = NULL;
                AstNode* ptr = obj->children[0];
                if (ptr && ptr->type == AST_IDENT && ptr->name) {
                    Symbol* psym = symtab_lookup_symbol(cg->symtab, ptr->name);
                    if (psym && psym->type_name)
                        stype = psym->type_name[0] == '*' ? psym->type_name + 1 : psym->type_name;
                }
                int psz = 8, psg = 0, poff = field_off, pvol = 0;
                int packed = field_mem_info(cg, stype, field_name, &poff, &psz, &psg, &pvol);
                gen_expr(cg, obj->children[0]);   // pointer value
                if (packed) {
                    char mem[64];
                    mem_base_disp(mem, 64, "rcx", poff);
                    emit(cg, "    mov rcx, rax\n    pop rax\n");
                    emit_sized_store_mark(cg, mem, psz, 0, pvol);
                } else {
                    emit(cg, "    mov rcx, rax\n    pop rax\n");
                    trunc_rax_field(cg, stype, field_name);
                    emit(cg, "    mov [rcx+%d], rax\n", field_off);
                }
            } else {
                emit(cg, "    pop rax\n");
            }
        } else if (obj->type == AST_FIELD_ACCESS) {
            // Nested scalar store: a.p.x = value. Address the embedded field
            // and write only its width, so the rest of the parent stays put.
            char* rec = obj_record_type(cg, obj);
            int field_off = rec ? typetab_field_offset(cg->types, rec, field_name) : -1;
            if (rec && field_off >= 0 && gen_field_addr(cg, obj, field_name)) {
                int psz = 8, psg = 0, poff = 0, pvol = 0;
                int sized = field_mem_info(cg, rec, field_name, &poff, &psz, &psg, &pvol);
                emit(cg, "    mov rcx, rax\n    pop rax\n");
                if (sized) emit_sized_store_mark(cg, "rcx", psz, 0, pvol);
                else {
                    trunc_rax_field(cg, rec, field_name);
                    emit(cg, "    mov [rcx], rax\n");
                }
            } else {
                emit(cg, "    pop rax\n");
                if (rec && field_off < 0)
                    diag_error(n->line, n->col, "struct '%s' has no field '%s'", rec, field_name);
            }
        } else if (obj->name) {
            // Simple struct field assignment: struct.field = value
            // Look up struct object in symbol table
            Symbol* sym = symtab_lookup_symbol(cg->symtab, obj->name);
            if (sym && sym->type_name) {
                // Get the actual struct type name (remove pointer if present)
                char* struct_type = sym->type_name;
                int is_pointer = 0;
                if (struct_type[0] == '*') {
                    struct_type = struct_type + 1;  // Skip the '*'
                    is_pointer = 1;
                }

                // Get field offset
                int field_off = typetab_field_offset(cg->types, struct_type, field_name);
                if (field_off >= 0) {
                    emit(cg, "    pop rcx\n");  // Restore value into rcx
                    emit(cg, "    ; Field assignment: %s.%s (offset %d)\n", obj->name, field_name, field_off);
                    int psz = 8, psg = 0, poff = field_off, pvol = 0;
                    int packed = field_mem_info(cg, struct_type, field_name, &poff, &psz, &psg, &pvol);

                    if (is_pointer) {
                        // For pointers: load pointer, add offset, store
                        emit(cg, "    mov rbx, [rbp%d]\n", sym->offset);  // Load pointer
                        if (packed) {
                            char mem[64];
                            mem_base_disp(mem, 64, "rbx", poff);
                            emit_sized_store_mark(cg, mem, psz, 1, pvol);
                        } else if (field_off == 0) {
                            trunc_rcx_named(cg, typetab_field_type(cg->types, struct_type, field_name));
                            emit(cg, "    mov [rbx], rcx\n");  // Store through pointer (no offset)
                        } else {
                            trunc_rcx_named(cg, typetab_field_type(cg->types, struct_type, field_name));
                            emit(cg, "    mov [rbx+%d], rcx\n", field_off);  // Store through pointer with offset
                        }
                    } else if (packed) {
                        char mem[64];
                        snprintf(mem, sizeof(mem), "rbp%+d", sym->offset + poff);
                        emit_sized_store_mark(cg, mem, psz, 1, pvol);
                    } else {
                        // For direct structs: store directly. An 8-byte slot still
                        // keeps only the field's declared width.
                        trunc_rcx_named(cg, typetab_field_type(cg->types, struct_type, field_name));
                        emit(cg, "    mov [rbp%d], rcx\n", sym->offset + field_off);
                    }
                }
            } else {
                GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, obj->name);
                StructType* gst = (gvar && gvar->type_name && !gvar->is_array && !gvar->is_pointer)
                    ? typetab_lookup(cg->types, gvar->type_name) : NULL;
                if (gst) {
                    int field_off = typetab_field_offset(cg->types, gvar->type_name, field_name);
                    if (field_off >= 0) {
                        emit(cg, "    pop rcx\n");
                        int psz = 8, psg = 0, poff = field_off, pvol = 0;
                        int packed = field_mem_info(cg, gvar->type_name, field_name, &poff, &psz, &psg, &pvol);
                        char mem[96];
                        mem_base_disp(mem, sizeof(mem), gvar->name, packed ? poff : field_off);
                        if (packed) emit_sized_store_mark(cg, mem, psz, 1, pvol);
                        else {
                            trunc_rcx_named(cg, typetab_field_type(cg->types, gvar->type_name, field_name));
                            emit(cg, "    mov [%s], rcx\n", mem);
                        }
                    } else {
                        emit(cg, "    pop rax\n");
                    }
                }
            }
        }
    }
}

// ---- hosted -O2: scalar locals in r12-r15 and, if no call is inside the
// live range, r10-r11. A call tick strictly between the first and last
// touch means the name crosses that call.
#define MAX_REG_CAND 128
#define MAX_CALL_TICK 512
typedef struct {
    char name[96];
    int decls, uses, bad, ord;
    int lo, hi;
} RegCand;
static RegCand g_reg_cand[MAX_REG_CAND];
static int g_reg_ncand;
static int g_reg_asm;
static int g_tick;
static int g_call_tick[MAX_CALL_TICK];
static int g_ncall_tick;
static int g_call_overflow;

static RegCand* reg_cand(const char* name) {
    if (!name || strlen(name) >= sizeof g_reg_cand[0].name) return NULL;
    for (int i = 0; i < g_reg_ncand; i++)
        if (!strcmp(g_reg_cand[i].name, name)) return &g_reg_cand[i];
    if (g_reg_ncand >= MAX_REG_CAND) return NULL;
    RegCand* c = &g_reg_cand[g_reg_ncand++];
    memset(c, 0, sizeof *c);
    memcpy(c->name, name, strlen(name) + 1);
    c->ord = g_reg_ncand;
    c->lo = 0x7fffffff;
    c->hi = -1;
    return c;
}

static void reg_touch(const char* name) {
    RegCand* c = reg_cand(name);
    if (!c) return;
    if (g_tick < c->lo) c->lo = g_tick;
    if (g_tick > c->hi) c->hi = g_tick;
    g_tick++;
}

static void reg_note_call(void) {
    if (g_ncall_tick >= MAX_CALL_TICK) { g_call_overflow = 1; return; }
    g_call_tick[g_ncall_tick++] = g_tick++;
}

static void reg_note_decl(const char* name, int bad) {
    RegCand* c = reg_cand(name);
    if (!c) return;
    c->decls++;
    if (bad) c->bad = 1;
}

static void reg_note_use(const char* name) {
    RegCand* c = reg_cand(name);
    if (c) c->uses++;
}

static void reg_walk(Codegen* cg, AstNode* n) {
    if (!n || n->type == AST_FUNCTION) return;
    if (n->type == AST_CALL && n->name && !strcmp(n->name, "asm")) g_reg_asm = 1;
    if (n->type == AST_LET && n->name) {
        for (int i = 0; i < n->child_count; i++) reg_walk(cg, n->children[i]);
        int bad = (n->flags & AF_VOLATILE) || n->is_pointer;
        if (n->struct_type && !strcmp(n->struct_type, "__array__")) bad = 1;
        else if (n->struct_type && typetab_lookup(cg->types, n->struct_type)) bad = 1;
        if (n->child_count > 0 && n->children[0] &&
            (n->children[0]->type == AST_ARRAY_LITERAL ||
             n->children[0]->type == AST_STRUCT_LITERAL))
            bad = 1;
        reg_note_decl(n->name, bad);
        reg_touch(n->name);
        return;
    }
    if (n->type == AST_ADDR_OF && n->child_count > 0 && n->children[0] &&
        n->children[0]->type == AST_IDENT)
        reg_note_decl(n->children[0]->name, 1);
    if ((n->type == AST_ASSIGN || n->type == AST_ARRAY_ASSIGN) && n->name) {
        for (int i = 0; i < n->child_count; i++) reg_walk(cg, n->children[i]);
        reg_note_use(n->name);
        reg_touch(n->name);
        return;
    }
    if (n->type == AST_IDENT && n->name) {
        reg_note_use(n->name);
        reg_touch(n->name);
        return;
    }
    for (int i = 0; i < n->child_count; i++) reg_walk(cg, n->children[i]);
    if (n->type == AST_CALL) reg_note_call();
}

static int reg_crosses(RegCand* c) {
    if (g_call_overflow) return 1;
    if (c->lo > c->hi) return 0;
    for (int i = 0; i < g_ncall_tick; i++) {
        int t = g_call_tick[i];
        if (c->lo < t && t < c->hi) return 1;
    }
    return 0;
}

static int reg_eligible(RegCand* c) {
    return !c->bad && c->decls == 1 && c->uses > 0;
}

// cross_mode: 1 only names live across a call, 0 only names that are not, -1 either.
static int reg_pick(int* taken, int cross_mode) {
    int best = -1;
    for (int i = 0; i < g_reg_ncand; i++) {
        if (taken[i]) continue;
        RegCand* c = &g_reg_cand[i];
        if (!reg_eligible(c)) continue;
        int cr = reg_crosses(c);
        if (cross_mode == 1 && !cr) continue;
        if (cross_mode == 0 && cr) continue;
        if (best < 0 || c->uses > g_reg_cand[best].uses ||
            (c->uses == g_reg_cand[best].uses && c->ord < g_reg_cand[best].ord))
            best = i;
    }
    return best;
}

// Callee-saved r12-r15 first, preferring names that cross a call. r10 and r11
// only for names that do not. A fifth crosser stays on the stack: there is
// no spill around the call. asm() disables the whole plan. Too many calls
// to remember is treated as every name crossing.
static void plan_reg_locals(Codegen* cg, AstNode* fn) {
    g_reg_local_n = 0;
    g_reg_callee_n = 0;
    g_reg_ncand = 0;
    g_reg_asm = 0;
    g_tick = 0;
    g_ncall_tick = 0;
    g_call_overflow = 0;
    for (int i = 0; i < MAX_REG_LOCAL; i++) g_reg_local_var[i][0] = '\0';
    if (!fn || fn->child_count < 1) return;
    int pc = fn->child_count - 1;
    for (int i = 0; i < pc; i++) {
        AstNode* par = fn->children[i];
        if (!par || !par->name) continue;
        int bad = par->is_pointer != 0;
        if (par->value && typetab_lookup(cg->types, par->value)) bad = 1;
        reg_note_decl(par->name, bad);
        reg_touch(par->name);
    }
    reg_walk(cg, fn->children[pc]);
    if (g_reg_asm) return;
    int taken[MAX_REG_CAND];
    memset(taken, 0, sizeof taken);
    int slot = 0;
    for (int pass = 0; pass < 2 && slot < MAX_REG_CALLEE; pass++) {
        int mode = pass == 0 ? 1 : -1;
        while (slot < MAX_REG_CALLEE) {
            int best = reg_pick(taken, mode);
            if (best < 0) break;
            taken[best] = 1;
            snprintf(g_reg_local_var[slot], sizeof g_reg_local_var[0], "%s", g_reg_cand[best].name);
            slot++;
        }
    }
    g_reg_callee_n = slot;
    int extra = 0;
    while (extra < MAX_REG_LOCAL - MAX_REG_CALLEE) {
        int best = reg_pick(taken, 0);
        if (best < 0) break;
        taken[best] = 1;
        snprintf(g_reg_local_var[MAX_REG_CALLEE + extra], sizeof g_reg_local_var[0], "%s", g_reg_cand[best].name);
        extra++;
    }
    g_reg_local_n = extra ? MAX_REG_CALLEE + extra : g_reg_callee_n;
}

static void emit_reg_restore(Codegen* cg) {
    for (int i = 0; i < g_reg_callee_n; i++)
        emit(cg, "    mov %s, [rbp - __frame_%s + %d]\n",
             g_reg_local_name[i], g_fn_name, 8 * (i + 1));
}

static void emit_hosted_epilogue(Codegen* cg) {
    if (g_slim) {
        // The pushes are the only frame. Early-out jumped here before them.
        if (g_slim_pad) emit(cg, "    add rsp, 8\n");
        for (int i = g_slim_nsave - 1; i >= 0; i--)
            emit(cg, "    pop %s\n", g_slim_save[i]);
        emit(cg, "    ret\n");
        return;
    }
    emit_reg_restore(cg);
    if (g_expr_tmp)
        emit(cg, "    mov %s, [rbp - __frame_%s + %d]\n",
             g_expr_tmp, g_fn_name, 8 * (g_reg_callee_n + 1));
    emit(cg, "    leave\n    ret\n");
}

// A naked scalar or pointer lives at a label, not in a frame. `bytes` is what
// symtab_add just added; an array or struct is left on the stack and rejected.
static void naked_take_slot(Symbol* sym, int bytes, int line, int col) {
    char buf[160];
    if (!(g_fn_flags & AF_NAKED) || !sym || sym->is_array) return;
    if (g_naked_slot_n >= MAX_NAKED_SLOTS) {
        diag_error(line, col, "too many locals in a naked function (maximum %d)", MAX_NAKED_SLOTS);
        return;
    }
    snprintf(buf, sizeof buf, "__loc_%s_%d", g_fn_name, g_naked_slot_seq++);
    sym->mem = strdup(buf);
    g_naked_slots[g_naked_slot_n++] = sym->mem;
    g_naked_slot_bytes += bytes;
}

// A parameter that arrives in a register, not a struct passed by pointer.
static const char* param_in_reg(Codegen* cg, AstNode* fn, const char* name) {
    static const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    int pc = fn->child_count - 1;
    if (pc < 1 || !name) return NULL;
    int nreg = pc < 6 ? pc : 6;
    for (int i = 0; i < nreg; i++) {
        AstNode* p = fn->children[i];
        if (!p || !p->name || strcmp(p->name, name)) continue;
        if (!p->is_pointer && p->value && typetab_lookup(cg->types, p->value)) return NULL;
        return regs[i];
    }
    return NULL;
}

static const char* jcc_when_true(const char* op, int swapped) {
    if (!op) return NULL;
    if (!strcmp(op, "==")) return "je";
    if (!strcmp(op, "!=")) return "jne";
    if (!swapped) {
        if (!strcmp(op, "<")) return ucond("jl");
        if (!strcmp(op, ">")) return ucond("jg");
        if (!strcmp(op, "<=")) return ucond("jle");
        if (!strcmp(op, ">=")) return ucond("jge");
    } else {
        if (!strcmp(op, "<")) return ucond("jg");
        if (!strcmp(op, ">")) return ucond("jl");
        if (!strcmp(op, "<=")) return ucond("jge");
        if (!strcmp(op, ">=")) return ucond("jle");
    }
    return NULL;
}

// Hosted -O2: `if (param <op> leaf) { return param; }` as the first statement.
// The compare uses the incoming register and returns before the frame exists,
// so the base case does not store and reload a callee-saved register.
typedef struct {
    int ok;
    const char* ret_reg;
    const char* lhs;
    const char* rhs;
    long long imm;
    int imm_rhs;
    const char* jcc;
} EarlyRet;

static int early_leaf(Codegen* cg, AstNode* fn, AstNode* n, const char** reg, long long* imm, int* is_imm) {
    if (!n) return 0;
    if (n->type == AST_NUMBER) {
        long long v;
        if (!parse_i64(n->value, &v) || !fits_imm32_val(v)) return 0;
        *is_imm = 1;
        *imm = v;
        return 1;
    }
    if (n->type == AST_IDENT && n->name) {
        const char* r = param_in_reg(cg, fn, n->name);
        if (!r) return 0;
        *is_imm = 0;
        *reg = r;
        return 1;
    }
    return 0;
}

static EarlyRet match_early_ret(Codegen* cg, AstNode* fn) {
    EarlyRet e;
    memset(&e, 0, sizeof e);
    if (!o2_hosted() || !fn || !fn->name) return e;
    if (fn->flags & (AF_INTERRUPT | AF_SYSCALL | AF_NAKED)) return e;
    int pc = fn->child_count - 1;
    if (pc < 1) return e;
    AstNode* body = fn->children[pc];
    if (!body || body->type != AST_BLOCK || body->child_count < 1) return e;
    AstNode* st = body->children[0];
    if (!st || st->type != AST_IF || st->child_count != 2) return e;
    AstNode* cond = st->children[0];
    AstNode* thenb = st->children[1];
    if (!cond || cond->type != AST_COMPARE || cond->child_count != 2 || !cond->op) return e;
    if (!thenb || thenb->type != AST_BLOCK || thenb->child_count != 1) return e;
    AstNode* ret = thenb->children[0];
    if (!ret || ret->type != AST_RETURN || ret->child_count != 1) return e;
    AstNode* rv = ret->children[0];
    if (!rv || rv->type != AST_IDENT || !rv->name) return e;
    const char* ret_reg = param_in_reg(cg, fn, rv->name);
    if (!ret_reg) return e;
    const char* lr = NULL;
    const char* rr = NULL;
    long long li = 0, ri = 0;
    int li_imm = 0, ri_imm = 0;
    if (!early_leaf(cg, fn, cond->children[0], &lr, &li, &li_imm)) return e;
    if (!early_leaf(cg, fn, cond->children[1], &rr, &ri, &ri_imm)) return e;
    if (li_imm && ri_imm) return e;
    const char* jcc = NULL;
    if (!li_imm) {
        jcc = jcc_when_true(cond->op, 0);
        e.lhs = lr;
        if (ri_imm) { e.imm_rhs = 1; e.imm = ri; }
        else e.rhs = rr;
    } else {
        jcc = jcc_when_true(cond->op, 1);
        e.lhs = rr;
        e.imm_rhs = 1;
        e.imm = li;
    }
    if (!jcc) return e;
    e.ok = 1;
    e.ret_reg = ret_reg;
    e.jcc = jcc;
    return e;
}

void gen_stmt(Codegen* cg, AstNode* n);

// Memory operand of `jmp [name]`: a naked slot, a data label, or a global.
// A register parameter has no address. NULL if the name is not memory.
static const char* jmp_mem_label(Codegen* cg, const char* name) {
    Symbol* sym;
    GlobalVar* gv;
    if (!name) return NULL;
    sym = cg->symtab ? symtab_lookup_symbol(cg->symtab, (char*)name) : NULL;
    if (sym) return sym->mem;
    if (is_data_name(cg, name)) return name;
    gv = cg->global_symtab ? global_symtab_lookup(cg->global_symtab, (char*)name) : NULL;
    if (gv) return gv->name;
    return NULL;
}

// Direct `jmp 0x8200` / `jmp label` stay available outside naked. A GPR name
// is an indirect jump in NASM (`jmp rax` is not a label), so that form is
// naked-only and is not emitted otherwise. `jmp [name]` is the same rule:
// the size prefix is the mode's address width (`qword` / `dword` / `word`).
static void gen_jmp(Codegen* cg, AstNode* n) {
    if (n->flags & AF_JMP_MEM) {
        const char* lab;
        const char* sz;
        if (!(g_fn_flags & AF_NAKED)) {
            diag_error(n->line, n->col, "an indirect jmp is only available in a naked function");
            return;
        }
        lab = jmp_mem_label(cg, n->name);
        if (!lab) {
            diag_error(n->line, n->col, "jmp [%s] takes a variable or a data label",
                       n->name ? n->name : "");
            return;
        }
        sz = g_fn_bits == 16 ? "word" : g_fn_bits == 32 ? "dword" : "qword";
        emit(cg, "    jmp %s [%s]\n", sz, lab);
        return;
    }
    if (n->name) {
        int cls = stack_reg_class(n->name);
        if (cls >= 2) {
            int bits = g_fn_bits ? g_fn_bits : 64;
            if (!(g_fn_flags & AF_NAKED)) {
                diag_error(n->line, n->col, "an indirect jmp is only available in a naked function");
                return;
            }
            if (!stack_reg_here(cls)) {
                diag_error(n->line, n->col, "register '%s' is not available in bits %d", n->name, bits);
                return;
            }
            emit(cg, "    jmp %s\n", n->name);
            return;
        }
        if (!find_func(cg, n->name) && !is_data_name(cg, n->name) && !is_code_label(cg, n->name))
            diag_error(n->line, n->col, "unknown jump target '%s'", n->name);
        emit(cg, "    jmp %s\n", n->name);
        return;
    }
    if (n->value) emit(cg, "    jmp %s\n", n->value);
}

static char* g_seen_labels[128];
static int g_nseen_labels;
static int regplan_has(const char* name) {
    if (!name) return 0;
    for (int i = 0; i < g_reg_local_n; i++)
        if (g_reg_local_var[i][0] && !strcmp(g_reg_local_var[i], name)) return 1;
    return 0;
}

// A value can_emit_rbx can put in rbx, judged before the symtab exists:
// a reg-plan local, a scalar global, a number, or an alu whose right is a leaf.
static int slim_leaf(Codegen* cg, AstNode* n) {
    if (!n) return 0;
    if (n->type == AST_NUMBER) return 1;
    if (n->type == AST_UNARY && n->op && n->child_count == 1 &&
        (n->op[0] == '-' || n->op[0] == '!' || n->op[0] == '~'))
        return slim_leaf(cg, n->children[0]);
    if (n->type != AST_IDENT || !n->name) return 0;
    if (regplan_has(n->name)) return 1;
    GlobalVar* gv = global_symtab_lookup(cg->global_symtab, n->name);
    return gv && !gv->is_array && !gv->is_pointer;
}

static int slim_rbx(Codegen* cg, AstNode* n) {
    if (!n) return 0;
    if (slim_leaf(cg, n)) return 1;
    if (n->type == AST_UNARY && n->op && n->child_count == 1 &&
        (n->op[0] == '-' || n->op[0] == '!' || n->op[0] == '~'))
        return slim_rbx(cg, n->children[0]);
    if (n->type == AST_BINOP && n->op && n->child_count == 2 &&
        n->op[0] != '/' && n->op[0] != '%')
        return slim_rbx(cg, n->children[0]) && slim_leaf(cg, n->children[1]);
    return 0;
}

// Direct user call whose arguments place into rdi..r9 with no push.
static int slim_call_shape(Codegen* cg, AstNode* n) {
    if (!n || n->type != AST_CALL || !n->name || !strcmp(n->name, "asm")) return 0;
    AstNode* fn = find_func(cg, n->name);
    if (!fn) return 0;
    int argc = n->child_count;
    int want = fn->child_count - 1;
    if (argc != want || argc > 6) return 0;
    for (int i = 0; i < argc; i++) {
        AstNode* arg = n->children[i];
        AstNode* p = fn->children[i];
        if (!arg || node_has_call(arg)) return 0;
        if (p && p->value && typetab_lookup(cg->types, p->value)) return 0;
        if (!slim_rbx(cg, arg)) return 0;
    }
    return 1;
}

// 1 if generating this expression would push. A slim frame has no matching pop.
static int expr_pushes(Codegen* cg, AstNode* n) {
    if (!n) return 0;
    if (n->type == AST_INDEX || n->type == AST_ADDR_OF || n->type == AST_DEREF ||
        n->type == AST_FIELD_ACCESS || n->type == AST_STRING || n->type == AST_CAST ||
        n->type == AST_ARRAY_LITERAL || n->type == AST_STRUCT_LITERAL ||
        n->type == AST_ARRAY_ASSIGN || n->type == AST_FIELD_ASSIGN)
        return 1;
    if (n->type == AST_CALL) {
        if (!slim_call_shape(cg, n)) return 1;
        for (int i = 0; i < n->child_count; i++)
            if (expr_pushes(cg, n->children[i])) return 1;
        return 0;
    }
    if ((n->type == AST_BINOP || n->type == AST_COMPARE) && n->child_count == 2) {
        AstNode* R = n->children[1];
        int leaves = n->type == AST_COMPARE && slim_leaf(cg, n->children[0]) && slim_leaf(cg, R);
        if (!leaves) {
            if (node_has_call(R)) {
                if (!(g_expr_tmp && expr_tmp_depth(R) == 0)) return 1;
            } else if (!slim_rbx(cg, R)) return 1;
        }
    }
    for (int i = 0; i < n->child_count; i++)
        if (expr_pushes(cg, n->children[i])) return 1;
    return 0;
}

static int slim_stmt_ok(Codegen* cg, AstNode* n) {
    if (!n) return 1;
    if (n->type == AST_BLOCK) {
        for (int i = 0; i < n->child_count; i++)
            if (!slim_stmt_ok(cg, n->children[i])) return 0;
        return 1;
    }
    if (n->type == AST_RETURN) {
        if (n->child_count == 0) return 1;
        return !expr_pushes(cg, n->children[0]);
    }
    if (n->type == AST_LET) {
        if ((n->flags & AF_VOLATILE) || n->is_pointer || n->struct_type) return 0;
        if (!regplan_has(n->name)) return 0;
        if (n->child_count > 0 && expr_pushes(cg, n->children[0])) return 0;
        return 1;
    }
    if (n->type == AST_ASSIGN) {
        if (!regplan_has(n->name)) return 0;
        return n->child_count > 0 && !expr_pushes(cg, n->children[0]);
    }
    if (n->type == AST_IF) {
        if (n->flags & AF_CARRY) return 0;
        for (int i = 0; i < n->child_count; i++) {
            if (i == 0) { if (expr_pushes(cg, n->children[0])) return 0; }
            else if (!slim_stmt_ok(cg, n->children[i])) return 0;
        }
        return 1;
    }
    if (n->type == AST_WHILE) {
        if (n->child_count < 1 || expr_pushes(cg, n->children[0])) return 0;
        for (int i = 1; i < n->child_count; i++)
            if (!slim_stmt_ok(cg, n->children[i])) return 0;
        return 1;
    }
    if (n->type == AST_BREAK || n->type == AST_CONTINUE) return 1;
    if (n->type == AST_CALL) return !expr_pushes(cg, n);
    return 0;
}

// Push/pop frame only when the body never addresses rbp and never pushes.
static void prepare_frame(Codegen* cg, AstNode* fn) {
    g_expr_tmp = NULL;
    g_slim = 0;
    g_slim_pad = 0;
    g_slim_nsave = 0;
    if (!o2_hosted() || !fn || g_reg_asm || fn->child_count < 1) return;
    int pc = fn->child_count - 1;
    AstNode* body = fn->children[pc];
    int depth = expr_tmp_depth(body);
    if (depth >= 1 && g_reg_callee_n < MAX_REG_CALLEE)
        g_expr_tmp = g_reg_local_name[g_reg_callee_n];
    if (pc > 6) return;
    for (int i = 0; i < pc; i++) {
        AstNode* p = fn->children[i];
        if (!p || !p->name || p->is_pointer) return;
        if (p->value && typetab_lookup(cg->types, p->value)) return;
        if (!regplan_has(p->name)) return;
    }
    if (depth > 1 || (depth >= 1 && !g_expr_tmp)) return;
    if (!slim_stmt_ok(cg, body)) return;
    g_slim = 1;
    int nsave = 0;
    if (g_expr_tmp) g_slim_save[nsave++] = g_expr_tmp;
    for (int i = 0; i < g_reg_callee_n && nsave < 5; i++)
        g_slim_save[nsave++] = g_reg_local_name[i];
    g_slim_nsave = nsave;
    // Entry rsp ≡ 8. An even number of pushes leaves rsp ≡ 8; one pad makes ≡ 0.
    g_slim_pad = (nsave % 2 == 0);
}

static const char* fail_jcc_sw(const char* op, int swapped) {
    const char* t = jcc_when_true(op, swapped);
    if (!t) return NULL;
    if (!strcmp(t, "je")) return "jne";
    if (!strcmp(t, "jne")) return "je";
    if (!strcmp(t, "jl")) return "jge";
    if (!strcmp(t, "jge")) return "jl";
    if (!strcmp(t, "jg")) return "jle";
    if (!strcmp(t, "jle")) return "jg";
    return NULL;
}

// Fall-through means this register is >= 0. Only a scalar compare, not an index.
static const char* nonneg_reg_of(Codegen* cg, AstNode* n) {
    if (!n || n->type != AST_COMPARE || n->child_count != 2 || !n->op) return NULL;
    AstNode* a = n->children[0];
    AstNode* b = n->children[1];
    const char* id = NULL;
    if (a && a->type == AST_IDENT && b && b->type == AST_NUMBER) {
        long long v;
        if (!parse_i64(b->value, &v)) return NULL;
        if (!((!strcmp(n->op, ">=") && v == 0) || (!strcmp(n->op, ">") && v == -1))) return NULL;
        id = a->name;
    } else if (b && b->type == AST_IDENT && a && a->type == AST_NUMBER) {
        long long v;
        if (!parse_i64(a->value, &v)) return NULL;
        if (!((!strcmp(n->op, "<=") && v == 0) || (!strcmp(n->op, "<") && v == -1))) return NULL;
        id = b->name;
    } else return NULL;
    Symbol* s = id ? symtab_lookup_symbol(cg->symtab, (char*)id) : NULL;
    return local_reg(s);
}

static int cond_cmp_ok(Codegen* cg, AstNode* n) {
    if (!n || n->type != AST_COMPARE || !n->op || !cmp_fail_jcc(n->op) || n->child_count != 2)
        return 0;
    AstNode* a = n->children[0];
    AstNode* b = n->children[1];
    int ai = a && a->type == AST_INDEX;
    int bi = b && b->type == AST_INDEX;
    if (ai && bi) return 0;
    if (!ai && !bi) {
        OpLeaf L, R;
        return scalar_leaf(cg, a, &L) && scalar_leaf(cg, b, &R);
    }
    OpLeaf L;
    return scalar_leaf(cg, ai ? b : a, &L);
}

static int can_branch_cond(Codegen* cg, AstNode* n) {
    if (!n || legacy_bits()) return 0;
    if (n->type == AST_LOGICAL && n->op && n->child_count == 2 &&
        (!strcmp(n->op, "&&") || !strcmp(n->op, "||")))
        return can_branch_cond(cg, n->children[0]) && can_branch_cond(cg, n->children[1]);
    return cond_cmp_ok(cg, n);
}

static void emit_cond_fail(Codegen* cg, AstNode* n, int fail_lab);
static void emit_cond_succeed(Codegen* cg, AstNode* n, int ok_lab);

static void note_fwd_index(AstNode* ix) {
    g_fwd_ok = 0;
    if (!ix || ix->child_count < 2) return;
    AstNode* arr = ix->children[0];
    AstNode* id = ix->children[1];
    if (!arr || arr->type != AST_IDENT || !arr->name) return;
    if (!id || id->type != AST_IDENT || !id->name) return;
    if (strlen(arr->name) >= sizeof g_fwd_arr || strlen(id->name) >= sizeof g_fwd_idx) return;
    snprintf(g_fwd_arr, sizeof g_fwd_arr, "%s", arr->name);
    snprintf(g_fwd_idx, sizeof g_fwd_idx, "%s", id->name);
    g_fwd_ok = 1;
}

static void emit_branch_cmp(Codegen* cg, AstNode* n, int lab, int succeed) {
    g_fwd_ok = 0;
    AstNode* a = n->children[0];
    AstNode* b = n->children[1];
    int ai = a && a->type == AST_INDEX;
    int bi = b && b->type == AST_INDEX;
    int swapped = 0;
    if (!ai && !bi) {
        emit_cmp_raw(cg, n);
    } else {
        AstNode* ix = ai ? a : b;
        AstNode* leaf = ai ? b : a;
        const char* saved = g_index_dest;
        g_index_dest = NULL;
        gen_array_index(cg, ix);
        g_index_dest = saved;
        note_fwd_index(ix);
        OpLeaf L;
        scalar_leaf(cg, leaf, &L);
        if (ai || L.kind == 3) {
            if (!ai) swapped = 1;
            if (L.kind == 3) emit(cg, "    cmp rax, %lld\n", L.imm);
            else if (L.kind == 1) emit(cg, "    cmp rax, %s\n", L.reg);
            else emit(cg, "    cmp rax, [%s]%s\n", L.mem, L.vol ? "  ; volatile" : "");
        } else if (L.kind == 1) {
            emit(cg, "    cmp %s, rax\n", L.reg);
        } else {
            emit(cg, "    cmp qword [%s], rax%s\n", L.mem, L.vol ? "  ; volatile" : "");
        }
    }
    const char* jcc = succeed ? jcc_when_true(n->op, swapped) : fail_jcc_sw(n->op, swapped);
    emit(cg, "    %s .L%d\n", jcc, lab);
}

static void emit_cond_fail(Codegen* cg, AstNode* n, int fail_lab) {
    if (n->type == AST_LOGICAL && n->op && !strcmp(n->op, "&&")) {
        emit_cond_fail(cg, n->children[0], fail_lab);
        const char* nn = nonneg_reg_of(cg, n->children[0]);
        const char* saved = g_nonneg_reg;
        if (nn) g_nonneg_reg = nn;
        emit_cond_fail(cg, n->children[1], fail_lab);
        g_nonneg_reg = saved;
        return;
    }
    if (n->type == AST_LOGICAL && n->op && !strcmp(n->op, "||")) {
        int ok = new_label(cg);
        emit_cond_succeed(cg, n->children[0], ok);
        emit_cond_fail(cg, n->children[1], fail_lab);
        emit(cg, ".L%d:\n", ok);
        return;
    }
    emit_branch_cmp(cg, n, fail_lab, 0);
}

static void emit_cond_succeed(Codegen* cg, AstNode* n, int ok_lab) {
    if (n->type == AST_LOGICAL && n->op && !strcmp(n->op, "||")) {
        emit_cond_succeed(cg, n->children[0], ok_lab);
        emit_cond_succeed(cg, n->children[1], ok_lab);
        return;
    }
    if (n->type == AST_LOGICAL && n->op && !strcmp(n->op, "&&")) {
        int fail = new_label(cg);
        emit_cond_fail(cg, n->children[0], fail);
        emit_cond_succeed(cg, n->children[1], ok_lab);
        emit(cg, ".L%d:\n", fail);
        return;
    }
    emit_branch_cmp(cg, n, ok_lab, 1);
}

static int num_eq(AstNode* n, long long want) {
    long long v;
    return n && n->type == AST_NUMBER && n->value && parse_i64(n->value, &v) && v == want;
}

static int is_plus_one(AstNode* n, const char* name) {
    if (!n || n->type != AST_ASSIGN || !n->name || !name || strcmp(n->name, name)) return 0;
    if (n->child_count != 1 || !n->children[0]) return 0;
    AstNode* e = n->children[0];
    if (e->type != AST_BINOP || !e->op || e->op[0] != '+' || e->op[1] || e->child_count != 2)
        return 0;
    AstNode* a = e->children[0];
    AstNode* b = e->children[1];
    int aid = a && a->type == AST_IDENT && a->name && !strcmp(a->name, name);
    int bid = b && b->type == AST_IDENT && b->name && !strcmp(b->name, name);
    return (aid && num_eq(b, 1)) || (bid && num_eq(a, 1));
}

static void count_writes(AstNode* n, const char* name, int* nw) {
    if (!n || !name) return;
    if ((n->type == AST_LET || n->type == AST_ASSIGN) && n->name && !strcmp(n->name, name))
        (*nw)++;
    for (int i = 0; i < n->child_count; i++) count_writes(n->children[i], name, nw);
}

static void tally_defs(AstNode* n, const char* name, long long* val, int* nconst, int* nother) {
    if (!n || !name) return;
    if ((n->type == AST_LET || n->type == AST_ASSIGN) && n->name && !strcmp(n->name, name)) {
        AstNode* e = n->child_count == 1 ? n->children[0] : NULL;
        long long v = 0;
        if (e && e->type == AST_NUMBER && parse_i64(e->value, &v)) {
            *val = v;
            (*nconst)++;
        } else (*nother)++;
    }
    for (int i = 0; i < n->child_count; i++)
        tally_defs(n->children[i], name, val, nconst, nother);
}

static int indexes_are(AstNode* n, const char* name) {
    if (!n) return 1;
    if (n->type == AST_CALL || n->type == AST_ADDR_OF) return 0;
    if (n->type == AST_INDEX || n->type == AST_ARRAY_ASSIGN) {
        AstNode* ix = n->child_count > 1 ? n->children[1] : NULL;
        if (!ix || ix->type != AST_IDENT || !ix->name || strcmp(ix->name, name)) return 0;
    }
    for (int i = 0; i < n->child_count; i++)
        if (!indexes_are(n->children[i], name)) return 0;
    return 1;
}

static int arrays_fit(Codegen* cg, AstNode* n, long long lim) {
    if (!n) return 1;
    if ((n->type == AST_INDEX || n->type == AST_ARRAY_ASSIGN) && n->children &&
        n->children[0] && n->children[0]->type == AST_IDENT && n->children[0]->name) {
        Symbol* s = symtab_lookup_symbol(cg->symtab, n->children[0]->name);
        if (s && s->is_array && !s->is_pointer && (long long)s->size < lim) return 0;
    }
    for (int i = 0; i < n->child_count; i++)
        if (!arrays_fit(cg, n->children[i], lim)) return 0;
    return 1;
}

static int nonneg_init(AstNode* st, const char* name) {
    if (!st || !st->name || !name || strcmp(st->name, name)) return 0;
    if (st->type != AST_ASSIGN && st->type != AST_LET) return 0;
    if (st->child_count != 1 || !st->children[0] || st->children[0]->type != AST_NUMBER) return 0;
    long long v;
    return parse_i64(st->children[0]->value, &v) && v >= 0;
}

// `i = C` (C >= 0) immediately before `while (i < lim)` whose body only
// increments i by 1 and indexes with i, and every local array is at least lim.
static int match_proven(Codegen* cg, AstNode* prev, AstNode* wh, const char** idx, long long* lim) {
    if (!wh || wh->type != AST_WHILE || wh->child_count != 2) return 0;
    AstNode* cond = wh->children[0];
    AstNode* body = wh->children[1];
    if (!cond || cond->type != AST_COMPARE || !cond->op || strcmp(cond->op, "<") || cond->child_count != 2)
        return 0;
    if (!cond->children[0] || cond->children[0]->type != AST_IDENT || !cond->children[0]->name)
        return 0;
    const char* name = cond->children[0]->name;
    AstNode* rhs = cond->children[1];
    long long bound = -1;
    if (rhs && rhs->type == AST_NUMBER) {
        if (!parse_i64(rhs->value, &bound) || bound < 0) return 0;
    } else if (rhs && rhs->type == AST_IDENT && rhs->name) {
        long long v = 0;
        int nc = 0, no = 0;
        tally_defs(g_cur_fn, rhs->name, &v, &nc, &no);
        if (nc != 1 || no != 0 || v < 0) return 0;
        bound = v;
    } else return 0;
    if (!nonneg_init(prev, name)) return 0;
    if (!body || body->type != AST_BLOCK || body->child_count < 1) return 0;
    if (!is_plus_one(body->children[body->child_count - 1], name)) return 0;
    for (int i = 0; i < body->child_count - 1; i++) {
        int w = 0;
        count_writes(body->children[i], name, &w);
        if (w) return 0;
    }
    if (!indexes_are(body, name) || !arrays_fit(cg, body, bound)) return 0;
    *idx = name;
    *lim = bound;
    return 1;
}

// a[i*n+k] or a[k*n+j]: (+ (* ident ident) ident), array is an ident.
typedef struct {
    int ok;
    char arr[96];
    char addend[96];
    char f0[96];
    char f1[96];
} MadIx;

static int mad_copy(char* dst, int cap, const char* s) {
    if (!s || !s[0] || (int)strlen(s) >= cap) return 0;
    snprintf(dst, cap, "%s", s);
    return 1;
}

static int parse_mad(AstNode* ix, MadIx* m) {
    memset(m, 0, sizeof *m);
    if (!ix || ix->type != AST_INDEX || ix->child_count < 2) return 0;
    AstNode* arr = ix->children[0];
    AstNode* e = ix->children[1];
    if (!arr || arr->type != AST_IDENT || !e || e->type != AST_BINOP || !e->op ||
        e->op[0] != '+' || e->op[1] || e->child_count != 2)
        return 0;
    AstNode* mul = NULL;
    AstNode* add = NULL;
    if (e->children[0] && e->children[0]->type == AST_BINOP && e->children[0]->op &&
        e->children[0]->op[0] == '*' && !e->children[0]->op[1]) {
        mul = e->children[0];
        add = e->children[1];
    } else if (e->children[1] && e->children[1]->type == AST_BINOP && e->children[1]->op &&
               e->children[1]->op[0] == '*' && !e->children[1]->op[1]) {
        mul = e->children[1];
        add = e->children[0];
    } else return 0;
    if (!mul || mul->child_count != 2 || !add || add->type != AST_IDENT) return 0;
    if (!mul->children[0] || mul->children[0]->type != AST_IDENT) return 0;
    if (!mul->children[1] || mul->children[1]->type != AST_IDENT) return 0;
    if (!mad_copy(m->arr, 96, arr->name) || !mad_copy(m->addend, 96, add->name) ||
        !mad_copy(m->f0, 96, mul->children[0]->name) ||
        !mad_copy(m->f1, 96, mul->children[1]->name))
        return 0;
    m->ok = 1;
    return 1;
}

static int i64_arr(Symbol* s, long long need) {
    if (!s || !s->is_array || s->is_pointer || !s->type_name) return 0;
    if (strcmp(s->type_name, "i64") && strcmp(s->type_name, "u64")) return 0;
    if ((long long)s->size < need) return 0;
    if (s->array_len > 0 && (long long)s->array_len < need) return 0;
    return 1;
}

static const char* distinct_reg(Codegen* cg, const char* name, const char** prev, int nprev) {
    if (!name) return NULL;
    Symbol* s = symtab_lookup_symbol(cg->symtab, (char*)name);
    const char* r = local_reg(s);
    if (!r) return NULL;
    for (int i = 0; i < nprev; i++)
        if (prev[i] && !strcmp(prev[i], r)) return NULL;
    return r;
}

// `k = 0; while (k < n) { s = s + a[i*n+k] * b[k*n+j]; k = k + 1; }`
// with n a sole positive constant and both arrays at least n*n. The fast
// path keeps two running pointers, one step per branch. An even unroll was
// measured and was slower, so it is not emitted. A bottom-tested loop with
// `imul` from memory was also measured and was slower, so the test stays
// at the top. A failed i/j guard runs the original loop.
static int try_strength_dot(Codegen* cg, AstNode* wh) {
    if (g_strength_busy || !o2_hosted() || !wh || wh->type != AST_WHILE || wh->child_count != 2)
        return 0;
    AstNode* prev = g_prev_stmt;
    AstNode* cond = wh->children[0];
    AstNode* body = wh->children[1];
    if (!cond || cond->type != AST_COMPARE || !cond->op || strcmp(cond->op, "<") || cond->child_count != 2)
        return 0;
    if (!cond->children[0] || cond->children[0]->type != AST_IDENT || !cond->children[0]->name)
        return 0;
    if (!cond->children[1] || cond->children[1]->type != AST_IDENT || !cond->children[1]->name)
        return 0;
    const char* kname = cond->children[0]->name;
    const char* nname = cond->children[1]->name;
    if (!nonneg_init(prev, kname) || !num_eq(prev->children[0], 0)) return 0;
    if (!body || body->type != AST_BLOCK || body->child_count != 2) return 0;
    if (!is_plus_one(body->children[1], kname)) return 0;
    AstNode* st = body->children[0];
    if (!st || st->type != AST_ASSIGN || !st->name || st->child_count != 1) return 0;
    AstNode* e = st->children[0];
    if (!e || e->type != AST_BINOP || !e->op || e->op[0] != '+' || e->op[1] || e->child_count != 2)
        return 0;
    AstNode* prod = NULL;
    if (e->children[0] && e->children[0]->type == AST_IDENT && e->children[0]->name &&
        !strcmp(e->children[0]->name, st->name))
        prod = e->children[1];
    else if (e->children[1] && e->children[1]->type == AST_IDENT && e->children[1]->name &&
             !strcmp(e->children[1]->name, st->name))
        prod = e->children[0];
    else return 0;
    if (!prod || prod->type != AST_BINOP || !prod->op || prod->op[0] != '*' || prod->op[1] ||
        prod->child_count != 2)
        return 0;
    MadIx m1, m2;
    if (!parse_mad(prod->children[0], &m1) || !parse_mad(prod->children[1], &m2)) return 0;
    MadIx* ak = NULL;
    MadIx* bk = NULL;
    if (!strcmp(m1.addend, kname) && (strcmp(m2.f0, kname) == 0 || strcmp(m2.f1, kname) == 0)) {
        ak = &m1; bk = &m2;
    } else if (!strcmp(m2.addend, kname) && (strcmp(m1.f0, kname) == 0 || strcmp(m1.f1, kname) == 0)) {
        ak = &m2; bk = &m1;
    } else return 0;
    const char* nb = !strcmp(bk->f0, kname) ? bk->f1 : bk->f0;
    if (!strcmp(bk->f0, kname) == !strcmp(bk->f1, kname)) return 0;
    if (strcmp(nb, nname)) return 0;
    const char* iname = !strcmp(ak->f0, nname) ? ak->f1 : ak->f0;
    if (!strcmp(ak->f0, nname) == !strcmp(ak->f1, nname)) return 0;
    const char* jname = bk->addend;
    const char* sname = st->name;
    if (!strcmp(iname, nname) || !strcmp(iname, jname) || !strcmp(iname, kname) ||
        !strcmp(iname, sname) || !strcmp(nname, jname) || !strcmp(nname, kname) ||
        !strcmp(nname, sname) || !strcmp(jname, kname) || !strcmp(jname, sname) ||
        !strcmp(kname, sname))
        return 0;
    long long N = 0;
    int nc = 0, no = 0;
    tally_defs(g_cur_fn, nname, &N, &nc, &no);
    if (nc != 1 || no != 0 || N <= 0 || N > 1000000) return 0;
    long long need = N * N;
    Symbol* sa = symtab_lookup_symbol(cg->symtab, ak->arr);
    Symbol* sb = symtab_lookup_symbol(cg->symtab, bk->arr);
    if (!i64_arr(sa, need) || !i64_arr(sb, need)) return 0;
    const char* got[5] = {0, 0, 0, 0, 0};
    const char* ri = distinct_reg(cg, iname, got, 0);
    got[0] = ri;
    const char* rj = distinct_reg(cg, jname, got, 1);
    got[1] = rj;
    const char* rk = distinct_reg(cg, kname, got, 2);
    got[2] = rk;
    const char* rn = distinct_reg(cg, nname, got, 3);
    got[3] = rn;
    const char* rs = distinct_reg(cg, sname, got, 4);
    if (!ri || !rj || !rk || !rn || !rs) return 0;
    int top = new_label(cg);
    int slow = new_label(cg);
    int end = new_label(cg);
    emit(cg, "    test %s, %s\n    js .L%d\n", ri, ri, slow);
    emit(cg, "    cmp %s, %s\n    jge .L%d\n", ri, rn, slow);
    emit(cg, "    test %s, %s\n    js .L%d\n", rj, rj, slow);
    emit(cg, "    cmp %s, %s\n    jge .L%d\n", rj, rn, slow);
    emit(cg, "    mov rax, %s\n    imul rax, %s\n", ri, rn);
    emit(cg, "    lea rdi, [rbp%lld+rax*8]\n", (long long)sa->offset);
    emit(cg, "    lea rsi, [rbp%lld+%s*8]\n", (long long)sb->offset, rj);
    emit(cg, "    mov rcx, %s\n    shl rcx, 3\n", rn);
    emit(cg, ".L%d:\n", top);
    emit(cg, "    cmp %s, %s\n    jge .L%d\n", rk, rn, end);
    emit(cg, "    mov rax, [rdi]\n    mov rbx, [rsi]\n    imul rax, rbx\n");
    emit(cg, "    add %s, rax\n", rs);
    emit(cg, "    add rdi, 8\n    add rsi, rcx\n    add %s, 1\n", rk);
    emit(cg, "    jmp .L%d\n", top);
    emit(cg, ".L%d:\n", slow);
    g_strength_busy = 1;
    gen_stmt(cg, wh);
    g_strength_busy = 0;
    emit(cg, ".L%d:\n", end);
    return 1;
}

static int i128_fits_i64(__int128 v) {
    return v >= (__int128)LLONG_MIN && v <= (__int128)LLONG_MAX;
}

static int name_addr_taken(AstNode* n, const char* name) {
    if (!n || !name) return 0;
    if (n->type == AST_ADDR_OF && n->child_count > 0 && n->children[0] &&
        n->children[0]->type == AST_IDENT && n->children[0]->name &&
        !strcmp(n->children[0]->name, name))
        return 1;
    for (int i = 0; i < n->child_count; i++)
        if (name_addr_taken(n->children[i], name)) return 1;
    return 0;
}

static int calls_asm(AstNode* n) {
    if (!n) return 0;
    if (n->type == AST_CALL && n->name && !strcmp(n->name, "asm")) return 1;
    for (int i = 0; i < n->child_count; i++)
        if (calls_asm(n->children[i])) return 1;
    return 0;
}

static void emit_mov_i64(Codegen* cg, const char* reg, long long v) {
    if (v == LLONG_MIN) emit(cg, "    mov %s, 0x8000000000000000\n", reg);
    else emit(cg, "    mov %s, %lld\n", reg, v);
}

static void emit_set_i64(Codegen* cg, Symbol* s, long long imm, const char* reg) {
    if (!s) return;
    const char* rr = local_reg(s);
    if (reg) {
        if (rr) {
            if (strcmp(rr, reg)) emit(cg, "    mov %s, %s\n", rr, reg);
        } else emit(cg, "    mov [rbp%d], %s\n", s->offset, reg);
        return;
    }
    if (rr) emit_mov_i64(cg, rr, imm);
    else if (fits_imm32_val(imm)) emit(cg, "    mov qword [rbp%d], %lld\n", s->offset, imm);
    else {
        emit_mov_i64(cg, "rax", imm);
        emit(cg, "    mov [rbp%d], rax\n", s->offset);
    }
}

static int const_init_val(AstNode* st, const char* name, long long* v) {
    if (!st || !name || !st->name || strcmp(st->name, name)) return 0;
    if (st->type != AST_ASSIGN && st->type != AST_LET) return 0;
    if (st->child_count != 1 || !st->children[0]) return 0;
    return st->children[0]->type == AST_NUMBER && parse_i64(st->children[0]->value, v);
}

static int sole_i64(const char* name, long long* v) {
    long long val = 0;
    int nc = 0, no = 0;
    if (!name || !g_cur_fn) return 0;
    tally_defs(g_cur_fn, name, &val, &nc, &no);
    if (nc != 1 || no != 0) return 0;
    *v = val;
    return 1;
}

static int is_add_op(AstNode* e) {
    return e && e->type == AST_BINOP && e->op && e->op[0] == '+' && !e->op[1] && e->child_count == 2;
}

static int is_mul_op(AstNode* e) {
    return e && e->type == AST_BINOP && e->op && e->op[0] == '*' && !e->op[1] && e->child_count == 2;
}

static int collect_add(AstNode* e, AstNode** t, int* n) {
    if (is_add_op(e)) {
        if (!collect_add(e->children[0], t, n)) return 0;
        if (!collect_add(e->children[1], t, n)) return 0;
        return 1;
    }
    if (*n >= 8) return 0;
    t[(*n)++] = e;
    return 1;
}

// Sum after k trips of (A*i + B), i starting at I0. Exact in __int128, or 0
// if an intermediate product does not fit there.
static int acc_sum(__int128 S0, __int128 A, __int128 I0, __int128 B, __int128 k, __int128* out) {
    __int128 t1, t2, t3, half, sum;
    if (k == 0) { *out = S0; return 1; }
    if (__builtin_mul_overflow(A, I0, &t1) || __builtin_mul_overflow(t1, k, &t1)) return 0;
    if (__builtin_mul_overflow(k, k - 1, &t2)) return 0;
    half = t2 / 2;
    if (__builtin_mul_overflow(A, half, &t2) || __builtin_mul_overflow(B, k, &t3)) return 0;
    if (__builtin_add_overflow(S0, t1, &sum) || __builtin_add_overflow(sum, t2, &sum)) return 0;
    if (__builtin_add_overflow(sum, t3, &sum)) return 0;
    *out = sum;
    return 1;
}

// Closed form of s = s + A*i + B, i = i + 1, while i < N. Only when every
// partial sum and every intermediate A*i fits in i64. A positive term added
// to a non-negative s, or a negative term added to a non-positive s, is
// monotonic, so the endpoints are enough. Anything else keeps the loop:
// wrapping is not the mathematical sum.
static int linear_closed(long long S0, long long A, long long I0, long long B, long long N,
                         long long* s_out, long long* i_out) {
    __int128 plo, phi, tlo, thi, total;
    __int128 i_lo, i_hi, trips;
    int pos, neg;
    if ((__int128)I0 >= (__int128)N) {
        *s_out = S0;
        *i_out = I0;
        return 1;
    }
    trips = (__int128)N - (__int128)I0;
    i_lo = I0;
    i_hi = (__int128)N - 1;
    if (__builtin_mul_overflow((__int128)A, i_lo, &plo) || !i128_fits_i64(plo)) return 0;
    if (__builtin_mul_overflow((__int128)A, i_hi, &phi) || !i128_fits_i64(phi)) return 0;
    if (__builtin_add_overflow(plo, (__int128)B, &tlo) || !i128_fits_i64(tlo)) return 0;
    if (__builtin_add_overflow(phi, (__int128)B, &thi) || !i128_fits_i64(thi)) return 0;
    pos = S0 >= 0 && B >= 0 && plo >= 0 && phi >= 0;
    neg = S0 <= 0 && B <= 0 && plo <= 0 && phi <= 0;
    if (!pos && !neg) return 0;
    if (!acc_sum(S0, A, I0, B, trips, &total) || !i128_fits_i64(total)) return 0;
    *s_out = (long long)total;
    *i_out = N;
    return 1;
}

static void emit_rep_fill(Codegen* cg, int base, const char* count_reg, int have_imm,
                          long long count, long long bytev) {
    emit(cg, "    push rdi\n    pushfq\n    cld\n");
    emit(cg, "    lea rdi, [rbp%d]\n", base);
    if (have_imm) emit(cg, "    mov rcx, %lld\n", count);
    else if (!count_reg || strcmp(count_reg, "rcx")) emit(cg, "    mov rcx, %s\n", count_reg);
    emit(cg, "    mov al, %lld\n", bytev);
    emit(cg, "    rep stosb\n");
    emit(cg, "    popfq\n    pop rdi\n");
}

// `i = 0; while (i < lim) { byte_array[i] = C; i = i + 1; }` with lim in range.
// One `rep stosb` from element 0. A limit that is not a proven in-range
// constant is checked at run time; out of range runs the original loop.
static int try_byte_fill(Codegen* cg, AstNode* wh) {
    AstNode* cond;
    AstNode* body;
    AstNode* st;
    AstNode* arr;
    AstNode* ix;
    AstNode* val;
    AstNode* rhs;
    Symbol* asym;
    Symbol* si;
    Symbol* lsym = NULL;
    const char* iname;
    const char* lreg = NULL;
    char* et;
    long long iz = 0, bytev = 0, enc = 0, alen = 0, lim = 0;
    int known = 0;
    int slow, end;
    if (g_strength_busy || !o2_hosted() || !wh || wh->type != AST_WHILE || wh->child_count != 2)
        return 0;
    if (calls_asm(g_cur_fn)) return 0;
    cond = wh->children[0];
    body = wh->children[1];
    if (!cond || cond->type != AST_COMPARE || !cond->op || strcmp(cond->op, "<") || cond->child_count != 2)
        return 0;
    if (!cond->children[0] || cond->children[0]->type != AST_IDENT || !cond->children[0]->name)
        return 0;
    iname = cond->children[0]->name;
    rhs = cond->children[1];
    if (!const_init_val(g_prev_stmt, iname, &iz) || iz != 0) return 0;
    if (!body || body->type != AST_BLOCK || body->child_count != 2) return 0;
    if (!is_plus_one(body->children[1], iname)) return 0;
    st = body->children[0];
    if (!st || st->type != AST_ARRAY_ASSIGN || st->child_count < 3) return 0;
    arr = st->children[0];
    ix = st->children[1];
    val = st->children[2];
    if (!arr || arr->type != AST_IDENT || !arr->name) return 0;
    if (!ix || ix->type != AST_IDENT || !ix->name || strcmp(ix->name, iname)) return 0;
    if (!val || val->type != AST_NUMBER || !parse_i64(val->value, &bytev) || !imm_store_enc(1, bytev, &enc))
        return 0;
    asym = symtab_lookup_symbol(cg->symtab, arr->name);
    if (!asym || !asym->is_array || asym->is_pointer || !asym->type_name) return 0;
    if (strcmp(asym->type_name, "u8") && strcmp(asym->type_name, "i8")) return 0;
    if (name_addr_taken(g_cur_fn, arr->name) || name_addr_taken(g_cur_fn, iname)) return 0;
    si = symtab_lookup_symbol(cg->symtab, (char*)iname);
    if (!si || si->is_array || si->is_pointer || sym_narrow_int(si)) return 0;
    alen = asym->array_len > 0 ? asym->array_len : asym->size;
    if (alen < 0 || !fits_imm32_val(alen)) return 0;
    if (rhs && rhs->type == AST_NUMBER) {
        if (!parse_i64(rhs->value, &lim) || lim < 0 || lim > alen) return 0;
        known = 1;
    } else if (rhs && rhs->type == AST_IDENT && rhs->name && strcmp(rhs->name, iname)) {
        if (sole_i64(rhs->name, &lim) && !name_addr_taken(g_cur_fn, rhs->name)) {
            if (lim < 0 || lim > alen) return 0;
            known = 1;
        } else {
            lsym = symtab_lookup_symbol(cg->symtab, rhs->name);
            if (!lsym || lsym->is_array || lsym->is_pointer || sym_narrow_int(lsym)) return 0;
            if (name_addr_taken(g_cur_fn, rhs->name)) return 0;
            lreg = local_reg(lsym);
        }
    } else return 0;
    et = array_elem_type_name(cg, arr);
    if (et) check_types(val, "", ty_from_spelled(cg, et), expr_ty(cg, val));
    if (!known && !lsym) return 0;
    if (known) {
        emit_rep_fill(cg, asym->offset, NULL, 1, lim, enc);
        emit_set_i64(cg, si, lim, NULL);
        return 1;
    }
    slow = new_label(cg);
    end = new_label(cg);
    if (lreg) emit(cg, "    mov rbx, %s\n", lreg);
    else emit(cg, "    mov rbx, [rbp%d]\n", lsym->offset);
    emit(cg, "    test rbx, rbx\n    js .L%d\n", slow);
    emit(cg, "    cmp rbx, %lld\n    jg .L%d\n", alen, slow);
    emit_rep_fill(cg, asym->offset, "rbx", 0, 0, enc);
    emit_set_i64(cg, si, 0, "rbx");
    emit(cg, "    jmp .L%d\n", end);
    emit(cg, ".L%d:\n", slow);
    g_strength_busy = 1;
    gen_stmt(cg, wh);
    g_strength_busy = 0;
    emit(cg, ".L%d:\n", end);
    return 1;
}

// `s = S; i = I; while (i < N) { s = s + A*i + B; i = i + 1; }` when the sum
// is a proved i64. N, A, B, S and I are constants. A loop that can wrap stays.
static int try_linear_sum(Codegen* cg, AstNode* wh) {
    AstNode* cond;
    AstNode* body;
    AstNode* st;
    AstNode* rhs;
    AstNode* terms[8];
    Symbol* si;
    Symbol* ss;
    const char* iname;
    long long I0 = 0, S0 = 0, N = 0, sfin = 0, ifin = 0;
    int nt = 0, ns = 0, ni = 0;
    __int128 A = 0, B = 0;
    if (g_strength_busy || !o2_hosted() || !wh || wh->type != AST_WHILE || wh->child_count != 2)
        return 0;
    if (calls_asm(g_cur_fn)) return 0;
    cond = wh->children[0];
    body = wh->children[1];
    if (!cond || cond->type != AST_COMPARE || !cond->op || strcmp(cond->op, "<") || cond->child_count != 2)
        return 0;
    if (!cond->children[0] || cond->children[0]->type != AST_IDENT || !cond->children[0]->name)
        return 0;
    iname = cond->children[0]->name;
    rhs = cond->children[1];
    if (rhs && rhs->type == AST_NUMBER) {
        if (!parse_i64(rhs->value, &N)) return 0;
    } else if (rhs && rhs->type == AST_IDENT && rhs->name) {
        if (name_addr_taken(g_cur_fn, rhs->name) || !sole_i64(rhs->name, &N)) return 0;
    } else return 0;
    if (!body || body->type != AST_BLOCK || body->child_count != 2) return 0;
    if (!is_plus_one(body->children[1], iname)) return 0;
    st = body->children[0];
    if (!st || st->type != AST_ASSIGN || !st->name || st->child_count != 1 || !st->children[0]) return 0;
    if (!strcmp(st->name, iname)) return 0;
    if (!const_init_val(g_prev_stmt, iname, &I0)) return 0;
    if (!const_init_val(g_prev2_stmt, st->name, &S0)) return 0;
    if (name_addr_taken(g_cur_fn, iname) || name_addr_taken(g_cur_fn, st->name)) return 0;
    si = symtab_lookup_symbol(cg->symtab, (char*)iname);
    ss = symtab_lookup_symbol(cg->symtab, st->name);
    if (!si || !ss || si->is_array || ss->is_array || si->is_pointer || ss->is_pointer) return 0;
    if (sym_narrow_int(si) || sym_narrow_int(ss)) return 0;
    if (!collect_add(st->children[0], terms, &nt)) return 0;
    for (int i = 0; i < nt; i++) {
        AstNode* e = terms[i];
        long long c = 0;
        if (e && e->type == AST_IDENT && e->name && !strcmp(e->name, st->name)) { ns++; continue; }
        if (e && e->type == AST_NUMBER && parse_i64(e->value, &c)) { B += c; continue; }
        if (e && e->type == AST_IDENT && e->name && !strcmp(e->name, iname)) { A += 1; ni++; continue; }
        if (is_mul_op(e)) {
            AstNode* a = e->children[0];
            AstNode* b = e->children[1];
            long long m = 0;
            if (a && a->type == AST_IDENT && a->name && !strcmp(a->name, iname) &&
                b && b->type == AST_NUMBER && parse_i64(b->value, &m)) {
                A += m; ni++; continue;
            }
            if (b && b->type == AST_IDENT && b->name && !strcmp(b->name, iname) &&
                a && a->type == AST_NUMBER && parse_i64(a->value, &m)) {
                A += m; ni++; continue;
            }
        }
        return 0;
    }
    if (ns != 1 || ni > 1 || !i128_fits_i64(A) || !i128_fits_i64(B)) return 0;
    if (!linear_closed(S0, (long long)A, I0, (long long)B, N, &sfin, &ifin)) return 0;
    check_assign_types(cg, st);
    check_assign_types(cg, body->children[1]);
    emit_set_i64(cg, ss, sfin, NULL);
    emit_set_i64(cg, si, ifin, NULL);
    return 1;
}

// `x = (x + y) & imm`, both names in registers, imm in 0..2^31-1.
// y may be x. The 64-bit `and` sign-extends an imm32, which matches this range.
static int match_mask_add(Codegen* cg, AstNode* st, const char** rd, const char** rs,
                          long long* imm) {
    AstNode* e;
    AstNode* add;
    AstNode* maskn;
    AstNode* L;
    AstNode* R;
    const char* other = NULL;
    Symbol* sd;
    Symbol* ss;
    long long m = 0;
    if (!o2_hosted() || !st || st->type != AST_ASSIGN || !st->name || st->child_count != 1)
        return 0;
    e = st->children[0];
    if (!e || e->type != AST_BINOP || !e->op || e->op[0] != '&' || e->op[1] || e->child_count != 2)
        return 0;
    add = e->children[0];
    maskn = e->children[1];
    if (!is_add_op(add) || !maskn || maskn->type != AST_NUMBER) return 0;
    if (!parse_i64(maskn->value, &m) || m < 0 || m > 2147483647LL) return 0;
    L = add->children[0];
    R = add->children[1];
    if (L && L->type == AST_IDENT && L->name && !strcmp(L->name, st->name) &&
        R && R->type == AST_IDENT && R->name)
        other = R->name;
    else if (R && R->type == AST_IDENT && R->name && !strcmp(R->name, st->name) &&
             L && L->type == AST_IDENT && L->name)
        other = L->name;
    else return 0;
    sd = symtab_lookup_symbol(cg->symtab, st->name);
    ss = symtab_lookup_symbol(cg->symtab, (char*)other);
    if (!sd || !ss || sd->is_array || ss->is_array || sd->is_pointer || ss->is_pointer)
        return 0;
    if (sym_narrow_int(sd) || sym_narrow_int(ss)) return 0;
    if (name_addr_taken(g_cur_fn, st->name) || name_addr_taken(g_cur_fn, other)) return 0;
    *rd = local_reg(sd);
    *rs = local_reg(ss);
    if (!*rd || !*rs) return 0;
    *imm = m;
    return 1;
}

// Straight-line `x = (x + y) & imm`. Emit every add, then every mask, when
// neither x nor y was a destination earlier in the run. One statement stays
// on the normal path. Returns how many statements were consumed.
static int try_sink_masks(Codegen* cg, AstNode* block, int start) {
    const char* rd[8];
    const char* rs[8];
    long long imm[8];
    int n = 0;
    int i, j;
    if (!o2_hosted() || !block || block->type != AST_BLOCK) return 0;
    for (i = start; i < block->child_count && n < 8; i++) {
        const char* d = NULL;
        const char* s = NULL;
        long long m = 0;
        int clash = 0;
        if (!match_mask_add(cg, block->children[i], &d, &s, &m)) break;
        for (j = 0; j < n; j++) {
            if (!strcmp(s, rd[j]) || !strcmp(d, rd[j])) clash = 1;
        }
        if (clash) break;
        rd[n] = d;
        rs[n] = s;
        imm[n] = m;
        n++;
    }
    if (n < 2) return 0;
    for (i = 0; i < n; i++) {
        check_assign_types(cg, block->children[start + i]);
        emit(cg, "    add %s, %s\n", rd[i], rs[i]);
    }
    for (i = 0; i < n; i++)
        emit(cg, "    and %s, %lld\n", rd[i], imm[i]);
    return n;
}

static int is_minus_one(AstNode* n, const char* name) {
    if (!n || n->type != AST_ASSIGN || !n->name || !name || strcmp(n->name, name)) return 0;
    if (n->child_count != 1 || !n->children[0]) return 0;
    AstNode* e = n->children[0];
    if (e->type != AST_BINOP || !e->op || e->op[0] != '-' || e->op[1] || e->child_count != 2)
        return 0;
    AstNode* a = e->children[0];
    AstNode* b = e->children[1];
    return a && a->type == AST_IDENT && a->name && !strcmp(a->name, name) && num_eq(b, 1);
}

// `j = i - 1` or `let j = i - 1`.
static int assign_sub_one(AstNode* n, const char** dest, const char** src) {
    if (!n || !n->name || (n->type != AST_ASSIGN && n->type != AST_LET)) return 0;
    if (n->child_count != 1 || !n->children[0]) return 0;
    AstNode* e = n->children[0];
    if (e->type != AST_BINOP || !e->op || e->op[0] != '-' || e->op[1] || e->child_count != 2)
        return 0;
    AstNode* a = e->children[0];
    AstNode* b = e->children[1];
    if (!a || a->type != AST_IDENT || !a->name || !num_eq(b, 1)) return 0;
    *dest = n->name;
    *src = a->name;
    return 1;
}

static int plus_one_name(AstNode* ix, const char** name) {
    if (!ix || ix->type != AST_BINOP || !ix->op || ix->op[0] != '+' || ix->op[1] || ix->child_count != 2)
        return 0;
    AstNode* a = ix->children[0];
    AstNode* b = ix->children[1];
    AstNode* id = NULL;
    if (a && a->type == AST_IDENT && num_eq(b, 1)) id = a;
    else if (b && b->type == AST_IDENT && num_eq(a, 1)) id = b;
    else return 0;
    if (!id->name) return 0;
    *name = id->name;
    return 1;
}

static int index_named(AstNode* n, const char* arr, const char* idx) {
    if (!n || n->type != AST_INDEX || n->child_count < 2 || !arr || !idx) return 0;
    AstNode* a = n->children[0];
    AstNode* i = n->children[1];
    return a && a->type == AST_IDENT && a->name && !strcmp(a->name, arr) &&
           i && i->type == AST_IDENT && i->name && !strcmp(i->name, idx);
}

// `a[j] > key` or `key < a[j]`.
static int elem_gt_key(AstNode* c, const char* arr, const char* idx, const char* key) {
    if (!c || c->type != AST_COMPARE || !c->op || c->child_count != 2 || !key) return 0;
    if (!strcmp(c->op, ">") && index_named(c->children[0], arr, idx) &&
        c->children[1] && c->children[1]->type == AST_IDENT && c->children[1]->name &&
        !strcmp(c->children[1]->name, key))
        return 1;
    if (!strcmp(c->op, "<") && index_named(c->children[1], arr, idx) &&
        c->children[0] && c->children[0]->type == AST_IDENT && c->children[0]->name &&
        !strcmp(c->children[0]->name, key))
        return 1;
    return 0;
}

static void check_elem_store(Codegen* cg, AstNode* st) {
    if (!st || st->type != AST_ARRAY_ASSIGN || st->child_count < 3) return;
    check_indexable(cg, st->children[0]);
    char* et = array_elem_type_name(cg, st->children[0]);
    if (et)
        check_types(st->children[2], "", ty_from_spelled(cg, et), expr_ty(cg, st->children[2]));
}

// Outer `i = C` (C >= 0) then `while (i < N)`, N a sole positive constant,
// body ends with `i = i + 1`, and i is not written earlier in that body.
// The array is a local i64 of at least N elements, so an index in
// [-1, N-2] used as j+1 stays inside it.
static int shift_outer_ok(Codegen* cg, const char* iname, Symbol* arr, long long* bound_out) {
    AstNode* wh = g_enclosing_while;
    AstNode* prev = g_enclosing_prev;
    AstNode* cond;
    AstNode* body;
    AstNode* rhs;
    long long bound = 0;
    int i;
    if (!iname || !arr || !wh || wh->type != AST_WHILE || wh->child_count != 2) return 0;
    cond = wh->children[0];
    body = wh->children[1];
    if (!cond || cond->type != AST_COMPARE || !cond->op || strcmp(cond->op, "<") ||
        cond->child_count != 2)
        return 0;
    if (!cond->children[0] || cond->children[0]->type != AST_IDENT || !cond->children[0]->name ||
        strcmp(cond->children[0]->name, iname))
        return 0;
    rhs = cond->children[1];
    if (rhs && rhs->type == AST_NUMBER) {
        if (!parse_i64(rhs->value, &bound)) return 0;
    } else if (rhs && rhs->type == AST_IDENT && rhs->name) {
        if (!sole_i64(rhs->name, &bound)) return 0;
    } else return 0;
    if (bound <= 0) return 0;
    if (!nonneg_init(prev, iname)) return 0;
    if (!body || body->type != AST_BLOCK || body->child_count < 1) return 0;
    if (!is_plus_one(body->children[body->child_count - 1], iname)) return 0;
    for (i = 0; i < body->child_count - 1; i++) {
        int w = 0;
        count_writes(body->children[i], iname, &w);
        if (w) return 0;
    }
    if (arr->is_volatile || !i64_arr(arr, bound)) return 0;
    if (strcmp(arr->type_name, "i64")) return 0;
    *bound_out = bound;
    return 1;
}

// Insertion shift inside a proved `while (i < N)`:
//   j = i - 1
//   while (j >= 0 && a[j] > key) { a[j + 1] = a[j]; j = j - 1; }
//   a[j + 1] = key
// j starts at i-1, and i is in [C, N-1] when the body runs, so j <= N-2.
// j only steps down by 1, and a[j] is read only after j >= 0, so j never
// goes below -1. a[j+1] is then in range, including the store at j == -1.
// The taken path is six instructions and has no upper-bound check.
// `a[j] > key && j >= 0` is not matched: that reads a[-1]. Returns 2 when
// the while and the following store are consumed.
static int try_insert_shift(Codegen* cg, AstNode* block, int start) {
    AstNode* wh;
    AstNode* post;
    AstNode* cond;
    AstNode* body;
    AstNode* st0;
    AstNode* st1;
    AstNode* base;
    AstNode* kv;
    AstNode* cleft;
    const char* arr;
    const char* jname;
    const char* jpost;
    const char* key;
    const char* jdest;
    const char* iname;
    const char* rj;
    const char* rkey;
    const char* ri;
    Symbol* sj;
    Symbol* sk;
    Symbol* si;
    Symbol* sa;
    long long bound = 0;
    long long off0, off1;
    int store, shift;
    char mem0[96], mem1[96];
    if (!o2_hosted() || g_strength_busy || !block || block->type != AST_BLOCK) return 0;
    if (start < 0 || start + 1 >= block->child_count) return 0;
    wh = block->children[start];
    post = block->children[start + 1];
    if (!wh || wh->type != AST_WHILE || wh->child_count != 2) return 0;
    if (node_has_call(wh) || node_has_call(post)) return 0;
    cond = wh->children[0];
    body = wh->children[1];
    if (!body || body->type != AST_BLOCK || body->child_count != 2) return 0;
    st0 = body->children[0];
    st1 = body->children[1];
    if (!st0 || st0->type != AST_ARRAY_ASSIGN || st0->child_count < 3) return 0;
    base = st0->children[0];
    if (!base || base->type != AST_IDENT || !base->name) return 0;
    arr = base->name;
    jname = NULL;
    if (!plus_one_name(st0->children[1], &jname)) return 0;
    if (!index_named(st0->children[2], arr, jname)) return 0;
    if (!is_minus_one(st1, jname)) return 0;
    if (!cond || cond->type != AST_LOGICAL || !cond->op || strcmp(cond->op, "&&") ||
        cond->child_count != 2)
        return 0;
    cleft = cond->children[0];
    if (!cleft || cleft->type != AST_COMPARE || !cleft->op || strcmp(cleft->op, ">=") ||
        cleft->child_count != 2)
        return 0;
    if (!cleft->children[0] || cleft->children[0]->type != AST_IDENT ||
        !cleft->children[0]->name || strcmp(cleft->children[0]->name, jname))
        return 0;
    if (!num_eq(cleft->children[1], 0)) return 0;
    if (!post || post->type != AST_ARRAY_ASSIGN || post->child_count < 3) return 0;
    if (!post->children[0] || post->children[0]->type != AST_IDENT || !post->children[0]->name ||
        strcmp(post->children[0]->name, arr))
        return 0;
    jpost = NULL;
    if (!plus_one_name(post->children[1], &jpost) || strcmp(jpost, jname)) return 0;
    kv = post->children[2];
    if (!kv || kv->type != AST_IDENT || !kv->name) return 0;
    key = kv->name;
    if (!strcmp(key, jname) || !elem_gt_key(cond->children[1], arr, jname, key)) return 0;
    jdest = NULL;
    iname = NULL;
    if (!assign_sub_one(g_prev_stmt, &jdest, &iname) || strcmp(jdest, jname)) return 0;
    if (!strcmp(iname, jname) || !strcmp(iname, key) || !strcmp(iname, arr)) return 0;
    sa = symtab_lookup_symbol(cg->symtab, (char*)arr);
    if (!shift_outer_ok(cg, iname, sa, &bound)) return 0;
    sj = symtab_lookup_symbol(cg->symtab, (char*)jname);
    sk = symtab_lookup_symbol(cg->symtab, (char*)key);
    si = symtab_lookup_symbol(cg->symtab, (char*)iname);
    if (sym_narrow_int(sj) || sym_narrow_int(sk) || sym_narrow_int(si)) return 0;
    rj = local_reg(sj);
    rkey = local_reg(sk);
    ri = local_reg(si);
    if (!rj || !rkey || !ri) return 0;
    if (!strcmp(rj, rkey) || !strcmp(rj, ri) || !strcmp(rkey, ri)) return 0;
    off0 = sa->offset;
    off1 = off0 + 8;
    if (!fits_imm32_val(off0) || !fits_imm32_val(off1)) return 0;
    check_elem_store(cg, st0);
    check_elem_store(cg, post);
    check_assign_types(cg, st1);
    (void)bound;
    store = new_label(cg);
    shift = new_label(cg);
    snprintf(mem0, sizeof mem0, "rbp%lld+%s*8", off0, rj);
    snprintf(mem1, sizeof mem1, "rbp%lld+%s*8", off1, rj);
    emit(cg, "    cmp %s, 0\n    jl .L%d\n", rj, store);
    emit(cg, "    mov rax, [%s]\n", mem0);
    emit(cg, "    cmp rax, %s\n    jle .L%d\n", rkey, store);
    emit(cg, ".L%d:\n", shift);
    emit_scaled_store(cg, mem1, 8, "rax");
    emit(cg, "    sub %s, 1\n    js .L%d\n", rj, store);
    emit(cg, "    mov rax, [%s]\n", mem0);
    emit(cg, "    cmp rax, %s\n    jg .L%d\n", rkey, shift);
    emit(cg, ".L%d:\n", store);
    emit_scaled_store(cg, mem1, 8, rkey);
    return 2;
}

static int byte_elem_name(const char* t) {
    return t && (!strcmp(t, "u8") || !strcmp(t, "i8"));
}

// Local or global [u8; N] / [i8; N]. *len is the element count. A local u8[N]
// occupies the first N bytes of its frame slot.
static int lookup_byte_arr(Codegen* cg, const char* name, int* is_global, const char** gname,
                           int* off, long long* len) {
    Symbol* s;
    GlobalVar* g;
    if (!name) return 0;
    s = symtab_lookup_symbol(cg->symtab, (char*)name);
    if (s) {
        if (!s->is_array || s->is_pointer || s->is_volatile || !byte_elem_name(s->type_name)) return 0;
        if (s->array_len <= 0) return 0;
        *is_global = 0;
        *gname = NULL;
        *off = s->offset;
        *len = s->array_len;
        return 1;
    }
    g = global_symtab_lookup(cg->global_symtab, (char*)name);
    if (!g || !g->is_array || g->is_pointer || g->is_volatile || !byte_elem_name(g->elem_type)) return 0;
    if (g->array_count <= 0) return 0;
    *is_global = 1;
    *gname = g->name;
    *off = 0;
    *len = g->array_count;
    return 1;
}

static void emit_arr_lea(Codegen* cg, const char* reg, int is_global, const char* gname, int off) {
    if (is_global) emit(cg, "    lea %s, [%s]\n", reg, gname);
    else emit(cg, "    lea %s, [rbp%d]\n", reg, off);
}

// Limit is a literal or the one constant definition of an ident. A parameter
// stays on the scalar loop: its value is not known here.
static int known_loop_lim(AstNode* rhs, const char* iname, long long* lim) {
    if (rhs && rhs->type == AST_NUMBER) return parse_i64(rhs->value, lim);
    if (rhs && rhs->type == AST_IDENT && rhs->name && iname && strcmp(rhs->name, iname)) {
        if (name_addr_taken(g_cur_fn, rhs->name)) return 0;
        return sole_i64(rhs->name, lim);
    }
    return 0;
}

// `i = 0; while (i < lim) { dst[i] = src[i]; i = i + 1; }`. lim is a proved
// in-range constant, so rep movsb copies the same bytes the scalar loop would.
// The same array on both sides is left alone (overlap). rsi/rdi are not locals.
static void emit_rep_copy(Codegen* cg, int dst_g, const char* dst_n, int dst_off,
                          int src_g, const char* src_n, int src_off, long long lim) {
    emit(cg, "    push rsi\n    push rdi\n    pushfq\n    cld\n");
    emit_arr_lea(cg, "rdi", dst_g, dst_n, dst_off);
    emit_arr_lea(cg, "rsi", src_g, src_n, src_off);
    emit(cg, "    mov rcx, %lld\n", lim);
    emit(cg, "    rep movsb\n");
    emit(cg, "    popfq\n    pop rdi\n    pop rsi\n");
}

static int try_byte_copy(Codegen* cg, AstNode* wh) {
    AstNode* cond;
    AstNode* body;
    AstNode* st;
    AstNode* dst;
    AstNode* src;
    AstNode* ix;
    AstNode* val;
    Symbol* si;
    const char* iname;
    long long iz = 0, lim = 0, dlen = 0, slen = 0;
    int dst_g = 0, src_g = 0, dst_off = 0, src_off = 0;
    const char* dst_n = NULL;
    const char* src_n = NULL;
    if (g_strength_busy || !o2_hosted() || !wh || wh->type != AST_WHILE || wh->child_count != 2)
        return 0;
    if (calls_asm(g_cur_fn)) return 0;
    cond = wh->children[0];
    body = wh->children[1];
    if (!cond || cond->type != AST_COMPARE || !cond->op || strcmp(cond->op, "<") || cond->child_count != 2)
        return 0;
    if (!cond->children[0] || cond->children[0]->type != AST_IDENT || !cond->children[0]->name)
        return 0;
    iname = cond->children[0]->name;
    if (!const_init_val(g_prev_stmt, iname, &iz) || iz != 0) return 0;
    if (!known_loop_lim(cond->children[1], iname, &lim) || lim < 0) return 0;
    if (!body || body->type != AST_BLOCK || body->child_count != 2) return 0;
    if (!is_plus_one(body->children[1], iname)) return 0;
    st = body->children[0];
    if (!st || st->type != AST_ARRAY_ASSIGN || st->child_count < 3) return 0;
    dst = st->children[0];
    ix = st->children[1];
    val = st->children[2];
    if (!dst || dst->type != AST_IDENT || !dst->name) return 0;
    if (!ix || ix->type != AST_IDENT || !ix->name || strcmp(ix->name, iname)) return 0;
    if (!val || val->type != AST_INDEX || val->child_count < 2) return 0;
    src = val->children[0];
    if (!src || src->type != AST_IDENT || !src->name || !strcmp(src->name, dst->name)) return 0;
    if (!val->children[1] || val->children[1]->type != AST_IDENT || !val->children[1]->name ||
        strcmp(val->children[1]->name, iname))
        return 0;
    if (!lookup_byte_arr(cg, dst->name, &dst_g, &dst_n, &dst_off, &dlen)) return 0;
    if (!lookup_byte_arr(cg, src->name, &src_g, &src_n, &src_off, &slen)) return 0;
    if (lim > dlen || lim > slen) return 0;
    if (name_addr_taken(g_cur_fn, dst->name) || name_addr_taken(g_cur_fn, src->name) ||
        name_addr_taken(g_cur_fn, iname))
        return 0;
    si = symtab_lookup_symbol(cg->symtab, (char*)iname);
    if (!si || si->is_array || si->is_pointer || si->is_volatile || sym_narrow_int(si)) return 0;
    check_elem_store(cg, st);
    emit_rep_copy(cg, dst_g, dst_n, dst_off, src_g, src_n, src_off, lim);
    emit_set_i64(cg, si, lim, NULL);
    return 1;
}

// Low byte of v, repeated across xmm. SSE2 only: movd / punpck / pshufd.
static void emit_broadcast_byte(Codegen* cg, const char* xmm, int from_reg, const char* reg,
                                int off, long long imm, int is_imm) {
    if (is_imm) emit(cg, "    mov eax, %lld\n", imm & 255);
    else if (from_reg) emit(cg, "    movzx eax, %s\n", reg_low(reg));
    else emit(cg, "    movzx eax, byte [rbp%d]\n", off);
    emit(cg, "    movd %s, eax\n", xmm);
    emit(cg, "    punpcklbw %s, %s\n", xmm, xmm);
    emit(cg, "    punpcklwd %s, %s\n", xmm, xmm);
    emit(cg, "    pshufd %s, %s, 0\n", xmm, xmm);
}

// `i = 0; while (i < lim) { buf[i] = (buf[i] + i + r) & 255; i = i + 1; }`
// lim is a positive multiple of 16 and in range. The low 8 bits of that sum
// are paddb of the byte, of (i mod 256), and of r's low byte. The index
// vector starts at 0..15 and adds 16 each step, which wraps the same way.
static int try_byte_bump(Codegen* cg, AstNode* wh) {
    AstNode* cond;
    AstNode* body;
    AstNode* st;
    AstNode* arr;
    AstNode* ix;
    AstNode* val;
    AstNode* mask;
    AstNode* add;
    AstNode* terms[8];
    AstNode* extra = NULL;
    Symbol* si;
    Symbol* sr = NULL;
    const char* iname;
    const char* gname = NULL;
    long long iz = 0, lim = 0, alen = 0, imm = 0;
    int nt = 0, i, nbuf = 0, ni = 0, is_g = 0, off = 0, is_imm = 0;
    int lab_idx, lab_step, lab_code, lab_loop;
    if (g_strength_busy || !o2_hosted() || !wh || wh->type != AST_WHILE || wh->child_count != 2)
        return 0;
    if (calls_asm(g_cur_fn)) return 0;
    cond = wh->children[0];
    body = wh->children[1];
    if (!cond || cond->type != AST_COMPARE || !cond->op || strcmp(cond->op, "<") || cond->child_count != 2)
        return 0;
    if (!cond->children[0] || cond->children[0]->type != AST_IDENT || !cond->children[0]->name)
        return 0;
    iname = cond->children[0]->name;
    if (!const_init_val(g_prev_stmt, iname, &iz) || iz != 0) return 0;
    if (!known_loop_lim(cond->children[1], iname, &lim) || lim < 0 || (lim % 16) != 0) return 0;
    if (!body || body->type != AST_BLOCK || body->child_count != 2) return 0;
    if (!is_plus_one(body->children[1], iname)) return 0;
    st = body->children[0];
    if (!st || st->type != AST_ARRAY_ASSIGN || st->child_count < 3) return 0;
    arr = st->children[0];
    ix = st->children[1];
    val = st->children[2];
    if (!arr || arr->type != AST_IDENT || !arr->name) return 0;
    if (!ix || ix->type != AST_IDENT || !ix->name || strcmp(ix->name, iname)) return 0;
    // `(e) as u8` is the explicit narrow store. The rewrite keeps the low byte.
    if (val && val->type == AST_CAST && !(val->is_pointer & 1) && val->child_count >= 1 &&
        val->value) {
        int sb = 64, ss = 1;
        if (int_name(val->value, &sb, &ss) && sb == 8) val = val->children[0];
    }
    if (!val || val->type != AST_BINOP || !val->op || val->op[0] != '&' || val->op[1] || val->child_count != 2)
        return 0;
    if (val->children[1] && val->children[1]->type == AST_NUMBER && num_eq(val->children[1], 255)) {
        mask = val->children[1];
        add = val->children[0];
    } else if (val->children[0] && num_eq(val->children[0], 255)) {
        mask = val->children[0];
        add = val->children[1];
    } else return 0;
    (void)mask;
    if (!collect_add(add, terms, &nt) || nt != 3) return 0;
    for (i = 0; i < nt; i++) {
        AstNode* e = terms[i];
        if (e && e->type == AST_INDEX && e->child_count >= 2 &&
            e->children[0] && e->children[0]->type == AST_IDENT && e->children[0]->name &&
            !strcmp(e->children[0]->name, arr->name) &&
            e->children[1] && e->children[1]->type == AST_IDENT && e->children[1]->name &&
            !strcmp(e->children[1]->name, iname)) {
            nbuf++;
            continue;
        }
        if (e && e->type == AST_IDENT && e->name && !strcmp(e->name, iname)) {
            ni++;
            continue;
        }
        if (extra) return 0;
        extra = e;
    }
    if (nbuf != 1 || ni != 1 || !extra) return 0;
    if (extra->type == AST_NUMBER) {
        if (!parse_i64(extra->value, &imm)) return 0;
        is_imm = 1;
    } else if (extra->type == AST_IDENT && extra->name && strcmp(extra->name, iname) &&
               strcmp(extra->name, arr->name)) {
        sr = symtab_lookup_symbol(cg->symtab, extra->name);
        if (!sr || sr->is_array || sr->is_pointer || sr->is_volatile || sym_narrow_int(sr)) return 0;
        if (name_addr_taken(g_cur_fn, extra->name)) return 0;
    } else return 0;
    if (!lookup_byte_arr(cg, arr->name, &is_g, &gname, &off, &alen)) return 0;
    if (lim > alen) return 0;
    if (name_addr_taken(g_cur_fn, arr->name) || name_addr_taken(g_cur_fn, iname)) return 0;
    si = symtab_lookup_symbol(cg->symtab, (char*)iname);
    if (!si || si->is_array || si->is_pointer || si->is_volatile || sym_narrow_int(si)) return 0;
    check_elem_store(cg, st);
    if (lim == 0) {
        emit_set_i64(cg, si, 0, NULL);
        return 1;
    }
    lab_idx = new_label(cg);
    lab_step = new_label(cg);
    lab_code = new_label(cg);
    lab_loop = new_label(cg);
    emit(cg, "    jmp .L%d\n", lab_code);
    emit(cg, "    align 16\n");
    emit(cg, ".L%d:\n", lab_idx);
    emit(cg, "    db 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15\n");
    emit(cg, ".L%d:\n", lab_step);
    emit(cg, "    db 16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16\n");
    emit(cg, ".L%d:\n", lab_code);
    emit_broadcast_byte(cg, "xmm1", sr && local_reg(sr), sr ? local_reg(sr) : NULL,
                        sr ? sr->offset : 0, imm, is_imm);
    emit(cg, "    movdqu xmm2, [rel .L%d]\n", lab_idx);
    emit(cg, "    movdqu xmm3, [rel .L%d]\n", lab_step);
    emit_arr_lea(cg, "rax", is_g, gname, off);
    emit(cg, "    mov rcx, %lld\n", lim / 16);
    emit(cg, ".L%d:\n", lab_loop);
    emit(cg, "    movdqu xmm0, [rax]\n");
    emit(cg, "    paddb xmm0, xmm2\n");
    emit(cg, "    paddb xmm0, xmm1\n");
    emit(cg, "    movdqu [rax], xmm0\n");
    emit(cg, "    paddb xmm2, xmm3\n");
    emit(cg, "    add rax, 16\n");
    emit(cg, "    sub rcx, 1\n");
    emit(cg, "    jnz .L%d\n", lab_loop);
    emit_set_i64(cg, si, lim, NULL);
    return 1;
}

static int field_of_arr(AstNode* e, const char** arr, const char** idx, const char** field) {
    AstNode* ix;
    AstNode* a;
    AstNode* i;
    if (!e || e->type != AST_FIELD_ACCESS || !e->name || e->child_count < 1) return 0;
    ix = e->children[0];
    if (!ix || ix->type != AST_INDEX || ix->child_count < 2) return 0;
    a = ix->children[0];
    i = ix->children[1];
    if (!a || a->type != AST_IDENT || !a->name) return 0;
    if (!i || i->type != AST_IDENT || !i->name) return 0;
    *arr = a->name;
    *idx = i->name;
    *field = e->name;
    return 1;
}

static int walk_nonneg(AstNode* cond, const char** iname) {
    AstNode* L;
    AstNode* R;
    if (!cond || cond->type != AST_COMPARE || !cond->op || cond->child_count != 2) return 0;
    L = cond->children[0];
    R = cond->children[1];
    if (!L || L->type != AST_IDENT || !L->name) return 0;
    if (!strcmp(cond->op, ">=") && num_eq(R, 0)) { *iname = L->name; return 1; }
    if (!strcmp(cond->op, ">") && num_eq(R, -1)) { *iname = L->name; return 1; }
    return 0;
}

static void fmt_rax_disp(char* buf, int n, int disp) {
    if (disp == 0) snprintf(buf, n, "rax");
    else snprintf(buf, n, "rax%+d", disp);
}

// `while (i >= 0) { sum = sum + arr[i].f; i = arr[i].g; }` on a global array
// of structs whose size is a power of two. Locals stay bounds-checked, so
// this form is globals only. One shl (sal is the same opcode) and one lea
// feed both loads. A size that is not a power of two keeps imul.
static int try_link_walk(Codegen* cg, AstNode* wh) {
    AstNode* body;
    AstNode* st;
    AstNode* nxt;
    AstNode* rhs;
    AstNode* fld;
    Symbol* si;
    Symbol* ss;
    Symbol* local_arr;
    GlobalVar* gv;
    StructType* styp;
    StructField* ff;
    StructField* fg;
    const char* iname = NULL;
    const char* aname = NULL;
    const char* iname2 = NULL;
    const char* fname = NULL;
    const char* ireg;
    const char* sreg;
    char vmem[32], nmem[32];
    int voff = 0, vsz = 0, vsg = 0, vvol = 0;
    int noff = 0, nsz = 0, nsg = 0, nvol = 0;
    int sh, top, done;
    if (g_strength_busy || !o2_hosted() || !wh || wh->type != AST_WHILE || wh->child_count != 2)
        return 0;
    if (calls_asm(g_cur_fn)) return 0;
    if (!walk_nonneg(wh->children[0], &iname)) return 0;
    body = wh->children[1];
    if (!body || body->type != AST_BLOCK || body->child_count != 2) return 0;
    st = body->children[0];
    nxt = body->children[1];
    if (!st || st->type != AST_ASSIGN || !st->name || st->child_count != 1 || !st->children[0]) return 0;
    if (!strcmp(st->name, iname)) return 0;
    rhs = st->children[0];
    if (!is_add_op(rhs)) return 0;
    if (rhs->children[0] && rhs->children[0]->type == AST_IDENT && rhs->children[0]->name &&
        !strcmp(rhs->children[0]->name, st->name))
        fld = rhs->children[1];
    else if (rhs->children[1] && rhs->children[1]->type == AST_IDENT && rhs->children[1]->name &&
             !strcmp(rhs->children[1]->name, st->name))
        fld = rhs->children[0];
    else return 0;
    if (!field_of_arr(fld, &aname, &iname2, &fname) || strcmp(iname2, iname)) return 0;
    if (!nxt || nxt->type != AST_ASSIGN || !nxt->name || strcmp(nxt->name, iname) ||
        nxt->child_count != 1)
        return 0;
    {
        const char* next_field = NULL;
        const char* arr2 = NULL;
        const char* idx2 = NULL;
        if (!field_of_arr(nxt->children[0], &arr2, &idx2, &next_field)) return 0;
        if (strcmp(arr2, aname) || strcmp(idx2, iname)) return 0;
        local_arr = symtab_lookup_symbol(cg->symtab, (char*)aname);
        if (local_arr) return 0;
        gv = global_symtab_lookup(cg->global_symtab, (char*)aname);
        if (!gv || !gv->is_array || gv->is_pointer || gv->is_volatile || !gv->elem_type) return 0;
        if (gv->array_count <= 0) return 0;
        styp = typetab_lookup(cg->types, gv->elem_type);
        if (!styp || styp->size <= 1 || !is_power_of_2((long)styp->size)) return 0;
        sh = get_log2((long)styp->size);
        if (sh <= 0 || sh > 63) return 0;
        ff = typetab_field(cg->types, gv->elem_type, (char*)fname);
        fg = typetab_field(cg->types, gv->elem_type, (char*)next_field);
        if (!ff || !fg || ff->is_pointer || fg->is_pointer || ff->array_len || fg->array_len) return 0;
        if (!field_mem_info(cg, gv->elem_type, fname, &voff, &vsz, &vsg, &vvol)) return 0;
        if (!field_mem_info(cg, gv->elem_type, next_field, &noff, &nsz, &nsg, &nvol)) return 0;
        if (vsz != 8 || nsz != 8 || vvol || nvol) return 0;
        (void)vsg;
        (void)nsg;
        if (name_addr_taken(g_cur_fn, iname) || name_addr_taken(g_cur_fn, st->name) ||
            name_addr_taken(g_cur_fn, aname))
            return 0;
        si = symtab_lookup_symbol(cg->symtab, (char*)iname);
        ss = symtab_lookup_symbol(cg->symtab, st->name);
        if (!si || !ss || si->is_array || ss->is_array || si->is_pointer || ss->is_pointer) return 0;
        if (si->is_volatile || ss->is_volatile || sym_narrow_int(si) || sym_narrow_int(ss)) return 0;
        check_assign_types(cg, st);
        check_assign_types(cg, nxt);
        ireg = local_reg(si);
        sreg = local_reg(ss);
        fmt_rax_disp(vmem, (int)sizeof vmem, voff);
        fmt_rax_disp(nmem, (int)sizeof nmem, noff);
        top = new_label(cg);
        done = new_label(cg);
        if (ireg) emit(cg, "    cmp %s, 0\n    jl .L%d\n", ireg, done);
        else emit(cg, "    cmp qword [rbp%d], 0\n    jl .L%d\n", si->offset, done);
        emit(cg, ".L%d:\n", top);
        if (!ireg) emit(cg, "    mov rax, [rbp%d]\n", si->offset);
        else if (strcmp(ireg, "rax")) emit(cg, "    mov rax, %s\n", ireg);
        emit(cg, "    shl rax, %d\n", sh);
        emit(cg, "    lea rax, [%s+rax]\n", gv->name);
        if (sreg) emit(cg, "    add %s, [%s]\n", sreg, vmem);
        else {
            emit(cg, "    mov rbx, [%s]\n", vmem);
            emit(cg, "    add qword [rbp%d], rbx\n", ss->offset);
        }
        if (ireg) emit(cg, "    mov %s, [%s]\n", ireg, nmem);
        else {
            emit(cg, "    mov rax, [%s]\n", nmem);
            emit(cg, "    mov [rbp%d], rax\n", si->offset);
        }
        if (ireg) emit(cg, "    cmp %s, 0\n    jge .L%d\n", ireg, top);
        else emit(cg, "    cmp qword [rbp%d], 0\n    jge .L%d\n", si->offset, top);
        emit(cg, ".L%d:\n", done);
        return 1;
    }
}

static int index_arr_idx(AstNode* n, const char** arr, const char** idx) {
    AstNode* a;
    AstNode* i;
    if (!n || n->type != AST_INDEX || n->child_count < 2) return 0;
    a = n->children[0];
    i = n->children[1];
    if (!a || a->type != AST_IDENT || !a->name) return 0;
    if (!i || i->type != AST_IDENT || !i->name) return 0;
    *arr = a->name;
    *idx = i->name;
    return 1;
}

// `(arr[idx] + imm)` or that sum masked with 255. The low 8 bits of either
// form are `add` of imm's low byte. A wider addend still wraps mod 256.
static int byte_plus_imm(AstNode* val, const char** arr, const char** idx, long long* imm) {
    AstNode* add = val;
    AstNode* ix = NULL;
    AstNode* num = NULL;
    if (!val) return 0;
    // `(e) as u8` is the explicit narrow store from 0.31.0. The low byte is the same.
    if (val->type == AST_CAST && !(val->is_pointer & 1) && val->child_count >= 1 && val->value) {
        int sb = 64, ss = 1;
        if (int_name(val->value, &sb, &ss) && sb == 8) val = val->children[0];
    }
    add = val;
    if (val->type == AST_BINOP && val->op && val->op[0] == '&' && !val->op[1] && val->child_count == 2) {
        if (num_eq(val->children[1], 255)) add = val->children[0];
        else if (num_eq(val->children[0], 255)) add = val->children[1];
        else return 0;
    }
    if (!is_add_op(add)) return 0;
    if (add->children[0] && add->children[0]->type == AST_INDEX &&
        add->children[1] && add->children[1]->type == AST_NUMBER) {
        ix = add->children[0];
        num = add->children[1];
    } else if (add->children[1] && add->children[1]->type == AST_INDEX &&
               add->children[0] && add->children[0]->type == AST_NUMBER) {
        ix = add->children[1];
        num = add->children[0];
    } else return 0;
    if (!parse_i64(num->value, imm)) return 0;
    return index_arr_idx(ix, arr, idx);
}

// `dst[i] = (src[i] + imm) & 255` or `dst[i] = src[i] + imm` on a byte array.
// The same array is one `add byte`. A different array uses al, not a movzx
// of the sum. Volatile arrays stay on the scalar path (lookup rejects them).
static int try_emit_byte_add(Codegen* cg, AstNode* value, const char* dst_mem,
                             const char* dst_name, const char* idx_name, const char* ireg) {
    const char* src = NULL;
    const char* six = NULL;
    const char* gname = NULL;
    long long imm = 0, len = 0;
    int is_g = 0, off = 0, b;
    char src_mem[128];
    if (!byte_plus_imm(value, &src, &six, &imm)) return 0;
    if (!idx_name || !six || strcmp(six, idx_name) || !dst_name) return 0;
    if (!lookup_byte_arr(cg, src, &is_g, &gname, &off, &len)) return 0;
    b = (int)(imm & 255);
    if (!strcmp(src, dst_name)) {
        if (b == 0) return 1;
        emit(cg, "    add byte [%s], %d\n", dst_mem, b);
        return 1;
    }
    if (!ireg || !strcmp(ireg, "rax")) return 0;
    if (is_g) snprintf(src_mem, sizeof src_mem, "%s+%s*1", gname, ireg);
    else snprintf(src_mem, sizeof src_mem, "rbp%d+%s*1", off, ireg);
    emit(cg, "    movzx eax, byte [%s]\n", src_mem);
    if (b) emit(cg, "    add al, %d\n", b);
    emit(cg, "    mov byte [%s], al\n", dst_mem);
    return 1;
}

static int arr_is_u8(Codegen* cg, const char* name) {
    Symbol* s;
    GlobalVar* g;
    if (!name) return 0;
    s = symtab_lookup_symbol(cg->symtab, (char*)name);
    if (s) return s->type_name && !strcmp(s->type_name, "u8");
    g = global_symtab_lookup(cg->global_symtab, (char*)name);
    return g && g->elem_type && !strcmp(g->elem_type, "u8");
}

// `sum = sum + u8arr[i]`. psadbw sums unsigned bytes. An i8 element is
// sign-extended by the scalar load, so it stays on that path.
static int match_u8_sum(Codegen* cg, AstNode* st, const char* iname, const char** arr) {
    AstNode* e;
    AstNode* ix = NULL;
    if (!st || st->type != AST_ASSIGN || !st->name || st->child_count != 1 || !iname) return 0;
    if (!strcmp(st->name, iname)) return 0;
    e = st->children[0];
    if (!is_add_op(e)) return 0;
    if (e->children[0] && e->children[0]->type == AST_IDENT && e->children[0]->name &&
        !strcmp(e->children[0]->name, st->name))
        ix = e->children[1];
    else if (e->children[1] && e->children[1]->type == AST_IDENT && e->children[1]->name &&
             !strcmp(e->children[1]->name, st->name))
        ix = e->children[0];
    else return 0;
    if (!ix || ix->type != AST_INDEX || ix->child_count < 2) return 0;
    if (!ix->children[0] || ix->children[0]->type != AST_IDENT || !ix->children[0]->name) return 0;
    if (!ix->children[1] || ix->children[1]->type != AST_IDENT || !ix->children[1]->name ||
        strcmp(ix->children[1]->name, iname))
        return 0;
    if (!arr_is_u8(cg, ix->children[0]->name)) return 0;
    *arr = ix->children[0]->name;
    return 1;
}

static int match_byte_inc(AstNode* st, const char* iname, const char** arr, long long* imm) {
    const char* src = NULL;
    const char* six = NULL;
    if (!st || st->type != AST_ARRAY_ASSIGN || st->child_count < 3) return 0;
    if (!st->children[0] || st->children[0]->type != AST_IDENT || !st->children[0]->name) return 0;
    if (!st->children[1] || st->children[1]->type != AST_IDENT || !st->children[1]->name ||
        strcmp(st->children[1]->name, iname))
        return 0;
    if (!byte_plus_imm(st->children[2], &src, &six, imm)) return 0;
    if (strcmp(src, st->children[0]->name) || !six || strcmp(six, iname)) return 0;
    *arr = st->children[0]->name;
    return 1;
}

// `i = 0; while (i < lim) { sum = sum + a[i]; b[i] = (b[i] + imm) & 255; i = i + 1 }`
// lim is a non-negative multiple of 16 and in range. The sum reads the old
// bytes (psadbw), then paddb stores the increment. i8 sums stay scalar.
static int try_byte_suminc(Codegen* cg, AstNode* wh) {
    AstNode* cond;
    AstNode* body;
    AstNode* stsum;
    Symbol* si;
    Symbol* ss;
    const char* iname;
    const char* sum_arr = NULL;
    const char* inc_arr = NULL;
    const char* sum_n = NULL;
    const char* inc_n = NULL;
    const char* sreg;
    long long iz = 0, lim = 0, imm = 0, slen = 0, ilen = 0;
    int sum_g = 0, inc_g = 0, sum_off = 0, inc_off = 0, same, loop;
    if (g_strength_busy || !o2_hosted() || !wh || wh->type != AST_WHILE || wh->child_count != 2)
        return 0;
    if (calls_asm(g_cur_fn)) return 0;
    cond = wh->children[0];
    body = wh->children[1];
    if (!cond || cond->type != AST_COMPARE || !cond->op || strcmp(cond->op, "<") || cond->child_count != 2)
        return 0;
    if (!cond->children[0] || cond->children[0]->type != AST_IDENT || !cond->children[0]->name)
        return 0;
    iname = cond->children[0]->name;
    if (!const_init_val(g_prev_stmt, iname, &iz) || iz != 0) return 0;
    if (!known_loop_lim(cond->children[1], iname, &lim) || lim < 0 || (lim % 16) != 0) return 0;
    if (!body || body->type != AST_BLOCK || body->child_count != 3) return 0;
    if (!is_plus_one(body->children[2], iname)) return 0;
    stsum = body->children[0];
    if (!match_u8_sum(cg, stsum, iname, &sum_arr)) return 0;
    if (!match_byte_inc(body->children[1], iname, &inc_arr, &imm)) return 0;
    if (!lookup_byte_arr(cg, sum_arr, &sum_g, &sum_n, &sum_off, &slen)) return 0;
    if (!lookup_byte_arr(cg, inc_arr, &inc_g, &inc_n, &inc_off, &ilen)) return 0;
    if (lim > slen || lim > ilen) return 0;
    if (name_addr_taken(g_cur_fn, sum_arr) || name_addr_taken(g_cur_fn, inc_arr) ||
        name_addr_taken(g_cur_fn, iname) || name_addr_taken(g_cur_fn, stsum->name))
        return 0;
    si = symtab_lookup_symbol(cg->symtab, (char*)iname);
    ss = symtab_lookup_symbol(cg->symtab, stsum->name);
    if (!si || si->is_array || si->is_pointer || si->is_volatile || sym_narrow_int(si)) return 0;
    if (!ss || ss->is_array || ss->is_pointer || ss->is_volatile || sym_narrow_int(ss)) return 0;
    check_assign_types(cg, stsum);
    check_elem_store(cg, body->children[1]);
    if (lim == 0) {
        emit_set_i64(cg, si, 0, NULL);
        return 1;
    }
    same = !strcmp(sum_arr, inc_arr);
    loop = new_label(cg);
    emit(cg, "    pxor xmm2, xmm2\n");
    emit(cg, "    pxor xmm3, xmm3\n");
    emit_broadcast_byte(cg, "xmm1", 0, NULL, 0, imm, 1);
    emit_arr_lea(cg, "rax", sum_g, sum_n, sum_off);
    if (!same) emit_arr_lea(cg, "rdx", inc_g, inc_n, inc_off);
    emit(cg, "    mov rcx, %lld\n", lim / 16);
    emit(cg, ".L%d:\n", loop);
    emit(cg, "    movdqu xmm0, [rax]\n");
    emit(cg, "    movdqa xmm4, xmm0\n");
    emit(cg, "    psadbw xmm4, xmm2\n");
    emit(cg, "    paddq xmm3, xmm4\n");
    if (!same) emit(cg, "    movdqu xmm0, [rdx]\n");
    emit(cg, "    paddb xmm0, xmm1\n");
    if (same) emit(cg, "    movdqu [rax], xmm0\n");
    else emit(cg, "    movdqu [rdx], xmm0\n");
    emit(cg, "    add rax, 16\n");
    if (!same) emit(cg, "    add rdx, 16\n");
    emit(cg, "    sub rcx, 1\n");
    emit(cg, "    jnz .L%d\n", loop);
    emit(cg, "    movq rax, xmm3\n");
    emit(cg, "    psrldq xmm3, 8\n");
    emit(cg, "    movq rdx, xmm3\n");
    emit(cg, "    add rax, rdx\n");
    sreg = local_reg(ss);
    if (sreg) emit(cg, "    add %s, rax\n", sreg);
    else emit(cg, "    add qword [rbp%d], rax\n", ss->offset);
    emit_set_i64(cg, si, lim, NULL);
    return 1;
}

static int try_hosted_rewrite(Codegen* cg, AstNode* n) {
    if (!o2_hosted()) return 0;
    if (try_link_walk(cg, n)) return 1;
    if (try_byte_copy(cg, n)) return 1;
    if (try_byte_suminc(cg, n)) return 1;
    if (try_byte_bump(cg, n)) return 1;
    if (try_strength_dot(cg, n)) return 1;
    if (try_byte_fill(cg, n)) return 1;
    if (try_linear_sum(cg, n)) return 1;
    return 0;
}

// if/while conditions jump on the flags. A comparison used as a value still
// materializes 0/1 in the accumulator; a branch must not, or a syscall
// result sitting in rax is gone before the next statement reads it.
static void emit_acc_jcc(Codegen* cg, int lab, int jump_if_zero) {
    if (legacy_bits()) emit(cg, "    test eax, eax\n");
    else emit(cg, "    test rax, rax\n");
    emit(cg, "    %s .L%d\n", jump_if_zero ? "jz" : "jnz", lab);
}

static void emit_cmp_acc(Codegen* cg, AstNode* n) {
    gen_expr(cg, n->children[0]);
    if (legacy_bits()) {
        // ebx holds the right-hand side for `cmp eax, ebx`. It is also a
        // naked lvalue (the INT 15h continuation token). Save it first, and
        // pop it after the compare: pop does not change the flags.
        emit(cg, "    push ebx\n    push eax\n    push edx\n");
        gen_expr(cg, n->children[1]);
        emit(cg, "    mov ebx, eax\n    pop edx\n    pop eax\n");
        emit(cg, "    cmp eax, ebx\n");
        emit(cg, "    pop ebx\n");
    } else if (optimization_level >= 2 && !g_freestanding && can_emit_rbx(cg, n->children[1])) {
        emit_rbx(cg, n->children[1]);
        emit(cg, "    cmp rax, rbx\n");
    } else {
        emit(cg, "    push rax\n");
        gen_expr(cg, n->children[1]);
        emit(cg, "    mov rbx, rax\n    pop rax\n");
        emit(cg, "    cmp rax, rbx\n");
    }
}

static int cond_is_not(AstNode* n) {
    return n && n->type == AST_UNARY && n->op && n->op[0] == '!' && !n->op[1] &&
        n->child_count > 0;
}

static void branch_fail(Codegen* cg, AstNode* n, int fail_lab);
static void branch_succeed(Codegen* cg, AstNode* n, int ok_lab);

static int emit_cmp_jcc(Codegen* cg, AstNode* n, int lab, int succeed) {
    const char* jcc;
    int uns;
    if (!n || n->type != AST_COMPARE || !n->op || n->child_count != 2) return 0;
    uns = cmp_legacy_unsigned(cg, n);
    g_cmp_unsigned = uns;
    jcc = succeed ? jcc_when_true(n->op, 0) : cmp_fail_jcc(n->op);
    g_cmp_unsigned = 0;
    if (!jcc) return 0;
    emit_cmp_acc(cg, n);
    emit(cg, "    %s .L%d\n", jcc, lab);
    return 1;
}

static void branch_fail(Codegen* cg, AstNode* n, int fail_lab) {
    if (!n) return;
    if (cond_is_not(n)) {
        branch_succeed(cg, n->children[0], fail_lab);
    } else if (emit_cmp_jcc(cg, n, fail_lab, 0)) {
    } else if (n->type == AST_LOGICAL && n->op && n->child_count == 2 && !strcmp(n->op, "&&")) {
        branch_fail(cg, n->children[0], fail_lab);
        branch_fail(cg, n->children[1], fail_lab);
    } else if (n->type == AST_LOGICAL && n->op && n->child_count == 2 && !strcmp(n->op, "||")) {
        int ok = new_label(cg);
        branch_succeed(cg, n->children[0], ok);
        branch_fail(cg, n->children[1], fail_lab);
        emit(cg, ".L%d:\n", ok);
    } else {
        gen_expr(cg, n);
        emit_acc_jcc(cg, fail_lab, 1);
    }
}

static void branch_succeed(Codegen* cg, AstNode* n, int ok_lab) {
    if (!n) return;
    if (cond_is_not(n)) {
        branch_fail(cg, n->children[0], ok_lab);
    } else if (emit_cmp_jcc(cg, n, ok_lab, 1)) {
    } else if (n->type == AST_LOGICAL && n->op && n->child_count == 2 && !strcmp(n->op, "||")) {
        branch_succeed(cg, n->children[0], ok_lab);
        branch_succeed(cg, n->children[1], ok_lab);
    } else if (n->type == AST_LOGICAL && n->op && n->child_count == 2 && !strcmp(n->op, "&&")) {
        int fail = new_label(cg);
        branch_fail(cg, n->children[0], fail);
        branch_succeed(cg, n->children[1], ok_lab);
        emit(cg, ".L%d:\n", fail);
    } else {
        gen_expr(cg, n);
        emit_acc_jcc(cg, ok_lab, 0);
    }
}

void gen_stmt(Codegen* cg, AstNode* n) {
    if (!n) return;  // Null safety
    int keep = g_fwd_hold;
    g_fwd_hold = 0;
    if (!keep) g_fwd_ok = 0;
    g_nonneg_reg = NULL;
    if (n->file) focus_file(n->file);
    if (n->type == AST_LABEL) {
        if (!n->name) return;
        int dup = 0;
        for (int i = 0; i < g_nseen_labels; i++)
            if (!strcmp(g_seen_labels[i], n->name)) dup = 1;
        if (dup || find_func(cg, n->name) || is_data_name(cg, n->name))
            diag_error(n->line, n->col, "label '%s' is already defined", n->name);
        else if (g_nseen_labels < 128)
            g_seen_labels[g_nseen_labels++] = n->name;
        emit(cg, "%s:\n", n->name);
        return;
    }
    if (n->type == AST_BLOCK) {
        // Names declared here hide outer ones and disappear at the closing brace.
        symtab_enter(cg->symtab);
        for (int i = 0; i < n->child_count; i++) {
            int sunk;
            g_prev2_stmt = i > 1 ? n->children[i - 2] : NULL;
            g_prev_stmt = i > 0 ? n->children[i - 1] : NULL;
            if (keep && i == 0) g_fwd_hold = 1;
            sunk = try_sink_masks(cg, n, i);
            if (sunk >= 2) {
                i += sunk - 1;
                continue;
            }
            sunk = try_insert_shift(cg, n, i);
            if (sunk >= 2) {
                i += sunk - 1;
                continue;
            }
            if (!try_hosted_rewrite(cg, n->children[i]))
                gen_stmt(cg, n->children[i]);
        }
        g_prev_stmt = NULL;
        g_prev2_stmt = NULL;
        symtab_leave(cg->symtab);
        return;
    }
    if (n->type == AST_RETURN) {
        if (n->child_count > 0) {
            if (g_cur_fn && !(g_cur_fn->struct_type && !strcmp(g_cur_fn->struct_type, "__array__"))) {
                VTy dst_ty = ty_decl(cg, 1, g_cur_fn->is_pointer, g_cur_fn->value);
                check_types(n->children[0], "return value", dst_ty, expr_ty(cg, n->children[0]));
            }
            gen_expr(cg, n->children[0]);
        } else emit(cg, "    xor rax, rax\n");
        if (g_fn_flags & (AF_INTERRUPT | AF_SYSCALL))
            emit(cg, "    jmp .Lret_%s\n", g_fn_name);
        else if (g_fn_flags & AF_NAKED)
            emit(cg, "    ret\n");
        else
            emit_hosted_epilogue(cg);
    } else if (n->type == AST_LET) {
        if (n->name && symtab_redeclared(cg->symtab, n->name)) {
            diag_error(n->line, n->col, "redeclaration of '%s' in the same block", n->name);
            return;
        }
        int size = 1;
        char* type_name = NULL;

        // Check if this is a typed array: let arr: [i32; 10];
        if (n->struct_type && !strcmp(n->struct_type, "__array__")) {
            if (n->flags & AF_VOLATILE) {
                diag_error(n->line, n->col,
                           "volatile cannot apply to an array; use a '*volatile T' pointer or a volatile field");
                return;
            }
            // Calculate size based on element type and array count
            int elem_size = 8;  // Default to 8 bytes
            if (n->value) {
                if (!strcmp(n->value, "i8") || !strcmp(n->value, "u8")) elem_size = 1;
                else if ((!strcmp(n->value, "i16") || !strcmp(n->value, "u16"))) elem_size = 2;
                else if (!strcmp(n->value, "i32") || !strcmp(n->value, "u32")) elem_size = 4;
                else if (!strcmp(n->value, "i64") || !strcmp(n->value, "u64")) elem_size = 8;
            }

            if (n->value) {
                StructType* est = typetab_lookup(cg->types, n->value);
                if (est && est->size > 0) elem_size = est->size;  // array of structs
            }
            int total_size = elem_size * n->array_size;
            int slots = (total_size + 7) / 8;
            if (slots < n->array_size) slots = n->array_size;
            symtab_add(cg->symtab, n->name, slots);
            cg->symtab->symbols[cg->symtab->count - 1].is_array = 1;
            cg->symtab->symbols[cg->symtab->count - 1].array_len = n->array_size;

            // Store element type in symbol table for later use
            if (cg->symtab && cg->symtab->count > 0) {
                cg->symtab->symbols[cg->symtab->count - 1].type_name = n->value;  // Store element type
            }

            emit(cg, "    ; Array local: %s: [%s; %d] (%d bytes total)\n", n->name, n->value ? n->value : "i64", n->array_size, total_size);

            // If there's an initializer, generate it
            if (n->child_count > 0) {
                gen_expr(cg, n->children[0]);
            }
            return;
        }

        // Check if this is a struct type: let p: Point;
        if (n->struct_type && strcmp(n->struct_type, "__array__") != 0) {
            StructType* st = typetab_lookup(cg->types, n->struct_type);
            if (st) {
                if (n->flags & AF_VOLATILE) {
                    diag_error(n->line, n->col,
                               "volatile cannot apply to struct '%s'; mark fields volatile or use '*volatile %s'",
                               n->struct_type, n->struct_type);
                    return;
                }
                emit(cg, "    ; Struct local: %s: %s (%d bytes)\n", n->name, n->struct_type, st->size);
                symtab_add_struct(cg->symtab, n->name, n->struct_type, st->size);

                if (n->child_count > 0) {
                    if (n->children[0]->type == AST_STRUCT_LITERAL || !expr_struct_type(cg, n->children[0])) {
                        gen_expr(cg, n->children[0]);
                    } else {
                        AstNode dst;
                        memset(&dst, 0, sizeof(dst));
                        dst.type = AST_IDENT;
                        dst.name = n->name;
                        dst.line = n->line;
                        dst.col = n->col;
                        gen_struct_assign(cg, &dst, n->children[0]);
                    }
                }
                return;
            }
        }

        if (n->child_count > 0) {
            if (n->children[0]->type == AST_ARRAY_LITERAL) {
                size = n->children[0]->array_size;
            } else if (n->children[0]->type == AST_STRUCT_LITERAL) {
                type_name = n->children[0]->struct_type;
                StructType* st = typetab_lookup(cg->types, type_name);
                if (st) {
                    symtab_add_struct(cg->symtab, n->name, type_name, st->size);
                    gen_expr(cg, n->children[0]);
                    return;
                }
            }
        }

        // Check if pointer type
        if (n->is_pointer) {
            int before = cg->symtab->stack_size;
            int off = symtab_add_pointer(cg->symtab, n->name);
            naked_take_slot(&cg->symtab->symbols[cg->symtab->count - 1],
                            cg->symtab->stack_size - before, n->line, n->col);

            // Store type name for element size calculation
            if (n->value && cg->symtab && cg->symtab->count > 0) {
                // Build type_name as "*BaseType"
                char* type_buf = malloc(strlen(n->value) + 2);
                sprintf(type_buf, "*%s", n->value);
                cg->symtab->symbols[cg->symtab->count - 1].type_name = type_buf;
            }
            if (n->flags & AF_VOLATILE)
                cg->symtab->symbols[cg->symtab->count - 1].is_volatile = 1;
            if (n->is_pointer & 4)
                cg->symtab->symbols[cg->symtab->count - 1].volatile_pointee = 1;

            if (n->child_count > 0) {
                VTy dst_ty = ty_decl(cg, 0, n->is_pointer, n->value);
                check_types(n->children[0], "", dst_ty, expr_ty(cg, n->children[0]));
                gen_expr(cg, n->children[0]);
                if (cg->symtab->symbols[cg->symtab->count - 1].mem && legacy_bits())
                    emit(cg, "    mov [%s], eax\n", cg->symtab->symbols[cg->symtab->count - 1].mem);
                else if (cg->symtab->symbols[cg->symtab->count - 1].mem)
                    emit(cg, "    mov [%s], rax\n", cg->symtab->symbols[cg->symtab->count - 1].mem);
                else emit(cg, "    mov [rbp%d], rax\n", off);
            }
            return;
        }

        int before = cg->symtab->stack_size;
        int off = symtab_add(cg->symtab, n->name, size);
        if (n->child_count > 0 && n->children[0]->type == AST_ARRAY_LITERAL)
            cg->symtab->symbols[cg->symtab->count - 1].is_array = 1;
        else
            naked_take_slot(&cg->symtab->symbols[cg->symtab->count - 1],
                            cg->symtab->stack_size - before, n->line, n->col);
        if (n->value && cg->symtab && cg->symtab->count > 0)
            cg->symtab->symbols[cg->symtab->count - 1].type_name = strdup(n->value);
        if (n->child_count > 0) {
            VTy src_ty = expr_ty(cg, n->children[0]);
            if (n->value)
                check_types(n->children[0], "", ty_decl(cg, 0, 0, n->value), src_ty);
            else if (cg->symtab && cg->symtab->count > 0 && src_ty.kind != VK_STRUCT) {
                // An untyped integer is i64. Inferring u8 from a byte load would
                // mask the slot and change code that only stored the widened value.
                // A struct is not inferred onto this 8-byte slot.
                if (src_ty.kind == VK_INT) src_ty = ty_i64();
                bind_vty(&cg->symtab->symbols[cg->symtab->count - 1], src_ty);
            }
            Symbol* sym = &cg->symtab->symbols[cg->symtab->count - 1];
            const char* rr = local_reg(sym);
            if (!sym_narrow_int(sym) && rr && n->children[0]->type != AST_ARRAY_LITERAL &&
                o2_hosted() && can_place(cg, n->children[0], rr)) {
                place_in(cg, n->children[0], rr);
            } else {
                gen_expr(cg, n->children[0]);
                if (n->children[0]->type != AST_ARRAY_LITERAL) {
                    if (!sym->is_pointer && !sym->is_array)
                        emit_store_trunc(cg, sym->type_name, n->children[0]);
                    if (rr) emit(cg, "    mov %s, rax\n", rr);
                    else if (sym->mem && legacy_bits()) emit(cg, "    mov [%s], eax\n", sym->mem);
                    else if (sym->mem) emit(cg, "    mov [%s], rax\n", sym->mem);
                    else emit(cg, "    mov [rbp%d], rax\n", off);
                }
            }
        }
    } else if (n->type == AST_IF) {
        int else_lab = new_label(cg);
        int end_lab = new_label(cg);
        if (n->flags & AF_CARRY) {
            emit(cg, "    jnc .L%d\n", else_lab);
            gen_stmt(cg, n->children[0]);
            emit(cg, "    jmp .L%d\n.L%d:\n", end_lab, else_lab);
            if (n->child_count > 1)
                gen_stmt(cg, n->children[1]);
            emit(cg, ".L%d:\n", end_lab);
        } else if (o2_hosted() && can_branch_cond(cg, n->children[0])) {
            g_fwd_ok = 0;
            emit_cond_fail(cg, n->children[0], else_lab);
            g_nonneg_reg = NULL;
            g_fwd_hold = 1;
            gen_stmt(cg, n->children[1]);
            emit(cg, "    jmp .L%d\n.L%d:\n", end_lab, else_lab);
            if (n->child_count > 2)
                gen_stmt(cg, n->children[2]);
            emit(cg, ".L%d:\n", end_lab);
        } else {
            branch_fail(cg, n->children[0], else_lab);
            gen_stmt(cg, n->children[1]);
            emit(cg, "    jmp .L%d\n.L%d:\n", end_lab, else_lab);
            if (n->child_count > 2)
                gen_stmt(cg, n->children[2]);
            emit(cg, ".L%d:\n", end_lab);
        }
    } else if (n->type == AST_WHILE) {
        int start_lab = new_label(cg);
        int end_lab = new_label(cg);
        int cont_lab = new_label(cg);
        emit(cg, ".L%d:\n", start_lab);
        int branched = 0;
        if (!legacy_bits() && o2_hosted() && can_branch_cond(cg, n->children[0])) {
            g_fwd_ok = 0;
            emit_cond_fail(cg, n->children[0], end_lab);
            g_nonneg_reg = NULL;
            branched = 1;
        } else {
            branch_fail(cg, n->children[0], end_lab);
        }
        if (g_loop_depth >= MAX_LOOP_DEPTH) { diag_error(n->line, n->col, "loops nested too deeply"); return; }
        g_loop_break[g_loop_depth] = end_lab;
        g_loop_cont[g_loop_depth] = cont_lab;
        g_loop_depth++;
        const char* save_idx = g_proven_idx;
        long long save_lim = g_proven_lim;
        AstNode* save_ew = g_enclosing_while;
        AstNode* save_ep = g_enclosing_prev;
        const char* pidx = NULL;
        long long plim = -1;
        if (o2_hosted() && match_proven(cg, g_prev_stmt, n, &pidx, &plim)) {
            g_proven_idx = pidx;
            g_proven_lim = plim;
        }
        g_enclosing_while = n;
        g_enclosing_prev = g_prev_stmt;
        if (branched) g_fwd_hold = 1;
        gen_stmt(cg, n->children[1]);
        g_enclosing_while = save_ew;
        g_enclosing_prev = save_ep;
        g_proven_idx = save_idx;
        g_proven_lim = save_lim;
        g_loop_depth--;
        emit(cg, ".L%d:\n", cont_lab);
        if (n->child_count > 2)
            gen_stmt(cg, n->children[2]);
        emit(cg, "    jmp .L%d\n.L%d:\n", start_lab, end_lab);
    } else if (n->type == AST_BREAK || n->type == AST_CONTINUE) {
        if (g_loop_depth == 0) {
            diag_error(n->line, n->col, "'%s' outside of a loop", n->type == AST_BREAK ? "break" : "continue");
            return;
        }
        emit(cg, "    jmp .L%d\n", n->type == AST_BREAK ? g_loop_break[g_loop_depth - 1] : g_loop_cont[g_loop_depth - 1]);
    } else if (n->type == AST_JMP) {
        gen_jmp(cg, n);
    } else if (n->type == AST_MEM_STORE) {
        gen_mem_store(cg, n);
    } else if (n->type == AST_CALL || n->type == AST_ASSIGN || n->type == AST_ARRAY_ASSIGN || n->type == AST_FIELD_ASSIGN) {
        if (n->type == AST_ASSIGN && !assign_dest_narrow(cg, n) &&
            (assign_direct(cg, n) || assign_index(cg, n))) return;
        gen_expr(cg, n);
    }
}

// Copy `sz` bytes from `src_reg` into the frame at `dst`. Whole qwords use the
// historical instruction text; a short tail covers packed structs.
static void emit_copy_param(Codegen* cg, const char* src_reg, int dst, int sz) {
    int f = 0;
    for (; f + 8 <= sz; f += 8) {
        emit(cg, "    mov rax, [%s+%d]\n", src_reg, f);
        emit(cg, "    mov [rbp%+d], rax\n", dst + f);
    }
    for (; f < sz; f++) {
        emit(cg, "    mov al, [%s+%d]\n", src_reg, f);
        emit(cg, "    mov [rbp%+d], al\n", dst + f);
    }
}

static void bind_param(Codegen* cg, AstNode* par, const char* src_reg, int stack_disp, int from_stack) {
    if (par->name && symtab_redeclared(cg->symtab, par->name)) {
        diag_error(par->line, par->col, "redeclaration of '%s' in the same block", par->name);
        return;
    }
    int off;
    if (par->is_pointer) {
        off = symtab_add_pointer(cg->symtab, par->name);
        if (par->value && cg->symtab && cg->symtab->count > 0) {
            char* type_with_ptr = malloc(strlen(par->value) + 2);
            sprintf(type_with_ptr, "*%s", par->value);
            cg->symtab->symbols[cg->symtab->count - 1].type_name = type_with_ptr;
        }
        if (par->is_pointer & 4)
            cg->symtab->symbols[cg->symtab->count - 1].volatile_pointee = 1;
    } else if (par->value && typetab_lookup(cg->types, par->value)) {
        StructType* stt = typetab_lookup(cg->types, par->value);
        int sz = stt->size > 0 ? stt->size : 8;
        off = symtab_add_struct(cg->symtab, par->name, par->value, sz);
        if (from_stack) {
            emit(cg, "    mov rbx, [rbp+%d]\n", stack_disp);
            emit_copy_param(cg, "rbx", off, sz);
        } else {
            emit_copy_param(cg, src_reg, off, sz);
        }
        return;
    } else {
        off = symtab_add(cg->symtab, par->name, 1);
        if (par->value && cg->symtab && cg->symtab->count > 0)
            cg->symtab->symbols[cg->symtab->count - 1].type_name = strdup(par->value);
    }
    Symbol* bound = &cg->symtab->symbols[cg->symtab->count - 1];
    const char* rr = local_reg(bound);
    {
        int bits = 64, sg = 1;
        int narrow = par->value && !(par->is_pointer) && int_name(par->value, &bits, &sg) && bits < 64;
        if (narrow) {
            if (from_stack) emit(cg, "    mov rax, [rbp+%d]\n", stack_disp);
            else emit(cg, "    mov rax, %s\n", src_reg);
            emit_width_trunc(cg, bits, sg);
            if (rr) emit(cg, "    mov %s, rax\n", rr);
            else emit(cg, "    mov [rbp%d], rax\n", off);
            return;
        }
    }
    if (rr) {
        if (from_stack) emit(cg, "    mov %s, [rbp+%d]\n", rr, stack_disp);
        else if (strcmp(rr, src_reg)) emit(cg, "    mov %s, %s\n", rr, src_reg);
    } else if (from_stack) {
        emit(cg, "    mov rax, [rbp+%d]\n", stack_disp);
        emit(cg, "    mov [rbp%d], rax\n", off);
    } else {
        emit(cg, "    mov [rbp%d], %s\n", off, src_reg);
    }
}

void gen_func(Codegen* cg, AstNode* n) {
    g_reg_local_n = 0;
    g_reg_callee_n = 0;
    g_slim = 0;
    g_slim_pad = 0;
    g_slim_nsave = 0;
    g_expr_tmp = NULL;
    g_fwd_ok = 0;
    g_fwd_hold = 0;
    g_nonneg_reg = NULL;
    g_proven_idx = NULL;
    g_proven_lim = -1;
    g_prev_stmt = NULL;
    g_prev2_stmt = NULL;
    g_strength_busy = 0;
    g_enclosing_while = NULL;
    g_enclosing_prev = NULL;
    if (!n || !n->name) return;  // Null safety
    if (n->file) focus_file(n->file);
    int param_count = n->child_count - 1;
    int is_int = (n->flags & AF_INTERRUPT) != 0;
    int is_sys = (n->flags & AF_SYSCALL) != 0;
    int naked = (n->flags & AF_NAKED) != 0;
    int bits = n->bits;
    // A pointer parameter is the CPU frame, not the integer error code.
    // error_code as the first field selects the #PF/#GP layout.
    int frame_ptr = 0;
    int frame_err = 0;
    int frame_slot = 0;
    AstNode* par0 = (param_count == 1) ? n->children[0] : NULL;
    if (is_int && par0 && (par0->is_pointer & 1)) {
        StructType* st = (par0->value && cg->types) ? typetab_lookup(cg->types, par0->value) : NULL;
        if (!st) {
            diag_error(par0->line, par0->col, "an interrupt frame parameter must point at a struct");
        } else {
            frame_err = st->field_count > 0 && st->fields[0].name &&
                        !strcmp(st->fields[0].name, "error_code");
            int need = frame_err ? 6 : 5;
            if (st->field_count < need) {
                diag_error(par0->line, par0->col, "interrupt frame '%s' needs %d fields", st->name, need);
            } else {
                frame_ptr = 1;
                for (int i = 0; i < need; i++) {
                    if (st->fields[i].offset != i * 8 || st->fields[i].size != 8) {
                        diag_error(par0->line, par0->col,
                                   "interrupt frame field '%s' must be an 8-byte field at offset %d",
                                   st->fields[i].name ? st->fields[i].name : "?", i * 8);
                        frame_ptr = 0;
                    }
                }
            }
        }
    } else if (is_int && par0 && !par0->is_pointer && par0->value &&
               cg->types && typetab_lookup(cg->types, par0->value)) {
        diag_error(par0->line, par0->col, "an interrupt frame parameter must be a pointer");
    }
    int has_err = is_int && param_count == 1 && par0 && !(par0->is_pointer & 1) && !frame_ptr;
    // A struct passed by value already diagnosed; do not also copy an error code.
    if (is_int && par0 && !par0->is_pointer && par0->value &&
        cg->types && typetab_lookup(cg->types, par0->value))
        has_err = 0;
    int align_err = has_err || (frame_ptr && frame_err);
    if (is_int && is_sys) {
        diag_error(n->line, n->col, "a function cannot be both interrupt and syscall");
    }
    if (naked && (is_int || is_sys)) {
        diag_error(n->line, n->col, "a naked function cannot be interrupt or syscall");
    }
    if ((is_int || is_sys) && bits && bits != 64) {
        diag_error(n->line, n->col, "interrupt and syscall functions are 64-bit");
    }
    if ((bits == 16 || bits == 32) && !naked) {
        diag_error(n->line, n->col,
                   "bits %d functions must be naked; the frame prologue uses 64-bit registers", bits);
    }
    if (naked && param_count > 0) {
        int mode = bits ? bits : 64;
        int maxp = (mode == 16 || mode == 32) ? 4 : 6;
        if (param_count > maxp)
            diag_error(n->line, n->col, "a naked function takes at most %d register parameters", maxp);
    }
    if (is_int && param_count > 1) {
        diag_error(n->line, n->col,
                   "an interrupt function takes 0 arguments, or 1 (the CPU error code)");
        g_fn_flags = 0;
        return;
    }
    if (is_sys && param_count > 7) {
        diag_error(n->line, n->col,
                   "a syscall function takes at most 7 arguments (nr, a1, a2, a3, a4, a5, a6)");
    }
    if (!is_int && !is_sys && param_count > 16) {
        diag_error(n->line, n->col, "function '%s' has %d parameters; the maximum is 16",
                   n->name, param_count);
    }

    g_fn_flags = n->flags;
    g_fn_bits = bits;
    g_cur_fn = n;
    g_naked_slot_n = 0;
    g_naked_slot_seq = 0;
    g_naked_slot_bytes = 0;
    snprintf(g_fn_name, sizeof(g_fn_name), "%s", n->name);

    if (bits) emit(cg, "\nbits %d\n%s:\n", bits, n->name);
    else emit(cg, "\n%s:\n", n->name);
    if (naked) {
        // No frame. The body is the machine code, in the selected bits mode.
    } else if (is_int) {
        // CPU already pushed SS, RSP, RFLAGS, CS, RIP, and maybe an error code.
        // Save every GPR: the interrupted context is not a normal caller.
        static const char* saved[] = {
            "rax", "rcx", "rdx", "rbx", "rbp", "rsi", "rdi",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
        };
        for (int i = 0; i < 15; i++) emit(cg, "    push %s\n", saved[i]);
        if (align_err) emit(cg, "    push rax\n");  // keep the frame 16-byte aligned
        emit(cg, "    mov rbp, rsp\n");
        emit(cg, "    sub rsp, __frame_%s\n", n->name);
    } else if (is_sys) {
        // syscall/sysret entry. Linux register convention in, System V for the body.
        // The kernel must call set_syscall_stack() with a 16-byte-aligned top first.
        cg->need_syscall_data = 1;
        emit(cg, "    mov [__syscall_user_rsp], rsp\n");
        emit(cg, "    mov rsp, [__syscall_rsp]\n");
        emit(cg, "    push qword [__syscall_user_rsp]\n");
        emit(cg, "    push rcx\n");             // user RIP
        emit(cg, "    push r11\n");             // user RFLAGS
        if (param_count >= 7) {
            emit(cg, "    push r9\n");          // a6
            emit(cg, "    push rax\n");         // spacer so a6 is at [rbp+16]
        }
        emit(cg, "    mov r9, r8\n");           // a5
        emit(cg, "    mov r8, r10\n");          // a4 (r10, because rcx is the user RIP)
        emit(cg, "    mov rcx, rdx\n");         // a3
        emit(cg, "    mov rdx, rsi\n");         // a2
        emit(cg, "    mov rsi, rdi\n");         // a1
        emit(cg, "    mov rdi, rax\n");         // syscall number
        emit(cg, "    push rbp\n    mov rbp, rsp\n");
        emit(cg, "    sub rsp, __frame_%s\n", n->name);
    } else {
        if (optimization_level >= 2 && !g_freestanding)
            plan_reg_locals(cg, n);
        prepare_frame(cg, n);
        EarlyRet early = match_early_ret(cg, n);
        if (early.ok) {
            if (early.imm_rhs) emit(cg, "    cmp %s, %lld\n", early.lhs, early.imm);
            else emit(cg, "    cmp %s, %s\n", early.lhs, early.rhs);
            emit(cg, "    %s .Lbase_%s\n", early.jcc, n->name);
        }
        if (g_slim) {
            // No rbp. Callee-saved regs (and the call temp) are pushed.
            // The early-out above jumped before these pushes.
            for (int i = 0; i < g_slim_nsave; i++)
                emit(cg, "    push %s\n", g_slim_save[i]);
            if (g_slim_pad) emit(cg, "    sub rsp, 8\n");
        } else {
            emit(cg, "    push rbp\n    mov rbp, rsp\n");
            // Frame size is only known after the body is generated: use a NASM
            // forward-referenced constant defined after the function.
            emit(cg, "    sub rsp, __frame_%s\n", n->name);
            // Callee-saved r12-r15 live in the frame's spare bytes, below the locals.
            // r10 and r11 are not saved: a local is placed there only when no
            // call falls inside its live range. The call temp, if any, is next.
            for (int i = 0; i < g_reg_callee_n; i++)
                emit(cg, "    mov [rbp - __frame_%s + %d], %s\n",
                     n->name, 8 * (i + 1), g_reg_local_name[i]);
            if (g_expr_tmp)
                emit(cg, "    mov [rbp - __frame_%s + %d], %s\n",
                     n->name, 8 * (g_reg_callee_n + 1), g_expr_tmp);
        }
    }

    const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    SymbolTable* old_symtab = cg->symtab;
    cg->symtab = symtab_new();
    // Parameters share the function body's scope, so `let x` next to them is a redeclaration.
    symtab_enter(cg->symtab);

    if (is_int) {
        if (frame_ptr) {
            if (symtab_redeclared(cg->symtab, par0->name))
                diag_error(par0->line, par0->col,
                           "redeclaration of '%s' in the same block", par0->name);
            // No error code: RIP is at [rbp+120]. With error_code, the CPU
            // qword is at [rbp+128] and RIP follows it. SavedRegs (r15 first)
            // sit 120 bytes below that first CPU qword either way.
            frame_slot = symtab_add_pointer(cg->symtab, par0->name);
            if (par0->value && cg->symtab->count > 0) {
                char* type_with_ptr = malloc(strlen(par0->value) + 2);
                sprintf(type_with_ptr, "*%s", par0->value);
                cg->symtab->symbols[cg->symtab->count - 1].type_name = type_with_ptr;
            }
            emit(cg, "    lea rax, [rbp+%d]\n", frame_err ? 128 : 120);
            emit(cg, "    mov [rbp%d], rax\n", frame_slot);
        } else if (has_err) {
            if (symtab_redeclared(cg->symtab, n->children[0]->name))
                diag_error(n->children[0]->line, n->children[0]->col,
                           "redeclaration of '%s' in the same block", n->children[0]->name);
            // 15 saved registers plus the alignment push: error code is at [rbp+128].
            int off = symtab_add(cg->symtab, n->children[0]->name, 1);
            if (n->children[0]->value && cg->symtab->count > 0)
                cg->symtab->symbols[cg->symtab->count - 1].type_name = strdup(n->children[0]->value);
            emit(cg, "    mov rax, [rbp+128]\n");
            emit(cg, "    mov [rbp%d], rax\n", off);
        }
    } else if (naked) {
        // No frame and no moves. The name is the incoming register.
        // bits 64: rdi, rsi, rdx, rcx, r8, r9 (the same registers a Copihue
        // call already fills). bits 32: eax, ecx, edx, esi. bits 16: ax, cx,
        // dx, si. ax/eax are the accumulator, so a later expression overwrites
        // that parameter. bx/ebx/rbx stay scratch and are not parameter registers.
        // bx/bp and ebx/ebp/esi/edi are naked lvalues in bits 16/32 (not here).
        int mode = bits ? bits : 64;
        int maxp = (mode == 16 || mode == 32) ? 4 : 6;
        static const char* p16[] = {"ax", "cx", "dx", "si"};
        static const char* p32[] = {"eax", "ecx", "edx", "esi"};
        static const char* p64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        const char** pregs = mode == 16 ? p16 : mode == 32 ? p32 : p64;
        int pw = mode == 16 ? 16 : mode == 32 ? 32 : 64;
        int nbind = param_count < maxp ? param_count : maxp;
        for (int i = 0; i < nbind; i++) {
            AstNode* par = n->children[i];
            if (!par) continue;
            if (par->name && symtab_redeclared(cg->symtab, par->name)) {
                diag_error(par->line, par->col, "redeclaration of '%s' in the same block", par->name);
                continue;
            }
            if (!par->is_pointer && par->value && typetab_lookup(cg->types, par->value)) {
                diag_error(par->line, par->col, "a naked parameter must be an integer or a pointer");
                continue;
            }
            symtab_add_hw(cg->symtab, par->name, pregs[i], pw, par->is_pointer, par->value);
        }
    } else {
        int nreg = param_count < 6 ? param_count : 6;
        for (int i = 0; i < nreg; i++)
            bind_param(cg, n->children[i], regs[i], 0, 0);
        for (int i = 6; i < param_count && i < 16; i++)
            bind_param(cg, n->children[i], NULL, 16 + (i - 6) * 8, 1);
    }

    AstNode* body = n->children[param_count];
    EarlyRet early_body = match_early_ret(cg, n);
    int body_from = early_body.ok ? 1 : 0;
    if (early_body.ok && g_cur_fn &&
        !(g_cur_fn->struct_type && !strcmp(g_cur_fn->struct_type, "__array__"))) {
        AstNode* ret = body->children[0]->children[1]->children[0];
        VTy dst_ty = ty_decl(cg, 1, g_cur_fn->is_pointer, g_cur_fn->value);
        check_types(ret->children[0], "return value", dst_ty, expr_ty(cg, ret->children[0]));
    }
    for (int i = body_from; i < body->child_count; i++) {
        int sunk;
        g_prev2_stmt = i > 1 ? body->children[i - 2] : NULL;
        g_prev_stmt = i > 0 ? body->children[i - 1] : NULL;
        sunk = try_sink_masks(cg, body, i);
        if (sunk >= 2) {
            i += sunk - 1;
            continue;
        }
        sunk = try_insert_shift(cg, body, i);
        if (sunk >= 2) {
            i += sunk - 1;
            continue;
        }
        if (!try_hosted_rewrite(cg, body->children[i]))
            gen_stmt(cg, body->children[i]);
    }
    g_prev_stmt = NULL;
    g_prev2_stmt = NULL;
    symtab_leave(cg->symtab);

    if (is_int) {
        static const char* saved[] = {
            "rax", "rcx", "rdx", "rbx", "rbp", "rsi", "rdi",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
        };
        emit(cg, ".Lret_%s:\n", n->name);
        if (frame_ptr) {
            // Re-read the parameter. A store to it selects another saved
            // frame; stores through it change rip/rsp before iretq.
            emit(cg, "    mov rax, [rbp%d]\n", frame_slot);
            emit(cg, "    lea rsp, [rax-120]\n");
            for (int i = 14; i >= 0; i--) emit(cg, "    pop %s\n", saved[i]);
            if (frame_err) emit(cg, "    add rsp, 8\n");
        } else {
            emit(cg, "    mov rsp, rbp\n");
            if (has_err) emit(cg, "    pop rax\n");
            for (int i = 14; i >= 0; i--) emit(cg, "    pop %s\n", saved[i]);
            if (has_err) emit(cg, "    add rsp, 8\n");  // drop the CPU error code
        }
        // RIP, CS, RFLAGS, RSP, SS are still below the saved GPRs. iretq
        // returns to the ring in that frame, including ring 3 when CS.RPL is 3.
        emit(cg, "    iretq\n");
    } else if (is_sys) {
        emit(cg, "    xor rax, rax\n");
        emit(cg, ".Lret_%s:\n", n->name);
        emit(cg, "    leave\n");
        if (param_count >= 7) {
            // Spacer and a6 sit above the saved r11. Drop them without
            // popping into rax: rax is the value sysret returns to ring 3.
            emit(cg, "    add rsp, 16\n");
        }
        emit(cg, "    pop r11\n");
        emit(cg, "    pop rcx\n");
        emit(cg, "    pop rsp\n");
        emit(cg, "    o64 sysret\n");
    } else if (naked) {
        // Scalars and pointers are labeled slots. Anything still on the stack
        // is an array or a struct, which this mode does not lay out.
        if (cg->symtab->stack_size != g_naked_slot_bytes)
            diag_error(n->line, n->col, "a naked function cannot have local variables");
        emit(cg, "    ret\n");
    } else {
        emit(cg, "    xor rax, rax\n");
        emit_hosted_epilogue(cg);
        EarlyRet early_tail = match_early_ret(cg, n);
        if (early_tail.ok) {
            emit(cg, ".Lbase_%s:\n", n->name);
            emit(cg, "    mov rax, %s\n    ret\n", early_tail.ret_reg);
        }
    }
    cold_flush(cg);
    g_reg_local_n = 0;
    g_reg_callee_n = 0;
    g_slim = 0;
    g_expr_tmp = NULL;
    g_proven_idx = NULL;
    g_enclosing_while = NULL;
    g_enclosing_prev = NULL;
    g_nonneg_reg = NULL;
    g_fwd_ok = 0;
    g_fwd_hold = 0;

    // All locals (including nested-scope ones) are now known.
    if (!naked) {
        int stack_size = cg->symtab->stack_size + 64;
        stack_size = ((stack_size + 15) / 16) * 16;   // keep rsp 16-byte aligned
        emit(cg, "__frame_%s equ %d\n", n->name, stack_size);
    }
    for (int i = 0; i < g_naked_slot_n; i++) {
        if (legacy_bits()) emit(cg, "%s: dd 0\n", g_naked_slots[i]);
        else emit(cg, "%s: dq 0\n", g_naked_slots[i]);
        free(g_naked_slots[i]);
        g_naked_slots[i] = NULL;
    }
    g_naked_slot_n = 0;

    if (bits && bits != 64) emit(cg, "bits 64\n");

    g_fn_flags = 0;
    g_fn_bits = 0;
    g_fn_name[0] = '\0';
    cg->symtab = old_symtab;
}

void gen_helpers(Codegen* cg) {
    emit(cg, "\n__print_int:\n");
    emit(cg, "    push rbp\n    mov rbp, rsp\n");
    emit(cg, "    sub rsp, 32\n");

    emit(cg, "    mov rbx, rax\n");
    emit(cg, "    test rbx, rbx\n");
    emit(cg, "    jns .positive\n");
    emit(cg, "    neg rbx\n");
    emit(cg, "    push rbx\n");
    emit(cg, "    mov byte [rbp-1], 45\n");
    emit(cg, "    lea rsi, [rbp-1]\n");
    emit(cg, "    mov rdi, 1\n    mov rdx, 1\n    mov rax, 1\n    syscall\n");
    emit(cg, "    pop rbx\n");

    emit(cg, ".positive:\n");
    emit(cg, "    lea rdi, [rbp-32]\n");
    emit(cg, "    mov rax, rbx\n");
    emit(cg, "    mov rcx, 10\n");

    emit(cg, ".loop:\n");
    emit(cg, "    xor rdx, rdx\n");
    emit(cg, "    div rcx\n");
    emit(cg, "    add dl, 48\n");
    emit(cg, "    mov [rdi], dl\n");
    emit(cg, "    inc rdi\n");
    emit(cg, "    test rax, rax\n");
    emit(cg, "    jnz .loop\n");

    emit(cg, "    mov r8, rdi\n");
    emit(cg, "    dec rdi\n");
    emit(cg, ".print_loop:\n");
    emit(cg, "    lea rax, [rbp-32]\n");
    emit(cg, "    cmp rdi, rax\n");
    emit(cg, "    jl .done\n");
    emit(cg, "    push rdi\n");
    emit(cg, "    mov rsi, rdi\n");
    emit(cg, "    mov rdi, 1\n    mov rdx, 1\n    mov rax, 1\n    syscall\n");
    emit(cg, "    pop rdi\n");
    emit(cg, "    dec rdi\n");
    emit(cg, "    jmp .print_loop\n");

    emit(cg, ".done:\n");
    emit(cg, "    leave\n    ret\n");

    // __strcmp: Compare two strings
    // Input: rdi = s1, rsi = s2
    // Output: rax = 0 if equal, -1 if s1 < s2, 1 if s1 > s2
    emit(cg, "\n__strcmp:\n");
    emit(cg, "    push rbp\n    mov rbp, rsp\n");
    emit(cg, ".strcmp_loop:\n");
    emit(cg, "    mov al, [rdi]\n");
    emit(cg, "    mov bl, [rsi]\n");
    emit(cg, "    cmp al, bl\n");
    emit(cg, "    jne .strcmp_diff\n");
    emit(cg, "    test al, al\n");
    emit(cg, "    jz .strcmp_equal\n");
    emit(cg, "    inc rdi\n");
    emit(cg, "    inc rsi\n");
    emit(cg, "    jmp .strcmp_loop\n");
    emit(cg, ".strcmp_equal:\n");
    emit(cg, "    xor rax, rax\n");
    emit(cg, "    leave\n    ret\n");
    emit(cg, ".strcmp_diff:\n");
    emit(cg, "    movzx rax, al\n");
    emit(cg, "    movzx rbx, bl\n");
    emit(cg, "    sub rax, rbx\n");
    emit(cg, "    leave\n    ret\n");

    // __strcpy: Copy string from src to dest
    // Input: rdi = dest, rsi = src
    // Output: rax = dest
    emit(cg, "\n__strcpy:\n");
    emit(cg, "    push rbp\n    mov rbp, rsp\n");
    emit(cg, "    mov rax, rdi\n");  // Save dest for return
    emit(cg, ".strcpy_loop:\n");
    emit(cg, "    mov bl, [rsi]\n");
    emit(cg, "    mov [rdi], bl\n");
    emit(cg, "    test bl, bl\n");
    emit(cg, "    jz .strcpy_done\n");
    emit(cg, "    inc rdi\n");
    emit(cg, "    inc rsi\n");
    emit(cg, "    jmp .strcpy_loop\n");
    emit(cg, ".strcpy_done:\n");
    emit(cg, "    leave\n    ret\n");

    // __strlen: Get string length
    // Input: rdi = s
    // Output: rax = length
    emit(cg, "\n__strlen:\n");
    emit(cg, "    push rbp\n    mov rbp, rsp\n");
    emit(cg, "    xor rax, rax\n");
    emit(cg, ".strlen_loop:\n");
    emit(cg, "    cmp byte [rdi+rax], 0\n");
    emit(cg, "    jz .strlen_done\n");
    emit(cg, "    inc rax\n");
    emit(cg, "    jmp .strlen_loop\n");
    emit(cg, ".strlen_done:\n");
    emit(cg, "    leave\n    ret\n");
}

void build_type_table(TypeTable* tt, AstNode* ast) {
    for (int i = 0; i < ast->child_count; i++) {
        if (ast->children[i]->type == AST_STRUCT_DEF) {
            AstNode* struct_def = ast->children[i];
            typetab_add(tt, struct_def->name);
            {
                StructType* added = typetab_lookup(tt, struct_def->name);
                if (added) {
                    added->is_packed = (struct_def->flags & AF_PACKED) ? 1 : 0;
                    added->explicit_align = struct_def->align;
                }
            }

            for (int j = 0; j < struct_def->child_count; j++) {
                AstNode* field = struct_def->children[j];
                // field->name = field name
                // field->value = base type (e.g., "i64", "Token", "*u8" for [*u8; N])
                // field->is_pointer = 1 if the field itself is a pointer
                // field->struct_type == "__array__" keeps [T; N]
                char* field_type = field->value;
                int is_ptr = field->is_pointer;
                int is_array = field->struct_type && !strcmp(field->struct_type, "__array__");
                int array_len = is_array ? (field->array_size > 0 ? field->array_size : -1) : 0;

                // Build full type name for pointer types (e.g., "*Token")
                char* full_type = NULL;
                if (is_ptr && field_type && !is_array) {
                    full_type = malloc(strlen(field_type) + 2);
                    sprintf(full_type, "*%s", field_type);
                } else if (field_type) {
                    full_type = strdup(field_type);
                }

                if (field->file) focus_file(field->file);
                typetab_add_field(tt, struct_def->name, field->name, full_type, is_ptr,
                                  (field->flags & AF_VOLATILE) ? 1 : 0, array_len,
                                  field->align, field->line, field->col);

                if (full_type) free(full_type);
            }
            {
                StructType* done = typetab_lookup(tt, struct_def->name);
                typetab_finish(done);
            }
        }
    }
}

// A call to section_start/section_end is compiled inside gen_func, which is
// too late to put __text_start before the first byte. Mark the bounds now.
static void note_section_calls(AstNode* n) {
    if (!n) return;
    if (n->type == AST_CALL && n->name &&
        (!strcmp(n->name, "section_start") || !strcmp(n->name, "section_end")) &&
        n->child_count == 1 && n->children[0] && n->children[0]->type == AST_STRING) {
        const char* sn = n->children[0]->value;
        int standard = sn && (!strcmp(sn, ".text") || !strcmp(sn, ".data") || !strcmp(sn, ".bss"));
        if (sn && (section_find(sn) || standard)) {
            NamedSection* sec = section_ensure(sn);
            if (sec) { sec->need_start = 1; sec->need_end = 1; }
        }
    }
    for (int i = 0; i < n->child_count; i++) note_section_calls(n->children[i]);
}

static void put_out(Codegen* cg, FILE* out, const char* s) {
    if (out) fputs(s, out);
    else emit(cg, "%s", s);
}

static void put_outf(Codegen* cg, FILE* out, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list copy;
    va_copy(copy, ap);
    int n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0) { va_end(ap); return; }
    char small[512];
    char* buf = small;
    if (n >= (int)sizeof small) buf = malloc((size_t)n + 1);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    put_out(cg, out, buf);
    if (buf != small) free(buf);
}

// Same bytes the .data/.bss file pass has always written. `out` NULL appends
// to the code buffer, which is where a named section other than .data/.bss
// (and every section of a boot image) has to live.
static void write_global_align(Codegen* cg, FILE* out, GlobalVar* gv) {
    int want = gv->align;
    StructType* st = NULL;
    if (!gv->is_pointer && gv->type_name)
        st = typetab_lookup(cg->types, gv->is_array && gv->elem_type ? gv->elem_type : gv->type_name);
    if (st && st->align > want) want = st->align;
    // Ordinary scalars and structs are already at most 8. Emitting align 8
    // in front of every global would pad flat images. align(N) and a type
    // stricter than 8 (a cache line) are the ones that move the counter.
    if (gv->align > 1 || want > 8) {
        int bss = !gv->is_initialized && (!gv->section || !strcmp(gv->section, ".bss"));
        // `align N` in a nobits section warns: the default fill initializes.
        // resb is the uninitialized pad. A progbits section uses a zero byte.
        if (want > 1) put_outf(cg, out, bss ? "align %d, resb 1\n" : "align %d, db 0\n", want);
    }
}

static void write_global_def(Codegen* cg, FILE* out, GlobalVar* gv) {
    if (!gv || gv->is_extern) return;
    write_global_align(cg, out, gv);
    if (gv->is_initialized) {
        if (gv->is_array && gv->array_init_values && gv->array_init_count > 0) {
            const char* directive = type_asm_directive(gv->elem_type);
            put_outf(cg, out, "%s: %s ", gv->name, directive);
            int total = (gv->array_count > gv->array_init_count) ? gv->array_count : gv->array_init_count;
            for (int j = 0; j < total; j++) {
                if (j < gv->array_init_count) put_out(cg, out, gv->array_init_values[j]);
                else put_out(cg, out, "0");
                if (j < total - 1) put_out(cg, out, ", ");
            }
            put_out(cg, out, "\n");
        } else if (gv->init_value) {
            put_outf(cg, out, "%s: dq %s\n", gv->name, gv->init_value);
        }
        return;
    }
    if (gv->is_array) {
        int elem_size = type_size(gv->elem_type);
        StructType* gst = gv->elem_type ? typetab_lookup(cg->types, gv->elem_type) : NULL;
        if (gst && gst->size > 0) {
            put_outf(cg, out, "%s: resb %d\n", gv->name, gv->array_count * gst->size);
            return;
        }
        const char* directive = "resq";
        if (elem_size == 1) directive = "resb";
        else if (elem_size == 2) directive = "resw";
        else if (elem_size == 4) directive = "resd";
        put_outf(cg, out, "%s: %s %d\n", gv->name, directive, gv->array_count);
        return;
    }
    StructType* gst = (gv->type_name && !gv->is_pointer)
        ? typetab_lookup(cg->types, gv->type_name) : NULL;
    if (gst && gst->size > 0) {
        put_outf(cg, out, "%s: resb %d\n", gv->name, gst->size);
        return;
    }
    put_outf(cg, out, "%s: resq 1\n", gv->name);
}

// .data and .bss of a non-boot image are written in the file trailer, so their
// start labels belong there (before the bytes). Everything else gets its
// start label at the first switch. align=1 keeps a named section packed;
// the default .text/.data/.bss lines stay exactly as they were.
// vstart is a nasm bin attribute. -c is nasm -f elf64, which warns and
// ignores it, so a relocatable object never asks for a load address.
static int section_bin_addr(const NamedSection* sec) {
    return sec && sec->has_addr && !g_reloc;
}

static int func_is_defined(AstNode* ast, const char* name) {
    if (!ast || !name) return 0;
    for (int i = 0; i < ast->child_count; i++) {
        AstNode* fn = ast->children[i];
        if (fn->type == AST_FUNCTION && fn->name && !fn->is_forward_decl &&
            !strcmp(fn->name, name))
            return 1;
    }
    return 0;
}

static int ast_calls(AstNode* n, const char* name) {
    if (!n || !name) return 0;
    if (n->type == AST_CALL && n->name && !strcmp(n->name, name)) return 1;
    for (int i = 0; i < n->child_count; i++)
        if (ast_calls(n->children[i], name)) return 1;
    return 0;
}

static void emit_once(Codegen* cg, const char* dir, const char* name,
                      const char** seen, int* nseen) {
    if (!name || !name[0]) return;
    for (int i = 0; i < *nseen; i++)
        if (!strcmp(seen[i], name)) return;
    emit(cg, "%s %s\n", dir, name);
    if (*nseen < 512) seen[(*nseen)++] = name;
}

// -c: definitions are NASM `global`, declarations are `extern`. One
// invocation is still one object. The linker sees the symbols.
static void emit_link_syms(Codegen* cg, AstNode* ast, GlobalSymbolTable* gst) {
    if (!g_reloc || !ast) return;
    const char* seen[512];
    int nseen = 0;
    int any = 0;
    for (int i = 0; i < ast->child_count; i++) {
        AstNode* fn = ast->children[i];
        if (fn->type != AST_FUNCTION || !fn->name || !(fn->flags & AF_EXTERN)) continue;
        if (func_is_defined(ast, fn->name)) continue;
        emit_once(cg, "extern", fn->name, seen, &nseen);
        any = 1;
    }
    if (gst) {
        for (int i = 0; i < gst->count; i++) {
            GlobalVar* gv = &gst->vars[i];
            if (!gv->is_extern || !gv->name) continue;
            if (!strcmp(gv->name, "__syscall_rsp") || !strcmp(gv->name, "__syscall_user_rsp"))
                continue;
            int defined = 0;
            for (int j = 0; j < ast->child_count; j++) {
                AstNode* d = ast->children[j];
                if (d->type == AST_DATA && d->name && !strcmp(d->name, gv->name)) defined = 1;
            }
            if (defined) continue;
            emit_once(cg, "extern", gv->name, seen, &nseen);
            any = 1;
        }
    }
    // Hosted helpers are defined in the object that has fn main. A library
    // object that calls them names the symbol; an unused extern is dropped
    // by nasm and does not become an undefined link requirement.
    if (!g_freestanding && !func_is_defined(ast, "main")) {
        if (ast_calls(ast, "print_int")) {
            emit_once(cg, "extern", "__print_int", seen, &nseen);
            any = 1;
        }
        if (ast_calls(ast, "strcmp")) {
            emit_once(cg, "extern", "__strcmp", seen, &nseen);
            any = 1;
        }
        if (ast_calls(ast, "strcpy")) {
            emit_once(cg, "extern", "__strcpy", seen, &nseen);
            any = 1;
        }
        if (ast_calls(ast, "strlen") || ast_calls(ast, "print") || ast_calls(ast, "println")) {
            emit_once(cg, "extern", "__strlen", seen, &nseen);
            any = 1;
        }
    }
    for (int i = 0; i < ast->child_count; i++) {
        AstNode* item = ast->children[i];
        if (item->type == AST_FUNCTION && item->name && !item->is_forward_decl) {
            emit_once(cg, "global", item->name, seen, &nseen);
            any = 1;
        } else if (item->type == AST_DATA && item->name) {
            emit_once(cg, "global", item->name, seen, &nseen);
            any = 1;
        }
    }
    if (gst) {
        for (int i = 0; i < gst->count; i++) {
            GlobalVar* gv = &gst->vars[i];
            if (!gv->name || gv->is_extern) continue;
            emit_once(cg, "global", gv->name, seen, &nseen);
            any = 1;
        }
    }
    if (!g_freestanding && func_is_defined(ast, "main")) {
        emit_once(cg, "global", "__print_int", seen, &nseen);
        emit_once(cg, "global", "__strcmp", seen, &nseen);
        emit_once(cg, "global", "__strcpy", seen, &nseen);
        emit_once(cg, "global", "__strlen", seen, &nseen);
        any = 1;
    }
    if (any) emit(cg, "\n");
}

static void emit_section_switch(Codegen* cg, const char* name) {
    const char* want = (name && name[0]) ? name : ".text";
    if (g_emit_section && !strcmp(g_emit_section, want)) return;
    NamedSection* sec = section_find(want);
    if (!strcmp(want, ".bss")) g_bss_tail = 1;
    int standard = !strcmp(want, ".text") || !strcmp(want, ".data") || !strcmp(want, ".bss");
    if (section_bin_addr(sec))
        emit(cg, "\nsection %s vstart=0x%llx align=1\n", want, sec->addr);
    else if (!standard)
        emit(cg, "\nsection %s align=1\n", want);
    else
        emit(cg, "\nsection %s\n", want);
    g_emit_section = (sec && sec->name) ? sec->name : want;
    if (sec && sec->need_start && !sec->started && (!standard || g_boot)) {
        emit(cg, "%s_start:\n", sec->sym);
        sec->started = 1;
    }
}

static int global_in_code(GlobalVar* gv) {
    if (!gv->section || gv->is_extern) return 0;
    if (g_boot) return 1;
    if (!strcmp(gv->section, ".data") && gv->is_initialized) return 0;
    if (!strcmp(gv->section, ".bss") && !gv->is_initialized) return 0;
    return 1;
}

static void emit_bound_ends(Codegen* cg) {
    for (int i = 0; i < g_nnamed; i++) {
        NamedSection* s = &g_named_secs[i];
        if (!s->need_end || s->ended) continue;
        // The file trailer owns .bss when no later code appends to it.
        // Boot has no trailer: the whole image is the code buffer.
        if (!g_boot && !strcmp(s->name, ".bss") && !g_bss_tail) continue;
        emit_section_switch(cg, s->name);
        emit(cg, "%s_end:\n", s->sym);
        s->ended = 1;
    }
}

void codegen(AstNode* ast, const char* file, StringTable* strtab, TypeTable* types) {
    Codegen cg;
    cg.out = NULL;
    cg.label_count = 0;
    cg.symtab = NULL;
    cg.strtab = strtab;
    cg.types = types;
    cg.code_buf = NULL;
    cg.code_len = 0;
    cg.code_cap = 0;
    cg.program = ast;
    cg.need_syscall_data = 0;

    // Initialize global symbol table
    GlobalSymbolTable global_symtab;
    global_symtab_init(&global_symtab);
    cg.global_symtab = &global_symtab;

    // First pass: process global variables
    for (int i = 0; i < ast->child_count; i++) {
        if (ast->children[i]->type == AST_GLOBAL_VAR) {
            AstNode* gvar = ast->children[i];
            char* type_name = gvar->value;  // Base type
            int is_initialized = (gvar->child_count > 0);
            char* init_value = NULL;

            // Extract type info from AST
            int is_array = (gvar->struct_type && !strcmp(gvar->struct_type, "__array__"));
            int array_count = gvar->array_size;
            int is_pointer = (gvar->is_pointer & 1);
            int is_mutable = (gvar->is_pointer & 2) >> 1;

            // If initialized, get the value
            if (is_initialized && gvar->children[0]->type == AST_NUMBER) {
                init_value = gvar->children[0]->value;
            }

            if ((gvar->flags & AF_VOLATILE) && is_array) {
                diag_error(gvar->line, gvar->col,
                           "volatile cannot apply to an array; use a '*volatile T' pointer or a volatile field");
            } else if ((gvar->flags & AF_VOLATILE) && !is_pointer && type_name &&
                       typetab_lookup(types, type_name)) {
                diag_error(gvar->line, gvar->col,
                           "volatile cannot apply to struct '%s'; mark fields volatile or use '*volatile %s'",
                           type_name, type_name);
            }
            global_symtab_add_full(&global_symtab, gvar->name, type_name, 0, is_initialized, init_value,
                                   is_array, array_count, is_pointer, is_mutable);
            if (is_initialized && !is_array && gvar->child_count > 0) {
                VTy dst_ty = ty_decl(&cg, 0, gvar->is_pointer, gvar->value);
                check_types(gvar->children[0], "", dst_ty, expr_ty(&cg, gvar->children[0]));
            }
            {
                GlobalVar* added = global_symtab_lookup(&global_symtab, gvar->name);
                if (added && (gvar->flags & AF_VOLATILE))
                    added->is_volatile = 1;
                if (added && (gvar->flags & AF_EXTERN)) {
                    added->is_extern = 1;
                    // These two are defined next to set_syscall_stack. Mentioning
                    // them is enough to emit that definition.
                    if (gvar->name &&
                        (!strcmp(gvar->name, "__syscall_rsp") ||
                         !strcmp(gvar->name, "__syscall_user_rsp")))
                        cg.need_syscall_data = 1;
                }
                if (added && (gvar->is_pointer & 4))
                    added->volatile_pointee = 1;
                if (added) added->section = gvar->section;
                if (added) added->align = gvar->align;
            }

            // If array initialization, extract values
            if (is_initialized && gvar->children[0]->type == AST_ARRAY_LITERAL) {
                AstNode* arr_lit = gvar->children[0];
                GlobalVar* gv = global_symtab_lookup(&global_symtab, gvar->name);
                if (gv) {
                    gv->array_init_count = arr_lit->child_count;
                    gv->array_init_values = malloc(sizeof(char*) * arr_lit->child_count);
                    for (int j = 0; j < arr_lit->child_count; j++) {
                        if (arr_lit->children[j]->type == AST_NUMBER) {
                            gv->array_init_values[j] = strdup(arr_lit->children[j]->value);
                        } else {
                            gv->array_init_values[j] = strdup("0");  // Default
                        }
                    }
                }
            }
            // If string literal initialization for byte array
            else if (is_initialized && gvar->children[0]->type == AST_STRING) {
                AstNode* str_lit = gvar->children[0];
                GlobalVar* gv = global_symtab_lookup(&global_symtab, gvar->name);
                if (gv && is_array) {
                    // Convert string to array of bytes
                    char* str_val = str_lit->value;
                    int str_len = strlen(str_val);
                    gv->array_init_count = str_len + 1;  // Include null terminator
                    gv->array_init_values = malloc(sizeof(char*) * (str_len + 1));
                    for (int j = 0; j < str_len; j++) {
                        char buf[8];
                        sprintf(buf, "%d", (unsigned char)str_val[j]);
                        gv->array_init_values[j] = strdup(buf);
                    }
                    gv->array_init_values[str_len] = strdup("0");  // Null terminator
                }
            }
        }
    }

    if (g_freestanding) note_section_calls(ast);
    emit_link_syms(&cg, ast, &global_symtab);
    NamedSection* text_sec = section_find(".text");
    // -c emits _start only in the hosted object that defines fn main.
    // A library object has no entry and no copy of the hosted helpers.
    int emit_entry = !g_freestanding && (!g_reloc || func_is_defined(ast, "main"));
    if (g_freestanding) {
        // No entry point and no runtime: the embedding code (e.g. a kernel's
        // asm entry) calls our functions with the System V calling convention.
        // `@` on ".text" is NASM vstart (the labels' load address). It is not
        // `start=`: that would insert a hole and, at 0x100000, a megabyte of zeros.
        // -c does not emit vstart: nasm -f elf64 would ignore it.
        if (section_bin_addr(text_sec))
            emit(&cg, "\nsection .text vstart=0x%llx align=1\n", text_sec->addr);
        else
            emit(&cg, "\nsection .text\n");
        if (text_sec && text_sec->need_start && !text_sec->started) {
            emit(&cg, "%s_start:\n", text_sec->sym);
            text_sec->started = 1;
        }
        emit(&cg, "\n");
    } else if (emit_entry) {
        emit(&cg, "\nsection .text\n    global _start\n\n");
        emit(&cg, "_start:\n    call main\n    mov rdi, rax\n");
        emit(&cg, "    mov rax, 60\n    syscall\n");
        gen_helpers(&cg);
    } else {
        emit(&cg, "\nsection .text\n");
    }
    g_emit_section = ".text";

    // --flat: the image starts executing at its first byte, so _start goes first.
    // A named section around _start is opened before it, so the entry is still
    // byte 0 of that section. A let or data block written before _start is
    // still emitted after it.
    AstNode* flat_entry = NULL;
    if (g_flat_org) {
        for (int i = 0; i < ast->child_count; i++)
            if (ast->children[i]->type == AST_FUNCTION && !ast->children[i]->is_forward_decl &&
                !strcmp(ast->children[i]->name, "_start")) flat_entry = ast->children[i];
        if (!flat_entry) diag_error(1, 1, g_boot
            ? "--boot needs a function named _start (the entry point)"
            : "--flat needs a function named _start (the entry point)");
        else if (g_boot && (flat_entry->bits != 16 || !(flat_entry->flags & AF_NAKED)))
            diag_error(flat_entry->line, flat_entry->col,
                       "--boot _start must be 'bits 16 naked fn' (real mode, no 64-bit frame)");
        else {
            if (flat_entry->section) emit_section_switch(&cg, flat_entry->section);
            gen_func(&cg, flat_entry);
        }
    }
    for (int i = 0; i < ast->child_count; i++) {
        AstNode* item = ast->children[i];
        if (item->type == AST_FUNCTION) {
            // Skip forward declarations, only generate code for full definitions
            if (!item->is_forward_decl && item != flat_entry) {
                emit_section_switch(&cg, item->section);
                gen_func(&cg, item);
            }
        } else if (item->type == AST_DATA) {
            AstNode* d = item;
            emit_section_switch(&cg, d->section);
            emit(&cg, "\n%s:\n", d->name);
            for (int j = 0; j < d->child_count; j++) {
                AstNode* it = d->children[j];
                if (!it || !it->name) continue;
                if (!strcmp(it->name, "label")) emit(&cg, "%s:\n", it->value);
                else if (!strcmp(it->name, "align")) emit(&cg, "align %s, db 0\n", it->value);
                else if (!strcmp(it->name, "times")) emit(&cg, "times %s\n", it->value);
                else emit(&cg, "    %s %s\n", it->name, it->value ? it->value : "0");
            }
        } else if (item->type == AST_GLOBAL_VAR && item->section) {
            GlobalVar* gv = global_symtab_lookup(&global_symtab, item->name);
            if (gv && global_in_code(gv)) {
                emit_section_switch(&cg, gv->section);
                write_global_def(&cg, NULL, gv);
            }
        }
    }
    emit_bound_ends(&cg);
    // The pad is in the section that holds _start, after that section's end
    // label, so the measured span excludes the signature. A boot image that
    // switches sections after _start pads whichever section is current at
    // the end only if _start itself has no section; with a section, the pad
    // returns to _start's section. Bytes in any other section still count
    // toward the file, so a multi-section sector can grow past 512.
    if (g_boot) {
        if (flat_entry && flat_entry->section)
            emit_section_switch(&cg, flat_entry->section);
        emit(&cg, "\ntimes 510-($-$$) db 0\ndw 0xAA55\n");
    }

    cg.out = fopen(file, "w");
    fprintf(cg.out, "; Generated by " LANG_NAME " " LANG_VERSION "%s\n\n",
            g_freestanding ? " (freestanding)" : "");
    if (g_boot) {
        // Real mode starts at the first byte. Do not emit `bits 64` or a
        // .data/.bss section in front of _start: that would move the entry
        // off byte 0, or assemble the prologue in the wrong mode.
        fprintf(cg.out, "org 0x7C00\n%s", cg.code_buf);
        fclose(cg.out);
        free(cg.code_buf);
        free(global_symtab.vars);
        return;
    }
    if (g_flat_org) fprintf(cg.out, "bits 64\norg %s\n\n", g_flat_org);
    NamedSection* data_sec = section_find(".data");
    if (section_bin_addr(data_sec))
        fprintf(cg.out, "section .data vstart=0x%llx align=1\n", data_sec->addr);
    else
        fprintf(cg.out, "section .data\n");
    if (data_sec && data_sec->need_start && !data_sec->started) {
        fprintf(cg.out, "%s_start:\n", data_sec->sym);
        data_sec->started = 1;
    }
    if (cg.need_syscall_data) {
        fprintf(cg.out, "__syscall_rsp: dq 0\n__syscall_user_rsp: dq 0\n");
    }

    // Emit string literals
    for (int i = 0; i < strtab->count; i++) {
        fprintf(cg.out, "%s: db ", strtab->strings[i].label);
        for (int j = 0; j < strtab->strings[i].len; j++) {
            fprintf(cg.out, "%d", (unsigned char)strtab->strings[i].value[j]);
            fprintf(cg.out, ", ");
        }
        fprintf(cg.out, "0\n");  // Null terminator
    }

    // Emit initialized global variables. A let placed in another section is
    // in the code buffer; only the default .data (and an explicit ".data")
    // stays in this block, in front of __data_end.
    for (int i = 0; i < global_symtab.count; i++) {
        GlobalVar* gv = &global_symtab.vars[i];
        if (gv->is_extern || !gv->is_initialized) continue;
        if (gv->section && strcmp(gv->section, ".data")) continue;
        write_global_def(&cg, cg.out, gv);
    }

    // Emit uninitialized global variables (.bss section). Flat mode always
    // publishes __bss_start/__bss_end, which bss_start()/bss_end() load.
    // section_start(".bss") reuses those names. Only the section named .bss
    // is nobits: nasm omits it from a flat binary. resq in any other section
    // is progbits and occupies zeros.
    int has_bss = 0;
    NamedSection* bss_sec = section_find(".bss");
    if (g_flat_org) {
        if (section_bin_addr(bss_sec))
            fprintf(cg.out, "\nsection .bss vstart=0x%llx align=1\n__bss_start:\n", bss_sec->addr);
        else
            fprintf(cg.out, "\nsection .bss\n__bss_start:\n");
        has_bss = 1;
        if (bss_sec) bss_sec->started = 1;
    } else if (bss_sec && bss_sec->need_start && !bss_sec->started) {
        if (section_bin_addr(bss_sec))
            fprintf(cg.out, "\nsection .bss vstart=0x%llx align=1\n", bss_sec->addr);
        else
            fprintf(cg.out, "\nsection .bss\n");
        fprintf(cg.out, "%s_start:\n", bss_sec->sym);
        bss_sec->started = 1;
        has_bss = 1;
    }
    for (int i = 0; i < global_symtab.count; i++) {
        GlobalVar* gv = &global_symtab.vars[i];
        if (gv->is_extern || gv->is_initialized) continue;
        if (gv->section && strcmp(gv->section, ".bss")) continue;
        if (!has_bss) {
            fprintf(cg.out, "\nsection .bss\n");
            has_bss = 1;
        }
        write_global_def(&cg, cg.out, gv);
    }

    if (g_flat_org && !g_bss_tail) {
        if (section_bin_addr(bss_sec))
            fprintf(cg.out, "\nsection .bss vstart=0x%llx align=1\n%s_end:\n", bss_sec->addr, bss_sec->sym);
        else
            fprintf(cg.out, "\nsection .bss\n__bss_end:\n");
        if (bss_sec) bss_sec->ended = 1;
    } else if (!g_bss_tail && bss_sec && bss_sec->need_end && !bss_sec->ended) {
        if (!has_bss) {
            if (section_bin_addr(bss_sec))
                fprintf(cg.out, "\nsection .bss vstart=0x%llx align=1\n", bss_sec->addr);
            else
                fprintf(cg.out, "\nsection .bss\n");
        }
        fprintf(cg.out, "%s_end:\n", bss_sec->sym);
        bss_sec->ended = 1;
    }
    fprintf(cg.out, "%s", cg.code_buf);

    fclose(cg.out);
    free(cg.code_buf);
    free(global_symtab.vars);
}

static void usage(FILE* o) {
    fprintf(o,
        "Usage: " COMPILER_BIN " [options] <file" SRC_EXT "> [file" SRC_EXT " ...]\n"
        "\n"
        "Options:\n"
        "  -O0 | -O1 | -O2     optimization level (default -O0)\n"
        "  -o <file>           output executable (default ./a.out),\n"
        "                      assembly file with -S / --freestanding,\n"
        "                      or relocatable object with -c (default output.o)\n"
        "  -S                  only generate NASM assembly, do not assemble or link\n"
        "  -c                  one ELF64 relocatable object (nasm -f elf64), no ld;\n"
        "                      global definitions, extern declarations, no vstart\n"
        "  --freestanding      no OS: no _start, no syscalls, no hosted builtins;\n"
        "                      emits assembly to be included by your own entry code (implies -S)\n"
        "  --flat <addr>       freestanding flat binary (nasm -f bin) loaded at <addr>;\n"
        "                      execution starts at fn _start, bss_start()/bss_end() give .bss bounds\n"
        "  --boot              boot sector: --flat 0x7C00, pad to 512 bytes, signature 0xAA55;\n"
        "                      _start must be 'bits 16 naked fn'\n"
        "  -v, --verbose       print compilation steps\n"
        "  --version           print version and exit\n"
        "  -h, --help          show this help\n");
}

static int run_cmd(const char* fmt, const char* a, const char* b) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), fmt, a, b);
    return system(cmd);
}

int main(int argc, char** argv) {
    const char* inputs[MAX_LOADED];
    int ninputs = 0;
    const char* out = NULL;
    int only_asm = 0, want_asm = 0, verbose = 0;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "-O0")) optimization_level = 0;
        else if (!strcmp(a, "-O1")) optimization_level = 1;
        else if (!strcmp(a, "-O2")) optimization_level = 2;
        else if (!strcmp(a, "-S")) want_asm = 1;
        else if (!strcmp(a, "-c")) g_reloc = 1;
        else if (!strcmp(a, "--freestanding")) g_freestanding = 1;
        else if (!strcmp(a, "--flat")) {
            if (i + 1 >= argc) { fprintf(stderr, COMPILER_BIN ": error: --flat needs a load address\n"); return 2; }
            if (g_boot) { fprintf(stderr, COMPILER_BIN ": error: --flat and --boot cannot be combined\n"); return 2; }
            g_flat_org = argv[++i]; g_freestanding = 1;
        }
        else if (!strcmp(a, "--boot")) {
            if (g_flat_org) { fprintf(stderr, COMPILER_BIN ": error: --flat and --boot cannot be combined\n"); return 2; }
            g_boot = 1; g_freestanding = 1; g_flat_org = "0x7C00";
        }
        else if (!strcmp(a, "-v") || !strcmp(a, "--verbose")) verbose = 1;
        else if (!strcmp(a, "--version")) { printf(LANG_NAME " " LANG_VERSION "\n"); return 0; }
        else if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(stdout); return 0; }
        else if (!strcmp(a, "-o")) {
            if (i + 1 >= argc) { fprintf(stderr, COMPILER_BIN ": error: -o needs a file name\n"); return 2; }
            out = argv[++i];
        }
        else if (a[0] == '-') { fprintf(stderr, COMPILER_BIN ": error: unknown option '%s'\n", a); usage(stderr); return 2; }
        else if (ninputs >= MAX_LOADED) {
            fprintf(stderr, COMPILER_BIN ": error: too many input files (maximum %d)\n", MAX_LOADED);
            return 2;
        } else inputs[ninputs++] = a;
    }
    if (g_reloc && g_flat_org) {
        fprintf(stderr, COMPILER_BIN ": error: -c cannot be combined with --flat or --boot\n");
        return 2;
    }
    // --freestanding alone still stops at assembly. -c assembles an object
    // unless the user also passed -S. --flat and --boot assemble a raw image.
    only_asm = want_asm || (g_freestanding && !g_flat_org && !g_reloc);
    if (ninputs == 0) { usage(stderr); return 2; }
    for (int i = 0; i < ninputs; i++) {
        const char* input = inputs[i];
        size_t il = strlen(input), ol = strlen(OLD_SRC_EXT), ol2 = strlen(OLDER_SRC_EXT);
        if (il > ol && !strcmp(input + il - ol, OLD_SRC_EXT))
            fprintf(stderr, "%s: warning: the '" OLD_SRC_EXT "' extension is deprecated; rename the file to '" SRC_EXT "'\n", input);
        else if (il > ol2 && !strcmp(input + il - ol2, OLDER_SRC_EXT))
            fprintf(stderr, "%s: warning: the '" OLDER_SRC_EXT "' extension is deprecated (the language was renamed from Chronos); use '" SRC_EXT "'\n", input);
    }

    const char* asm_out = only_asm ? (out ? out : "output.asm") : "output.asm";
    const char* exe_out = out ? out : (g_reloc ? "output.o" : "a.out");

    if (verbose) {
        fprintf(stderr, LANG_NAME " " LANG_VERSION ": compiling %s (-O%d%s)\n",
                inputs[0], optimization_level, g_freestanding ? ", freestanding" : "");
        for (int i = 1; i < ninputs; i++)
            fprintf(stderr, "  + %s\n", inputs[i]);
    }

    AstNode* ast = ast_new(AST_PROGRAM);
    sections_reset();
    for (int i = 0; i < ninputs; i++) {
        if (!parse_file_into(ast, inputs[i], 0, 1, 1)) {
            if (g_error_count > 0)
                fprintf(stderr, "%d error%s, compilation failed\n", g_error_count, g_error_count == 1 ? "" : "s");
            return 1;
        }
    }
    // parse_file_into restores the previous buffer. Program-level errors
    // (a missing _start) name the first file, the same as a one-file compile.
    if (g_nloaded > 0) {
        g_src_file = g_loaded[0].name;
        g_src_text = g_loaded[0].text;
    }

    TypeTable* types = typetab_new();
    build_type_table(types, ast);

    StringTable* strtab = strtab_new();
    codegen(ast, asm_out, strtab, types);

    if (g_error_count > 0) {
        fprintf(stderr, "%d error%s, compilation failed\n", g_error_count, g_error_count == 1 ? "" : "s");
        remove(asm_out);
        return 1;
    }
    if (verbose && !g_flat_org) fprintf(stderr, "  wrote %s\n", asm_out);
    if (only_asm) return 0;
    if (g_flat_org) {
        if (run_cmd("nasm -f bin '%s' -o '%s'", asm_out, exe_out) != 0) {
            fprintf(stderr, COMPILER_BIN ": error: assembler (nasm) failed\n");
            return 1;
        }
        remove(asm_out);
        if (verbose) fprintf(stderr, "  wrote %s (flat binary, org %s)\n", exe_out, g_flat_org);
        return 0;
    }
    if (g_reloc) {
        if (run_cmd("nasm -f elf64 '%s' -o '%s'", asm_out, exe_out) != 0) {
            fprintf(stderr, COMPILER_BIN ": error: assembler (nasm) failed; this is a compiler bug, please report it\n");
            return 1;
        }
        remove(asm_out);
        if (verbose) fprintf(stderr, "  wrote %s (elf64 relocatable)\n", exe_out);
        return 0;
    }

    remove("output.o");
    remove(exe_out);
    if (run_cmd("nasm -f elf64 '%s' -o output.o%s", asm_out, "") != 0) {
        fprintf(stderr, COMPILER_BIN ": error: assembler (nasm) failed; this is a compiler bug, please report it\n");
        return 1;
    }
    if (run_cmd("ld output.o -o '%s'%s", exe_out, "") != 0) {
        fprintf(stderr, COMPILER_BIN ": error: linker (ld) failed\n");
        return 1;
    }
    if (verbose) fprintf(stderr, "  linked ./%s\n", exe_out);
    return 0;
}
