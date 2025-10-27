/* CHRONOS Bootstrap Lexer - Minimal viable implementation
 * Author: ipenas-cl
 * Goal: Get working, then rewrite in Chronos to kill C
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    T_EOF, T_IDENT, T_NUM, T_STR,
    T_FN, T_LET, T_IF, T_ELSE, T_WHILE, T_FOR, T_RET, T_STRUCT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_LBRACK, T_RBRACK,
    T_SEMI, T_COLON, T_COMMA, T_DOT,
    T_PLUS, T_MINUS, T_STAR, T_SLASH,
    T_EQ, T_EQEQ, T_NEQ, T_LT, T_GT, T_LTE, T_GTE,
    T_AND, T_OR, T_ARROW
} TokType;

typedef struct { TokType t; char* s; int len, line, col; } Tok;
typedef struct { char* src; char* cur; int line, col; } Lex;

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
    if (len == 2 && !memcmp(s, "if", 2)) return T_IF;
    if (len == 4 && !memcmp(s, "else", 4)) return T_ELSE;
    if (len == 5 && !memcmp(s, "while", 5)) return T_WHILE;
    if (len == 3 && !memcmp(s, "for", 3)) return T_FOR;
    if (len == 6 && !memcmp(s, "return", 6)) return T_RET;
    if (len == 6 && !memcmp(s, "struct", 6)) return T_STRUCT;
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
        int len = l->cur - st;
        return (Tok){kw(st, len), st, len, sline, scol};
    }
    
    if (isdigit(c)) {
        while (isdigit(peek(l))) adv(l);
        if (peek(l) == '.' && isdigit(l->cur[1])) {
            adv(l);
            while (isdigit(peek(l))) adv(l);
        }
        return (Tok){T_NUM, st, l->cur - st, sline, scol};
    }
    
    switch (c) {
        case '(': return (Tok){T_LPAREN, st, 1, sline, scol};
        case ')': return (Tok){T_RPAREN, st, 1, sline, scol};
        case '{': return (Tok){T_LBRACE, st, 1, sline, scol};
        case '}': return (Tok){T_RBRACE, st, 1, sline, scol};
        case '[': return (Tok){T_LBRACK, st, 1, sline, scol};
        case ']': return (Tok){T_RBRACK, st, 1, sline, scol};
        case ';': return (Tok){T_SEMI, st, 1, sline, scol};
        case ':': return (Tok){T_COLON, st, 1, sline, scol};
        case ',': return (Tok){T_COMMA, st, 1, sline, scol};
        case '.': return (Tok){T_DOT, st, 1, sline, scol};
        case '+': return (Tok){T_PLUS, st, 1, sline, scol};
        case '*': return (Tok){T_STAR, st, 1, sline, scol};
        case '/': return (Tok){T_SLASH, st, 1, sline, scol};
        case '=':
            if (peek(l) == '=') { adv(l); return (Tok){T_EQEQ, st, 2, sline, scol}; }
            return (Tok){T_EQ, st, 1, sline, scol};
        case '!':
            if (peek(l) == '=') { adv(l); return (Tok){T_NEQ, st, 2, sline, scol}; }
            break;
        case '<':
            if (peek(l) == '=') { adv(l); return (Tok){T_LTE, st, 2, sline, scol}; }
            return (Tok){T_LT, st, 1, sline, scol};
        case '>':
            if (peek(l) == '=') { adv(l); return (Tok){T_GTE, st, 2, sline, scol}; }
            return (Tok){T_GT, st, 1, sline, scol};
        case '&':
            if (peek(l) == '&') { adv(l); return (Tok){T_AND, st, 2, sline, scol}; }
            break;
        case '|':
            if (peek(l) == '|') { adv(l); return (Tok){T_OR, st, 2, sline, scol}; }
            break;
        case '-':
            if (peek(l) == '>') { adv(l); return (Tok){T_ARROW, st, 2, sline, scol}; }
            return (Tok){T_MINUS, st, 1, sline, scol};
    }
    return (Tok){T_EOF, st, 0, sline, scol};
}

const char* tok_name(TokType t) {
    const char* n[] = {"EOF","ID","NUM","STR","fn","let","if","else","while","for",
                       "return","struct","(",")","{","}","[","]",";","::",",",".",
                       "+","-","*","/","=","==","!=","<",">","<=",">=","&&","||","->"};
    return t < sizeof(n)/sizeof(n[0]) ? n[t] : "?";
}

void tok_print(Tok* t) {
    printf("%-8s '%.*s' @ %d:%d\n", tok_name(t->t), t->len, t->s, t->line, t->col);
}

int main(int argc, char** argv) {
    char code[] = "fn main() -> i32 {\n    let x = 42;\n    return x;\n}";
    
    printf("🔥 CHRONOS LEXER - Bootstrap Test\n");
    printf("Goal: Destroy C/C++/Go/Rust\n");
    printf("\nTokenizing:\n%s\n\n", code);
    
    Lex lex;
    lex_init(&lex, code);
    
    Tok t;
    do {
        t = lex_tok(&lex);
        tok_print(&t);
    } while (t.t != T_EOF);
    
    printf("\n✅ Lexer works! Phase 0 started.\n");
    return 0;
}
