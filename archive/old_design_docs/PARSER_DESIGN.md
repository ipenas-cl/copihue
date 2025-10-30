# Diseño del Parser (Recursive Descent) - Chronos Compiler

**Versión:** 1.0
**Fecha:** 29 de octubre de 2025
**Para:** Chronos Self-Hosted Compiler v1.0

---

## 1. Objetivo

Convertir la secuencia de **tokens** (del lexer) en un **AST** (Abstract Syntax Tree) usando técnica **recursive descent**.

### Pipeline Completo
```
Source Code → [LEXER] → Tokens → [PARSER] → AST → [CODEGEN] → Assembly
```

### Recursive Descent Parser

**Técnica:** Cada regla de la gramática se convierte en una función recursiva.

**Ejemplo:**
```
Grammar Rule:  expression = term (('+' | '-') term)*
Function:      fn parse_expression() -> *ASTNode { ... }
```

---

## 2. Gramática de Chronos

### 2.1 Gramática Simplificada (EBNF)

```ebnf
program        = declaration*

declaration    = function_decl
               | struct_decl

function_decl  = 'fn' IDENT '(' param_list? ')' '->' type block

struct_decl    = 'struct' IDENT '{' field_list '}'

param_list     = param (',' param)*
param          = IDENT ':' type

field_list     = field (',' field)*
field          = IDENT ':' type

block          = '{' statement* '}'

statement      = let_stmt
               | assign_stmt
               | return_stmt
               | if_stmt
               | while_stmt
               | expr_stmt

let_stmt       = 'let' IDENT ':' type ('=' expression)? ';'
assign_stmt    = IDENT '=' expression ';'
return_stmt    = 'return' expression? ';'
if_stmt        = 'if' '(' expression ')' block ('else' block)?
while_stmt     = 'while' '(' expression ')' block
expr_stmt      = expression ';'

expression     = comparison

comparison     = addition (('==' | '!=' | '<' | '>' | '<=' | '>=') addition)*

addition       = multiplication (('+' | '-') multiplication)*

multiplication = unary (('*' | '/') unary)*

unary          = ('-' | '&' | '*') unary
               | postfix

postfix        = primary ('.' IDENT | '(' arg_list? ')')*

primary        = NUMBER
               | IDENT
               | '(' expression ')'

type           = 'i64' | 'i8' | '*' type | '[' type ';' NUMBER ']'

arg_list       = expression (',' expression)*
```

### 2.2 Precedencia de Operadores

```
Highest precedence (evaluated first):
  1. Primary (numbers, identifiers, parentheses)
  2. Postfix (field access, function calls)
  3. Unary (-, &, *)
  4. Multiplication (*, /)
  5. Addition (+, -)
  6. Comparison (==, !=, <, >, <=, >=)
Lowest precedence (evaluated last)
```

**Ejemplo:** `2 + 3 * 4` se parsea como `2 + (3 * 4)` porque `*` tiene mayor precedencia que `+`.

---

## 3. Parser State

```chronos
struct Parser {
    tokens: *TokenList,  // List of tokens from lexer
    pos: i64,            // Current position in token list
    current: *Token,     // Current token being examined
    previous: *Token     // Previously consumed token
}
```

---

## 4. Parser Core Functions

### 4.1 Initialization

```chronos
fn parser_init(tokens: *TokenList) -> *Parser {
    let p: *Parser = malloc(32);
    p.tokens = tokens;
    p.pos = 0;
    p.current = tokens.tokens;  // First token
    p.previous = 0;
    return p;
}
```

### 4.2 Token Navigation

```chronos
fn parser_advance(p: *Parser) -> *Token {
    // Move to next token
    if (p.current.type != TOK_EOF) {
        p.previous = p.current;
        p.pos = p.pos + 1;

        // Get next token from array
        let offset: i64 = p.pos * 40;  // sizeof(Token) = 40
        p.current = p.tokens.tokens + offset;
    }
    return p.previous;
}

fn parser_check(p: *Parser, type: i64) -> i64 {
    // Check if current token is of given type
    if (p.current.type == TOK_EOF) {
        return 0;
    }
    return p.current.type == type;
}

fn parser_match(p: *Parser, type: i64) -> i64 {
    // Check and consume if match
    if (parser_check(p, type)) {
        parser_advance(p);
        return 1;
    }
    return 0;
}

fn parser_consume(p: *Parser, type: i64, error_msg: *i8) -> *Token {
    // Consume token, error if not match
    if (parser_check(p, type)) {
        return parser_advance(p);
    }

    // Error
    print("Parse error at line ");
    print_int(p.current.line);
    print(": ");
    print(error_msg);
    println("");
    return 0;
}

fn parser_peek(p: *Parser, ahead: i64) -> *Token {
    // Look ahead without consuming
    let peek_pos: i64 = p.pos + ahead;
    if (peek_pos >= p.tokens.count) {
        // Return EOF token
        let offset: i64 = (p.tokens.count - 1) * 40;
        return p.tokens.tokens + offset;
    }
    let offset: i64 = peek_pos * 40;
    return p.tokens.tokens + offset;
}
```

---

## 5. Parsing Functions (Top-Down)

### 5.1 Program (Entry Point)

```chronos
fn parse_program(tokens: *TokenList) -> *ASTNode {
    let p: *Parser = parser_init(tokens);

    let program: *ASTNode = ast_alloc();
    program.node_type = NODE_PROGRAM;
    program.children = 0;  // Linked list of declarations

    // Parse declarations until EOF
    while (p.current.type != TOK_EOF) {
        let decl: *ASTNode = parse_declaration(p);
        if (decl == 0) {
            println("Parse error in declaration");
            return 0;
        }
        program.children = ast_list_append(program.children, decl);
    }

    return program;
}
```

### 5.2 Declarations

```chronos
fn parse_declaration(p: *Parser) -> *ASTNode {
    // function_decl | struct_decl

    if (parser_match(p, TOK_FN)) {
        return parse_function(p);
    }

    if (parser_match(p, TOK_STRUCT)) {
        return parse_struct(p);
    }

    print("Expected declaration, found: ");
    print(p.current.value);
    println("");
    return 0;
}

fn parse_function(p: *Parser) -> *ASTNode {
    // fn IDENT ( param_list? ) -> type block

    let line: i64 = p.previous.line;
    let column: i64 = p.previous.column;

    // Function name
    let name_tok: *Token = parser_consume(p, TOK_IDENT, "Expected function name");
    if (name_tok == 0) {
        return 0;
    }

    // Parameters
    parser_consume(p, TOK_LPAREN, "Expected '(' after function name");

    let params: *ASTNode = 0;  // Linked list of parameters
    if (parser_check(p, TOK_IDENT) == 0) {
        // No params
    } else {
        params = parse_param_list(p);
    }

    parser_consume(p, TOK_RPAREN, "Expected ')' after parameters");

    // Return type
    parser_consume(p, TOK_ARROW, "Expected '->' before return type");
    let return_type: *ASTNode = parse_type(p);

    // Body
    let body: *ASTNode = parse_block(p);

    return ast_function(name_tok.value, params, return_type, body, line, column);
}

fn parse_param_list(p: *Parser) -> *ASTNode {
    // param (',' param)*

    let params: *ASTNode = 0;

    let first_param: *ASTNode = parse_param(p);
    params = first_param;

    while (parser_match(p, TOK_COMMA)) {
        let next_param: *ASTNode = parse_param(p);
        params = ast_list_append(params, next_param);
    }

    return params;
}

fn parse_param(p: *Parser) -> *ASTNode {
    // IDENT : type

    let name_tok: *Token = parser_consume(p, TOK_IDENT, "Expected parameter name");
    parser_consume(p, TOK_COLON, "Expected ':' after parameter name");
    let type_node: *ASTNode = parse_type(p);

    let param: *ASTNode = ast_alloc();
    param.node_type = NODE_PARAM;
    param.name = name_tok.value;
    param.type_node = type_node;
    param.line = name_tok.line;
    param.column = name_tok.column;

    return param;
}

fn parse_struct(p: *Parser) -> *ASTNode {
    // struct IDENT { field_list }

    let line: i64 = p.previous.line;
    let column: i64 = p.previous.column;

    let name_tok: *Token = parser_consume(p, TOK_IDENT, "Expected struct name");
    parser_consume(p, TOK_LBRACE, "Expected '{' after struct name");

    let fields: *ASTNode = 0;
    while (parser_check(p, TOK_RBRACE) == 0 && p.current.type != TOK_EOF) {
        let field: *ASTNode = parse_field(p);
        fields = ast_list_append(fields, field);

        // Optional comma
        parser_match(p, TOK_COMMA);
    }

    parser_consume(p, TOK_RBRACE, "Expected '}' after struct fields");

    let struct_node: *ASTNode = ast_alloc();
    struct_node.node_type = NODE_STRUCT;
    struct_node.name = name_tok.value;
    struct_node.children = fields;
    struct_node.line = line;
    struct_node.column = column;

    return struct_node;
}

fn parse_field(p: *Parser) -> *ASTNode {
    // IDENT : type

    let name_tok: *Token = parser_consume(p, TOK_IDENT, "Expected field name");
    parser_consume(p, TOK_COLON, "Expected ':' after field name");
    let type_node: *ASTNode = parse_type(p);

    let field: *ASTNode = ast_alloc();
    field.node_type = NODE_FIELD_DECL;
    field.name = name_tok.value;
    field.type_node = type_node;
    field.line = name_tok.line;
    field.column = name_tok.column;

    return field;
}
```

### 5.3 Statements

```chronos
fn parse_block(p: *Parser) -> *ASTNode {
    // { statement* }

    let line: i64 = p.current.line;
    let column: i64 = p.current.column;

    parser_consume(p, TOK_LBRACE, "Expected '{'");

    let statements: *ASTNode = 0;
    while (parser_check(p, TOK_RBRACE) == 0 && p.current.type != TOK_EOF) {
        let stmt: *ASTNode = parse_statement(p);
        if (stmt != 0) {
            statements = ast_list_append(statements, stmt);
        }
    }

    parser_consume(p, TOK_RBRACE, "Expected '}'");

    return ast_block(statements, line, column);
}

fn parse_statement(p: *Parser) -> *ASTNode {
    // let_stmt | assign_stmt | return_stmt | if_stmt | while_stmt | expr_stmt

    if (parser_match(p, TOK_LET)) {
        return parse_let_statement(p);
    }

    if (parser_match(p, TOK_RETURN)) {
        return parse_return_statement(p);
    }

    if (parser_match(p, TOK_IF)) {
        return parse_if_statement(p);
    }

    if (parser_match(p, TOK_WHILE)) {
        return parse_while_statement(p);
    }

    // Try to parse as expression or assignment
    return parse_expr_or_assign_statement(p);
}

fn parse_let_statement(p: *Parser) -> *ASTNode {
    // let IDENT : type (= expression)? ;

    let line: i64 = p.previous.line;

    let name_tok: *Token = parser_consume(p, TOK_IDENT, "Expected variable name");
    parser_consume(p, TOK_COLON, "Expected ':' after variable name");
    let type_node: *ASTNode = parse_type(p);

    let initializer: *ASTNode = 0;
    if (parser_match(p, TOK_EQ)) {
        initializer = parse_expression(p);
    }

    parser_consume(p, TOK_SEMICOLON, "Expected ';' after let statement");

    return ast_let(name_tok.value, type_node, initializer, line, name_tok.column);
}

fn parse_return_statement(p: *Parser) -> *ASTNode {
    // return expression? ;

    let line: i64 = p.previous.line;
    let column: i64 = p.previous.column;

    let expr: *ASTNode = 0;
    if (parser_check(p, TOK_SEMICOLON) == 0) {
        expr = parse_expression(p);
    }

    parser_consume(p, TOK_SEMICOLON, "Expected ';' after return");

    return ast_return(expr, line, column);
}

fn parse_if_statement(p: *Parser) -> *ASTNode {
    // if ( expression ) block (else block)?

    let line: i64 = p.previous.line;
    let column: i64 = p.previous.column;

    parser_consume(p, TOK_LPAREN, "Expected '(' after if");
    let condition: *ASTNode = parse_expression(p);
    parser_consume(p, TOK_RPAREN, "Expected ')' after condition");

    let then_block: *ASTNode = parse_block(p);

    let else_block: *ASTNode = 0;
    if (parser_match(p, TOK_ELSE)) {
        else_block = parse_block(p);
    }

    return ast_if(condition, then_block, else_block, line, column);
}

fn parse_while_statement(p: *Parser) -> *ASTNode {
    // while ( expression ) block

    let line: i64 = p.previous.line;
    let column: i64 = p.previous.column;

    parser_consume(p, TOK_LPAREN, "Expected '(' after while");
    let condition: *ASTNode = parse_expression(p);
    parser_consume(p, TOK_RPAREN, "Expected ')' after condition");

    let body: *ASTNode = parse_block(p);

    let while_node: *ASTNode = ast_alloc();
    while_node.node_type = NODE_WHILE;
    while_node.condition = condition;
    while_node.body = body;
    while_node.line = line;
    while_node.column = column;

    return while_node;
}

fn parse_expr_or_assign_statement(p: *Parser) -> *ASTNode {
    // expression ; | IDENT = expression ;

    let line: i64 = p.current.line;
    let column: i64 = p.current.column;

    // Parse expression
    let expr: *ASTNode = parse_expression(p);

    // Check if it's an assignment
    if (parser_match(p, TOK_EQ)) {
        // This is an assignment: expr = value ;
        let value: *ASTNode = parse_expression(p);
        parser_consume(p, TOK_SEMICOLON, "Expected ';' after assignment");

        let assign: *ASTNode = ast_alloc();
        assign.node_type = NODE_ASSIGN;
        assign.left = expr;  // Left side (should be lvalue)
        assign.right = value;
        assign.line = line;
        assign.column = column;

        return assign;
    }

    // Just an expression statement
    parser_consume(p, TOK_SEMICOLON, "Expected ';' after expression");
    return expr;
}
```

### 5.4 Expressions (Precedence Climbing)

```chronos
fn parse_expression(p: *Parser) -> *ASTNode {
    // Entry point: lowest precedence (comparison)
    return parse_comparison(p);
}

fn parse_comparison(p: *Parser) -> *ASTNode {
    // comparison = addition (('==' | '!=' | '<' | '>' | '<=' | '>=') addition)*

    let expr: *ASTNode = parse_addition(p);

    while (1) {
        let op: i64 = 0;

        if (parser_match(p, TOK_EQEQ)) {
            op = TOK_EQEQ;
        } else if (parser_match(p, TOK_NEQ)) {
            op = TOK_NEQ;
        } else if (parser_match(p, TOK_LT)) {
            op = TOK_LT;
        } else if (parser_match(p, TOK_GT)) {
            op = TOK_GT;
        } else if (parser_match(p, TOK_LTEQ)) {
            op = TOK_LTEQ;
        } else if (parser_match(p, TOK_GTEQ)) {
            op = TOK_GTEQ;
        } else {
            break;  // No more comparison operators
        }

        let line: i64 = p.previous.line;
        let column: i64 = p.previous.column;
        let right: *ASTNode = parse_addition(p);
        expr = ast_binary_op(op, expr, right, line, column);
    }

    return expr;
}

fn parse_addition(p: *Parser) -> *ASTNode {
    // addition = multiplication (('+' | '-') multiplication)*

    let expr: *ASTNode = parse_multiplication(p);

    while (1) {
        let op: i64 = 0;

        if (parser_match(p, TOK_PLUS)) {
            op = TOK_PLUS;
        } else if (parser_match(p, TOK_MINUS)) {
            op = TOK_MINUS;
        } else {
            break;
        }

        let line: i64 = p.previous.line;
        let column: i64 = p.previous.column;
        let right: *ASTNode = parse_multiplication(p);
        expr = ast_binary_op(op, expr, right, line, column);
    }

    return expr;
}

fn parse_multiplication(p: *Parser) -> *ASTNode {
    // multiplication = unary (('*' | '/') unary)*

    let expr: *ASTNode = parse_unary(p);

    while (1) {
        let op: i64 = 0;

        if (parser_match(p, TOK_STAR)) {
            op = TOK_STAR;
        } else if (parser_match(p, TOK_SLASH)) {
            op = TOK_SLASH;
        } else {
            break;
        }

        let line: i64 = p.previous.line;
        let column: i64 = p.previous.column;
        let right: *ASTNode = parse_unary(p);
        expr = ast_binary_op(op, expr, right, line, column);
    }

    return expr;
}

fn parse_unary(p: *Parser) -> *ASTNode {
    // unary = ('-' | '&' | '*') unary | postfix

    if (parser_match(p, TOK_MINUS) || parser_match(p, TOK_AMPERSAND) || parser_match(p, TOK_STAR)) {
        let op: i64 = p.previous.type;
        let line: i64 = p.previous.line;
        let column: i64 = p.previous.column;
        let operand: *ASTNode = parse_unary(p);  // Recursive!

        let unary: *ASTNode = ast_alloc();
        unary.node_type = NODE_UNARY_OP;
        unary.op = op;
        unary.left = operand;
        unary.line = line;
        unary.column = column;

        return unary;
    }

    return parse_postfix(p);
}

fn parse_postfix(p: *Parser) -> *ASTNode {
    // postfix = primary ('.' IDENT | '(' arg_list? ')')*

    let expr: *ASTNode = parse_primary(p);

    while (1) {
        if (parser_match(p, TOK_DOT)) {
            // Field access: expr.field
            let field_tok: *Token = parser_consume(p, TOK_IDENT, "Expected field name");

            let field_access: *ASTNode = ast_alloc();
            field_access.node_type = NODE_FIELD_ACCESS;
            field_access.left = expr;
            field_access.name = field_tok.value;
            field_access.line = field_tok.line;
            field_access.column = field_tok.column;

            expr = field_access;

        } else if (parser_match(p, TOK_LPAREN)) {
            // Function call: expr(args)
            let args: *ASTNode = 0;
            if (parser_check(p, TOK_RPAREN) == 0) {
                args = parse_arg_list(p);
            }
            parser_consume(p, TOK_RPAREN, "Expected ')' after arguments");

            let call: *ASTNode = ast_alloc();
            call.node_type = NODE_CALL;
            call.left = expr;
            call.children = args;
            call.line = p.previous.line;
            call.column = p.previous.column;

            expr = call;

        } else {
            break;  // No more postfix operators
        }
    }

    return expr;
}

fn parse_primary(p: *Parser) -> *ASTNode {
    // primary = NUMBER | IDENT | '(' expression ')'

    // Number
    if (parser_match(p, TOK_NUMBER)) {
        let value: i64 = str_to_num(p.previous.value);
        return ast_number(value, p.previous.line, p.previous.column);
    }

    // Identifier
    if (parser_match(p, TOK_IDENT)) {
        return ast_ident(p.previous.value, p.previous.line, p.previous.column);
    }

    // Grouped expression
    if (parser_match(p, TOK_LPAREN)) {
        let expr: *ASTNode = parse_expression(p);
        parser_consume(p, TOK_RPAREN, "Expected ')' after expression");
        return expr;
    }

    print("Unexpected token: ");
    print(p.current.value);
    println("");
    return 0;
}

fn parse_arg_list(p: *Parser) -> *ASTNode {
    // expression (',' expression)*

    let args: *ASTNode = 0;

    let first_arg: *ASTNode = parse_expression(p);
    args = first_arg;

    while (parser_match(p, TOK_COMMA)) {
        let next_arg: *ASTNode = parse_expression(p);
        args = ast_list_append(args, next_arg);
    }

    return args;
}
```

### 5.5 Types

```chronos
fn parse_type(p: *Parser) -> *ASTNode {
    // type = 'i64' | 'i8' | '*' type | '[' type ';' NUMBER ']'

    if (parser_match(p, TOK_I64)) {
        let type_node: *ASTNode = ast_alloc();
        type_node.node_type = NODE_TYPE_I64;
        return type_node;
    }

    if (parser_match(p, TOK_I8)) {
        let type_node: *ASTNode = ast_alloc();
        type_node.node_type = NODE_TYPE_I8;
        return type_node;
    }

    // Pointer type: *T
    if (parser_match(p, TOK_STAR)) {
        let inner_type: *ASTNode = parse_type(p);  // Recursive!

        let ptr_type: *ASTNode = ast_alloc();
        ptr_type.node_type = NODE_TYPE_PTR;
        ptr_type.left = inner_type;
        return ptr_type;
    }

    // Array type: [T; N]
    if (parser_match(p, TOK_LBRACKET)) {
        let element_type: *ASTNode = parse_type(p);
        parser_consume(p, TOK_SEMICOLON, "Expected ';' in array type");
        let size_tok: *Token = parser_consume(p, TOK_NUMBER, "Expected array size");
        parser_consume(p, TOK_RBRACKET, "Expected ']'");

        let array_type: *ASTNode = ast_alloc();
        array_type.node_type = NODE_TYPE_ARRAY;
        array_type.left = element_type;
        array_type.value = str_to_num(size_tok.value);
        return array_type;
    }

    // Custom type (struct name)
    if (parser_match(p, TOK_IDENT)) {
        let custom_type: *ASTNode = ast_alloc();
        custom_type.node_type = NODE_TYPE_CUSTOM;
        custom_type.name = p.previous.value;
        return custom_type;
    }

    println("Expected type");
    return 0;
}
```

---

## 6. Ejemplo Completo de Parsing

### Source Code
```chronos
fn factorial(n: i64) -> i64 {
    if (n == 0) {
        return 1;
    } else {
        return n * factorial(n - 1);
    }
}
```

### Paso 1: Lexer genera tokens
```
TOK_FN, TOK_IDENT("factorial"), TOK_LPAREN, TOK_IDENT("n"), TOK_COLON, TOK_I64, TOK_RPAREN, TOK_ARROW, TOK_I64, TOK_LBRACE,
TOK_IF, TOK_LPAREN, TOK_IDENT("n"), TOK_EQEQ, TOK_NUMBER("0"), TOK_RPAREN, TOK_LBRACE,
TOK_RETURN, TOK_NUMBER("1"), TOK_SEMICOLON, TOK_RBRACE, TOK_ELSE, TOK_LBRACE,
TOK_RETURN, TOK_IDENT("n"), TOK_STAR, TOK_IDENT("factorial"), TOK_LPAREN, TOK_IDENT("n"), TOK_MINUS, TOK_NUMBER("1"), TOK_RPAREN, TOK_SEMICOLON,
TOK_RBRACE, TOK_RBRACE, TOK_EOF
```

### Paso 2: Parser construye AST
```
Call Stack:
parse_program()
  └─ parse_declaration()
      └─ parse_function()
          ├─ parse_param_list()
          │   └─ parse_param() → NODE_PARAM(name="n", type=i64)
          ├─ parse_type() → NODE_TYPE_I64
          └─ parse_block()
              └─ parse_statement()
                  └─ parse_if_statement()
                      ├─ parse_expression()
                      │   └─ parse_comparison()
                      │       └─ NODE_BINARY_OP(op=EQEQ, left=IDENT("n"), right=NUMBER(0))
                      ├─ parse_block() → then_block
                      │   └─ parse_return_statement()
                      │       └─ NODE_RETURN(NUMBER(1))
                      └─ parse_block() → else_block
                          └─ parse_return_statement()
                              └─ NODE_RETURN(
                                  NODE_BINARY_OP(op=STAR,
                                    left=IDENT("n"),
                                    right=NODE_CALL(
                                      func=IDENT("factorial"),
                                      args=[NODE_BINARY_OP(op=MINUS, left=IDENT("n"), right=NUMBER(1))]
                                    )
                                  )
                                )
```

**AST Resultante:**
```
NODE_FUNCTION (name="factorial")
├─ params: [NODE_PARAM(name="n", type=i64)]
├─ return_type: NODE_TYPE_I64
└─ body: NODE_BLOCK
   └─ children: NODE_IF
      ├─ condition: NODE_BINARY_OP(op=EQEQ)
      │  ├─ left: NODE_IDENT("n")
      │  └─ right: NODE_NUMBER(0)
      ├─ then_block: NODE_BLOCK
      │  └─ children: NODE_RETURN(NODE_NUMBER(1))
      └─ else_block: NODE_BLOCK
         └─ children: NODE_RETURN(
            NODE_BINARY_OP(op=STAR,
              left=NODE_IDENT("n"),
              right=NODE_CALL(
                left=NODE_IDENT("factorial"),
                children=[NODE_BINARY_OP(op=MINUS, left=NODE_IDENT("n"), right=NUMBER(1))]
              )
            )
         )
```

---

## 7. Error Handling

### 7.1 Synchronization

```chronos
fn parser_synchronize(p: *Parser) -> i64 {
    // Skip tokens until we find a statement boundary
    parser_advance(p);

    while (p.current.type != TOK_EOF) {
        if (p.previous.type == TOK_SEMICOLON) {
            return 0;  // Found statement boundary
        }

        // Start of new statement
        if (p.current.type == TOK_LET) { return 0; }
        if (p.current.type == TOK_FN) { return 0; }
        if (p.current.type == TOK_STRUCT) { return 0; }
        if (p.current.type == TOK_IF) { return 0; }
        if (p.current.type == TOK_WHILE) { return 0; }
        if (p.current.type == TOK_RETURN) { return 0; }

        parser_advance(p);
    }

    return 0;
}
```

### 7.2 Error Reporting

```chronos
fn parser_error(p: *Parser, message: *i8) -> i64 {
    print("Parse error at line ");
    print_int(p.current.line);
    print(", column ");
    print_int(p.current.column);
    print(": ");
    print(message);
    println("");
    println("Token:");
    print("  Type: ");
    print_int(p.current.type);
    println("");
    print("  Value: ");
    print(p.current.value);
    println("");
    return 0;
}
```

---

## 8. Testing the Parser

```chronos
fn test_parser() -> i64 {
    // Test 1: Simple expression
    let source1: *i8 = "fn main() -> i64 { return 42; }";
    let tokens1: *TokenList = lex_all(source1);
    let ast1: *ASTNode = parse_program(tokens1);
    ast_print(ast1, 0);

    // Test 2: Arithmetic
    let source2: *i8 = "fn main() -> i64 { return 10 + 20 * 30; }";
    let tokens2: *TokenList = lex_all(source2);
    let ast2: *ASTNode = parse_program(tokens2);
    ast_print(ast2, 0);

    // Test 3: Nested expressions
    let source3: *i8 = "fn main() -> i64 { return (10 + 20) * (30 + 40); }";
    let tokens3: *TokenList = lex_all(source3);
    let ast3: *ASTNode = parse_program(tokens3);
    ast_print(ast3, 0);

    return 0;
}
```

---

## 9. Ventajas del Recursive Descent

### vs. Parser Actual (char-by-char)

**ANTES:**
❌ Código frágil y duplicado
❌ Difícil de extender
❌ Sin soporte para precedencia
❌ Sin expresiones anidadas
❌ Sin error reporting útil

**DESPUÉS:**
✅ Código modular y mantenible
✅ Fácil de extender (agregar nueva regla → agregar función)
✅ Precedencia correcta automática
✅ Expresiones anidadas ilimitadas
✅ Error reporting con line/column

---

## 10. Próximos Pasos

Con el **Parser** diseñado, ahora tenemos el pipeline completo:

```
Source → [LEXER] → Tokens → [PARSER] → AST
```

**Falta:**
```
AST → [CODEGEN] → Assembly
```

**Próximo Documento:** `COMPILER_ARCHITECTURE.md` - Visión general de todos los componentes y cómo se integran.

---

**Estado:** Parser design completo ✅
**Próximo:** Arquitectura general del compilador
