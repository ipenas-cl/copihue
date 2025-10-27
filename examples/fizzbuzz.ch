// FizzBuzz - Classic programming interview question
// Demonstrates: loops, conditionals, modulo, printing

fn main() -> i32 {
    let i = 1;

    while (i <= 100) {
        // Check divisibility by 3 and 5
        let div3 = (i % 3 == 0);
        let div5 = (i % 5 == 0);

        if (div3) {
            if (div5) {
                println("FizzBuzz");
            } else {
                println("Fizz");
            }
        } else {
            if (div5) {
                println("Buzz");
            } else {
                print_int(i);
                println("");
            }
        }

        i = i + 1;
    }

    return 0;
}
