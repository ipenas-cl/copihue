// Test: strlen - string length
fn main() -> i32 {
    let len1 = strlen("hello");
    let len2 = strlen("");
    let len3 = strlen("Chronos");

    // Expected: 5, 0, 7
    print_int(len1);
    println("");
    print_int(len2);
    println("");
    print_int(len3);
    println("");

    return 0;
}
