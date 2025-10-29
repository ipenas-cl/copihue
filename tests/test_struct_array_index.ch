// Test array indexing through struct pointer field

struct Buffer {
    data: *i8
}

fn main() -> i64 {
    println("Testing struct field array indexing");

    let arr: [i8; 10];
    arr[0] = 102;  // 'f'
    arr[1] = 110;  // 'n'
    arr[2] = 0;

    let buf: Buffer;
    buf.data = arr;

    print("buf.data[0] = ");
    print_int(buf.data[0]);
    println("");

    print("buf.data[1] = ");
    print_int(buf.data[1]);
    println("");

    return 0;
}
