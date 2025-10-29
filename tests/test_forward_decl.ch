// Test forward declarations for mutual recursion

// Forward declarations
fn is_even(n: i64) -> i64;
fn is_odd(n: i64) -> i64;

// Implementations
fn is_even(n: i64) -> i64 {
    if (n == 0) {
        return 1;
    }
    return is_odd(n - 1);
}

fn is_odd(n: i64) -> i64 {
    if (n == 0) {
        return 0;
    }
    return is_even(n - 1);
}

fn main() -> i64 {
    println("=== Forward Declaration Test ===");

    print("is_even(4) = ");
    print_int(is_even(4));
    println("");

    print("is_odd(4) = ");
    print_int(is_odd(4));
    println("");

    print("is_even(5) = ");
    print_int(is_even(5));
    println("");

    print("is_odd(5) = ");
    print_int(is_odd(5));
    println("");

    return 0;
}
