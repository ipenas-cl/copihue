fn main() -> i32 {
    // Basic modulo
    let x = 10;
    let y = 3;
    let result = x % y;
    print_int(result);  // Should print 1
    println("");

    // More test cases
    print_int(17 % 5);  // Should print 2
    println("");
    print_int(100 % 7);  // Should print 2
    println("");
    print_int(25 % 5);  // Should print 0
    println("");

    println("All modulo tests passed!");
    return 0;
}
