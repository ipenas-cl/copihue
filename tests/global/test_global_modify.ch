// Test 3: Modify global from multiple functions
let shared = 100;

fn add_ten() -> i32 {
    shared = shared + 10;
    return shared;
}

fn subtract_five() -> i32 {
    shared = shared - 5;
    return shared;
}

fn main() -> i32 {
    add_ten();      // 110
    add_ten();      // 120
    subtract_five(); // 115
    print_int(shared);
    println("");
    return 0;
}
