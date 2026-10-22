// Signed / and % truncate toward zero at every optimization level.
fn main() -> i32 {
    let a = 0 - 7; let b = 7; let c = 0 - 8;
    print_int(a / 2); print(" "); print_int(a % 4); print(" "); print_int(b / 4); print(" "); print_int(b % 4); print(" ");
    print_int(c / 8); print(" "); print_int(c % 8); print(" "); print_int(a * 4); print(" "); print_int(a / 1); println("");
    return 0;
}
