fn main() -> i32 {
    let i = 15;
    let mod3 = i - (i / 3) * 3;
    print_int(mod3);
    println(" (should be 0)");
    return 0;
}
