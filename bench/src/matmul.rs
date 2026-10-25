// BENCH: matmul N=384
const N: usize = 384;

fn main() {
    let n: i64 = 384;
    let mut a = [0i64; N * N];
    let mut b = [0i64; N * N];
    let mut c = [0i64; N * N];
    let mut i: i64 = 0;
    while i < n {
        let mut j: i64 = 0;
        while j < n {
            a[(i * n + j) as usize] = (i + j) & 7;
            b[(i * n + j) as usize] = (i * 3 + j) & 7;
            c[(i * n + j) as usize] = 0;
            j = j + 1;
        }
        i = i + 1;
    }
    i = 0;
    while i < n {
        let mut j: i64 = 0;
        while j < n {
            let mut s: i64 = 0;
            let mut k: i64 = 0;
            while k < n {
                s = s + a[(i * n + k) as usize] * b[(k * n + j) as usize];
                k = k + 1;
            }
            c[(i * n + j) as usize] = s;
            j = j + 1;
        }
        i = i + 1;
    }
    let mut sum: i64 = 0;
    i = 0;
    while i < n * n {
        sum = sum + c[i as usize];
        i = i + 1;
    }
    println!("{}", sum);
}
