// Prime Number Generator
// Demonstrates: nested loops, division, mathematical algorithms

fn is_prime(n: i32) -> i32 {
    if (n <= 1) {
        return 0;
    }
    if (n == 2) {
        return 1;
    }

    // Check if even
    if (n % 2 == 0) {
        return 0;
    }

    let i = 3;
    while (i * i <= n) {
        // Check if divisible by i
        if (n % i == 0) {
            return 0;
        }
        i = i + 2;
    }

    return 1;
}

fn main() -> i32 {
    println("Prime numbers up to 100:");

    let n = 2;
    let count = 0;

    while (n <= 100) {
        if (is_prime(n)) {
            print_int(n);
            println("");
            count = count + 1;
        }
        n = n + 1;
    }

    println("");
    print_int(count);
    println(" primes found");

    return count;
}
