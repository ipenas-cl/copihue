// Chronos Parser v1.0
// Recursive Descent Parser with Precedence Climbing
// Converts tokens from lexer into AST

// NOTE: This file requires lexer.ch and ast.ch to be compiled together
// For now, we'll create inline versions of needed structures

// ==== TOKEN TYPES (from lexer.ch) ====

let TOK_EOF: i64 = 0;
let TOK_ERROR: i64 = 1;
let TOK_NUMBER: i64 = 10;
let TOK_IDENT: i64 = 11;
let TOK_FN: i64 = 20;
let TOK_LET: i64 = 21;
let TOK_RETURN: i64 = 22;
let TOK_PLUS: i64 = 40;
let TOK_MINUS: i64 = 41;
let TOK_STAR: i64 = 42;
let TOK_SLASH: i64 = 43;
let TOK_EQ: i64 = 44;
let TOK_EQEQ: i64 = 45;
let TOK_NEQ: i64 = 46;
let TOK_LT: i64 = 47;
let TOK_GT: i64 = 48;
let TOK_LTEQ: i64 = 49;
let TOK_GTEQ: i64 = 50;
let TOK_LPAREN: i64 = 60;
let TOK_RPAREN: i64 = 61;
let TOK_LBRACE: i64 = 62;
let TOK_RBRACE: i64 = 63;
let TOK_SEMICOLON: i64 = 66;
let TOK_COLON: i64 = 67;
let TOK_ARROW: i64 = 70;
let TOK_AMPERSAND: i64 = 71;

struct Token {
    type: i64,
    value: *i8,
    line: i64,
    column: i64,
    length: i64
}

struct TokenList {
    tokens: *Token,
    count: i64,
    capacity: i64
}

// ==== AST NODE TYPES (from ast.ch) ====

let NODE_NUMBER: i64 = 1;
let NODE_IDENT: i64 = 3;
let NODE_ADD: i64 = 10;
let NODE_SUB: i64 = 11;
let NODE_MUL: i64 = 12;
let NODE_DIV: i64 = 13;
let NODE_EQ: i64 = 20;
let NODE_NEQ: i64 = 21;
let NODE_LT: i64 = 22;
let NODE_GT: i64 = 23;
let NODE_LTEQ: i64 = 24;
let NODE_GTEQ: i64 = 25;
let NODE_NEG: i64 = 30;
let NODE_DEREF: i64 = 31;
let NODE_ADDR: i64 = 32;
let NODE_LET: i64 = 40;
let NODE_RETURN: i64 = 42;
let NODE_BLOCK: i64 = 45;
let NODE_EXPR_STMT: i64 = 46;
let NODE_FUNCTION: i64 = 50;
let NODE_PROGRAM: i64 = 70;

struct ASTNode {
    node_type: i64,
    left: *ASTNode,
    right: *ASTNode,
    condition: *ASTNode,
    body: *ASTNode,
    else_body: *ASTNode,
    value: i64,
    name: *i8,
    type_name: *i8,
    children: *ASTNode,
    next: *ASTNode,
    line: i64,
    column: i64
}

// Stub functions - will be replaced with real implementations
fn ast_new(node_type: i64) -> i64 {
    let node: *ASTNode = malloc(128);
    node.node_type = node_type;
    node.left = 0;
    node.right = 0;
    node.condition = 0;
    node.body = 0;
    node.else_body = 0;
    node.value = 0;
    node.name = 0;
    node.type_name = 0;
    node.children = 0;
    node.next = 0;
    node.line = 0;
    node.column = 0;
    return node;
}

fn ast_number(value: i64) -> i64 {
    let node: *ASTNode = ast_new(NODE_NUMBER);
    node.value = value;
    return node;
}

fn ast_ident(name: *i8) -> i64 {
    let node: *ASTNode = ast_new(NODE_IDENT);
    node.name = name;
    return node;
}

fn ast_binary_op(op: i64, left: *ASTNode, right: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(op);
    node.left = left;
    node.right = right;
    return node;
}

fn ast_unary_op(op: i64, operand: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(op);
    node.left = operand;
    return node;
}

fn ast_let(name: *i8, type_name: *i8, init: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(NODE_LET);
    node.name = name;
    node.type_name = type_name;
    node.right = init;
    return node;
}

fn ast_return(expr: *ASTNode) -> i64 {
    let node: *ASTNode = ast_new(NODE_RETURN);
    node.left = expr;
    return node;
}

fn ast_block() -> i64 {
    return ast_new(NODE_BLOCK);
}

fn ast_function(name: *i8) -> i64 {
    let node: *ASTNode = ast_new(NODE_FUNCTION);
    node.name = name;
    return node;
}

fn ast_program() -> i64 {
    return ast_new(NODE_PROGRAM);
}

fn ast_add_child(parent: *ASTNode, child: *ASTNode) -> i64 {
    if (parent == 0 || child == 0) {
        return 1;
    }
    if (parent.children == 0) {
        parent.children = child;
        return 0;
    }
    let current: *ASTNode = parent.children;
    let max_iterations: i64 = 1000;
    let count: i64 = 0;
    while (current.next != 0 && count < max_iterations) {
        current = current.next;
        count = count + 1;
    }
    current.next = child;
    return 0;
}

// ==== PARSER STATE ====

struct Parser {
    tokens: *TokenList,
    pos: i64,
    current: *Token,
    had_error: i64
}

// ==== PARSER INITIALIZATION ====

fn parser_init(tokens: *TokenList) -> i64 {
    let p: *Parser = malloc(32);
    p.tokens = tokens;
    p.pos = 0;
    p.had_error = 0;

    // Set current token
    if (tokens.count > 0) {
        p.current = tokens.tokens;
    } else {
        p.current = 0;
    }

    return p;
}

// ==== TOKEN FIELD ACCESS ====

// Helper: get token type from token pointer
// Token struct: type, value, line, column, length (each 8 bytes)
fn token_get_type(tok: *Token) -> i64 {
    let ptr: *i64 = tok;
    return ptr[0];
}

fn token_get_value(tok: *Token) -> i64 {
    let ptr: *i64 = tok;
    return ptr[1];
}

// ==== TOKEN NAVIGATION ====

fn parser_peek(p: *Parser) -> i64 {
    if (p.current == 0) {
        return TOK_EOF;
    }
    return token_get_type(p.current);
}

fn parser_advance(p: *Parser) -> i64 {
    if (p.pos >= p.tokens.count) {
        return 0;
    }

    let current_tok: *Token = p.current;
    p.pos = p.pos + 1;

    if (p.pos < p.tokens.count) {
        let offset: i64 = p.pos * 40;  // Token size = 40 bytes
        p.current = p.tokens.tokens + offset;
    } else {
        p.current = 0;
    }

    return current_tok;
}

fn parser_check(p: *Parser, type: i64) -> i64 {
    return parser_peek(p) == type;
}

fn parser_match(p: *Parser, type: i64) -> i64 {
    if (parser_check(p, type)) {
        parser_advance(p);
        return 1;
    }
    return 0;
}

fn parser_expect(p: *Parser, type: i64) -> i64 {
    if (parser_check(p, type)) {
        return parser_advance(p);
    }

    print("Parse error: expected token type ");
    print_int(type);
    print(", got ");
    print_int(parser_peek(p));
    println("");
    p.had_error = 1;
    return 0;
}

// ==== EXPRESSION PARSING ====

// Forward declarations (workaround for bootstrap compiler)
fn parse_primary(p: *Parser) -> i64;
fn parse_expression(p: *Parser) -> i64;

// Parse primary expression (numbers, identifiers, parentheses)
fn parse_primary(p: *Parser) -> i64 {
    let tok_type: i64 = parser_peek(p);

    // Number literal
    if (tok_type == TOK_NUMBER) {
        let tok: *Token = parser_advance(p);
        // Convert token value (string) to number
        let value_str: *i8 = token_get_value(tok);
        let value: i64 = str_to_int(value_str);
        return ast_number(value);
    }

    // Identifier
    if (tok_type == TOK_IDENT) {
        let tok: *Token = parser_advance(p);
        let ident_str: *i8 = token_get_value(tok);
        return ast_ident(ident_str);
    }

    // Parenthesized expression
    if (tok_type == TOK_LPAREN) {
        parser_advance(p);
        let expr: *ASTNode = parse_expression(p);
        parser_expect(p, TOK_RPAREN);
        return expr;
    }

    // Unary operators
    if (tok_type == TOK_MINUS) {
        parser_advance(p);
        let operand: *ASTNode = parse_primary(p);
        return ast_unary_op(NODE_NEG, operand);
    }

    if (tok_type == TOK_STAR) {
        parser_advance(p);
        let operand: *ASTNode = parse_primary(p);
        return ast_unary_op(NODE_DEREF, operand);
    }

    if (tok_type == TOK_AMPERSAND) {
        parser_advance(p);
        let operand: *ASTNode = parse_primary(p);
        return ast_unary_op(NODE_ADDR, operand);
    }

    print("Parse error: unexpected token ");
    print_int(tok_type);
    println("");
    p.had_error = 1;
    return 0;
}

// Helper: convert string to integer
fn str_to_int(s: *i8) -> i64 {
    let result: i64 = 0;
    let i: i64 = 0;
    let max_digits: i64 = 19;  // Security: prevent overflow

    while (s[i] != 0 && i < max_digits) {
        let digit: i64 = s[i] - 48;  // '0' = 48
        if (digit >= 0 && digit <= 9) {
            result = result * 10;
            result = result + digit;
        }
        i = i + 1;
    }

    return result;
}

// Parse binary expression with precedence
fn parse_binary_expr(p: *Parser, min_prec: i64) -> i64 {
    let left: *ASTNode = parse_primary(p);

    let done: i64 = 0;
    let max_iterations: i64 = 100;  // Security
    let iterations: i64 = 0;

    while (done == 0 && iterations < max_iterations) {
        let tok_type: i64 = parser_peek(p);
        let prec: i64 = get_precedence(tok_type);

        if (prec < min_prec) {
            done = 1;
        } else {
            let op: i64 = tok_type;
            parser_advance(p);

            let right: *ASTNode = parse_binary_expr(p, prec + 1);

            // Convert token type to AST node type
            let node_type: i64 = token_to_node_type(op);
            left = ast_binary_op(node_type, left, right);
        }

        iterations = iterations + 1;
    }

    return left;
}

fn parse_expression(p: *Parser) -> i64 {
    return parse_binary_expr(p, 0);
}

// Get operator precedence
fn get_precedence(tok_type: i64) -> i64 {
    // Comparison (lowest)
    if (tok_type == TOK_EQEQ) { return 1; }
    if (tok_type == TOK_NEQ) { return 1; }
    if (tok_type == TOK_LT) { return 1; }
    if (tok_type == TOK_GT) { return 1; }
    if (tok_type == TOK_LTEQ) { return 1; }
    if (tok_type == TOK_GTEQ) { return 1; }

    // Addition/Subtraction
    if (tok_type == TOK_PLUS) { return 2; }
    if (tok_type == TOK_MINUS) { return 2; }

    // Multiplication/Division (highest)
    if (tok_type == TOK_STAR) { return 3; }
    if (tok_type == TOK_SLASH) { return 3; }

    return 0;  // Not an operator
}

// Convert token type to AST node type
fn token_to_node_type(tok_type: i64) -> i64 {
    if (tok_type == TOK_PLUS) { return NODE_ADD; }
    if (tok_type == TOK_MINUS) { return NODE_SUB; }
    if (tok_type == TOK_STAR) { return NODE_MUL; }
    if (tok_type == TOK_SLASH) { return NODE_DIV; }
    if (tok_type == TOK_EQEQ) { return NODE_EQ; }
    if (tok_type == TOK_NEQ) { return NODE_NEQ; }
    if (tok_type == TOK_LT) { return NODE_LT; }
    if (tok_type == TOK_GT) { return NODE_GT; }
    if (tok_type == TOK_LTEQ) { return NODE_LTEQ; }
    if (tok_type == TOK_GTEQ) { return NODE_GTEQ; }
    return 0;
}

// ==== STATEMENT PARSING ====

fn parse_return_stmt(p: *Parser) -> i64 {
    parser_expect(p, TOK_RETURN);
    let expr: *ASTNode = parse_expression(p);
    parser_expect(p, TOK_SEMICOLON);
    return ast_return(expr);
}

fn parse_let_stmt(p: *Parser) -> i64 {
    parser_expect(p, TOK_LET);

    let name_tok: *Token = parser_expect(p, TOK_IDENT);
    let name: *i8 = token_get_value(name_tok);

    let type_name: *i8 = 0;
    if (parser_match(p, TOK_COLON)) {
        // Optional type annotation
        let type_tok: *Token = parser_advance(p);
        type_name = token_get_value(type_tok);
    }

    let init: *ASTNode = 0;
    if (parser_match(p, TOK_EQ)) {
        init = parse_expression(p);
    }

    parser_expect(p, TOK_SEMICOLON);
    return ast_let(name, type_name, init);
}

fn parse_expr_stmt(p: *Parser) -> i64 {
    let expr: *ASTNode = parse_expression(p);
    parser_expect(p, TOK_SEMICOLON);
    let stmt: *ASTNode = ast_new(NODE_EXPR_STMT);
    stmt.left = expr;
    return stmt;
}

fn parse_statement(p: *Parser) -> i64 {
    let tok_type: i64 = parser_peek(p);

    if (tok_type == TOK_RETURN) {
        return parse_return_stmt(p);
    }

    if (tok_type == TOK_LET) {
        return parse_let_stmt(p);
    }

    if (tok_type == TOK_LBRACE) {
        return parse_block(p);
    }

    // Default: expression statement
    return parse_expr_stmt(p);
}

fn parse_block(p: *Parser) -> i64 {
    parser_expect(p, TOK_LBRACE);

    let block: *ASTNode = ast_block();
    let max_statements: i64 = 1000;  // Security
    let count: i64 = 0;

    while (parser_peek(p) != TOK_RBRACE && parser_peek(p) != TOK_EOF && count < max_statements) {
        let stmt: *ASTNode = parse_statement(p);
        if (stmt != 0) {
            ast_add_child(block, stmt);
        }
        count = count + 1;
    }

    parser_expect(p, TOK_RBRACE);
    return block;
}

// ==== FUNCTION PARSING ====

fn parse_function(p: *Parser) -> i64 {
    parser_expect(p, TOK_FN);

    let name_tok: *Token = parser_expect(p, TOK_IDENT);
    let func_name: *i8 = token_get_value(name_tok);
    let func: *ASTNode = ast_function(func_name);

    parser_expect(p, TOK_LPAREN);

    // TODO: Parse parameters

    parser_expect(p, TOK_RPAREN);

    // Optional return type
    if (parser_match(p, TOK_ARROW)) {
        let ret_type_tok: *Token = parser_advance(p);
        let ret_type_name: *i8 = token_get_value(ret_type_tok);
        func.type_name = ret_type_name;
    }

    // Function body
    let body: *ASTNode = parse_block(p);
    func.body = body;

    return func;
}

// ==== PROGRAM PARSING ====

fn parse_program(p: *Parser) -> i64 {
    let program: *ASTNode = ast_program();
    let max_declarations: i64 = 1000;  // Security
    let count: i64 = 0;

    while (parser_peek(p) != TOK_EOF && count < max_declarations) {
        let tok_type: i64 = parser_peek(p);

        if (tok_type == TOK_FN) {
            let func: *ASTNode = parse_function(p);
            if (func != 0) {
                ast_add_child(program, func);
            }
        } else {
            print("Parse error: unexpected token at top level: ");
            print_int(tok_type);
            println("");
            parser_advance(p);  // Skip invalid token
        }

        count = count + 1;
    }

    return program;
}

// ==== HELPER: Create manual token ====

fn make_token(type: i64, value: *i8) -> i64 {
    let tok: *Token = malloc(40);
    // Manually set fields using pointer arithmetic
    let ptr: *i64 = tok;
    ptr[0] = type;      // type
    ptr[1] = value;     // value
    ptr[2] = 1;         // line
    ptr[3] = 1;         // column
    ptr[4] = 0;         // length
    return tok;
}

fn make_token_list() -> i64 {
    let list: *TokenList = malloc(24);
    list.capacity = 256;
    list.count = 0;
    list.tokens = malloc(list.capacity * 40);
    return list;
}

fn add_token(list: *TokenList, tok: *Token) -> i64 {
    let offset: i64 = list.count * 40;
    let dest: *Token = list.tokens + offset;

    // Copy all fields using pointer arithmetic
    let src_ptr: *i64 = tok;
    let dest_ptr: *i64 = dest;
    dest_ptr[0] = src_ptr[0];  // type
    dest_ptr[1] = src_ptr[1];  // value
    dest_ptr[2] = src_ptr[2];  // line
    dest_ptr[3] = src_ptr[3];  // column
    dest_ptr[4] = src_ptr[4];  // length

    list.count = list.count + 1;
    return 0;
}

// ==== TEST/DEMO MAIN ====

fn main() -> i64 {
    println("========================================");
    println("  CHRONOS PARSER v1.0");
    println("========================================");
    println("");

    // Test 1: Simple expression "2 + 3"
    println("Test 1: Parse expression '2 + 3'");
    println("Creating token list...");
    let tokens1: *TokenList = make_token_list();
    println("Token list created");

    println("Creating token 1...");
    let tok1: *Token = make_token(TOK_NUMBER, "2");
    println("Token 1 created");

    println("Adding token 1...");
    add_token(tokens1, tok1);
    println("Token 1 added");

    println("Creating token 2...");
    let tok2: *Token = make_token(TOK_PLUS, "+");
    add_token(tokens1, tok2);

    println("Creating token 3...");
    let tok3: *Token = make_token(TOK_NUMBER, "3");
    add_token(tokens1, tok3);

    println("Creating EOF token...");
    let tok4: *Token = make_token(TOK_EOF, "");
    add_token(tokens1, tok4);

    print("Token count: ");
    print_int(tokens1.count);
    println("");

    println("Initializing parser...");
    let parser1: *Parser = parser_init(tokens1);
    println("Parser initialized");
    println("Parsing expression...");
    let ast1: *ASTNode = parse_expression(parser1);
    println("Expression parsed");

    print("AST node_type: ");
    print_int(ast1.node_type);
    println("");
    if (ast1.left != 0) {
        print("  left value: ");
        print_int(ast1.left.value);
        println("");
    }
    if (ast1.right != 0) {
        print("  right value: ");
        print_int(ast1.right.value);
        println("");
    }
    println("");

    // Test 2: Expression with precedence "2 + 3 * 4"
    println("Test 2: Parse '2 + 3 * 4' (should be 2 + (3 * 4))");
    let tokens2: *TokenList = make_token_list();
    add_token(tokens2, make_token(TOK_NUMBER, "2"));
    add_token(tokens2, make_token(TOK_PLUS, "+"));
    add_token(tokens2, make_token(TOK_NUMBER, "3"));
    add_token(tokens2, make_token(TOK_STAR, "*"));
    add_token(tokens2, make_token(TOK_NUMBER, "4"));
    add_token(tokens2, make_token(TOK_EOF, ""));

    let parser2: *Parser = parser_init(tokens2);
    let ast2: *ASTNode = parse_expression(parser2);

    print("Root node_type (should be ADD=10): ");
    print_int(ast2.node_type);
    println("");
    if (ast2.right != 0) {
        print("Right child node_type (should be MUL=12): ");
        print_int(ast2.right.node_type);
        println("");
    }
    println("");

    // Test 3: Parenthesized expression "(10 + 20) * 3"
    println("Test 3: Parse '(10 + 20) * 3'");
    let tokens3: *TokenList = make_token_list();
    add_token(tokens3, make_token(TOK_LPAREN, "("));
    add_token(tokens3, make_token(TOK_NUMBER, "10"));
    add_token(tokens3, make_token(TOK_PLUS, "+"));
    add_token(tokens3, make_token(TOK_NUMBER, "20"));
    add_token(tokens3, make_token(TOK_RPAREN, ")"));
    add_token(tokens3, make_token(TOK_STAR, "*"));
    add_token(tokens3, make_token(TOK_NUMBER, "3"));
    add_token(tokens3, make_token(TOK_EOF, ""));

    let parser3: *Parser = parser_init(tokens3);
    let ast3: *ASTNode = parse_expression(parser3);

    print("Root node_type (should be MUL=12): ");
    print_int(ast3.node_type);
    println("");
    if (ast3.left != 0) {
        print("Left child node_type (should be ADD=10): ");
        print_int(ast3.left.node_type);
        println("");
    }
    println("");

    println("✅ Parser tests complete!");

    return 0;
}
