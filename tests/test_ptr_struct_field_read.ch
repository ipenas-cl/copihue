// Test reading non-pointer fields from pointer to struct

struct Counter {
    value: i64,
    step: i64
}

fn get_value(c: *Counter) -> i64 {
    return c.value;
}

fn main() -> i64 {
    println("Testing pointer to struct field read");

    let counter: Counter;
    counter.value = 42;
    counter.step = 5;

    print("counter.value = ");
    print_int(counter.value);
    println("");

    let val = get_value(&counter);
    print("get_value(&counter) = ");
    print_int(val);
    println("");

    return 0;
}
