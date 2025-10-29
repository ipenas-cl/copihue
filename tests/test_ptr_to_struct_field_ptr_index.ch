// Test: index through a pointer field, accessed via a pointer to the struct

struct Container {
    ptr: *i8
}

fn store_ptr(cont: *Container, p: *i8) -> i64 {
    print("  In store_ptr: p addr = ");
    print_int(p);
    println("");
    print("  In store_ptr: p[0] = ");
    print_int(p[0]);
    println("");

    cont.ptr = p;

    print("  After assignment: cont.ptr = ");
    print_int(cont.ptr);
    println("");
    print("  After assignment: cont.ptr[0] = ");
    print_int(cont.ptr[0]);
    println("");

    return 0;
}

fn main() -> i64 {
    println("=== Test Pointer-to-Struct Field Pointer Index ===");

    // Create array
    let arr: [i8; 5];
    arr[0] = 65;  // 'A'
    arr[1] = 66;  // 'B'
    arr[2] = 0;

    print("arr[0] = ");
    print_int(arr[0]);
    println("");

    // Create struct and pass pointer to it
    let cont: Container;
    store_ptr(&cont, arr);

    print("After store_ptr: cont.ptr = ");
    print_int(cont.ptr);
    println("");
    print("After store_ptr: cont.ptr[0] = ");
    print_int(cont.ptr[0]);
    println("");

    return 0;
}
