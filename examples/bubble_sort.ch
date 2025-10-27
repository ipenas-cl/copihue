// Bubble Sort Algorithm
// Demonstrates: arrays, nested loops, swapping elements

// Global array (local typed arrays not supported)
let arr: [i32; 10];

fn bubble_sort(arr: *i32, len: i32) -> i32 {
    let i = 0;

    while (i < len) {
        let j = 0;
        while (j < len - i - 1) {
            if (arr[j] > arr[j + 1]) {
                // Swap
                let temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
            j = j + 1;
        }
        i = i + 1;
    }

    return 0;
}

fn print_array(arr: *i32, len: i32) -> i32 {
    let i = 0;
    while (i < len) {
        print_int(arr[i]);
        if (i < len - 1) {
            print(" ");
        }
        i = i + 1;
    }
    println("");
    return 0;
}

fn main() -> i32 {
    arr[0] = 64;
    arr[1] = 34;
    arr[2] = 25;
    arr[3] = 12;
    arr[4] = 22;
    arr[5] = 11;
    arr[6] = 90;
    arr[7] = 88;
    arr[8] = 45;
    arr[9] = 50;

    println("Before sorting:");
    print_array(&arr[0], 10);

    bubble_sort(&arr[0], 10);

    println("After sorting:");
    print_array(&arr[0], 10);

    return 0;
}
