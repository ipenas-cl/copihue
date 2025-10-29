// Test passing parser

struct Token {
    type: i64,
    value: i64
}

struct Parser {
    tokens: *Token,
    count: i64,
    pos: i64
}

fn read_token(p: *Parser) -> i64 {
    print("In function, p.pos = ");
    print_int(p.pos);
    println("");

    print("tokens[0].value = ");
    print_int(p.tokens[0].value);
    println("");

    return p.tokens[0].value;
}

fn main() -> i64 {
    println("=== Parser Pass Test ===");

    let tokens: [Token; 2];
    tokens[0].type = 1;
    tokens[0].value = 42;
    tokens[1].type = 2;
    tokens[1].value = 99;

    let parser: Parser;
    parser.tokens = tokens;
    parser.count = 2;
    parser.pos = 0;

    print("Before call, parser.pos = ");
    print_int(parser.pos);
    println("");

    let val: i64 = read_token(parser);

    print("Returned value: ");
    print_int(val);
    println("");

    println("✅ Test complete!");
    return 0;
}
