// Greatest Common Divisor (GCD) - Euclidean Algorithm
// Demonstrates: recursive algorithms, mathematical computation

fn gcd(a: i32, b: i32) -> i32 {
    if (b == 0) {
        return a;
    }
    return gcd(b, a % b);
}

fn lcm(a: i32, b: i32) -> i32 {
    return (a * b) / gcd(a, b);
}

fn main() -> i32 {
    let a = 48;
    let b = 18;

    print("GCD of ");
    print_int(a);
    print(" and ");
    print_int(b);
    print(" = ");
    print_int(gcd(a, b));
    println("");

    print("LCM of ");
    print_int(a);
    print(" and ");
    print_int(b);
    print(" = ");
    print_int(lcm(a, b));
    println("");

    return gcd(a, b);
}
