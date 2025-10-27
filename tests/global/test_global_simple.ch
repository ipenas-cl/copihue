// Test 1: Simple global variable
let global_counter = 0;

fn increment() -> i32 {
    global_counter = global_counter + 1;
    return global_counter;
}

fn main() -> i32 {
    increment();
    increment();
    increment();
    print_int(global_counter);
    println("");
    return 0;
}
