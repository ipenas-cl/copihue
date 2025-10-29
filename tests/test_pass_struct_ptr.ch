// Test passing pointer to local struct

struct Counter {
    value: i64,
    step: i64
}

fn print_counter(c: *Counter) -> i64 {
    print("In print_counter: c.value = ");
    print_int(c.value);
    println("");
    return 0;
}

fn main() -> i64 {
    println("Testing passing pointer to struct");

    let counter: Counter;
    counter.value = 99;
    counter.step = 5;

    print("In main: counter.value = ");
    print_int(counter.value);
    println("");

    print_counter(&counter);

    return 0;
}
