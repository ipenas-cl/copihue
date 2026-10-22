// Minimal test for struct pointer field

struct Buffer {
    data: *i8
}

fn main() -> i64 {
    println("Testing struct with pointer field");

    let arr: [i8; 5];
    arr[0] = 65;  // 'A'
    arr[1] = 66;  // 'B'
    arr[2] = 0;

    print("Direct access: arr[0] = ");
    print_int(arr[0]);
    println("");

    let buf: Buffer;
    buf.data = arr;

    print("After assignment: buf.data addr = ");
    print_int(buf.data);
    println("");

    return 0;
}
