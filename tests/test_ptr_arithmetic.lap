// Test pointer arithmetic

fn check_bytes(ptr: *i8, len: i64) -> i64 {
    print("  ptr[0] = ");
    print_int(ptr[0]);
    println("");
    
    print("  ptr[1] = ");
    print_int(ptr[1]);
    println("");

    if (len == 2 && ptr[0] == 102 && ptr[1] == 110) {
        return 1;  // "fn" detected
    }
    
    return 0;
}

fn main() -> i64 {
    println("=== Test Pointer Arithmetic ===");

    let arr: [i8; 10];
    arr[0] = 102;  // 'f'
    arr[1] = 110;  // 'n'
    arr[2] = 32;   // ' '
    arr[3] = 109;  // 'm'

    println("Test 1: Pass array directly");
    let result1 = check_bytes(arr, 2);
    print("Result: ");
    print_int(result1);
    println("");

    println("");
    println("Test 2: Pass array + 2 (should see ' m')");
    let result2 = check_bytes(arr + 2, 2);
    print("Result: ");
    print_int(result2);
    println("");

    return 0;
}
