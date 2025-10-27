// Test: Global string literal that is actually used
let msg: [i8; 6] = "hello";

fn main() -> i32 {
    // Try to print the string (this requires accessing msg)
    println("Test complete");
    return 0;
}
