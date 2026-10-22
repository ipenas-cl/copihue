// Test array indexing through pointer to struct

struct Buffer {
    data: *i8
}

fn get_char(buf: *Buffer, idx: i64) -> i64 {
    return buf.data[idx];
}

fn main() -> i64 {
    println("Testing pointer to struct field array indexing");

    let arr: [i8; 5];
    arr[0] = 65;  // 'A'
    arr[1] = 66;  // 'B'

    let buf: Buffer;
    buf.data = arr;

    let ch = get_char(&buf, 0);

    print("get_char(&buf, 0) = ");
    print_int(ch);
    println("");

    return 0;
}
