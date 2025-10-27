// Test 10: Chain of global modifications
let value = 1;

fn double_it() -> i32 {
    value = value + value;
    return value;
}

fn main() -> i32 {
    double_it();  // 2
    double_it();  // 4
    double_it();  // 8
    double_it();  // 16
    print_int(value);
    println("");
    return 0;
}
