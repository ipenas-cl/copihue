// Test reading struct fields through pointers

struct Point {
    x: i64,
    y: i64
}

fn get_x(p: *Point) -> i64 {
    return p.x;
}

fn main() -> i64 {
    println("Testing struct pointer field read");

    let p: Point;
    p.x = 42;
    p.y = 99;

    let x_val = get_x(&p);

    print("p.x = ");
    print_int(p.x);
    println("");

    print("get_x(&p) = ");
    print_int(x_val);
    println("");

    return 0;
}
