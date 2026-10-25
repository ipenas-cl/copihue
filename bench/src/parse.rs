// BENCH: parse n=65536 reps=512
fn main() {
    let n: i64 = 65536;
    let reps: i64 = 512;
    let mut buf = [0u8; 65536];
    let mut i: i64 = 0;
    while i < n {
        if (i % 10) == 9 {
            buf[i as usize] = 44;
        } else {
            buf[i as usize] = (48 + (i % 10)) as u8;
        }
        i = i + 1;
    }
    let mut r: i64 = 0;
    let mut sum: i64 = 0;
    while r < reps {
        i = 0;
        let mut v: i64 = 0;
        while i < n {
            let c: i64 = buf[i as usize] as i64;
            if c == 44 {
                sum = sum + v;
                v = 0;
            } else {
                v = v * 10 + (c - 48);
            }
            i = i + 1;
        }
        sum = sum + v;
        r = r + 1;
    }
    println!("{}", sum);
}
