// BENCH: vec N=1000000
fn main() {
    let n: i64 = 1000000;
    let mut v: Vec<i64> = Vec::new();
    let mut i: i64 = 0;
    while i < n {
        v.push(i);
        i += 1;
    }
    let mut s: i64 = 0;
    for x in &v {
        s += *x;
    }
    println!("{} {} {}", s / 1000000, s % 1000000, v.len());
}
