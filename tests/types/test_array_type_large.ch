// Large i64 array: [i64; 1000]
let large_arr: [i64; 1000];

fn main() -> i32 {
    // Should allocate 8000 bytes in .bss
    println("Large array declared");
    return 0;
}
