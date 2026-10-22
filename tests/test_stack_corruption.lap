// Test stack corruption with nested function calls

struct Data {
    ptr: *i8
}

fn process(d: *Data) -> i64 {
    print("  [process] Start: d.ptr[0]=");
    print_int(d.ptr[0]);
    println("");
    
    let x = 42;
    
    print("  [process] After let x: d.ptr[0]=");
    print_int(d.ptr[0]);
    println("");
    
    let y = 99;
    
    print("  [process] After let y: d.ptr[0]=");
    print_int(d.ptr[0]);
    println("");
    
    return 0;
}

fn main() -> i64 {
    println("=== Test Stack Corruption ===");

    let arr: [i8; 10];
    arr[0] = 102;
    arr[1] = 110;
    arr[2] = 0;

    print("[main] arr[0]=");
    print_int(arr[0]);
    println("");

    let data: Data;
    data.ptr = arr;

    print("[main] After init: arr[0]=");
    print_int(arr[0]);
    println("");

    process(&data);

    print("[main] After process: arr[0]=");
    print_int(arr[0]);
    println("");

    return 0;
}
