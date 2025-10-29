// CHRONOS PARSER DEMO - Simplified Parser to demonstrate concepts
// This is a PROOF OF CONCEPT for the self-hosted compiler
// Author: Chronos Project
// Date: 2025-10-29
//
// LIMITATIONS:
// - Chronos doesn't support forward declarations yet
// - Chronos doesn't have malloc/dynamic memory yet
// - Full recursive descent needs mutual recursion
// - This demo shows the parser structure without full recursion
//
// NEXT STEPS for full parser:
// 1. Implement forward declarations in compiler
// 2. Implement malloc/free for dynamic AST building
// 3. Implement full recursive descent with proper tree structure

// ============================================
// TOKEN TYPES (from lexer)
// ============================================

let T_EOF = 0;
let T_NUM = 2;
let T_PLUS = 24;
let T_MINUS = 25;
let T_STAR = 26;
let T_LPAREN = 13;
let T_RPAREN = 14;

// ============================================
// AST NODE TYPES (simplified)
// ============================================

let AST_NUMBER = 9;
let AST_BINOP = 10;

// ============================================
// STRUCTURES
// ============================================

struct Token {
    type: i64,
    start: i64,
    length: i64,
    line: i64,
    value: i64  // For demo: store number value directly
}

struct Parser {
    tokens: *Token,
    token_count: i64,
    pos: i64,
    has_error: i64
}

// Simple AST node (without children for now)
struct AstNode {
    node_type: i64,
    value: i64,      // For numbers
    op: i64,         // For operators ('+' = 43, etc.)
    left_val: i64,   // Simplified: store value instead of pointer
    right_val: i64
}

// ============================================
// PARSER HELPERS
// ============================================

fn parser_init(p: *Parser, tokens: *Token, count: i64) -> i64 {
    p.tokens = tokens;
    p.token_count = count;
    p.pos = 0;
    p.has_error = 0;
    return 0;
}

fn peek_token(p: *Parser) -> i64 {
    if (p.pos >= p.token_count) {
        return T_EOF;
    }
    return p.tokens[p.pos].type;
}

fn get_current_token(p: *Parser, result: *Token) -> i64 {
    if (p.pos < p.token_count) {
        result.type = p.tokens[p.pos].type;
        result.value = p.tokens[p.pos].value;
        result.line = p.tokens[p.pos].line;
        return 1;
    }
    return 0;
}

fn advance_token(p: *Parser) -> i64 {
    if (p.pos < p.token_count) {
        p.pos = p.pos + 1;
    }
    return 0;
}

fn check_token(p: *Parser, expected: i64) -> i64 {
    if (peek_token(p) == expected) {
        return 1;
    }
    return 0;
}

fn expect_token(p: *Parser, expected: i64) -> i64 {
    if (check_token(p, expected)) {
        advance_token(p);
        return 1;
    }

    print("Parse error: expected token ");
    print_int(expected);
    print(" but got ");
    print_int(peek_token(p));
    println("");
    p.has_error = 1;
    return 0;
}

// ============================================
// SIMPLIFIED PARSING (no recursion)
// ============================================

// Parse a simple expression: number (+ | - | *) number
// Example: "5 + 3" or "10 * 2"
fn parse_simple_binop(p: *Parser, result: *AstNode) -> i64 {
    result.node_type = AST_BINOP;
    result.left_val = 0;
    result.right_val = 0;
    result.op = 0;

    // Parse left operand (must be number)
    if (check_token(p, T_NUM) == 0) {
        println("Error: expected number");
        p.has_error = 1;
        return 0;
    }

    let left_tok: Token;
    get_current_token(p, &left_tok);
    result.left_val = left_tok.value;
    advance_token(p);

    // Parse operator
    let op_type = peek_token(p);
    if (op_type == T_PLUS || op_type == T_MINUS || op_type == T_STAR) {
        result.op = op_type;
        advance_token(p);
    } else {
        println("Error: expected operator");
        p.has_error = 1;
        return 0;
    }

    // Parse right operand (must be number)
    if (check_token(p, T_NUM) == 0) {
        println("Error: expected number");
        p.has_error = 1;
        return 0;
    }

    let right_tok: Token;
    get_current_token(p, &right_tok);
    result.right_val = right_tok.value;
    advance_token(p);

    return 1;
}

// Evaluate the parsed AST (simple interpreter)
fn eval_ast(ast: *AstNode) -> i64 {
    if (ast.node_type == AST_NUMBER) {
        return ast.value;
    }

    if (ast.node_type == AST_BINOP) {
        let left = ast.left_val;
        let right = ast.right_val;

        if (ast.op == T_PLUS) {
            return left + right;
        }
        if (ast.op == T_MINUS) {
            return left - right;
        }
        if (ast.op == T_STAR) {
            return left * right;
        }
    }

    return 0;
}

// ============================================
// TESTING FUNCTION
// ============================================

fn test_parse(tokens: *Token, count: i64, expected: i64) -> i64 {
    let p: Parser;
    parser_init(&p, tokens, count);

    let ast: AstNode;
    parse_simple_binop(&p, &ast);

    if (p.has_error) {
        println("  ❌ Parse failed");
        return 0;
    }

    let result = eval_ast(&ast);

    print("  Parsed: ");
    print_int(ast.left_val);

    if (ast.op == T_PLUS) {
        print(" + ");
    } else {
        if (ast.op == T_MINUS) {
            print(" - ");
        } else {
            if (ast.op == T_STAR) {
                print(" * ");
            }
        }
    }

    print_int(ast.right_val);
    print(" = ");
    print_int(result);
    println("");

    if (result == expected) {
        println("  ✅ Correct!");
        return 1;
    } else {
        print("  ❌ Expected ");
        print_int(expected);
        println("");
        return 0;
    }
}

fn main() -> i64 {
    println("=== Chronos Parser Demo - Self-Hosted! ===");
    println("");
    println("This is a simplified parser demonstrating core concepts.");
    println("Full parser requires forward declarations + malloc.");
    println("");

    // Test 1: 5 + 3 = 8
    println("Test 1: 5 + 3");
    let tokens1: [Token; 3];
    tokens1[0].type = T_NUM;
    tokens1[0].value = 5;
    tokens1[1].type = T_PLUS;
    tokens1[2].type = T_NUM;
    tokens1[2].value = 3;

    test_parse(tokens1, 3, 8);
    println("");

    // Test 2: 10 - 4 = 6
    println("Test 2: 10 - 4");
    let tokens2: [Token; 3];
    tokens2[0].type = T_NUM;
    tokens2[0].value = 10;
    tokens2[1].type = T_MINUS;
    tokens2[2].type = T_NUM;
    tokens2[2].value = 4;

    test_parse(tokens2, 3, 6);
    println("");

    // Test 3: 7 * 6 = 42
    println("Test 3: 7 * 6");
    let tokens3: [Token; 3];
    tokens3[0].type = T_NUM;
    tokens3[0].value = 7;
    tokens3[1].type = T_STAR;
    tokens3[2].type = T_NUM;
    tokens3[2].value = 6;

    test_parse(tokens3, 3, 42);
    println("");

    println("✅ Parser demo complete!");
    println("");
    println("NEXT STEPS:");
    println("1. Add forward declarations to Chronos compiler");
    println("2. Add malloc/free for dynamic memory");
    println("3. Implement full recursive descent parser");
    println("4. Build complete AST tree structure");

    return 0;
}
