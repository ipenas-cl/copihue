// BENCH: fill n=262144 reps=128
fn main() {
    let n: i64 = 262144;
    let reps: i64 = 128;
    let mut buf = [0u8; 262144];
    let mut r: i64 = 0;
    let mut sum: i64 = 0;
    while r < reps {
        let mut i: i64 = 0;
        while i < n {
            buf[i as usize] = ((buf[i as usize] as i64 + i + r) & 255) as u8;
            i = i + 1;
        }
        i = 0;
        while i < n {
            sum = sum + buf[i as usize] as i64;
            i = i + 1;
        }
        r = r + 1;
    }
    println!("{}", sum);
}
