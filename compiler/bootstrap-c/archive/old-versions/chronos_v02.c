/* CHRONOS v0.2 - Control Flow + Function Calls
 * Adding: if/else, while, function calls, comparison ops
 * Author: ipenas-cl
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

typedef enum {
    T_EOF, T_IDENT, T_NUM, T_STR,
    T_FN, T_LET, T_IF, T_ELSE, T_WHILE, T_FOR, T_RET, T_STRUCT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_SEMI, T_COLON, T_COMMA,
    T_PLUS, T_MINUS, T_STAR, T_SLASH,
    T_EQ, T_EQEQ, T_NEQ, T_LT, T_GT, T_LTE, T_GTE,
    T_AND, T_OR, T_ARROW
} TokType;

typedef struct { TokType t; char* s; int len, line, col; } Tok;
typedef struct { char* src; char* cur; int line, col; } Lex;

typedef enum {
    AST_PROGRAM, AST_FUNCTION, AST_BLOCK, AST_RETURN,
    AST_LET, AST_IF, AST_WHILE, AST_CALL, AST_IDENT, AST_NUMBER,
    AST_BINOP, AST_COMPARE, AST_STRING
} AstType;

typedef struct AstNode {
    AstType type;
    char* name;
    struct AstNode** children;
    int child_count;
    char* value;
    char* op;
} AstNode;

typedef struct { Tok* tokens; int pos, count; } Parser;
typedef struct { FILE* out; int label_count; } Codegen;

// ==== LEXER ====
void lex_init(Lex* l, char* s) { l->src = l->cur = s; l->line = 1; l->col = 1; }
char peek(Lex* l) { return *l->cur; }
char peek_next(Lex* l) { return l->cur[1]; }
char adv(Lex* l) { l->col++; return *l->cur++; }

void skip(Lex* l) {
    for (;;) {
        char c = peek(l);
        if (c == ' ' || c == '\t' || c == '\r') adv(l);
        else if (c == '\n') { l->line++; l->col = 0; adv(l); }
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
    
    if (c == '"') {
        while (peek(l) != '"' && peek(l)) {
            if (peek(l) == '\\') adv(l);
            adv(l);
        }
        if (peek(l) == '"') adv(l);
        return (Tok){T_STR, st, l->cur - st, sline, scol};
    }
    
    if (c == '(') return (Tok){T_LPAREN, st, 1, sline, scol};
    if (c == ')') return (Tok){T_RPAREN, st, 1, sline, scol};
    if (c == '{') return (Tok){T_LBRACE, st, 1, sline, scol};
    if (c == '}') return (Tok){T_RBRACE, st, 1, sline, scol};
    if (c == ';') return (Tok){T_SEMI, st, 1, sline, scol};
    if (c == ':') return (Tok){T_COLON, st, 1, sline, scol};
    if (c == ',') return (Tok){T_COMMA, st, 1, sline, scol};
    if (c == '+') return (Tok){T_PLUS, st, 1, sline, scol};
    if (c == '*') return (Tok){T_STAR, st, 1, sline, scol};
    if (c == '/') return (Tok){T_SLASH, st, 1, sline, scol};
    if (c == '=' && peek(l) == '=') { adv(l); return (Tok){T_EQEQ, st, 2, sline, scol}; }
    if (c == '=') return (Tok){T_EQ, st, 1, sline, scol};
    if (c == '!' && peek(l) == '=') { adv(l); return (Tok){T_NEQ, st, 2, sline, scol}; }
    if (c == '<' && peek(l) == '=') { adv(l); return (Tok){T_LTE, st, 2, sline, scol}; }
    if (c == '<') return (Tok){T_LT, st, 1, sline, scol};
    if (c == '>' && peek(l) == '=') { adv(l); return (Tok){T_GTE, st, 2, sline, scol}; }
    if (c == '>') return (Tok){T_GT, st, 1, sline, scol};
    if (c == '&' && peek(l) == '&') { adv(l); return (Tok){T_AND, st, 2, sline, scol}; }
    if (c == '|' && peek(l) == '|') { adv(l); return (Tok){T_OR, st, 2, sline, scol}; }
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
void expect(Parser* p, TokType t) { if (!match_tok(p, t)) { fprintf(stderr, "Parse error at line %d\n", peek_tok(p).line); exit(1); }}

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
        AstNode* n = ast_new(AST_IDENT);
        n->name = strndup(t.s, t.len);
        return n;
    }
    if (match_tok(p, T_LPAREN)) {
        AstNode* n = parse_expr(p);
        expect(p, T_RPAREN);
        return n;
    }
    fprintf(stderr, "Parse error: unexpected token\n"); exit(1);
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
        if (match_tok(p, T_ELSE)) {
            ast_add(ifnode, parse_block(p));
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
    // TODO: parse parameters
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

int new_label(Codegen* cg) { return cg->label_count++; }

void gen_expr(Codegen* cg, AstNode* n);

void gen_expr(Codegen* cg, AstNode* n) {
    if (n->type == AST_NUMBER) {
        emit(cg, "    mov rax, %s\n", n->value);
    } else if (n->type == AST_IDENT) {
        emit(cg, "    mov rax, 0  ; TODO: %s\n", n->name);
    } else if (n->type == AST_BINOP) {
        gen_expr(cg, n->children[0]);
        emit(cg, "    push rax\n");
        gen_expr(cg, n->children[1]);
        emit(cg, "    mov rbx, rax\n    pop rax\n");
        char op = n->op[0];
        if (op == '+') emit(cg, "    add rax, rbx\n");
        else if (op == '-') emit(cg, "    sub rax, rbx\n");
        else if (op == '*') emit(cg, "    imul rax, rbx\n");
        else if (op == '/') emit(cg, "    xor rdx, rdx\n    idiv rbx\n");
    } else if (n->type == AST_COMPARE) {
        gen_expr(cg, n->children[0]);
        emit(cg, "    push rax\n");
        gen_expr(cg, n->children[1]);
        emit(cg, "    mov rbx, rax\n    pop rax\n");
        emit(cg, "    cmp rax, rbx\n");
        
        int lab = new_label(cg);
        if (!strcmp(n->op, "==")) emit(cg, "    sete al\n");
        else if (!strcmp(n->op, "!=")) emit(cg, "    setne al\n");
        else if (!strcmp(n->op, "<")) emit(cg, "    setl al\n");
        else if (!strcmp(n->op, ">")) emit(cg, "    setg al\n");
        else if (!strcmp(n->op, "<=")) emit(cg, "    setle al\n");
        else if (!strcmp(n->op, ">=")) emit(cg, "    setge al\n");
        
        emit(cg, "    movzx rax, al\n");
    } else if (n->type == AST_CALL) {
        for (int i = 0; i < n->child_count; i++) {
            gen_expr(cg, n->children[i]);
            if (i == 0) emit(cg, "    mov rdi, rax\n");
            else if (i == 1) emit(cg, "    mov rsi, rax\n");
        }
        emit(cg, "    call %s\n", n->name);
    }
}

void gen_stmt(Codegen* cg, AstNode* n);

void gen_stmt(Codegen* cg, AstNode* n) {
    if (n->type == AST_RETURN) {
        if (n->child_count > 0) gen_expr(cg, n->children[0]);
        else emit(cg, "    xor rax, rax\n");
        emit(cg, "    ret\n");
    } else if (n->type == AST_LET && n->child_count > 0) {
        gen_expr(cg, n->children[0]);
    } else if (n->type == AST_IF) {
        int else_lab = new_label(cg);
        int end_lab = new_label(cg);
        
        gen_expr(cg, n->children[0]);
        emit(cg, "    test rax, rax\n");
        emit(cg, "    jz .L%d\n", else_lab);
        
        for (int i = 0; i < n->children[1]->child_count; i++) {
            gen_stmt(cg, n->children[1]->children[i]);
        }
        
        emit(cg, "    jmp .L%d\n", end_lab);
        emit(cg, ".L%d:\n", else_lab);
        
        if (n->child_count > 2) {
            for (int i = 0; i < n->children[2]->child_count; i++) {
                gen_stmt(cg, n->children[2]->children[i]);
            }
        }
        
        emit(cg, ".L%d:\n", end_lab);
    } else if (n->type == AST_WHILE) {
        int start_lab = new_label(cg);
        int end_lab = new_label(cg);
        
        emit(cg, ".L%d:\n", start_lab);
        gen_expr(cg, n->children[0]);
        emit(cg, "    test rax, rax\n");
        emit(cg, "    jz .L%d\n", end_lab);
        
        for (int i = 0; i < n->children[1]->child_count; i++) {
            gen_stmt(cg, n->children[1]->children[i]);
        }
        
        emit(cg, "    jmp .L%d\n", start_lab);
        emit(cg, ".L%d:\n", end_lab);
    } else if (n->type == AST_CALL) {
        gen_expr(cg, n);
    }
}

void gen_func(Codegen* cg, AstNode* n) {
    emit(cg, "\n%s:\n", n->name);
    for (int i = 0; i < n->children[0]->child_count; i++) {
        gen_stmt(cg, n->children[0]->children[i]);
    }
}

void codegen(AstNode* ast, const char* file) {
    Codegen cg; cg.out = fopen(file, "w"); cg.label_count = 0;
    emit(&cg, "; CHRONOS v0.2 - Control Flow + Function Calls\n\n");
    emit(&cg, "section .text\n    global _start\n\n");
    emit(&cg, "_start:\n    call main\n    mov rdi, rax\n");
    emit(&cg, "    mov rax, 60\n    syscall\n");
    for (int i = 0; i < ast->child_count; i++) gen_func(&cg, ast->children[i]);
    fclose(cg.out);
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
    
    printf("🔥 CHRONOS v0.2 - Control Flow Edition\n");
    printf("Compiling: %s\n", argv[1]);
    
    int count;
    Tok* toks = tokenize(src, &count);
    Parser parser = {toks, 0, count};
    AstNode* ast = parse(&parser);
    
    codegen(ast, "output.asm");
    
    printf("✅ Generated output.asm\n");
    system("nasm -f elf64 output.asm -o output.o 2>&1 | head -10");
    system("ld output.o -o chronos_program 2>&1 | head -10");
    
    printf("✅ Build complete!\n");
    return 0;
}
