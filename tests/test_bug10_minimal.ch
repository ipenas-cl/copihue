// Minimal test for Bug #10: Struct array parameter crash

struct Point {
    x: i64,
    y: i64
}

fn process(points: *Point) -> i64 {
    return points[0].x;
}

fn main() -> i64 {
    println("=== Bug #10 Test ===");

    let arr: [Point; 2];
    arr[0].x = 10;
    arr[0].y = 20;

    let result = process(arr);

    print("Result: ");
    print_int(result);
    println("");

    return 0;
}
