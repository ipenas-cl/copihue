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
#include <math.h>

// Optimization level (0 = none, 1 = basic, 2 = aggressive)
int optimization_level = 0;

// TOKENS
typedef enum {
    T_EOF, T_IDENT, T_NUM, T_STR,
    T_FN, T_LET, T_IF, T_ELSE, T_WHILE, T_FOR, T_RET, T_STRUCT, T_MUT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_LBRACKET, T_RBRACKET,
    T_SEMI, T_COLON, T_COMMA, T_DOT, T_AMP,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_MOD,
    T_EQ, T_EQEQ, T_NEQ, T_LT, T_GT, T_LTE, T_GTE, T_ARROW,
    T_AND_AND, T_OR_OR, T_BANG,
    T_PLUSPLUS, T_MINUSMINUS, T_PLUSEQ, T_MINUSEQ, T_STAREQ, T_SLASHEQ, T_MODEQ
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
    char* type_name;      // Type of the field (e.g., "i64", "*Token")
    int is_pointer;       // 1 if pointer type, 0 otherwise
} StructField;

typedef struct StructType {
    char* name;
    StructField* fields;
    int field_count;
    int size;
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
    AST_LOGICAL  // && and || operators with short-circuit evaluation
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
    int is_pointer;  // For type tracking
    int is_forward_decl;  // For function forward declarations
} AstNode;

// Symbol table
typedef struct {
    char* name;
    int offset;
    int size;
    char* type_name;
    int is_pointer;
} Symbol;

typedef struct {
    Symbol* symbols;
    int count;
    int stack_size;
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
    char* pointee_type; // Type being pointed to
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
} Codegen;

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
}

void typetab_add_field(TypeTable* tt, char* struct_name, char* field_name, char* field_type, int is_pointer) {
    StructType* st = typetab_lookup(tt, struct_name);
    if (!st) return;

    st->field_count++;
    st->fields = safe_realloc(st->fields, sizeof(StructField) * st->field_count);
    st->fields[st->field_count - 1].name = strdup(field_name);
    st->fields[st->field_count - 1].offset = st->size;
    st->fields[st->field_count - 1].type_name = field_type ? strdup(field_type) : NULL;
    st->fields[st->field_count - 1].is_pointer = is_pointer;
    st->size += 8;
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
    return -st->stack_size;
}

Symbol* symtab_lookup_symbol(SymbolTable* st, char* name) {
    for (int i = 0; i < st->count; i++) {
        if (!strcmp(st->symbols[i].name, name)) {
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
        while (isdigit(peek(l))) adv(l);
        return (Tok){T_NUM, st, l->cur - st, tok_line, tok_col};
    }
    if (c == '"') {
        while (peek(l) && peek(l) != '"') {
            if (peek(l) == '\\' && peek_next(l)) adv(l);  // Skip escape sequence
            adv(l);
        }
        if (peek(l) != '"') {
            fprintf(stderr, "Error at line %d, col %d: Unterminated string literal\n", tok_line, tok_col);
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
    if (c == '&' && peek(l) == '&') { adv(l); return (Tok){T_AND_AND, st, 2, tok_line, tok_col}; }
    if (c == '&') return (Tok){T_AMP, st, 1, tok_line, tok_col};
    if (c == '|' && peek(l) == '|') { adv(l); return (Tok){T_OR_OR, st, 2, tok_line, tok_col}; }
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

    return (Tok){T_EOF, st, 0, tok_line, tok_col};
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
        case T_ARROW: return "'->'";
        default: return "token";
    }
}

Tok peek_tok(Parser* p) { return p->tokens[p->pos]; }
Tok advance_tok(Parser* p) { return p->tokens[p->pos++]; }
int check_tok(Parser* p, TokType t) { return peek_tok(p).t == t; }
int match_tok(Parser* p, TokType t) { if (check_tok(p, t)) { advance_tok(p); return 1; } return 0; }

void expect(Parser* p, TokType t) {
    if (!match_tok(p, t)) {
        Tok got = peek_tok(p);
        fprintf(stderr, "Parse error at line %d, col %d: expected %s, got %s\n",
                got.line, got.col, tok_name(t), tok_name(got.t));
        // Show a snippet of what we got
        if (got.len > 0 && got.len < 50) {
            fprintf(stderr, "  Got: '%.*s'\n", got.len, got.s);
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

AstNode* parse_expr(Parser* p);
AstNode* parse_stmt(Parser* p);
TypeSpec parse_type(Parser* p);

AstNode* parse_primary(Parser* p) {
    if (check_tok(p, T_NUM)) {
        Tok t = advance_tok(p);
        AstNode* n = ast_new(AST_NUMBER);
        n->value = strndup(t.s, t.len);
        return n;
    }
    if (check_tok(p, T_STR)) {
        Tok t = advance_tok(p);
        AstNode* n = ast_new(AST_STRING);
        n->value = strndup(t.s + 1, t.len - 2);
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

        // Assignment
        if (check_tok(p, T_EQ)) {
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
    fprintf(stderr, "Parse error at line %d, col %d: unexpected %s\n",
            err.line, err.col, tok_name(err.t));
    if (err.len > 0 && err.len < 50) {
        fprintf(stderr, "  Got: '%.*s'\n", err.len, err.s);
    }
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
            ast_add(deref, parse_unary(p));
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
            ast_add(deref, parse_postfix(p));  // Recursive to handle postfix ops
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

AstNode* parse_multiplicative(Parser* p) {
    AstNode* left = parse_postfix(p);
    while (check_tok(p, T_STAR) || check_tok(p, T_SLASH) || check_tok(p, T_MOD)) {
        Tok op = advance_tok(p);
        AstNode* right = parse_postfix(p);

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

AstNode* parse_comparison(Parser* p) {
    AstNode* left = parse_additive(p);
    while (check_tok(p, T_EQEQ) || check_tok(p, T_NEQ) ||
           check_tok(p, T_LT) || check_tok(p, T_GT) ||
           check_tok(p, T_LTE) || check_tok(p, T_GTE)) {
        Tok op = advance_tok(p);
        AstNode* right = parse_additive(p);
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

AstNode* parse_stmt(Parser* p) {
    if (match_tok(p, T_RET)) {
        AstNode* ret = ast_new(AST_RETURN);
        if (!check_tok(p, T_SEMI)) ast_add(ret, parse_expr(p));
        expect(p, T_SEMI);
        return ret;
    }
    if (match_tok(p, T_LET)) {
        Tok name = advance_tok(p);
        AstNode* let = ast_new(AST_LET);
        let->name = strndup(name.s, name.len);

        // Optional type annotation: let x: i32 or let arr: [i32; 10] or let ptr: *i32 or let p: Point
        if (match_tok(p, T_COLON)) {
            TypeSpec spec = parse_type(p);

            // Store type info in AST node (similar to global vars)
            let->value = spec.base_type;          // Base type name
            let->array_size = spec.array_count;   // Array count (0 if not array)
            let->is_pointer = spec.is_pointer | (spec.is_mutable << 1);  // Pointer flags

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
        expect(p, T_LPAREN);
        ast_add(ifnode, parse_expr(p));
        expect(p, T_RPAREN);
        ast_add(ifnode, parse_block(p));
        if (match_tok(p, T_ELSE)) ast_add(ifnode, parse_block(p));
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

        // Parse increment (i = i + 1)
        AstNode* inc = parse_expr(p);
        expect(p, T_RPAREN);

        // Parse body
        AstNode* body = parse_block(p);

        // Desugar to: { init; while (cond) { body; inc; } }
        AstNode* block = ast_new(AST_BLOCK);
        ast_add(block, init);

        AstNode* whilenode = ast_new(AST_WHILE);
        ast_add(whilenode, cond);

        // Create new body block with original body + increment
        AstNode* while_body = ast_new(AST_BLOCK);
        // Copy all statements from body
        for (int i = 0; i < body->child_count; i++) {
            ast_add(while_body, body->children[i]);
        }
        // Add increment as expression statement
        AstNode* inc_stmt = ast_new(AST_BLOCK);  // Wrap in statement node
        ast_add(inc_stmt, inc);
        ast_add(while_body, inc_stmt);

        ast_add(whilenode, while_body);
        ast_add(block, whilenode);

        return block;
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
            expect(p, T_SEMI);
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
            expect(p, T_SEMI);
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
            expect(p, T_SEMI);
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
        expect(p, T_SEMI);
        return array_assign;
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
        expect(p, T_SEMI);
        return field_assign;
    }

    expect(p, T_SEMI);
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

AstNode* parse_struct_def(Parser* p) {
    expect(p, T_STRUCT);
    Tok name = advance_tok(p);
    AstNode* struct_def = ast_new(AST_STRUCT_DEF);
    struct_def->name = strndup(name.s, name.len);

    expect(p, T_LBRACE);
    while (!check_tok(p, T_RBRACE)) {
        Tok field_name = advance_tok(p);
        expect(p, T_COLON);

        // Parse field type (handles i32, *i8, [i32; 10], etc.)
        TypeSpec field_type = parse_type(p);

        AstNode* field = ast_new(AST_IDENT);
        field->name = strndup(field_name.s, field_name.len);
        field->value = field_type.base_type;      // Store type name
        field->is_pointer = field_type.is_pointer;  // Store if it's a pointer
        ast_add(struct_def, field);

        if (!check_tok(p, T_RBRACE)) expect(p, T_COMMA);
    }
    expect(p, T_RBRACE);

    return struct_def;
}

// Parse type specification: i32, [i32; 10], *i32, *mut i32, Point (struct)
TypeSpec parse_type(Parser* p) {
    TypeSpec spec = {0};

    // Check for pointer: * or *mut
    if (check_tok(p, T_STAR)) {
        advance_tok(p);
        spec.is_pointer = 1;

        // Check for mut
        if (check_tok(p, T_MUT)) {
            advance_tok(p);
            spec.is_mutable = 1;
        }

        // Get pointee type
        Tok type = advance_tok(p);
        spec.base_type = strndup(type.s, type.len);
        return spec;
    }

    // Check for array: [T; N]
    if (check_tok(p, T_LBRACKET)) {
        advance_tok(p);
        spec.is_array = 1;

        // Get element type
        Tok elem_type = advance_tok(p);
        spec.base_type = strndup(elem_type.s, elem_type.len);

        expect(p, T_SEMI);

        // Get array count
        Tok count = advance_tok(p);
        spec.array_count = atoi(count.s);

        expect(p, T_RBRACKET);
        return spec;
    }

    // Basic type or struct name (i32, i64, Point, Vector, etc.)
    Tok type = advance_tok(p);
    spec.base_type = strndup(type.s, type.len);
    return spec;
}

AstNode* parse_global_var(Parser* p) {
    expect(p, T_LET);
    Tok name = advance_tok(p);

    AstNode* global_var = ast_new(AST_GLOBAL_VAR);
    global_var->name = strndup(name.s, name.len);

    // Optional type annotation: let x: i32 = ... or let arr: [i32; 10];
    if (match_tok(p, T_COLON)) {
        TypeSpec spec = parse_type(p);

        // Store type info in AST node (creative use of fields)
        global_var->value = spec.base_type;          // Base type
        global_var->array_size = spec.array_count;   // Array count
        global_var->is_pointer = spec.is_pointer | (spec.is_mutable << 1);  // Flags
        // Note: is_pointer bit 0 = is_pointer, bit 1 = is_mutable

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
            // Handle pointer types: *Type
            if (match_tok(p, T_STAR)) {
                par->is_pointer = 1;
                Tok type_tok = advance_tok(p);  // Get base type
                par->value = strndup(type_tok.s, type_tok.len);  // Save type name
            } else {
                Tok type_tok = advance_tok(p);  // Get type
                par->value = strndup(type_tok.s, type_tok.len);  // Save type name
            }
        }
        if (!check_tok(p, T_RPAREN)) expect(p, T_COMMA);
    }
    expect(p, T_RPAREN);

    if (match_tok(p, T_ARROW)) advance_tok(p);

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

AstNode* parse(Parser* p) {
    AstNode* prog = ast_new(AST_PROGRAM);
    while (!check_tok(p, T_EOF)) {
        if (check_tok(p, T_STRUCT)) {
            ast_add(prog, parse_struct_def(p));
        } else if (check_tok(p, T_LET)) {
            ast_add(prog, parse_global_var(p));
        } else {
            ast_add(prog, parse_func(p));
        }
    }
    return prog;
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

void gen_expr(Codegen* cg, AstNode* n);

// Helper: Generate builtin function calls
void gen_builtin_call(Codegen* cg, AstNode* n) {
    if (!strcmp(n->name, "print")) {
        if (n->child_count > 0) {
            gen_expr(cg, n->children[0]);
            emit(cg, "    mov rsi, rax\n    mov rdx, rbx\n");
            emit(cg, "    mov rdi, 1\n    mov rax, 1\n    syscall\n");
        }
    } else if (!strcmp(n->name, "println")) {
        if (n->child_count > 0) {
            gen_expr(cg, n->children[0]);
            emit(cg, "    mov rsi, rax\n    mov rdx, rbx\n");
            emit(cg, "    mov rdi, 1\n    mov rax, 1\n    syscall\n");
        }
        emit(cg, "    mov byte [rbp-256], 10\n");
        emit(cg, "    lea rsi, [rbp-256]\n");
        emit(cg, "    mov rdi, 1\n    mov rdx, 1\n    mov rax, 1\n    syscall\n");
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
            gen_expr(cg, n->children[0]);  // filename
            emit(cg, "    mov rdi, rax\n");
            gen_expr(cg, n->children[1]);  // flags
            emit(cg, "    mov rsi, rax\n");
            if (n->child_count >= 3) {
                gen_expr(cg, n->children[2]);  // mode (optional)
                emit(cg, "    mov rdx, rax\n");
            } else {
                emit(cg, "    mov rdx, 0644\n");  // Default permissions
            }
            emit(cg, "    mov rax, 2\n");  // sys_open
            emit(cg, "    syscall\n");
        }
    } else if (!strcmp(n->name, "read")) {
        // read(fd, buffer, count) -> bytes_read
        // syscall 0: rax=0, rdi=fd, rsi=buffer, rdx=count
        if (n->child_count >= 3) {
            gen_expr(cg, n->children[0]);  // fd
            emit(cg, "    mov rdi, rax\n");
            gen_expr(cg, n->children[1]);  // buffer
            emit(cg, "    mov rsi, rax\n");
            gen_expr(cg, n->children[2]);  // count
            emit(cg, "    mov rdx, rax\n");
            emit(cg, "    mov rax, 0\n");  // sys_read
            emit(cg, "    syscall\n");
        }
    } else if (!strcmp(n->name, "write")) {
        // write(fd, buffer, count) -> bytes_written
        // syscall 1: rax=1, rdi=fd, rsi=buffer, rdx=count
        if (n->child_count >= 3) {
            gen_expr(cg, n->children[0]);  // fd
            emit(cg, "    mov rdi, rax\n");
            gen_expr(cg, n->children[1]);  // buffer
            emit(cg, "    mov rsi, rax\n");
            gen_expr(cg, n->children[2]);  // count
            emit(cg, "    mov rdx, rax\n");
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
            emit(cg, "    mov r12, rax\n");  // Save original size in r12
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
            emit(cg, "    mov [rax], r12\n");  // Store original size in first 8 bytes
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
    } else if (!strcmp(n->name, "syscall") || !strcmp(n->name, "syscall6")) {
        // Generic syscall: syscall6(num, arg1, arg2, arg3, arg4, arg5, arg6)
        // This allows Chronos code to make ANY syscall directly
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
        // Regular function call
        const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        for (int i = 0; i < n->child_count && i < 6; i++) {
            gen_expr(cg, n->children[i]);
            emit(cg, "    mov %s, rax\n", regs[i]);
        }
        emit(cg, "    call %s\n", n->name);
    }
}

// Helper: Generate address-of operator (&)
void gen_addr_of(Codegen* cg, AstNode* n) {
    if (!n->children || n->child_count == 0) return;
    AstNode* var = n->children[0];
    if (!var) return;

    if (var->type == AST_IDENT) {
        int off = symtab_lookup(cg->symtab, var->name);
        if (off) {
            // Check if this is already a pointer - if so, just load the value
            Symbol* sym = symtab_lookup_symbol(cg->symtab, var->name);
            if (sym && sym->is_pointer) {
                // Variable is already a pointer - just load it
                emit(cg, "    mov rax, [rbp%d]\n", off);
            } else {
                // Regular variable - take its address
                emit(cg, "    lea rax, [rbp%d]\n", off);
            }
        }
    } else if (var->type == AST_INDEX) {
        // &array[index] - compute address of element
        if (!var->children || var->child_count < 2) return;
        AstNode* arr = var->children[0];
        if (!arr || !arr->name) return;

        Symbol* sym = symtab_lookup_symbol(cg->symtab, arr->name);
        if (sym) {
            gen_expr(cg, var->children[1]);  // Index expression

            // Bounds checking for address-of
            if (sym->size > 0) {
                int array_count = sym->size / 8;
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

            // Calculate address: rbp + offset + (index * 8)
            emit(cg, "    imul rax, 8\n");
            emit(cg, "    lea rbx, [rbp%d]\n", sym->offset);
            emit(cg, "    add rax, rbx\n");
        }
    }
}

// Helper: Generate array indexing with bounds checking
void gen_array_index(Codegen* cg, AstNode* n) {
    AstNode* arr = n->children[0];

    // Handle field access indexing: lex.source[i]
    if (arr->type == AST_FIELD_ACCESS) {
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
                    } else if (!strcmp(base_type, "i16")) {
                        elem_size = 2;
                    } else if (!strcmp(base_type, "i32") || !strcmp(base_type, "u32")) {
                        elem_size = 4;
                    } else if (!strcmp(base_type, "i64") || !strcmp(base_type, "u64")) {
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
                    emit(cg, "    imul rax, %d\n", elem_size);
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
        gen_expr(cg, n->children[1]);  // Index in rax

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
                } else if (!strcmp(base_type, "i16")) {
                    elem_size = 2;
                } else if (!strcmp(base_type, "i32") || !strcmp(base_type, "u32")) {
                    elem_size = 4;
                } else if (!strcmp(base_type, "i64") || !strcmp(base_type, "u64")) {
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
                emit(cg, "    imul rax, %d\n", elem_size);
            }

            emit(cg, "    add rbx, rax\n");  // Add scaled index

            // For structs, return address in rax (for field access)
            // For primitives, load the value
            if (is_struct) {
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

        // It's a real array - do bounds checking
        // Determine element size from type_name
        int elem_size = 8;  // Default to 8 bytes (i64)
        int array_count = sym->size;  // size is already the count for typed arrays
        int is_struct = 0;

        if (sym->type_name) {
            if (!strcmp(sym->type_name, "i8") || !strcmp(sym->type_name, "u8")) {
                elem_size = 1;
            } else if (!strcmp(sym->type_name, "i16")) {
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

        // Bounds checking
        if (sym->size > 0) {
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

        // Calculate offset: index * elem_size
        if (elem_size > 1) {
            emit(cg, "    imul rax, %d\n", elem_size);
        }
        emit(cg, "    mov rbx, rax\n");

        // For structs, return address; for primitives, load value
        if (is_struct) {
            emit(cg, "    lea rax, [rbp%d+rbx]\n", sym->offset);
        } else {
            // Load value with correct width
            if (elem_size == 1) {
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
            gen_expr(cg, n->children[1]);  // Index in rax

            // For i8 arrays, element size is 1 byte
            // For i32/i64, element size is 4/8 bytes
            int elem_size = 1;  // Default to byte for i8
            if (gvar->elem_type) {
                if (!strcmp(gvar->elem_type, "i32")) elem_size = 4;
                else if (!strcmp(gvar->elem_type, "i64")) elem_size = 8;
                else if (!strcmp(gvar->elem_type, "i16")) elem_size = 2;
            }

            // Calculate offset: index * elem_size
            if (elem_size > 1) {
                emit(cg, "    imul rax, %d\n", elem_size);
            }

            // Load from global array: array_name + offset
            emit(cg, "    lea rbx, [%s]\n", gvar->name);
            emit(cg, "    add rbx, rax\n");

            // Load based on element size
            if (elem_size == 1) {
                emit(cg, "    movzx rax, byte [rbx]\n");
            } else if (elem_size == 2) {
                emit(cg, "    movzx rax, word [rbx]\n");
            } else if (elem_size == 4) {
                emit(cg, "    mov eax, [rbx]\n");
            } else {
                emit(cg, "    mov rax, [rbx]\n");
            }
        } else {
            emit(cg, "    mov rax, 0  ; array not found\n");
        }
    }
}

void gen_expr(Codegen* cg, AstNode* n) {
    if (!n) return;  // Null safety guard
    if (n->type == AST_NUMBER) {
        emit(cg, "    mov rax, %s\n", n->value);
    } else if (n->type == AST_STRING) {
        char* label = strtab_add(cg->strtab, n->value, strlen(n->value));
        emit(cg, "    mov rax, %s\n", label);
        emit(cg, "    mov rbx, %d\n", (int)strlen(n->value));
    } else if (n->type == AST_IDENT) {
        // Try local variable first
        int off = symtab_lookup(cg->symtab, n->name);
        if (off) {
            // Check if this is an array - if so, load address not value
            Symbol* sym = symtab_lookup_symbol(cg->symtab, n->name);
            if (sym && sym->size > 1 && sym->type_name && !sym->is_pointer) {
                // This is an array - use lea to get address
                emit(cg, "    lea rax, [rbp%d]\n", off);
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
                } else {
                    emit(cg, "    mov rax, [%s]\n", gvar->name);
                }
            } else {
                emit(cg, "    mov rax, 0  ; unknown var %s\n", n->name);
            }
        }
    } else if (n->type == AST_ADDR_OF) {
        gen_addr_of(cg, n);
    } else if (n->type == AST_DEREF) {
        // Dereference: *ptr
        gen_expr(cg, n->children[0]);
        emit(cg, "    mov rax, [rax]\n");  // Load value at address in rax
    } else if (n->type == AST_UNARY) {
        // Unary operators: '-' (negation) and '!' (logical NOT)
        if (n->children && n->child_count > 0) {
            gen_expr(cg, n->children[0]);
            if (n->op && n->op[0] == '-') {
                emit(cg, "    neg rax\n");  // Two's complement negation
            } else if (n->op && n->op[0] == '!') {
                // Logical NOT: convert non-zero to 0, zero to 1
                emit(cg, "    test rax, rax\n");
                emit(cg, "    setz al  ; Set al to 1 if rax was 0\n");
                emit(cg, "    movzx rax, al\n");
            }
        }
    } else if (n->type == AST_ASSIGN) {
        gen_expr(cg, n->children[0]);
        // Try local variable first
        int off = symtab_lookup(cg->symtab, n->name);
        if (off) {
            emit(cg, "    mov [rbp%d], rax\n", off);
        } else {
            // Try global variable
            GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, n->name);
            if (gvar) {
                emit(cg, "    mov [%s], rax\n", gvar->name);
            }
        }
    } else if (n->type == AST_BINOP) {
        // Strength reduction optimization (O2+): Check if right operand is power of 2
        AstNode* right = n->children[1];
        int use_shift = 0;
        long right_val = 0;
        int shift_amount = 0;

        if (optimization_level >= 2 && right && right->type == AST_NUMBER) {
            right_val = atol(right->value);
            if (is_power_of_2(right_val)) {
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
            emit(cg, "    ; Optimized: x / %ld => x >> %d\n", right_val, shift_amount);
            emit(cg, "    sar rax, %d\n", shift_amount);
        } else if (use_shift && n->op[0] == '%') {
            // Modulo by power of 2: use AND mask
            long mask = right_val - 1;
            emit(cg, "    ; Optimized: x %% %ld => x & %ld\n", right_val, mask);
            emit(cg, "    and rax, %ld\n", mask);
        } else {
            // Normal codegen
            emit(cg, "    push rax\n");
            gen_expr(cg, n->children[1]);
            emit(cg, "    mov rbx, rax\n    pop rax\n");
            if (n->op[0] == '+') emit(cg, "    add rax, rbx\n");
            else if (n->op[0] == '-') emit(cg, "    sub rax, rbx\n");
            else if (n->op[0] == '*') emit(cg, "    imul rax, rbx\n");
            else if (n->op[0] == '/') {
            // Division by zero check
            int skip_label = new_label(cg);
            emit(cg, "    test rbx, rbx\n");
            emit(cg, "    jnz .L%d\n", skip_label);
            emit(cg, "    ; Division by zero - return 0\n");
            emit(cg, "    xor rax, rax\n");
            emit(cg, "    jmp .L%d_end\n", skip_label);
            emit(cg, ".L%d:\n", skip_label);
            emit(cg, "    xor rdx, rdx\n    idiv rbx\n");
            emit(cg, ".L%d_end:\n", skip_label);
        }
        else if (n->op[0] == '%') {
            // Modulo operation (remainder after division)
            int skip_label = new_label(cg);
            emit(cg, "    test rbx, rbx\n");
            emit(cg, "    jnz .L%d\n", skip_label);
            emit(cg, "    ; Modulo by zero - return 0\n");
            emit(cg, "    xor rax, rax\n");
            emit(cg, "    jmp .L%d_end\n", skip_label);
            emit(cg, ".L%d:\n", skip_label);
            emit(cg, "    xor rdx, rdx\n    idiv rbx\n");
            emit(cg, "    mov rax, rdx  ; Move remainder to rax\n");
            emit(cg, ".L%d_end:\n", skip_label);
            }
        }
    } else if (n->type == AST_COMPARE) {
        gen_expr(cg, n->children[0]);
        emit(cg, "    push rax\n");
        gen_expr(cg, n->children[1]);
        emit(cg, "    mov rbx, rax\n    pop rax\n");
        emit(cg, "    cmp rax, rbx\n");
        if (!strcmp(n->op, "==")) emit(cg, "    sete al\n");
        else if (!strcmp(n->op, "!=")) emit(cg, "    setne al\n");
        else if (!strcmp(n->op, "<")) emit(cg, "    setl al\n");
        else if (!strcmp(n->op, ">")) emit(cg, "    setg al\n");
        else if (!strcmp(n->op, "<=")) emit(cg, "    setle al\n");
        else if (!strcmp(n->op, ">=")) emit(cg, "    setge al\n");
        emit(cg, "    movzx rax, al\n");
    } else if (n->type == AST_LOGICAL) {
        if (!strcmp(n->op, "&&")) {
            // Short-circuit AND: if left is false (0), don't evaluate right
            int false_label = new_label(cg);
            int end_label = new_label(cg);

            gen_expr(cg, n->children[0]);  // Evaluate left
            emit(cg, "    test rax, rax\n");
            emit(cg, "    jz .L%d  ; Jump if left is false\n", false_label);

            gen_expr(cg, n->children[1]);  // Evaluate right
            emit(cg, "    test rax, rax\n");
            emit(cg, "    jz .L%d  ; Jump if right is false\n", false_label);

            emit(cg, "    mov rax, 1  ; Both true\n");
            emit(cg, "    jmp .L%d\n", end_label);

            emit(cg, ".L%d:\n", false_label);
            emit(cg, "    xor rax, rax  ; Result is false\n");
            emit(cg, ".L%d:\n", end_label);
        } else if (!strcmp(n->op, "||")) {
            // Short-circuit OR: if left is true (non-zero), don't evaluate right
            int true_label = new_label(cg);
            int end_label = new_label(cg);

            gen_expr(cg, n->children[0]);  // Evaluate left
            emit(cg, "    test rax, rax\n");
            emit(cg, "    jnz .L%d  ; Jump if left is true\n", true_label);

            gen_expr(cg, n->children[1]);  // Evaluate right
            emit(cg, "    test rax, rax\n");
            emit(cg, "    jnz .L%d  ; Jump if right is true\n", true_label);

            emit(cg, "    xor rax, rax  ; Both false\n");
            emit(cg, "    jmp .L%d\n", end_label);

            emit(cg, ".L%d:\n", true_label);
            emit(cg, "    mov rax, 1  ; Result is true\n");
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
            for (int i = 0; i < n->child_count; i++) {
                AstNode* field_assign = n->children[i];
                char* field_name = field_assign->name;
                AstNode* field_value = field_assign->children[0];

                int field_off = typetab_field_offset(cg->types, n->struct_type, field_name);
                gen_expr(cg, field_value);
                emit(cg, "    mov [rbp%d], rax\n", sym->offset + field_off);
            }
            emit(cg, "    lea rax, [rbp%d]\n", sym->offset);
        }
    } else if (n->type == AST_FIELD_ACCESS) {
        AstNode* obj = n->children[0];
        char* field_name = n->name;

        // Check if object is a dereference (pointer access)
        if (obj->type == AST_DEREF) {
            // ptr->field case (already desugared)
            AstNode* ptr = obj->children[0];
            Symbol* sym = symtab_lookup_symbol(cg->symtab, ptr->name);
            if (sym && sym->is_pointer && sym->type_name) {
                int field_off = typetab_field_offset(cg->types, sym->type_name, field_name);
                if (field_off >= 0) {
                    emit(cg, "    mov rax, [rbp%d]\n", sym->offset);  // Load pointer
                    emit(cg, "    mov rax, [rax+%d]\n", field_off);   // Access field
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
                    if (is_pointer) {
                        // For pointers: load pointer, then load field
                        emit(cg, "    mov rax, [rbp%d]\n", sym->offset);  // Load pointer
                        if (field_off == 0) {
                            emit(cg, "    mov rax, [rax]\n");  // Load field at offset 0
                        } else {
                            emit(cg, "    mov rax, [rax+%d]\n", field_off);  // Load field
                        }
                    } else {
                        // For direct structs: load directly
                        emit(cg, "    mov rax, [rbp%d]\n", sym->offset + field_off);
                    }
                }
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
                        if (field_off == 0) {
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

        if (!arr || !arr->name) return;

        // Evaluate value expression and save it
        gen_expr(cg, value_expr);
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
                    else if (!strcmp(base_type, "i16")) elem_size = 2;
                    else if (!strcmp(base_type, "i32") || !strcmp(base_type, "u32")) elem_size = 4;
                    else if (!strcmp(base_type, "i64") || !strcmp(base_type, "u64")) elem_size = 8;
                }

                // Scale index by element size
                if (elem_size > 1) {
                    emit(cg, "    imul rax, %d\n", elem_size);
                }

                // Load pointer and add offset
                emit(cg, "    mov rbx, [rbp%d]\n", sym->offset);  // Load pointer
                emit(cg, "    add rbx, rax\n");  // Add scaled index
                emit(cg, "    pop rax\n");  // Restore value

                // Store with correct width
                if (elem_size == 1) {
                    emit(cg, "    mov byte [rbx], al\n");
                } else if (elem_size == 2) {
                    emit(cg, "    mov word [rbx], ax\n");
                } else if (elem_size == 4) {
                    emit(cg, "    mov dword [rbx], eax\n");
                } else {
                    emit(cg, "    mov qword [rbx], rax\n");
                }
            } else {
                // Local array assignment
                // Determine element size from type_name
                int elem_size = 8;  // Default to 8 bytes (i64)
                if (sym->type_name) {
                    if (!strcmp(sym->type_name, "i8") || !strcmp(sym->type_name, "u8")) elem_size = 1;
                    else if (!strcmp(sym->type_name, "i16")) elem_size = 2;
                    else if (!strcmp(sym->type_name, "i32") || !strcmp(sym->type_name, "u32")) elem_size = 4;
                    else if (!strcmp(sym->type_name, "i64") || !strcmp(sym->type_name, "u64")) elem_size = 8;
                }

                // Local array assignment with correct element size
                if (elem_size > 1) {
                    emit(cg, "    imul rax, %d\n", elem_size);
                }
                emit(cg, "    mov rbx, rax\n");
                emit(cg, "    pop rax\n");  // Restore value

                // Store with correct width
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
        } else {
            // Try global array
            GlobalVar* gvar = global_symtab_lookup(cg->global_symtab, arr->name);
            if (gvar && gvar->is_array) {
                // Calculate element size
                int elem_size = 1;  // i8
                if (gvar->elem_type) {
                    if (!strcmp(gvar->elem_type, "i32")) elem_size = 4;
                    else if (!strcmp(gvar->elem_type, "i64")) elem_size = 8;
                    else if (!strcmp(gvar->elem_type, "i16")) elem_size = 2;
                }

                // Calculate offset
                if (elem_size > 1) {
                    emit(cg, "    imul rax, %d\n", elem_size);
                }
                emit(cg, "    lea rbx, [%s]\n", gvar->name);
                emit(cg, "    add rbx, rax\n");
                emit(cg, "    pop rax\n");  // Restore value

                // Store with correct width
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

        // Evaluate value expression
        gen_expr(cg, value_expr);
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
                        emit(cg, "    imul rbx, %d\n", elem_size);
                    }
                    emit(cg, "    lea rcx, [rbp%d+rbx]\n", sym->offset);  // rcx = address of array element

                    // Add field offset
                    if (field_off > 0) {
                        emit(cg, "    add rcx, %d\n", field_off);
                    }

                    emit(cg, "    pop rax\n");  // Restore value
                    emit(cg, "    mov [rcx], rax\n");  // Store value at field location
                }
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

                    if (is_pointer) {
                        // For pointers: load pointer, add offset, store
                        emit(cg, "    mov rbx, [rbp%d]\n", sym->offset);  // Load pointer
                        if (field_off == 0) {
                            emit(cg, "    mov [rbx], rcx\n");  // Store through pointer (no offset)
                        } else {
                            emit(cg, "    mov [rbx+%d], rcx\n", field_off);  // Store through pointer with offset
                        }
                    } else {
                        // For direct structs: store directly
                        emit(cg, "    mov [rbp%d], rcx\n", sym->offset + field_off);
                    }
                }
            }
        }
    }
}

void gen_stmt(Codegen* cg, AstNode* n);

void gen_stmt(Codegen* cg, AstNode* n) {
    if (!n) return;  // Null safety
    if (n->type == AST_BLOCK) {
        // Handle block statements (for 'for' loop desugaring)
        for (int i = 0; i < n->child_count; i++) {
            gen_stmt(cg, n->children[i]);
        }
        return;
    }
    if (n->type == AST_RETURN) {
        if (n->child_count > 0) gen_expr(cg, n->children[0]);
        else emit(cg, "    xor rax, rax\n");
        emit(cg, "    leave\n    ret\n");
    } else if (n->type == AST_LET) {
        int size = 1;
        char* type_name = NULL;

        // Check if this is a typed array: let arr: [i32; 10];
        if (n->struct_type && !strcmp(n->struct_type, "__array__")) {
            // Calculate size based on element type and array count
            int elem_size = 8;  // Default to 8 bytes
            if (n->value) {
                if (!strcmp(n->value, "i8") || !strcmp(n->value, "u8")) elem_size = 1;
                else if (!strcmp(n->value, "i16")) elem_size = 2;
                else if (!strcmp(n->value, "i32") || !strcmp(n->value, "u32")) elem_size = 4;
                else if (!strcmp(n->value, "i64") || !strcmp(n->value, "u64")) elem_size = 8;
            }

            int total_size = elem_size * n->array_size;
            int off = symtab_add(cg->symtab, n->name, n->array_size);  // Use array_size as count

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
                emit(cg, "    ; Struct local: %s: %s (%d bytes)\n", n->name, n->struct_type, st->size);
                int off = symtab_add_struct(cg->symtab, n->name, n->struct_type, st->size);

                // If there's an initializer (struct literal), generate it
                if (n->child_count > 0) {
                    gen_expr(cg, n->children[0]);
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
                    int off = symtab_add_struct(cg->symtab, n->name, type_name, st->size);
                    gen_expr(cg, n->children[0]);
                    return;
                }
            }
        }

        // Check if pointer type
        if (n->is_pointer) {
            int off = symtab_add_pointer(cg->symtab, n->name);

            // Store type name for element size calculation
            if (n->value && cg->symtab && cg->symtab->count > 0) {
                // Build type_name as "*BaseType"
                char* type_buf = malloc(strlen(n->value) + 2);
                sprintf(type_buf, "*%s", n->value);
                cg->symtab->symbols[cg->symtab->count - 1].type_name = type_buf;
            }

            if (n->child_count > 0) {
                gen_expr(cg, n->children[0]);
                emit(cg, "    mov [rbp%d], rax\n", off);
            }
            return;
        }

        int off = symtab_add(cg->symtab, n->name, size);
        if (n->child_count > 0) {
            gen_expr(cg, n->children[0]);
            if (n->children[0]->type != AST_ARRAY_LITERAL) {
                emit(cg, "    mov [rbp%d], rax\n", off);
            }
        }
    } else if (n->type == AST_IF) {
        int else_lab = new_label(cg);
        int end_lab = new_label(cg);
        gen_expr(cg, n->children[0]);
        emit(cg, "    test rax, rax\n    jz .L%d\n", else_lab);
        for (int i = 0; i < n->children[1]->child_count; i++)
            gen_stmt(cg, n->children[1]->children[i]);
        emit(cg, "    jmp .L%d\n.L%d:\n", end_lab, else_lab);
        if (n->child_count > 2) {
            for (int i = 0; i < n->children[2]->child_count; i++)
                gen_stmt(cg, n->children[2]->children[i]);
        }
        emit(cg, ".L%d:\n", end_lab);
    } else if (n->type == AST_WHILE) {
        int start_lab = new_label(cg);
        int end_lab = new_label(cg);
        emit(cg, ".L%d:\n", start_lab);
        gen_expr(cg, n->children[0]);
        emit(cg, "    test rax, rax\n    jz .L%d\n", end_lab);
        for (int i = 0; i < n->children[1]->child_count; i++)
            gen_stmt(cg, n->children[1]->children[i]);
        emit(cg, "    jmp .L%d\n.L%d:\n", start_lab, end_lab);
    } else if (n->type == AST_CALL || n->type == AST_ASSIGN || n->type == AST_ARRAY_ASSIGN || n->type == AST_FIELD_ASSIGN) {
        gen_expr(cg, n);
    }
}

void gen_func(Codegen* cg, AstNode* n) {
    if (!n || !n->name) return;  // Null safety
    emit(cg, "\n%s:\n", n->name);
    emit(cg, "    push rbp\n    mov rbp, rsp\n");

    int param_count = n->child_count - 1;
    const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

    SymbolTable* old_symtab = cg->symtab;
    cg->symtab = symtab_new();

    for (int i = 0; i < param_count && i < 6; i++) {
        int off;
        if (n->children[i]->is_pointer) {
            off = symtab_add_pointer(cg->symtab, n->children[i]->name);
            // Set the type_name for pointer parameters (needed for struct field access)
            if (n->children[i]->value && cg->symtab && cg->symtab->count > 0) {
                char* type_with_ptr = malloc(strlen(n->children[i]->value) + 2);
                sprintf(type_with_ptr, "*%s", n->children[i]->value);
                cg->symtab->symbols[cg->symtab->count - 1].type_name = type_with_ptr;
            }
        } else {
            off = symtab_add(cg->symtab, n->children[i]->name, 1);
            // Set the type_name for non-pointer parameters
            if (n->children[i]->value && cg->symtab && cg->symtab->count > 0) {
                cg->symtab->symbols[cg->symtab->count - 1].type_name = strdup(n->children[i]->value);
            }
        }
        emit(cg, "    mov [rbp%d], %s\n", off, regs[i]);
    }

    // Allocate stack space based on actual usage (aligned to 16 bytes)
    int stack_size = cg->symtab->stack_size;
    // Add 1024 bytes for temporary operations (print_int buffer, debug code, etc.)
    stack_size += 1024;
    // Align to 16 bytes (required by x86-64 ABI)
    stack_size = ((stack_size + 15) / 16) * 16;
    if (stack_size > 0) {
        emit(cg, "    sub rsp, %d\n", stack_size);
    }

    AstNode* body = n->children[param_count];
    for (int i = 0; i < body->child_count; i++)
        gen_stmt(cg, body->children[i]);

    emit(cg, "    xor rax, rax\n    leave\n    ret\n");

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

            for (int j = 0; j < struct_def->child_count; j++) {
                AstNode* field = struct_def->children[j];
                // field->name = field name
                // field->value = base type (e.g., "i64", "Token")
                // field->is_pointer = 1 if pointer type
                char* field_type = field->value;
                int is_ptr = field->is_pointer;

                // Build full type name for pointer types (e.g., "*Token")
                char* full_type = NULL;
                if (is_ptr && field_type) {
                    full_type = malloc(strlen(field_type) + 2);
                    sprintf(full_type, "*%s", field_type);
                } else if (field_type) {
                    full_type = strdup(field_type);
                }

                typetab_add_field(tt, struct_def->name, field->name, full_type, is_ptr);

                if (full_type) free(full_type);
            }
        }
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

            global_symtab_add_full(&global_symtab, gvar->name, type_name, 0, is_initialized, init_value,
                                   is_array, array_count, is_pointer, is_mutable);

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

    emit(&cg, "\nsection .text\n    global _start\n\n");
    emit(&cg, "_start:\n    call main\n    mov rdi, rax\n");
    emit(&cg, "    mov rax, 60\n    syscall\n");

    gen_helpers(&cg);

    for (int i = 0; i < ast->child_count; i++) {
        if (ast->children[i]->type == AST_FUNCTION) {
            // Skip forward declarations, only generate code for full definitions
            if (!ast->children[i]->is_forward_decl) {
                gen_func(&cg, ast->children[i]);
            }
        }
    }

    cg.out = fopen(file, "w");
    fprintf(cg.out, "; CHRONOS v0.11 - Global Variables\n\n");
    fprintf(cg.out, "section .data\n");

    // Emit string literals
    for (int i = 0; i < strtab->count; i++) {
        fprintf(cg.out, "%s: db ", strtab->strings[i].label);
        for (int j = 0; j < strtab->strings[i].len; j++) {
            fprintf(cg.out, "%d", (unsigned char)strtab->strings[i].value[j]);
            fprintf(cg.out, ", ");
        }
        fprintf(cg.out, "0\n");  // Null terminator
    }

    // Emit initialized global variables
    for (int i = 0; i < global_symtab.count; i++) {
        GlobalVar* gv = &global_symtab.vars[i];
        if (gv->is_initialized) {
            // Array with initialization values
            if (gv->is_array && gv->array_init_values && gv->array_init_count > 0) {
                const char* directive = type_asm_directive(gv->elem_type);
                fprintf(cg.out, "%s: %s ", gv->name, directive);

                // FIX Bug #11: Use declared array_count, not just array_init_count
                // This allows: let buf: [i8; 1000] = "hello"; to allocate full 1000 bytes
                int total_count = (gv->array_count > gv->array_init_count) ? gv->array_count : gv->array_init_count;

                for (int j = 0; j < total_count; j++) {
                    if (j < gv->array_init_count) {
                        // Emit initialized value
                        fprintf(cg.out, "%s", gv->array_init_values[j]);
                    } else {
                        // Pad remaining elements with zeros
                        fprintf(cg.out, "0");
                    }
                    if (j < total_count - 1) {
                        fprintf(cg.out, ", ");
                    }
                }
                fprintf(cg.out, "\n");
            }
            // Scalar or single value
            else if (gv->init_value) {
                const char* directive = type_asm_directive(gv->type_name);
                fprintf(cg.out, "%s: %s %s\n", gv->name, directive, gv->init_value);
            }
        }
    }

    // Emit uninitialized global variables (.bss section)
    int has_bss = 0;
    for (int i = 0; i < global_symtab.count; i++) {
        GlobalVar* gv = &global_symtab.vars[i];
        if (!gv->is_initialized) {
            if (!has_bss) {
                fprintf(cg.out, "\nsection .bss\n");
                has_bss = 1;
            }

            // Calculate appropriate directive
            const char* directive;
            int count;
            if (gv->is_array) {
                // Array: reserve N elements of appropriate size
                int elem_size = type_size(gv->elem_type);
                if (elem_size == 1) directive = "resb";
                else if (elem_size == 2) directive = "resw";
                else if (elem_size == 4) directive = "resd";
                else directive = "resq";
                count = gv->array_count;
            } else {
                // Scalar or pointer: reserve appropriate size
                int size = gv->is_pointer ? 8 : gv->size;
                if (size == 1) { directive = "resb"; count = 1; }
                else if (size == 2) { directive = "resw"; count = 1; }
                else if (size == 4) { directive = "resd"; count = 1; }
                else { directive = "resq"; count = 1; }
            }

            fprintf(cg.out, "%s: %s %d\n", gv->name, directive, count);
        }
    }

    fprintf(cg.out, "%s", cg.code_buf);

    fclose(cg.out);
    free(cg.code_buf);
    free(global_symtab.vars);
}

int main(int argc, char** argv) {
    // Parse optimization flags
    int file_arg = 1;
    if (argc >= 3 && argv[1][0] == '-' && argv[1][1] == 'O') {
        if (argv[1][2] == '0') optimization_level = 0;
        else if (argv[1][2] == '1') optimization_level = 1;
        else if (argv[1][2] == '2') optimization_level = 2;
        file_arg = 2;
    }

    if (argc < 2 || (file_arg == 2 && argc < 3)) {
        printf("Usage: chronos [-O0|-O1|-O2] <file.ch>\n");
        printf("  -O0: No optimizations (default)\n");
        printf("  -O1: Basic (constant folding)\n");
        printf("  -O2: Aggressive (constant folding + strength reduction)\n");
        return 1;
    }

    FILE* f = fopen(argv[file_arg], "r");
    if (!f) { perror("Error"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* src = malloc(size + 1);
    fread(src, 1, size, f);
    src[size] = '\0';
    fclose(f);

    printf("🔥 CHRONOS v0.17 - COMPILER OPTIMIZATIONS\n");
    printf("Constant folding, strength reduction, -O flags\n");
    printf("Optimization level: -O%d\n", optimization_level);
    printf("Compiling: %s\n", argv[file_arg]);

    int count;
    Tok* toks = tokenize(src, &count);
    Parser parser = {toks, 0, count};
    AstNode* ast = parse(&parser);

    TypeTable* types = typetab_new();
    build_type_table(types, ast);

    StringTable* strtab = strtab_new();
    codegen(ast, "output.asm", strtab, types);

    printf("✅ Code generated\n");
    system("nasm -f elf64 output.asm -o output.o 2>&1 | head -5");
    system("ld output.o -o chronos_program 2>&1 | head -5");
    printf("✅ Compilation complete: ./chronos_program\n");

    return 0;
}
