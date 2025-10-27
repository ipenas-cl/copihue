// Test to demonstrate compiler optimizations

fn test_constant_folding() -> i32 {
    // These should be folded at compile time with -O1
    let a = 10 + 20;           // Should become 30
    let b = 100 - 25;          // Should become 75
    let c = 5 * 8;             // Should become 40
    let d = 100 / 4;           // Should become 25
    let e = 17 % 5;            // Should become 2

    return a + b + c + d + e;  // 30 + 75 + 40 + 25 + 2 = 172
}

fn test_strength_reduction() -> i32 {
    let x = 1000;

    // These should use shifts/masks with -O2
    let a = x * 2;    // Should become: shl rax, 1
    let b = x * 4;    // Should become: shl rax, 2
    let c = x / 8;    // Should become: sar rax, 3
    let d = x % 16;   // Should become: and rax, 15

    return a + b + c + d;  // 2000 + 4000 + 125 + 8 = 6133
}

fn main() -> i32 {
    println("Testing Compiler Optimizations");
    println("");

    print("Constant Folding: ");
    print_int(test_constant_folding());
    println("");

    print("Strength Reduction: ");
    print_int(test_strength_reduction());
    println("");

    return 0;
}
