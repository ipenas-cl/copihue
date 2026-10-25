// BENCH: fold n=500000000
fn main() {
    let n: i64 = 500000000;
    let mut s: i64 = 0;
    let mut i: i64 = 0;
    while i < n {
        s = s + i * 3 + 1;
        i = i + 1;
    }
    println!("{}", s);
}
