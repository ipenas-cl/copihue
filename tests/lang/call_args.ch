// Nested calls inside argument lists must not clobber earlier arguments.
fn add3(a: i64, b: i64, c: i64) -> i64 { return a * 100 + b * 10 + c; }
fn sq(x: i64) -> i64 { return x * x; }
fn fib(n: i64) -> i64 {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
fn main() -> i32 {
    print_int(add3(1, sq(2), 3)); println("");
    print_int(add3(sq(1), sq(2), sq(3))); println("");
    print_int(fib(20)); println("");
    return 0;
}
