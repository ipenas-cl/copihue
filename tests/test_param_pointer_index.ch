// Test indexing a pointer parameter

fn read_byte(ptr: *i8, idx: i64) -> i64 {
    print("  In read_byte: ptr = ");
    print_int(ptr);
    println("");

    let val = ptr[idx];

    print("  ptr[idx] = ");
    print_int(val);
    println("");

    return val;
}

fn main() -> i64 {
    println("=== Test Pointer Parameter Indexing ===");

    let arr: [i8; 5];
    arr[0] = 65;  // 'A'
    arr[1] = 66;  // 'B'
    arr[2] = 67;  // 'C'

    print("arr[0] in main = ");
    print_int(arr[0]);
    println("");

    let result = read_byte(arr, 0);

    print("Result = ");
    print_int(result);
    println("");

    return 0;
}
