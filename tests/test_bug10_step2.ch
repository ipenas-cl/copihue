// Bug #10 Step 2: Call function with struct array

struct Point {
    x: i64,
    y: i64
}

fn process(points: *Point) -> i64 {
    return 0;
}

fn main() -> i64 {
    println("Step 2");

    let arr: [Point; 2];
    arr[0].x = 10;

    let result = process(arr);

    println("Done");
    return 0;
}
