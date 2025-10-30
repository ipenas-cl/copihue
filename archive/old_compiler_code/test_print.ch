fn main() -> i64 {
    let s1: *i8 = "hello";
    let s2: *i8 = "world";
    let s3: *i8 = "test";

    println("Testing print function:");
    print("s1: ");
    println(s1);
    print("s2: ");
    println(s2);
    print("s3: ");
    println(s3);

    return 0;
}
