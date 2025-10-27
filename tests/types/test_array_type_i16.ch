// i16 array: [i16; 50]
let words: [i16; 50];

fn main() -> i32 {
    // Should allocate 100 bytes (50 * 2) in .bss
    println("i16 array declared");
    return 0;
}
