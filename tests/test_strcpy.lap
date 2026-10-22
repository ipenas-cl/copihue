// Test: strcpy - string copy
fn main() -> i32 {
    // Allocate buffers on stack (arrays)
    let buffer = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    // Copy "hello" to buffer
    let dest = strcpy(&buffer[0], "hello");

    // Verify: strcmp should return 0
    let result = strcmp(&buffer[0], "hello");

    print_int(result);
    println("");

    // Also check strlen
    let len = strlen(&buffer[0]);
    print_int(len);
    println("");

    return 0;
}
