// Test struct field assignment through pointers

struct Point {
    x: i32,
    y: i32
}

fn set_point(p: *Point, a: i32, b: i32) -> i32 {
    p.x = a;
    p.y = b;
    return 0;
}

fn main() -> i32 {
    println("Testing struct pointer assignment");

    let p: Point;
    set_point(&p, 10, 20);

    print("p.x = ");
    print_int(p.x);
    println("");
    print("p.y = ");
    print_int(p.y);
    println("");

    return 0;
}
