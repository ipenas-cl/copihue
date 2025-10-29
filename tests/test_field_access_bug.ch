// Minimal test for field access bug

struct Point {
    x: i64,
    y: i64
}

fn test_direct(arr: *Point) -> i64 {
    // Direct array access
    print("Direct: arr[0].x = ");
    print_int(arr[0].x);
    println("");

    print("Direct: arr[0].y = ");
    print_int(arr[0].y);
    println("");

    return 0;
}

fn main() -> i64 {
    println("=== Field Access Bug Test ===");

    let points: [Point; 2];
    points[0].x = 42;
    points[0].y = 99;

    print("Main: points[0].x = ");
    print_int(points[0].x);
    println("");

    print("Main: points[0].y = ");
    print_int(points[0].y);
    println("");

    test_direct(points);

    println("✅ Test complete!");
    return 0;
}
