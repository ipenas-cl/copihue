// Complete Struct Demonstration
// Shows all struct features working in Chronos

struct Point {
    x: i32,
    y: i32
}

struct Rectangle {
    width: i32,
    height: i32
}

fn calculate_distance(p1: Point, p2: Point) -> i32 {
    let dx = p2.x - p1.x;
    let dy = p2.y - p1.y;
    return dx + dy;  // Manhattan distance
}

fn area(r: Rectangle) -> i32 {
    return r.width * r.height;
}

fn main() -> i32 {
    println("=== Chronos Struct Demonstration ===");
    println("");

    // 1. Struct declaration with type
    println("1. Declaration with type:");
    let p1: Point;
    p1.x = 10;
    p1.y = 20;
    print("   p1 = (");
    print_int(p1.x);
    print(", ");
    print_int(p1.y);
    println(")");

    // 2. Struct literal initialization
    println("2. Struct literal:");
    let p2 = Point { x: 30, y: 40 };
    print("   p2 = (");
    print_int(p2.x);
    print(", ");
    print_int(p2.y);
    println(")");

    // 3. Field access in expressions
    println("3. Field access in expressions:");
    let sum_x = p1.x + p2.x;
    let sum_y = p1.y + p2.y;
    print("   sum = (");
    print_int(sum_x);
    print(", ");
    print_int(sum_y);
    println(")");

    // 4. Struct parameters (pass by value)
    println("4. Function with struct parameters:");
    let dist = calculate_distance(p1, p2);
    print("   distance(p1, p2) = ");
    print_int(dist);
    println("");

    // 5. Multiple struct types
    println("5. Multiple struct types:");
    let rect: Rectangle;
    rect.width = 15;
    rect.height = 25;
    let a = area(rect);
    print("   rect(");
    print_int(rect.width);
    print("x");
    print_int(rect.height);
    print(") area = ");
    print_int(a);
    println("");

    // 6. Field modification
    println("6. Field modification:");
    p1.x = p1.x + 5;
    p1.y = p1.y * 2;
    print("   modified p1 = (");
    print_int(p1.x);
    print(", ");
    print_int(p1.y);
    println(")");

    println("");
    println("=== All struct features working! ===");

    return 0;
}
