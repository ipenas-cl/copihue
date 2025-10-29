// Test check_keyword function

let T_EOF = 0;
let T_IDENT = 1;
let T_FN = 4;

fn check_keyword(start: *i8, len: i64) -> i64 {
    // fn
    if (len == 2 && start[0] == 102 && start[1] == 110) {
        return T_FN;
    }
    
    return T_IDENT;
}

fn main() -> i64 {
    println("=== Test check_keyword ===");

    let arr: [i8; 10];
    arr[0] = 102;  // 'f'
    arr[1] = 110;  // 'n'
    arr[2] = 0;

    print("arr[0] = ");
    print_int(arr[0]);
    println("");
    
    print("arr[1] = ");
    print_int(arr[1]);
    println("");

    let result = check_keyword(arr, 2);

    print("check_keyword result: ");
    print_int(result);
    println("");

    print("T_FN = ");
    print_int(T_FN);
    println("");

    if (result == T_FN) {
        println("✅ Correctly identified as T_FN");
    } else {
        println("❌ Wrong type");
    }

    return 0;
}
