/* CHRONOS COMPILER - Complete bootstrap compiler
 * Lexer + Parser + Codegen integrated
 * Author: ipenas-cl
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

// TOKEN TYPES
typedef enum {
    T_EOF, T_IDENT, T_NUM, T_STR,
    T_FN, T_LET, T_IF, T_ELSE, T_WHILE, T_FOR, T_RET, T_STRUCT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_SEMI, T_COLON, T_COMMA,
    T_PLUS, T_MINUS, T_STAR, T_SLASH,
    T_EQ, T_EQEQ, T_ARROW
} TokType;

typedef struct { TokType t; char* s; int len, line, col; } Tok;
typedef struct { char* src; char* cur; int line, col; } Lex;

// AST TYPES
typedef enum {
    AST_PROGRAM, AST_FUNCTION, AST_BLOCK, AST_RETURN,
    AST_LET, AST_CALL, AST_IDENT, AST_NUMBER, AST_BINOP
} AstType;

typedef struct AstNode {
    AstType type;
    char* name;
    struct AstNode** children;
    int child_count;
    char* value;
} AstNode;

typedef struct { Tok* tokens; int pos, count; } Parser;
typedef struct { FILE* out; } Codegen;

// ==== LEXER ====
void lex_init(Lex* l, char* s) { l->src = l->cur = s; l->line = 1; l->col = 1; }
char peek(Lex* l) { return *l->cur; }
char adv(Lex* l) { l->col++; return *l->cur++; }

void skip(Lex* l) {
    for (;;) {
        char c = peek(l);
        if (c == ' ' || c == '\t' || c == '\r') adv(l);
        else if (c == '\n') { l->line++; l->col = 0; adv(l); }
        else if (c == '/' && l->cur[1] == '/') {
            while (peek(l) != '\n' && peek(l)) adv(l);
        } else break;
    }
}

TokType kw(char* s, int len) {
    if (len == 2 && !memcmp(s, "fn", 2)) return T_FN;
    if (len == 3 && !memcmp(s, "let", 3)) return T_LET;
    if (len == 6 && !memcmp(s, "return", 6)) return T_RET;
    return T_IDENT;
}

Tok lex_tok(Lex* l) {
    skip(l);
    char* st = l->cur;
    int sline = l->line, scol = l->col;
    char c = adv(l);
    
    if (!c) return (Tok){T_EOF, st, 0, sline, scol};
    if (isalpha(c) || c == '_') {
        while (isalnum(peek(l)) || peek(l) == '_') adv(l);
        return (Tok){kw(st, l->cur - st), st, l->cur - st, sline, scol};
    }
    if (isdigit(c)) {
        while (isdigit(peek(l))) adv(l);
        return (Tok){T_NUM, st, l->cur - st, sline, scol};
    }
    
    if (c == '(') return (Tok){T_LPAREN, st, 1, sline, scol};
    if (c == ')') return (Tok){T_RPAREN, st, 1, sline, scol};
    if (c == '{') return (Tok){T_LBRACE, st, 1, sline, scol};
    if (c == '}') return (Tok){T_RBRACE, st, 1, sline, scol};
    if (c == ';') return (Tok){T_SEMI, st, 1, sline, scol};
    if (c == ':') return (Tok){T_COLON, st, 1, sline, scol};
    if (c == ',') return (Tok){T_COMMA, st, 1, sline, scol};
    if (c == '+') return (Tok){T_PLUS, st, 1, sline, scol};
    if (c == '=') return (Tok){T_EQ, st, 1, sline, scol};
    if (c == '-' && peek(l) == '>') { adv(l); return (Tok){T_ARROW, st, 2, sline, scol}; }
    if (c == '-') return (Tok){T_MINUS, st, 1, sline, scol};
    return (Tok){T_EOF, st, 0, sline, scol};
}

Tok* tokenize(char* src, int* count) {
    Lex l; lex_init(&l, src);
    Tok* toks = malloc(sizeof(Tok) * 1000);
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

void expect(Parser* p, TokType t) {
    if (!match_tok(p, t)) { fprintf(stderr, "Parse error\n"); exit(1); }
}

AstNode* parse_expr(Parser* p);

AstNode* parse_primary(Parser* p) {
    if (check_tok(p, T_NUM)) {
        Tok t = advance_tok(p);
        AstNode* n = ast_new(AST_NUMBER);
        n->value = strndup(t.s, t.len);
        return n;
    }
    if (check_tok(p, T_IDENT)) {
        Tok t = advance_tok(p);
        AstNode* n = ast_new(AST_IDENT);
        n->name = strndup(t.s, t.len);
        return n;
    }
    fprintf(stderr, "Parse error: unexpected token\n"); exit(1);
}

AstNode* parse_expr(Parser* p) {
    AstNode* left = parse_primary(p);
    while (check_tok(p, T_PLUS) || check_tok(p, T_MINUS)) {
        Tok op = advance_tok(p);
        AstNode* right = parse_primary(p);
        AstNode* binop = ast_new(AST_BINOP);
        binop->value = strndup(op.s, op.len);
        ast_add(binop, left);
        ast_add(binop, right);
        left = binop;
    }
    return left;
}

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
    fprintf(stderr, "Unknown statement\n"); exit(1);
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
    vfprintf(cg->out, fmt, args);
    va_end(args);
}

void gen_expr(Codegen* cg, AstNode* n) {
    if (n->type == AST_NUMBER) emit(cg, "    mov rax, %s\n", n->value);
    else if (n->type == AST_IDENT) emit(cg, "    mov rax, 0  ; %s\n", n->name);
    else if (n->type == AST_BINOP) {
        gen_expr(cg, n->children[0]);
        emit(cg, "    push rax\n");
        gen_expr(cg, n->children[1]);
        emit(cg, "    mov rbx, rax\n    pop rax\n");
        if (n->value[0] == '+') emit(cg, "    add rax, rbx\n");
        else emit(cg, "    sub rax, rbx\n");
    }
}

void gen_stmt(Codegen* cg, AstNode* n) {
    if (n->type == AST_RETURN) {
        if (n->child_count > 0) gen_expr(cg, n->children[0]);
        else emit(cg, "    xor rax, rax\n");
        emit(cg, "    ret\n");
    } else if (n->type == AST_LET && n->child_count > 0) {
        gen_expr(cg, n->children[0]);
    }
}

void gen_block(Codegen* cg, AstNode* n) {
    for (int i = 0; i < n->child_count; i++) gen_stmt(cg, n->children[i]);
}

void gen_func(Codegen* cg, AstNode* n) {
    emit(cg, "\n%s:\n", n->name);
    gen_block(cg, n->children[0]);
}

void codegen(AstNode* ast, const char* file) {
    Codegen cg; cg.out = fopen(file, "w");
    emit(&cg, "; CHRONOS v0.1 - Deterministic Systems Language\n\n");
    emit(&cg, "section .text\n    global _start\n\n");
    emit(&cg, "_start:\n    call main\n    mov rdi, rax\n");
    emit(&cg, "    mov rax, 60\n    syscall\n");
    for (int i = 0; i < ast->child_count; i++) gen_func(&cg, ast->children[i]);
    fclose(cg.out);
}

// ==== MAIN ====
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: chronos <file.ch>\n");
        return 1;
    }
    
    FILE* f = fopen(argv[1], "r");
    if (!f) { perror("Error"); return 1; }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* src = malloc(size + 1);
    fread(src, 1, size, f);
    src[size] = '\0';
    fclose(f);
    
    printf("🔥 CHRONOS COMPILER v0.1\n");
    printf("Compiling: %s\n", argv[1]);
    
    int count;
    Tok* toks = tokenize(src, &count);
    Parser parser = {toks, 0, count};
    AstNode* ast = parse(&parser);
    
    codegen(ast, "output.asm");
    
    printf("✅ Generated output.asm\n");
    printf("📦 Assembling...\n");
    
    system("nasm -f elf64 output.asm -o output.o");
    system("ld output.o -o chronos_program");
    
    printf("✅ Compilation complete!\n");
    printf("Run: ./chronos_program\n");
    
    return 0;
}
