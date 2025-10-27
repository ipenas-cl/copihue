// Test: Basic pointer operations - address-of and dereference
fn main() -> i32 {
    let x = 42;
    let ptr: *i32 = &x;
    let value = *ptr;

    print_int(value);
    println("");

    return 0;
}
