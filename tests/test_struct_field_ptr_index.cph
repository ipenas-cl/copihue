// Test: index through a pointer stored in a struct field

struct Container {
    ptr: *i8
}

fn main() -> i64 {
    println("=== Test Struct Field Pointer Index ===");

    // Create array
    let arr: [i8; 5];
    arr[0] = 65;  // 'A'
    arr[1] = 66;  // 'B'
    arr[2] = 0;

    print("arr[0] = ");
    print_int(arr[0]);
    println("");

    // Store pointer in struct
    let cont: Container;
    cont.ptr = arr;

    print("cont.ptr addr = ");
    print_int(cont.ptr);
    println("");

    // Try to index through struct field pointer
    let val = cont.ptr[0];

    print("cont.ptr[0] = ");
    print_int(val);
    println("");

    if (val == 65) {
        println("✅ Success!");
    } else {
        println("❌ Wrong value!");
    }

    return 0;
}
