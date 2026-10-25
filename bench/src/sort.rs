// BENCH: sort n=28000
fn main() {
    let n: i64 = 28000;
    let mut a = [0i64; 28000];
    let mut i: i64 = 0;
    while i < n {
        a[i as usize] = (i * 97 + 13) & 4095;
        i = i + 1;
    }
    i = 1;
    while i < n {
        let key = a[i as usize];
        let mut j = i - 1;
        while j >= 0 && a[j as usize] > key {
            a[(j + 1) as usize] = a[j as usize];
            j = j - 1;
        }
        a[(j + 1) as usize] = key;
        i = i + 1;
    }
    let mut sum: i64 = 0;
    i = 0;
    while i < n {
        sum = sum + a[i as usize] * (i + 1);
        i = i + 1;
    }
    println!("{}", sum);
}
