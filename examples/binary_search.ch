// Binary Search Algorithm
// Demonstrates: efficient search, divide and conquer

// Global array (local typed arrays not supported)
let arr: [i32; 10];

fn binary_search(arr: *i32, len: i32, target: i32) -> i32 {
    let left = 0;
    let right = len - 1;

    while (left <= right) {
        let mid = left + (right - left) / 2;

        if (arr[mid] == target) {
            return mid;  // Found!
        }

        if (arr[mid] < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return -1;  // Not found
}

fn main() -> i32 {
    // Sorted array
    arr[0] = 1;
    arr[1] = 3;
    arr[2] = 5;
    arr[3] = 7;
    arr[4] = 9;
    arr[5] = 11;
    arr[6] = 13;
    arr[7] = 15;
    arr[8] = 17;
    arr[9] = 19;

    println("Searching for 13 in sorted array:");
    let result = binary_search(&arr[0], 10, 13);

    if (result != -1) {
        print("Found at index: ");
        print_int(result);
        println("");
    } else {
        println("Not found");
    }

    println("");
    println("Searching for 8 (not in array):");
    result = binary_search(&arr[0], 10, 8);

    if (result != -1) {
        print("Found at index: ");
        print_int(result);
        println("");
    } else {
        println("Not found");
    }

    return result;
}
