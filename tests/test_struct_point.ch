// Test: Struct Point definition and field access
struct Point {
    x: i32,
    y: i32
}

fn main() -> i32 {
    let p = Point { x: 10, y: 20 };
    let a = p.x;
    let b = p.y;
    print_int(a);
    println("");
    print_int(b);
    println("");
    return 0;
}
