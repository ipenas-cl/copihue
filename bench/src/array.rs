// BENCH: array n=100000 rounds=1000
fn main() {
    let n: i64 = 100000;
    let rounds: i64 = 1000;
    let mut a = [0i64; 100000];
    let mut i: i64 = 0;
    while i < n {
        a[i as usize] = (i * 3 + 1) & 1023;
        i = i + 1;
    }
    let mut round: i64 = 0;
    while round < rounds {
        i = 0;
        while i < n {
            let v = a[i as usize];
            a[i as usize] = (v * 3 + (i & 7) + 1) & 1023;
            i = i + 1;
        }
        round = round + 1;
    }
    let mut sum: i64 = 0;
    i = 0;
    while i < n {
        sum = sum + a[i as usize] * (i + 1);
        i = i + 1;
    }
    println!("{}", sum);
}
