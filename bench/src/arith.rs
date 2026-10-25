// BENCH: arith n=100000000
fn main() {
    let n: i64 = 100000000;
    let mut a: i64 = 1;
    let mut b: i64 = 2;
    let mut c: i64 = 3;
    let mut i: i64 = 0;
    while i < n {
        a = (a + b) & 2147483647;
        b = (b + c) & 2147483647;
        c = (c + i) & 2147483647;
        i = i + 1;
    }
    println!("{}", a + b + c);
}
