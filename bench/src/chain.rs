// BENCH: chain n=80000000
fn main() {
    let n: i64 = 80000000;
    let mut x: i64 = 1;
    let mut i: i64 = 0;
    while i < n {
        x = (x + i) & 2147483647;
        x = (x * 13) & 2147483647;
        x = x ^ (x >> 7);
        i = i + 1;
    }
    println!("{}", x);
}
