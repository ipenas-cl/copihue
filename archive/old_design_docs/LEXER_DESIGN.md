# Diseño del Lexer (Tokenizador) - Chronos Compiler

**Versión:** 1.0
**Fecha:** 29 de octubre de 2025
**Para:** Chronos Self-Hosted Compiler v1.0

---

## 1. Objetivo

Convertir el source code de Chronos (texto plano) en una secuencia de **tokens** que el parser puede procesar fácilmente.

### Problema Actual
```chronos
// ANTES (char-by-char parsing):
while (source[i] == 108) { // 'l'
    if (source[i+1] == 101) { // 'e'
        if (source[i+2] == 116) { // 't'
            // Found "let"!
```

❌ Frágil, difícil de mantener, propenso a errores

### Solución Propuesta
```chronos
// DESPUÉS (token-based):
let token: *Token = lex_next(lexer);
if (token.type == TOK_LET) {
    // Found "let" keyword!
```

✅ Robusto, fácil de mantener, extensible

---

## 2. Estructuras de Datos

### 2.1 TokenType (Tipos de Tokens)

```chronos
// Token types (using i64 constants since we don't have enums yet)
let TOK_EOF: i64 = 0;           // End of file
let TOK_ERROR: i64 = 1;         // Lexical error

// Literals
let TOK_NUMBER: i64 = 10;       // 123, 456
let TOK_IDENT: i64 = 11;        // variable_name, foo, bar

// Keywords
let TOK_FN: i64 = 20;           // fn
let TOK_LET: i64 = 21;          // let
let TOK_RETURN: i64 = 22;       // return
let TOK_IF: i64 = 23;           // if
let TOK_ELSE: i64 = 24;         // else
let TOK_WHILE: i64 = 25;        // while
let TOK_STRUCT: i64 = 26;       // struct

// Types
let TOK_I64: i64 = 30;          // i64
let TOK_I8: i64 = 31;           // i8

// Operators
let TOK_PLUS: i64 = 40;         // +
let TOK_MINUS: i64 = 41;        // -
let TOK_STAR: i64 = 42;         // *
let TOK_SLASH: i64 = 43;        // /
let TOK_EQ: i64 = 44;           // =
let TOK_EQEQ: i64 = 45;         // ==
let TOK_NEQ: i64 = 46;          // !=
let TOK_LT: i64 = 47;           // <
let TOK_GT: i64 = 48;           // >
let TOK_LTEQ: i64 = 49;         // <=
let TOK_GTEQ: i64 = 50;         // >=

// Punctuation
let TOK_LPAREN: i64 = 60;       // (
let TOK_RPAREN: i64 = 61;       // )
let TOK_LBRACE: i64 = 62;       // {
let TOK_RBRACE: i64 = 63;       // }
let TOK_LBRACKET: i64 = 64;     // [
let TOK_RBRACKET: i64 = 65;     // ]
let TOK_SEMICOLON: i64 = 66;    // ;
let TOK_COLON: i64 = 67;        // :
let TOK_COMMA: i64 = 68;        // ,
let TOK_DOT: i64 = 69;          // .
let TOK_ARROW: i64 = 70;        // ->
let TOK_AMPERSAND: i64 = 71;    // &
```

### 2.2 Token Structure

```chronos
struct Token {
    type: i64,          // TokenType (one of TOK_* constants)
    value: *i8,         // Text of the token (malloc'd string)
    line: i64,          // Line number (for error messages)
    column: i64,        // Column number (for error messages)
    length: i64         // Length of token text
}
```

**Ejemplo:**
```
Source: "let x = 42;"
         ^^^^^^^^
Token 1: { type: TOK_LET,    value: "let", line: 1, column: 1,  length: 3 }
Token 2: { type: TOK_IDENT,  value: "x",   line: 1, column: 5,  length: 1 }
Token 3: { type: TOK_EQ,     value: "=",   line: 1, column: 7,  length: 1 }
Token 4: { type: TOK_NUMBER, value: "42",  line: 1, column: 9,  length: 2 }
Token 5: { type: TOK_SEMICOLON, value: ";", line: 1, column: 11, length: 1 }
Token 6: { type: TOK_EOF,    value: "",    line: 1, column: 12, length: 0 }
```

### 2.3 Lexer Structure

```chronos
struct Lexer {
    source: *i8,        // Entire source code
    pos: i64,           // Current position in source
    line: i64,          // Current line number
    column: i64,        // Current column number
    current_char: i64   // Current character (or -1 for EOF)
}
```

### 2.4 TokenList (Token Array)

```chronos
struct TokenList {
    tokens: *Token,     // Array of tokens (malloc'd)
    count: i64,         // Number of tokens
    capacity: i64       // Allocated capacity
}
```

---

## 3. Algoritmo del Lexer

### 3.1 Estado del Lexer

```chronos
fn lexer_init(source: *i8) -> *Lexer {
    let lex: *Lexer = malloc(32);
    lex.source = source;
    lex.pos = 0;
    lex.line = 1;
    lex.column = 1;
    lex.current_char = source[0];
    return lex;
}

fn lexer_advance(lex: *Lexer) -> i64 {
    // Move to next character
    if (lex.current_char == 10) {  // newline
        lex.line = lex.line + 1;
        lex.column = 1;
    } else {
        lex.column = lex.column + 1;
    }

    lex.pos = lex.pos + 1;
    if (lex.source[lex.pos] == 0) {
        lex.current_char = -1;  // EOF
    } else {
        lex.current_char = lex.source[lex.pos];
    }
    return lex.current_char;
}

fn lexer_peek(lex: *Lexer, ahead: i64) -> i64 {
    // Look ahead without advancing
    let peek_pos: i64 = lex.pos + ahead;
    if (lex.source[peek_pos] == 0) {
        return -1;  // EOF
    }
    return lex.source[peek_pos];
}
```

### 3.2 Función Principal: lex_next()

```chronos
fn lex_next(lex: *Lexer) -> *Token {
    // Skip whitespace
    while (is_whitespace(lex.current_char)) {
        lexer_advance(lex);
    }

    // EOF
    if (lex.current_char == -1) {
        return make_token(TOK_EOF, "", lex.line, lex.column, 0);
    }

    let start_line: i64 = lex.line;
    let start_column: i64 = lex.column;

    // Numbers: 0-9
    if (is_digit(lex.current_char)) {
        return lex_number(lex, start_line, start_column);
    }

    // Identifiers and keywords: a-z, A-Z, _
    if (is_alpha(lex.current_char)) {
        return lex_identifier(lex, start_line, start_column);
    }

    // Single-character operators
    let ch: i64 = lex.current_char;

    // Two-character operators (==, !=, <=, >=, ->)
    if (ch == 61) {  // '='
        if (lexer_peek(lex, 1) == 61) {  // "=="
            lexer_advance(lex);
            lexer_advance(lex);
            return make_token(TOK_EQEQ, "==", start_line, start_column, 2);
        }
        lexer_advance(lex);
        return make_token(TOK_EQ, "=", start_line, start_column, 1);
    }

    if (ch == 33) {  // '!'
        if (lexer_peek(lex, 1) == 61) {  // "!="
            lexer_advance(lex);
            lexer_advance(lex);
            return make_token(TOK_NEQ, "!=", start_line, start_column, 2);
        }
        // Single '!' not supported yet, error
        lexer_advance(lex);
        return make_token(TOK_ERROR, "!", start_line, start_column, 1);
    }

    if (ch == 60) {  // '<'
        if (lexer_peek(lex, 1) == 61) {  // "<="
            lexer_advance(lex);
            lexer_advance(lex);
            return make_token(TOK_LTEQ, "<=", start_line, start_column, 2);
        }
        lexer_advance(lex);
        return make_token(TOK_LT, "<", start_line, start_column, 1);
    }

    if (ch == 62) {  // '>'
        if (lexer_peek(lex, 1) == 61) {  // ">="
            lexer_advance(lex);
            lexer_advance(lex);
            return make_token(TOK_GTEQ, ">=", start_line, start_column, 2);
        }
        lexer_advance(lex);
        return make_token(TOK_GT, ">", start_line, start_column, 1);
    }

    if (ch == 45) {  // '-'
        if (lexer_peek(lex, 1) == 62) {  // "->"
            lexer_advance(lex);
            lexer_advance(lex);
            return make_token(TOK_ARROW, "->", start_line, start_column, 2);
        }
        lexer_advance(lex);
        return make_token(TOK_MINUS, "-", start_line, start_column, 1);
    }

    // Single-character tokens
    if (ch == 43) {  // '+'
        lexer_advance(lex);
        return make_token(TOK_PLUS, "+", start_line, start_column, 1);
    }
    if (ch == 42) {  // '*'
        lexer_advance(lex);
        return make_token(TOK_STAR, "*", start_line, start_column, 1);
    }
    if (ch == 47) {  // '/'
        lexer_advance(lex);
        return make_token(TOK_SLASH, "/", start_line, start_column, 1);
    }
    if (ch == 40) {  // '('
        lexer_advance(lex);
        return make_token(TOK_LPAREN, "(", start_line, start_column, 1);
    }
    if (ch == 41) {  // ')'
        lexer_advance(lex);
        return make_token(TOK_RPAREN, ")", start_line, start_column, 1);
    }
    if (ch == 123) {  // '{'
        lexer_advance(lex);
        return make_token(TOK_LBRACE, "{", start_line, start_column, 1);
    }
    if (ch == 125) {  // '}'
        lexer_advance(lex);
        return make_token(TOK_RBRACE, "}", start_line, start_column, 1);
    }
    if (ch == 91) {  // '['
        lexer_advance(lex);
        return make_token(TOK_LBRACKET, "[", start_line, start_column, 1);
    }
    if (ch == 93) {  // ']'
        lexer_advance(lex);
        return make_token(TOK_RBRACKET, "]", start_line, start_column, 1);
    }
    if (ch == 59) {  // ';'
        lexer_advance(lex);
        return make_token(TOK_SEMICOLON, ";", start_line, start_column, 1);
    }
    if (ch == 58) {  // ':'
        lexer_advance(lex);
        return make_token(TOK_COLON, ":", start_line, start_column, 1);
    }
    if (ch == 44) {  // ','
        lexer_advance(lex);
        return make_token(TOK_COMMA, ",", start_line, start_column, 1);
    }
    if (ch == 46) {  // '.'
        lexer_advance(lex);
        return make_token(TOK_DOT, ".", start_line, start_column, 1);
    }
    if (ch == 38) {  // '&'
        lexer_advance(lex);
        return make_token(TOK_AMPERSAND, "&", start_line, start_column, 1);
    }

    // Unknown character, error
    lexer_advance(lex);
    let error_buf: [i8; 2];
    error_buf[0] = ch;
    error_buf[1] = 0;
    return make_token(TOK_ERROR, error_buf, start_line, start_column, 1);
}
```

### 3.3 Lex Number

```chronos
fn lex_number(lex: *Lexer, start_line: i64, start_column: i64) -> *Token {
    let start_pos: i64 = lex.pos;
    let digit_count: i64 = 0;
    let max_digits: i64 = 19;  // Security: prevent overflow

    while (is_digit(lex.current_char) && digit_count < max_digits) {
        lexer_advance(lex);
        digit_count = digit_count + 1;
    }

    // Extract number string
    let length: i64 = lex.pos - start_pos;
    let value: *i8 = malloc(length + 1);
    let i: i64 = 0;
    while (i < length) {
        value[i] = lex.source[start_pos + i];
        i = i + 1;
    }
    value[length] = 0;

    return make_token(TOK_NUMBER, value, start_line, start_column, length);
}
```

### 3.4 Lex Identifier (and Keywords)

```chronos
fn lex_identifier(lex: *Lexer, start_line: i64, start_column: i64) -> *Token {
    let start_pos: i64 = lex.pos;

    // First character is letter or underscore (already checked)
    lexer_advance(lex);

    // Remaining characters can be letters, digits, or underscores
    while (is_alpha(lex.current_char) || is_digit(lex.current_char)) {
        lexer_advance(lex);
    }

    // Extract identifier string
    let length: i64 = lex.pos - start_pos;
    let value: *i8 = malloc(length + 1);
    let i: i64 = 0;
    while (i < length) {
        value[i] = lex.source[start_pos + i];
        i = i + 1;
    }
    value[length] = 0;

    // Check if it's a keyword
    let token_type: i64 = keyword_or_ident(value);

    return make_token(token_type, value, start_line, start_column, length);
}

fn keyword_or_ident(word: *i8) -> i64 {
    // Check against known keywords
    if (str_equals(word, "fn")) { return TOK_FN; }
    if (str_equals(word, "let")) { return TOK_LET; }
    if (str_equals(word, "return")) { return TOK_RETURN; }
    if (str_equals(word, "if")) { return TOK_IF; }
    if (str_equals(word, "else")) { return TOK_ELSE; }
    if (str_equals(word, "while")) { return TOK_WHILE; }
    if (str_equals(word, "struct")) { return TOK_STRUCT; }
    if (str_equals(word, "i64")) { return TOK_I64; }
    if (str_equals(word, "i8")) { return TOK_I8; }

    // Not a keyword, it's an identifier
    return TOK_IDENT;
}
```

### 3.5 Helper Functions

```chronos
fn is_whitespace(ch: i64) -> i64 {
    if (ch == 32) { return 1; }  // space
    if (ch == 9) { return 1; }   // tab
    if (ch == 10) { return 1; }  // newline
    if (ch == 13) { return 1; }  // carriage return
    return 0;
}

fn is_digit(ch: i64) -> i64 {
    if (ch >= 48 && ch <= 57) { return 1; }  // '0' to '9'
    return 0;
}

fn is_alpha(ch: i64) -> i64 {
    // a-z
    if (ch >= 97 && ch <= 122) { return 1; }
    // A-Z
    if (ch >= 65 && ch <= 90) { return 1; }
    // underscore
    if (ch == 95) { return 1; }
    return 0;
}

fn make_token(type: i64, value: *i8, line: i64, column: i64, length: i64) -> *Token {
    let tok: *Token = malloc(40);  // sizeof(Token) = 40 bytes
    tok.type = type;
    tok.value = value;
    tok.line = line;
    tok.column = column;
    tok.length = length;
    return tok;
}
```

---

## 4. Uso del Lexer

### 4.1 Tokenizar Todo el Archivo

```chronos
fn lex_all(source: *i8) -> *TokenList {
    let lex: *Lexer = lexer_init(source);
    let list: *TokenList = tokenlist_init();

    let done: i64 = 0;
    while (done == 0) {
        let tok: *Token = lex_next(lex);
        tokenlist_add(list, tok);

        if (tok.type == TOK_EOF) {
            done = 1;
        }
        if (tok.type == TOK_ERROR) {
            println("Lexical error at line:");
            print_int(tok.line);
            println("");
            done = 1;
        }
    }

    return list;
}

fn tokenlist_init() -> *TokenList {
    let list: *TokenList = malloc(24);
    list.capacity = 256;
    list.count = 0;
    list.tokens = malloc(list.capacity * 40);  // 40 bytes per token
    return list;
}

fn tokenlist_add(list: *TokenList, tok: *Token) -> i64 {
    if (list.count >= list.capacity) {
        // TODO: grow array (for now, error)
        println("ERROR: Too many tokens");
        return 1;
    }

    // Copy token into array
    let offset: i64 = list.count * 40;
    let dest: *Token = list.tokens + offset;
    dest.type = tok.type;
    dest.value = tok.value;
    dest.line = tok.line;
    dest.column = tok.column;
    dest.length = tok.length;

    list.count = list.count + 1;
    return 0;
}
```

### 4.2 Ejemplo Completo

```chronos
fn main() -> i64 {
    let source: *i8 = "fn main() -> i64 { return 42; }";

    let tokens: *TokenList = lex_all(source);

    println("Tokens:");
    let i: i64 = 0;
    while (i < tokens.count) {
        let tok: *Token = tokens.tokens + (i * 40);
        print("  [");
        print_int(tok.type);
        print("] ");
        print(tok.value);
        println("");
        i = i + 1;
    }

    return 0;
}
```

**Output esperado:**
```
Tokens:
  [20] fn
  [11] main
  [60] (
  [61] )
  [70] ->
  [30] i64
  [62] {
  [22] return
  [10] 42
  [66] ;
  [63] }
  [0]
```

---

## 5. Ventajas del Nuevo Lexer

### Antes (char-by-char)
❌ Frágil y difícil de mantener
❌ Código duplicado para cada construcción
❌ Sin información de posición (line/column)
❌ Difícil de extender
❌ Propenso a errores

### Después (token-based)
✅ Robusto y mantenible
✅ Código reutilizable
✅ Información de posición completa
✅ Fácil de extender (solo agregar nuevo TOK_*)
✅ Mejor error reporting

---

## 6. Extensiones Futuras

### 6.1 Comentarios
```chronos
// Skip single-line comments
if (ch == 47 && lexer_peek(lex, 1) == 47) {  // "//"
    while (lex.current_char != 10 && lex.current_char != -1) {
        lexer_advance(lex);
    }
    return lex_next(lex);  // Recursively get next token
}
```

### 6.2 Strings
```chronos
// Lex string literals: "hello world"
if (ch == 34) {  // '"'
    return lex_string(lex, start_line, start_column);
}
```

### 6.3 Más Operadores
```chronos
let TOK_PLUSEQ: i64 = 80;    // +=
let TOK_MINUSEQ: i64 = 81;   // -=
let TOK_AMPAMP: i64 = 82;    // &&
let TOK_PIPEPIPE: i64 = 83;  // ||
```

---

## 7. Notas de Implementación

### Memory Management
- Cada token aloca memoria para `value` (string)
- TokenList aloca array de tokens
- **TODO:** Implementar `tokenlist_free()` para liberar memoria

### Security
- Max 19 dígitos en números (previene overflow)
- Max 256 tokens (previene DoS, ajustable)
- Loop bounds en todas las funciones

### Bootstrap Constraints
- No usamos enums (todavía no existen), usamos constantes i64
- No usamos strings (no hay soporte), usamos *i8 con malloc
- Struct Token debe ser layout compatible con bootstrap compiler

---

## 8. Tests del Lexer

```chronos
// Test 1: Simple expression
assert_lex("42 + 10", [TOK_NUMBER, TOK_PLUS, TOK_NUMBER, TOK_EOF]);

// Test 2: Function declaration
assert_lex("fn foo()", [TOK_FN, TOK_IDENT, TOK_LPAREN, TOK_RPAREN, TOK_EOF]);

// Test 3: Operators
assert_lex("x == y", [TOK_IDENT, TOK_EQEQ, TOK_IDENT, TOK_EOF]);

// Test 4: Error handling
assert_lex("@invalid", [TOK_ERROR]);
```

---

**Próximo Paso:** Diseñar el AST (Abstract Syntax Tree) para representar la estructura del programa.
