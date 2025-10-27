/* CHRONOS v0.4 - Print() + Strings for debugging
 * Adding I/O for production debugging
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
    T_FN, T_LET, T_IF, T_ELSE, T_WHILE, T_FOR, T_RET,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_SEMI, T_COLON, T_COMMA,
    T_PLUS, T_MINUS, T_STAR, T_SLASH,
    T_EQ, T_EQEQ, T_NEQ, T_LT, T_GT, T_LTE, T_GTE, T_ARROW
} TokType;

typedef struct { TokType t; char* s; int len; } Tok;
typedef struct { char* src; char* cur; } Lex;

// AST
typedef enum {
    AST_PROGRAM, AST_FUNCTION, AST_BLOCK, AST_RETURN, AST_LET,
    AST_IF, AST_WHILE, AST_CALL, AST_IDENT, AST_NUMBER,
    AST_BINOP, AST_COMPARE, AST_STRING, AST_ASSIGN
} AstType;

typedef struct AstNode {
    AstType type;
    char* name;
    struct AstNode** children;
    int child_count;
    char* value;
    char* op;
    int offset;
} AstNode;

// Symbol table
typedef struct {
    char* name;
    int offset;
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
    char* code_buf;
    int code_len;
    int code_cap;
} Codegen;

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

int symtab_add(SymbolTable* st, char* name) {
    st->count++;
    st->symbols = realloc(st->symbols, sizeof(Symbol) * st->count);
    st->stack_size += 8;
    st->symbols[st->count - 1].name = strdup(name);
    st->symbols[st->count - 1].offset = -st->stack_size;
    return -st->stack_size;
}

int symtab_lookup(SymbolTable* st, char* name) {
    for (int i = 0; i < st->count; i++) {
        if (!strcmp(st->symbols[i].name, name)) {
            return st->symbols[i].offset;
        }
    }
    return 0;
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
    if (c == ';') return (Tok){T_SEMI, st, 1};
    if (c == ':') return (Tok){T_COLON, st, 1};
    if (c == ',') return (Tok){T_COMMA, st, 1};
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
        n->value = strndup(t.s + 1, t.len - 2);  // Strip quotes
        return n;
    }
    if (check_tok(p, T_IDENT)) {
        Tok t = advance_tok(p);
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
        if (check_tok(p, T_EQ)) {
            advance_tok(p);
            AstNode* assign = ast_new(AST_ASSIGN);
            assign->name = strndup(t.s, t.len);
            ast_add(assign, parse_expr(p));
            return assign;
        }
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

AstNode* parse_comparison(Parser* p) {
    AstNode* left = parse_primary(p);
    while (check_tok(p, T_EQEQ) || check_tok(p, T_NEQ) ||
           check_tok(p, T_LT) || check_tok(p, T_GT) ||
           check_tok(p, T_LTE) || check_tok(p, T_GTE)) {
        Tok op = advance_tok(p);
        AstNode* right = parse_primary(p);
        AstNode* cmp = ast_new(AST_COMPARE);
        cmp->op = strndup(op.s, op.len);
        ast_add(cmp, left);
        ast_add(cmp, right);
        left = cmp;
    }
    return left;
}

AstNode* parse_expr(Parser* p) {
    AstNode* left = parse_comparison(p);
    while (check_tok(p, T_PLUS) || check_tok(p, T_MINUS) ||
           check_tok(p, T_STAR) || check_tok(p, T_SLASH)) {
        Tok op = advance_tok(p);
        AstNode* right = parse_comparison(p);
        AstNode* binop = ast_new(AST_BINOP);
        binop->op = strndup(op.s, op.len);
        ast_add(binop, left);
        ast_add(binop, right);
        left = binop;
    }
    return left;
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
    while (!check_tok(p, T_EOF)) ast_add(prog, parse_func(p));
    return prog;
}

// ==== CODEGEN ====
void emit(Codegen* cg, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Calculate needed size
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    // Ensure buffer capacity
    while (cg->code_len + needed + 1 > cg->code_cap) {
        cg->code_cap = cg->code_cap ? cg->code_cap * 2 : 4096;
        cg->code_buf = realloc(cg->code_buf, cg->code_cap);
    }

    // Write to buffer
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
        // Add string to data section and return its label
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
        // Check for builtin print()
        if (!strcmp(n->name, "print")) {
            if (n->child_count > 0) {
                gen_expr(cg, n->children[0]);
                // rax = string address, rbx = length
                emit(cg, "    mov rsi, rax\n");
                emit(cg, "    mov rdx, rbx\n");
                emit(cg, "    mov rdi, 1\n");      // stdout
                emit(cg, "    mov rax, 1\n");      // sys_write
                emit(cg, "    syscall\n");
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
}

void gen_stmt(Codegen* cg, AstNode* n);

void gen_stmt(Codegen* cg, AstNode* n) {
    if (n->type == AST_RETURN) {
        if (n->child_count > 0) gen_expr(cg, n->children[0]);
        else emit(cg, "    xor rax, rax\n");
        emit(cg, "    leave\n    ret\n");
    } else if (n->type == AST_LET) {
        int off = symtab_add(cg->symtab, n->name);
        if (n->child_count > 0) {
            gen_expr(cg, n->children[0]);
            emit(cg, "    mov [rbp%d], rax\n", off);
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
        int off = symtab_add(cg->symtab, n->children[i]->name);
        emit(cg, "    mov [rbp%d], %s\n", off, regs[i]);
    }

    emit(cg, "    sub rsp, 64\n");

    AstNode* body = n->children[param_count];
    for (int i = 0; i < body->child_count; i++)
        gen_stmt(cg, body->children[i]);

    emit(cg, "    xor rax, rax\n    leave\n    ret\n");

    cg->symtab = old_symtab;
}

void codegen(AstNode* ast, const char* file, StringTable* strtab) {
    Codegen cg;
    cg.out = NULL;  // Will open later
    cg.label_count = 0;
    cg.symtab = NULL;
    cg.strtab = strtab;
    cg.code_buf = NULL;
    cg.code_len = 0;
    cg.code_cap = 0;

    // Generate all code to buffer (this will populate string table)
    emit(&cg, "\nsection .text\n    global _start\n\n");
    emit(&cg, "_start:\n    call main\n    mov rdi, rax\n");
    emit(&cg, "    mov rax, 60\n    syscall\n");

    for (int i = 0; i < ast->child_count; i++)
        gen_func(&cg, ast->children[i]);

    // Now write to file: .data first, then code
    cg.out = fopen(file, "w");
    fprintf(cg.out, "; CHRONOS v0.4 - Print() + Strings\n\n");
    fprintf(cg.out, "section .data\n");

    // Emit string literals (now populated)
    for (int i = 0; i < strtab->count; i++) {
        fprintf(cg.out, "%s: db ", strtab->strings[i].label);
        for (int j = 0; j < strtab->strings[i].len; j++) {
            fprintf(cg.out, "%d", (unsigned char)strtab->strings[i].value[j]);
            if (j < strtab->strings[i].len - 1) fprintf(cg.out, ", ");
        }
        fprintf(cg.out, "\n");
    }

    // Write buffered code
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

    printf("🔥 CHRONOS v0.4 - I/O ENABLED\n");
    printf("print() syscall integration\n");
    printf("Compiling: %s\n", argv[1]);

    int count;
    Tok* toks = tokenize(src, &count);
    Parser parser = {toks, 0, count};
    AstNode* ast = parse(&parser);

    StringTable* strtab = strtab_new();
    codegen(ast, "output.asm", strtab);

    printf("✅ Code generated\n");
    system("nasm -f elf64 output.asm -o output.o 2>&1 | head -5");
    system("ld output.o -o chronos_program 2>&1 | head -5");
    printf("✅ Compilation complete: ./chronos_program\n");

    return 0;
}
