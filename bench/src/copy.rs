// BENCH: copy n=262144 reps=192
fn main() {
    let n: i64 = 262144;
    let reps: i64 = 192;
    let mut src = [0u8; 262144];
    let mut dst = [0u8; 262144];
    let mut i: i64 = 0;
    while i < n {
        src[i as usize] = (i & 255) as u8;
        i = i + 1;
    }
    let mut r: i64 = 0;
    let mut sum: i64 = 0;
    while r < reps {
        i = 0;
        while i < n {
            dst[i as usize] = src[i as usize];
            i = i + 1;
        }
        i = 0;
        while i < n {
            sum = sum + dst[i as usize] as i64;
            src[i as usize] = ((src[i as usize] as i64 + 1) & 255) as u8;
            i = i + 1;
        }
        r = r + 1;
    }
    println!("{}", sum);
}
