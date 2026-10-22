// Fibonacci Sequence Generator
// Demonstrates: iterative algorithms, variable updates

fn fibonacci(n: i32) -> i32 {
    if (n <= 1) {
        return n;
    }

    let a = 0;
    let b = 1;
    let i = 2;

    while (i <= n) {
        let temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }

    return b;
}

fn main() -> i32 {
    println("Fibonacci sequence (first 20 numbers):");

    let i = 0;
    while (i < 20) {
        let fib = fibonacci(i);
        print_int(fib);
        println("");
        i = i + 1;
    }

    return 0;
}
