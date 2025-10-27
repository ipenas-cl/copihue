fn main() -> i32 {
    let nums = [1, 2, 3, 4, 5];
    let sum = 0;
    let i = 0;
    while (i < 5) {
        sum = sum + nums[i];
        i = i + 1;
    }
    print("Sum: ");
    print_int(sum);
    println("");
    return 0;
}
