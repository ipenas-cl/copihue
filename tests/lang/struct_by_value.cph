// Structs passed by value are copied: the callee cannot modify the caller's copy.
struct Point { x: i64, y: i64 }
struct Rect { a: Point, b: Point }

fn area(r: Rect) -> i64 {
    return (r.b.x - r.a.x) * (r.b.y - r.a.y);
}

fn bump(p: Point) -> i64 {
    p.x = p.x + 100;
    return p.x;
}

fn main() -> i32 {
    let r = Rect { a: Point { x: 1, y: 2 }, b: Point { x: 4, y: 7 } };
    print_int(area(r)); println("");
    let p = Point { x: 5, y: 6 };
    print_int(bump(p)); println("");
    print_int(p.x); println("");
    return 0;
}
