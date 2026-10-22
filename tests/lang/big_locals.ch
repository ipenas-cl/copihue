// Frames are sized from the actual locals (no fixed 1 KB limit).
fn main() -> i32 {
    let big: [i64; 600];
    let i = 0;
    while (i < 600) { big[i] = i; i++; }
    let sum = 0;
    i = 0;
    while (i < 600) { sum = sum + big[i]; i++; }
    print_int(sum); println("");
    return 0;
}
