// Test token assignment through pointer

struct Token {
    type: i64,
    start: i64,
    length: i64,
    line: i64
}

let T_FN = 4;

fn check_keyword(start: *i8, len: i64) -> i64 {
    if (len == 2 && start[0] == 102 && start[1] == 110) {
        return T_FN;
    }
    return 1;  // T_IDENT
}

fn get_token(tok: *Token, source: *i8) -> i64 {
    tok.type = check_keyword(source, 2);
    tok.length = 2;
    tok.start = 0;
    tok.line = 1;
    return 0;
}

fn main() -> i64 {
    println("=== Test Token Assignment ===");

    let arr: [i8; 5];
    arr[0] = 102;  // 'f'
    arr[1] = 110;  // 'n'
    arr[2] = 0;

    let tok: Token;
    get_token(&tok, arr);

    print("tok.type = ");
    print_int(tok.type);
    println("");

    print("tok.length = ");
    print_int(tok.length);
    println("");

    if (tok.type == T_FN) {
        println("✅ Correct!");
    } else {
        println("❌ Wrong!");
    }

    return 0;
}
