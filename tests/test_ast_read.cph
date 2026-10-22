// Test read value

fn helper_set(ptr: *i64, value: i64) -> i64 {
    ptr[0] = value;
    return 0;
}

fn helper_get(ptr: *i64) -> i64 {
    return ptr[0];
}

fn main() -> i64 {
    println("Step 1");

    let node_ptr = malloc(80);

    if (node_ptr == 0) {
        println("ERROR: malloc failed!");
        return 1;
    }

    println("Setting value to 42...");
    helper_set(node_ptr, 42);

    println("Reading value...");
    let val = helper_get(node_ptr);

    print("val = ");
    print_int(val);
    println("");

    free(node_ptr);

    println("Done!");
    return 0;
}
