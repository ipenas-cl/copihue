// BENCH: sieve limit=400000 reps=80
fn main() {
    let limit: i64 = 400000;
    let reps: i64 = 80;
    let mut flags = [0u8; 400000];
    let mut sum: i64 = 0;
    let mut r: i64 = 0;
    while r < reps {
        let mut i: i64 = 0;
        while i < limit {
            flags[i as usize] = 1;
            i = i + 1;
        }
        flags[0] = 0;
        flags[1] = 0;
        i = 2;
        while i * i < limit {
            if flags[i as usize] != 0 {
                let mut j: i64 = i * i;
                while j < limit {
                    flags[j as usize] = 0;
                    j = j + i;
                }
            }
            i = i + 1;
        }
        sum = 0;
        i = 2;
        while i < limit {
            if flags[i as usize] != 0 {
                sum = sum + i;
            }
            i = i + 1;
        }
        r = r + 1;
    }
    println!("{}", sum);
}
