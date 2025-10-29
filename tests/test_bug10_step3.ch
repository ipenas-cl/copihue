// Bug #10 Step 3: Access the parameter

struct Point {
    x: i64,
    y: i64
}

fn process(points: *Point) -> i64 {
    return points[0].x;
}

fn main() -> i64 {
    println("Step 3");

    let arr: [Point; 2];
    arr[0].x = 10;

    let result = process(arr);

    print("Result: ");
    print_int(result);
    println("");

    return 0;
}
