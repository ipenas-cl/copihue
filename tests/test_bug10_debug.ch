// Bug #10 Debug

struct Point {
    x: i64,
    y: i64
}

fn process(points: *Point) -> i64 {
    print("  In process, points[0].x = ");
    print_int(points[0].x);
    println("");
    return points[0].x;
}

fn main() -> i64 {
    println("=== Bug #10 Debug ===");

    let arr: [Point; 2];
    arr[0].x = 42;
    arr[0].y = 99;
    arr[1].x = 10;
    arr[1].y = 20;

    print("In main, arr[0].x = ");
    print_int(arr[0].x);
    println("");

    print("In main, arr[0].y = ");
    print_int(arr[0].y);
    println("");

    let result = process(arr);

    print("Result: ");
    print_int(result);
    println("");

    if (result == 42) {
        println("✅ Correct!");
    } else {
        println("❌ Wrong!");
    }

    return 0;
}
