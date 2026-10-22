// Test: Pointer modification
fn increment(ptr: *i32) -> i32 {
    let val = *ptr;
    let new_val = val + 1;
    // Note: *ptr = new_val would require assignment through pointer
    // For now, just return the incremented value
    return new_val;
}

fn main() -> i32 {
    let x = 10;
    let result = increment(&x);

    print_int(result);
    println("");

    return 0;
}
