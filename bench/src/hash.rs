// BENCH: hash n=131072 reps=256
fn main() {
    let n: i64 = 131072;
    let reps: i64 = 256;
    let mut buf = [0u8; 131072];
    let mut i: i64 = 0;
    while i < n {
        buf[i as usize] = (i & 255) as u8;
        i = i + 1;
    }
    let mut r: i64 = 0;
    let mut total: i64 = 0;
    while r < reps {
        let mut h: i64 = 18652613;
        i = 0;
        while i < n {
            h = (h * 16777619 + buf[i as usize] as i64) & 2147483647;
            i = i + 1;
        }
        total = total + h;
        r = r + 1;
    }
    println!("{}", total);
}
