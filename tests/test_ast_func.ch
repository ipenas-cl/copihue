// Test function call

fn helper(ptr: *i64, value: i64) -> i64 {
    println("Inside helper");
    ptr[0] = value;
    println("Set value");
    return 0;
}

fn main() -> i64 {
    println("Step 1");

    let node_ptr = malloc(80);

    if (node_ptr == 0) {
        println("ERROR: malloc failed!");
        return 1;
    }

    print("node_ptr = ");
    print_int(node_ptr);
    println("");

    println("Calling helper...");
    helper(node_ptr, 42);

    println("Back from helper");

    print("ptr[0] = ");
    print_int(node_ptr[0]);
    println("");

    free(node_ptr);

    println("Done!");
    return 0;
}
