/* CHRONOS v0.8 - STRUCTS SUPPORT
 * Structs: struct Point { x: i32, y: i32 }
 * Struct literals: let p = Point { x: 10, y: 20 };
 * Field access: p.x, p.y
 * Author: Ignacio Peña
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

// TOKENS
typedef enum {
    T_EOF, T_IDENT, T_NUM, T_STR,
    T_FN, T_LET, T_IF, T_ELSE, T_WHILE, T_FOR, T_RET, T_STRUCT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_LBRACKET, T_RBRACKET,
    T_SEMI, T_COLON, T_COMMA, T_DOT,
    T_PLUS, T_MINUS, T_STAR, T_SLASH,
    T_EQ, T_EQEQ, T_NEQ, T_LT, T_GT, T_LTE, T_GTE, T_ARROW
} TokType;

typedef struct { TokType t; char* s; int len; } Tok;
typedef struct { char* src; char* cur; } Lex;

// TYPE SYSTEM
typedef struct StructField {
    char* name;
    int offset;  // Offset within struct
} StructField;

typedef struct StructType {
    char* name;
    StructField* fields;
    int field_count;
    int size;  // Total size in bytes
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
    AST_ARRAY_LITERAL, AST_INDEX, AST_STRUCT_DEF, AST_STRUCT_LITERAL, AST_FIELD_ACCESS
} AstType;

typedef struct AstNode {
    AstType type;
    char* name;
    struct AstNode** children;
    int child_count;
    char* value;
    char* op;
    int offset;
    int array_size;  // For arrays
    char* struct_type;  // For struct instances
} AstNode;

// Symbol table
typedef struct {
    char* name;
    int offset;
    int size;  // For arrays: number of elements; for structs: size in bytes
    char* type_name;  // For struct instances
} Symbol;

typedef struct {
    Symbol* symbols;
    int count;
    int stack_size;
} SymbolTable;

// String table for .data section
typedef struct {
    char* label;
    char* value;
    int len;
} StringEntry;

typedef struct {
    StringEntry* strings;
    int count;
} StringTable;

typedef struct { Tok* tokens; int pos, count; } Parser;
typedef struct {
    FILE* out;
    int label_count;
    SymbolTable* symtab;
    StringTable* strtab;
    TypeTable* types;
    char* code_buf;
    int code_len;
    int code_cap;
} Codegen;

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
    tt->types = realloc(tt->types, sizeof(StructType) * tt->count);
    tt->types[tt->count - 1].name = strdup(name);
    tt->types[tt->count - 1].fields = NULL;
    tt->types[tt->count - 1].field_count = 0;
    tt->types[tt->count - 1].size = 0;
}

void typetab_add_field(TypeTable* tt, char* struct_name, char* field_name) {
    StructType* st = typetab_lookup(tt, struct_name);
    if (!st) return;

    st->field_count++;
    st->fields = realloc(st->fields, sizeof(StructField) * st->field_count);
    st->fields[st->field_count - 1].name = strdup(field_name);
    st->fields[st->field_count - 1].offset = st->size;
    st->size += 8;  // Each field is 8 bytes (i32 for now)
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

// ==== STRING TABLE ====
StringTable* strtab_new() {
    StringTable* st = calloc(1, sizeof(StringTable));
    return st;
}

char* strtab_add(StringTable* st, char* value, int len) {
    st->count++;
    st->strings = realloc(st->strings, sizeof(StringEntry) * st->count);

    char* label = malloc(32);
    sprintf(label, "str_%d", st->count - 1);

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
    st->symbols = realloc(st->symbols, sizeof(Symbol) * st->count);
    int bytes = size * 8;  // 8 bytes per element (i32 for now)
    st->stack_size += bytes;
    st->symbols[st->count - 1].name = strdup(name);
    st->symbols[st->count - 1].offset = -st->stack_size;
    st->symbols[st->count - 1].size = size;
    st->symbols[st->count - 1].type_name = NULL;
    return -st->stack_size;
}

int symtab_add_struct(SymbolTable* st, char* name, char* type_name, int size_bytes) {
    st->count++;
    st->symbols = realloc(st->symbols, sizeof(Symbol) * st->count);
    st->stack_size += size_bytes;
    st->symbols[st->count - 1].name = strdup(name);
    st->symbols[st->count - 1].offset = -st->stack_size;
    st->symbols[st->count - 1].size = size_bytes;
    st->symbols[st->count - 1].type_name = strdup(type_name);
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

// ==== LEXER ====
void lex_init(Lex* l, char* s) { l->src = l->cur = s; }
char peek(Lex* l) { return *l->cur; }
char peek_next(Lex* l) { return l->cur[1]; }
char adv(Lex* l) { return *l->cur++; }

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
    if (len == 5 && !memcmp(s, "while", 5)) return T_WHILE;
    if (len == 6 && !memcmp(s, "return", 6)) return T_RET;
    if (len == 6 && !memcmp(s, "struct", 6)) return T_STRUCT;  // NEW
    return T_IDENT;
}

Tok lex_tok(Lex* l) {
    skip(l);
    char* st = l->cur;
    char c = adv(l);

    if (!c) return (Tok){T_EOF, st, 0};
    if (isalpha(c) || c == '_') {
        while (isalnum(peek(l)) || peek(l) == '_') adv(l);
        return (Tok){kw(st, l->cur - st), st, l->cur - st};
    }
    if (isdigit(c)) {
        while (isdigit(peek(l))) adv(l);
        return (Tok){T_NUM, st, l->cur - st};
    }
    if (c == '"') {
        while (peek(l) != '"' && peek(l)) { if (peek(l) == '\\') adv(l); adv(l); }
        if (peek(l) == '"') adv(l);
        return (Tok){T_STR, st, l->cur - st};
    }

    if (c == '(') return (Tok){T_LPAREN, st, 1};
    if (c == ')') return (Tok){T_RPAREN, st, 1};
    if (c == '{') return (Tok){T_LBRACE, st, 1};
    if (c == '}') return (Tok){T_RBRACE, st, 1};
    if (c == '[') return (Tok){T_LBRACKET, st, 1};
    if (c == ']') return (Tok){T_RBRACKET, st, 1};
    if (c == ';') return (Tok){T_SEMI, st, 1};
    if (c == ':') return (Tok){T_COLON, st, 1};
    if (c == ',') return (Tok){T_COMMA, st, 1};
    if (c == '.') return (Tok){T_DOT, st, 1};
    if (c == '+') return (Tok){T_PLUS, st, 1};
    if (c == '*') return (Tok){T_STAR, st, 1};
    if (c == '/') return (Tok){T_SLASH, st, 1};
    if (c == '=' && peek(l) == '=') { adv(l); return (Tok){T_EQEQ, st, 2}; }
    if (c == '=') return (Tok){T_EQ, st, 1};
    if (c == '!' && peek(l) == '=') { adv(l); return (Tok){T_NEQ, st, 2}; }
    if (c == '<' && peek(l) == '=') { adv(l); return (Tok){T_LTE, st, 2}; }
    if (c == '<') return (Tok){T_LT, st, 1};
    if (c == '>' && peek(l) == '=') { adv(l); return (Tok){T_GTE, st, 2}; }
    if (c == '>') return (Tok){T_GT, st, 1};
    if (c == '-' && peek(l) == '>') { adv(l); return (Tok){T_ARROW, st, 2}; }
    if (c == '-') return (Tok){T_MINUS, st, 1};

    return (Tok){T_EOF, st, 0};
}

Tok* tokenize(char* src, int* count) {
    Lex l; lex_init(&l, src);
    Tok* toks = malloc(sizeof(Tok) * 2000);
    *count = 0;
    do { toks[(*count)++] = lex_tok(&l); } while (toks[*count - 1].t != T_EOF);
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
    p->children = realloc(p->children, sizeof(AstNode*) * p->child_count);
    p->children[p->child_count - 1] = c;
}

Tok peek_tok(Parser* p) { return p->tokens[p->pos]; }
Tok advance_tok(Parser* p) { return p->tokens[p->pos++]; }
int check_tok(Parser* p, TokType t) { return peek_tok(p).t == t; }
int match_tok(Parser* p, TokType t) { if (check_tok(p, t)) { advance_tok(p); return 1; } return 0; }
void expect(Parser* p, TokType t) { if (!match_tok(p, t)) { fprintf(stderr, "Parse error\n"); exit(1); }}

AstNode* parse_expr(Parser* p);
AstNode* parse_stmt(Parser* p);

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
        // Array literal: [1, 2, 3]
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

        // Struct literal: Point { x: 10, y: 20 }
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
    fprintf(stderr, "Parse error\n"); exit(1);
}

AstNode* parse_postfix(Parser* p) {
    AstNode* left = parse_primary(p);
    while (1) {
        if (check_tok(p, T_LBRACKET)) {
            // Array indexing: arr[index]
            advance_tok(p);
            AstNode* index_node = ast_new(AST_INDEX);
            ast_add(index_node, left);
            ast_add(index_node, parse_expr(p));
            expect(p, T_RBRACKET);
            left = index_node;
        } else if (check_tok(p, T_DOT)) {
            // Field access: obj.field
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
    while (check_tok(p, T_STAR) || check_tok(p, T_SLASH)) {
        Tok op = advance_tok(p);
        AstNode* right = parse_postfix(p);
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

AstNode* parse_expr(Parser* p) {
    return parse_comparison(p);
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
        if (match_tok(p, T_COLON)) advance_tok(p);
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
    AstNode* expr = parse_expr(p);
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

// Parse struct definition
AstNode* parse_struct_def(Parser* p) {
    expect(p, T_STRUCT);
    Tok name = advance_tok(p);
    AstNode* struct_def = ast_new(AST_STRUCT_DEF);
    struct_def->name = strndup(name.s, name.len);

    expect(p, T_LBRACE);
    while (!check_tok(p, T_RBRACE)) {
        Tok field_name = advance_tok(p);
        expect(p, T_COLON);
        advance_tok(p);  // Type (ignored for now, assume i32)

        AstNode* field = ast_new(AST_IDENT);
        field->name = strndup(field_name.s, field_name.len);
        ast_add(struct_def, field);

        if (!check_tok(p, T_RBRACE)) expect(p, T_COMMA);
    }
    expect(p, T_RBRACE);

    return struct_def;
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
        if (match_tok(p, T_COLON)) advance_tok(p);
        if (!check_tok(p, T_RPAREN)) expect(p, T_COMMA);
    }
    expect(p, T_RPAREN);

    if (match_tok(p, T_ARROW)) advance_tok(p);
    ast_add(func, parse_block(p));
    return func;
}

AstNode* parse(Parser* p) {
    AstNode* prog = ast_new(AST_PROGRAM);
    while (!check_tok(p, T_EOF)) {
        if (check_tok(p, T_STRUCT)) {
            ast_add(prog, parse_struct_def(p));
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
        cg->code_buf = realloc(cg->code_buf, cg->code_cap);
    }

    vsnprintf(cg->code_buf + cg->code_len, needed + 1, fmt, args);
    cg->code_len += needed;

    va_end(args);
}

int new_label(Codegen* cg) { return cg->label_count++; }

void gen_expr(Codegen* cg, AstNode* n);

void gen_expr(Codegen* cg, AstNode* n) {
    if (n->type == AST_NUMBER) {
        emit(cg, "    mov rax, %s\n", n->value);
    } else if (n->type == AST_STRING) {
        char* label = strtab_add(cg->strtab, n->value, strlen(n->value));
        emit(cg, "    mov rax, %s\n", label);
        emit(cg, "    mov rbx, %d\n", (int)strlen(n->value));
    } else if (n->type == AST_IDENT) {
        int off = symtab_lookup(cg->symtab, n->name);
        if (off) emit(cg, "    mov rax, [rbp%d]\n", off);
        else emit(cg, "    mov rax, 0  ; unknown var %s\n", n->name);
    } else if (n->type == AST_ASSIGN) {
        gen_expr(cg, n->children[0]);
        int off = symtab_lookup(cg->symtab, n->name);
        if (off) emit(cg, "    mov [rbp%d], rax\n", off);
    } else if (n->type == AST_BINOP) {
        gen_expr(cg, n->children[0]);
        emit(cg, "    push rax\n");
        gen_expr(cg, n->children[1]);
        emit(cg, "    mov rbx, rax\n    pop rax\n");
        if (n->op[0] == '+') emit(cg, "    add rax, rbx\n");
        else if (n->op[0] == '-') emit(cg, "    sub rax, rbx\n");
        else if (n->op[0] == '*') emit(cg, "    imul rax, rbx\n");
        else if (n->op[0] == '/') emit(cg, "    xor rdx, rdx\n    idiv rbx\n");
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
    } else if (n->type == AST_CALL) {
        // Builtins
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
            // Print newline (use safe offset far from user variables)
            emit(cg, "    mov byte [rbp-256], 10\n");  // '\n'
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
        } else {
            // Regular function call
            const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            for (int i = 0; i < n->child_count && i < 6; i++) {
                gen_expr(cg, n->children[i]);
                emit(cg, "    mov %s, rax\n", regs[i]);
            }
            emit(cg, "    call %s\n", n->name);
        }
    } else if (n->type == AST_ARRAY_LITERAL) {
        // Array literal: Store to stack, return base address
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
        // Array indexing: arr[index]
        AstNode* arr = n->children[0];
        Symbol* sym = symtab_lookup_symbol(cg->symtab, arr->name);
        if (sym) {
            gen_expr(cg, n->children[1]);
            emit(cg, "    imul rax, 8\n");
            emit(cg, "    mov rbx, rax\n");
            emit(cg, "    mov rax, [rbp%d+rbx]\n", sym->offset);
        }
    } else if (n->type == AST_STRUCT_LITERAL) {
        // Struct literal: Point { x: 10, y: 20 }
        // Store fields to stack
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
        // Field access: obj.field
        AstNode* obj = n->children[0];
        char* field_name = n->name;

        Symbol* sym = symtab_lookup_symbol(cg->symtab, obj->name);
        if (sym && sym->type_name) {
            int field_off = typetab_field_offset(cg->types, sym->type_name, field_name);
            if (field_off >= 0) {
                emit(cg, "    mov rax, [rbp%d]\n", sym->offset + field_off);
            } else {
                emit(cg, "    ; unknown field %s\n", field_name);
            }
        }
    }
}

void gen_stmt(Codegen* cg, AstNode* n);

void gen_stmt(Codegen* cg, AstNode* n) {
    if (n->type == AST_RETURN) {
        if (n->child_count > 0) gen_expr(cg, n->children[0]);
        else emit(cg, "    xor rax, rax\n");
        emit(cg, "    leave\n    ret\n");
    } else if (n->type == AST_LET) {
        int size = 1;  // Default: scalar
        char* type_name = NULL;

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
    } else if (n->type == AST_CALL || n->type == AST_ASSIGN) {
        gen_expr(cg, n);
    }
}

void gen_func(Codegen* cg, AstNode* n) {
    emit(cg, "\n%s:\n", n->name);
    emit(cg, "    push rbp\n    mov rbp, rsp\n");

    int param_count = n->child_count - 1;
    const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

    SymbolTable* old_symtab = cg->symtab;
    cg->symtab = symtab_new();

    for (int i = 0; i < param_count && i < 6; i++) {
        int off = symtab_add(cg->symtab, n->children[i]->name, 1);
        emit(cg, "    mov [rbp%d], %s\n", off, regs[i]);
    }

    emit(cg, "    sub rsp, 128\n");  // Increased for structs

    AstNode* body = n->children[param_count];
    for (int i = 0; i < body->child_count; i++)
        gen_stmt(cg, body->children[i]);

    emit(cg, "    xor rax, rax\n    leave\n    ret\n");

    cg->symtab = old_symtab;
}

void gen_helpers(Codegen* cg) {
    // __print_int: Convert rax to decimal string and print
    emit(cg, "\n__print_int:\n");
    emit(cg, "    push rbp\n    mov rbp, rsp\n");
    emit(cg, "    sub rsp, 32\n");

    // Handle negative
    emit(cg, "    mov rbx, rax\n");
    emit(cg, "    test rbx, rbx\n");
    emit(cg, "    jns .positive\n");
    emit(cg, "    neg rbx\n");
    emit(cg, "    push rbx\n");
    emit(cg, "    mov byte [rbp-1], 45\n");  // '-'
    emit(cg, "    lea rsi, [rbp-1]\n");
    emit(cg, "    mov rdi, 1\n    mov rdx, 1\n    mov rax, 1\n    syscall\n");
    emit(cg, "    pop rbx\n");

    emit(cg, ".positive:\n");
    // Convert to string
    emit(cg, "    lea rdi, [rbp-32]\n");
    emit(cg, "    mov rax, rbx\n");
    emit(cg, "    mov rcx, 10\n");

    emit(cg, ".loop:\n");
    emit(cg, "    xor rdx, rdx\n");
    emit(cg, "    div rcx\n");
    emit(cg, "    add dl, 48\n");  // '0'
    emit(cg, "    mov [rdi], dl\n");
    emit(cg, "    inc rdi\n");
    emit(cg, "    test rax, rax\n");
    emit(cg, "    jnz .loop\n");

    // Print reversed
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
}

void build_type_table(TypeTable* tt, AstNode* ast) {
    for (int i = 0; i < ast->child_count; i++) {
        if (ast->children[i]->type == AST_STRUCT_DEF) {
            AstNode* struct_def = ast->children[i];
            typetab_add(tt, struct_def->name);

            for (int j = 0; j < struct_def->child_count; j++) {
                typetab_add_field(tt, struct_def->name, struct_def->children[j]->name);
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

    // Generate code
    emit(&cg, "\nsection .text\n    global _start\n\n");
    emit(&cg, "_start:\n    call main\n    mov rdi, rax\n");
    emit(&cg, "    mov rax, 60\n    syscall\n");

    // Generate helper functions
    gen_helpers(&cg);

    for (int i = 0; i < ast->child_count; i++) {
        if (ast->children[i]->type == AST_FUNCTION) {
            gen_func(&cg, ast->children[i]);
        }
    }

    // Write to file
    cg.out = fopen(file, "w");
    fprintf(cg.out, "; CHRONOS v0.8 - Structs Support\n\n");
    fprintf(cg.out, "section .data\n");

    for (int i = 0; i < strtab->count; i++) {
        fprintf(cg.out, "%s: db ", strtab->strings[i].label);
        for (int j = 0; j < strtab->strings[i].len; j++) {
            fprintf(cg.out, "%d", (unsigned char)strtab->strings[i].value[j]);
            if (j < strtab->strings[i].len - 1) fprintf(cg.out, ", ");
        }
        fprintf(cg.out, "\n");
    }

    fprintf(cg.out, "%s", cg.code_buf);

    fclose(cg.out);
    free(cg.code_buf);
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("Usage: chronos <file.ch>\n"); return 1; }

    FILE* f = fopen(argv[1], "r");
    if (!f) { perror("Error"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* src = malloc(size + 1);
    fread(src, 1, size, f);
    src[size] = '\0';
    fclose(f);

    printf("🔥 CHRONOS v0.8 - STRUCTS\n");
    printf("Struct definitions + literals + field access enabled\n");
    printf("Compiling: %s\n", argv[1]);

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
