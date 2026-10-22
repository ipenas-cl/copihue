// Escape sequences and printing strings stored in buffers.
let buf: [i8; 64];
fn main() -> i32 {
    print("tab:\t|\n");
    print("quote:\" backslash:\\\n");
    strcpy(buf, "from buffer");
    println(buf);
    print_int(strlen("a\nb")); println("");
    let s = "via variable";
    println(s);
    return 0;
}
