// Debug AST test

fn main() -> i64 {
    println("Step 1");

    let size = 80;
    println("Step 2");

    let node_ptr = malloc(size);
    println("Step 3");

    if (node_ptr == 0) {
        println("ERROR: malloc failed!");
        return 1;
    }

    print("node_ptr = ");
    print_int(node_ptr);
    println("");

    println("Step 4");

    free(node_ptr);

    println("Step 5");

    return 0;
}
