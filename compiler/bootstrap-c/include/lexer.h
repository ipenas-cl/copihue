/*
 * TEMPO LEXER - Tokenizer for Tempo Programming Language
 * Author: Ignacio Peña Sepúlveda (ipenas-cl)
 * Date: 2025-10-20
 */

#ifndef TEMPO_LEXER_H
#define TEMPO_LEXER_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_IDENT, TOKEN_NUMBER, TOKEN_STRING,
    TOKEN_FN, TOKEN_LET, TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, 
    TOKEN_FOR, TOKEN_RETURN, TOKEN_STRUCT, TOKEN_ENUM, TOKEN_TYPE,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_BANG_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_AMPERSAND_AMPERSAND, TOKEN_PIPE_PIPE,
    TOKEN_ARROW,
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_SEMICOLON, TOKEN_COLON, TOKEN_COMMA, TOKEN_DOT, TOKEN_AT,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    size_t length;
    int line;
    int column;
} Token;

typedef struct {
    const char* start;
    const char* current;
    const char* source;
    int line;
    int column;
} Lexer;

void lexer_init(Lexer* lexer, const char* source);
Token lexer_next_token(Lexer* lexer);
const char* token_type_string(TokenType type);
void token_print(Token* token);

#endif
