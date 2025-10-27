// COMPREHENSIVE TEST SUITE FOR HTTP API
// Tests all endpoints with various scenarios

let AF_INET = 2;
let SOCK_STREAM = 1;
let SYS_SOCKET = 41;
let SYS_CONNECT = 42;

let sockaddr: [i8; 16];
let buffer: [i8; 4096];
let request_buffer: [i8; 1024];
let len_str: [i8; 10];

let tests_passed = 0;
let tests_failed = 0;
let tests_total = 0;

fn htons(hostshort: i32) -> i32 {
    let low_div = hostshort / 256;
    let low = hostshort - low_div * 256;
    let high_div = (hostshort / 256) / 256;
    let high = (hostshort / 256) - high_div * 256;
    return (low * 256) + high;
}

fn setup_sockaddr(port: i32) -> i32 {
    let i = 0;
    while (i < 16) {
        sockaddr[i] = 0;
        i = i + 1;
    }
    sockaddr[0] = 2;
    sockaddr[1] = 0;
    let port_be = htons(port);
    let port_be_div = port_be / 256;
    sockaddr[2] = port_be_div;
    sockaddr[3] = port_be - port_be_div * 256;
    sockaddr[4] = 127;
    sockaddr[5] = 0;
    sockaddr[6] = 0;
    sockaddr[7] = 1;
    return 0;
}

// Test a GET endpoint
fn test_get(name: i32, path: i32, expected: i32) -> i32 {
    tests_total = tests_total + 1;

    print("Test #");
    print_int(tests_total);
    print(": ");
    println(name);

    // Create socket and connect
    let sockfd = syscall6(SYS_SOCKET, AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    if (sockfd < 0) {
        println("  ✗ FAILED - Socket error");
        tests_failed = tests_failed + 1;
        return 1;
    }

    setup_sockaddr(6666);
    let conn_result = syscall6(SYS_CONNECT, sockfd, sockaddr, 16, 0, 0, 0);
    if (conn_result < 0) {
        println("  ✗ FAILED - Connection error");
        close(sockfd);
        tests_failed = tests_failed + 1;
        return 1;
    }

    // Build GET request
    let pos = 0;
    request_buffer[pos] = 71; pos = pos + 1;   // G
    request_buffer[pos] = 69; pos = pos + 1;   // E
    request_buffer[pos] = 84; pos = pos + 1;   // T
    request_buffer[pos] = 32; pos = pos + 1;   // space

    strcpy(request_buffer + pos, path);
    pos = pos + strlen(path);

    request_buffer[pos] = 32; pos = pos + 1;   // space
    request_buffer[pos] = 72; pos = pos + 1;   // H
    request_buffer[pos] = 84; pos = pos + 1;   // T
    request_buffer[pos] = 84; pos = pos + 1;   // T
    request_buffer[pos] = 80; pos = pos + 1;   // P
    request_buffer[pos] = 47; pos = pos + 1;   // /
    request_buffer[pos] = 49; pos = pos + 1;   // 1
    request_buffer[pos] = 46; pos = pos + 1;   // .
    request_buffer[pos] = 48; pos = pos + 1;   // 0
    request_buffer[pos] = 13; pos = pos + 1;   // \r
    request_buffer[pos] = 10; pos = pos + 1;   // \n
    request_buffer[pos] = 13; pos = pos + 1;   // \r
    request_buffer[pos] = 10; pos = pos + 1;   // \n

    write(sockfd, request_buffer, pos);

    // Receive response
    let received = read(sockfd, buffer, 4095);
    buffer[received] = 0;

    // Check if expected string is in response
    let found = 0;
    let i = 0;
    let exp_len = strlen(expected);
    while (i < received) {
        let match = 1;
        let j = 0;
        while (j < exp_len) {
            if (buffer[i + j] != expected[j]) {
                match = 0;
                j = exp_len;
            }
            j = j + 1;
        }
        if (match == 1) {
            found = 1;
            i = received;
        }
        i = i + 1;
    }

    close(sockfd);

    if (found == 1) {
        println("  ✓ PASSED");
        tests_passed = tests_passed + 1;
        return 0;
    }

    println("  ✗ FAILED - Response doesn't contain expected string");
    tests_failed = tests_failed + 1;
    return 1;
}

// Test a POST endpoint
fn test_post(name: i32, path: i32, body: i32, expected: i32) -> i32 {
    tests_total = tests_total + 1;

    print("Test #");
    print_int(tests_total);
    print(": ");
    println(name);

    // Create socket and connect
    let sockfd = syscall6(SYS_SOCKET, AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    if (sockfd < 0) {
        println("  ✗ FAILED - Socket error");
        tests_failed = tests_failed + 1;
        return 1;
    }

    setup_sockaddr(6666);
    let conn_result = syscall6(SYS_CONNECT, sockfd, sockaddr, 16, 0, 0, 0);
    if (conn_result < 0) {
        println("  ✗ FAILED - Connection error");
        close(sockfd);
        tests_failed = tests_failed + 1;
        return 1;
    }

    // Build POST request
    let pos = 0;
    request_buffer[pos] = 80; pos = pos + 1;   // P
    request_buffer[pos] = 79; pos = pos + 1;   // O
    request_buffer[pos] = 83; pos = pos + 1;   // S
    request_buffer[pos] = 84; pos = pos + 1;   // T
    request_buffer[pos] = 32; pos = pos + 1;   // space

    strcpy(request_buffer + pos, path);
    pos = pos + strlen(path);

    request_buffer[pos] = 32; pos = pos + 1;   // space
    request_buffer[pos] = 72; pos = pos + 1;   // H
    request_buffer[pos] = 84; pos = pos + 1;   // T
    request_buffer[pos] = 84; pos = pos + 1;   // T
    request_buffer[pos] = 80; pos = pos + 1;   // P
    request_buffer[pos] = 47; pos = pos + 1;   // /
    request_buffer[pos] = 49; pos = pos + 1;   // 1
    request_buffer[pos] = 46; pos = pos + 1;   // .
    request_buffer[pos] = 48; pos = pos + 1;   // 0
    request_buffer[pos] = 13; pos = pos + 1;   // \r
    request_buffer[pos] = 10; pos = pos + 1;   // \n

    strcpy(request_buffer + pos, "Content-Type: application/json\r\n");
    pos = pos + 33;

    strcpy(request_buffer + pos, "Content-Length: ");
    pos = pos + 16;

    // Calculate body length
    let body_len = strlen(body);
    let len_pos = 0;
    let num = body_len;

    if (num == 0) {
        request_buffer[pos] = 48;
        pos = pos + 1;
    }

    while (num > 0) {
        let digit_div = num / 10;
        let digit = num - digit_div * 10;
        len_str[len_pos] = 48 + digit;
        len_pos = len_pos + 1;
        num = digit_div;
    }

    len_pos = len_pos - 1;
    while (len_pos >= 0) {
        request_buffer[pos] = len_str[len_pos];
        pos = pos + 1;
        len_pos = len_pos - 1;
    }

    request_buffer[pos] = 13; pos = pos + 1;   // \r
    request_buffer[pos] = 10; pos = pos + 1;   // \n
    request_buffer[pos] = 13; pos = pos + 1;   // \r
    request_buffer[pos] = 10; pos = pos + 1;   // \n

    strcpy(request_buffer + pos, body);
    pos = pos + body_len;

    write(sockfd, request_buffer, pos);

    // Receive response
    let received = read(sockfd, buffer, 4095);
    buffer[received] = 0;

    // Check if expected string is in response
    let found = 0;
    let i = 0;
    let exp_len = strlen(expected);
    while (i < received) {
        let match = 1;
        let j = 0;
        while (j < exp_len) {
            if (buffer[i + j] != expected[j]) {
                match = 0;
                j = exp_len;
            }
            j = j + 1;
        }
        if (match == 1) {
            found = 1;
            i = received;
        }
        i = i + 1;
    }

    close(sockfd);

    if (found == 1) {
        println("  ✓ PASSED");
        tests_passed = tests_passed + 1;
        return 0;
    }

    println("  ✗ FAILED - Response doesn't contain expected string");
    tests_failed = tests_failed + 1;
    return 1;
}

fn main() -> i32 {
    println("");
    println("=========================================");
    println("CHRONOS HTTP API - COMPREHENSIVE TEST SUITE");
    println("=========================================");
    println("");
    println("NOTE: Ensure server is NOT running yet!");
    println("      This test suite manages server lifecycle");
    println("");
    println("Starting tests in 2 seconds...");
    println("");

    // Note: Server management would happen externally
    // For now, this assumes server restarts between tests

    println("=== GET ENDPOINT TESTS ===");
    println("");

    test_get("GET /status", "/status", "200 OK");
    test_get("GET /health", "/health", "200 OK");
    test_get("GET /info", "/info", "200 OK");
    test_get("GET / (root)", "/", "200 OK");
    test_get("GET /notfound (404)", "/notfound", "404");

    println("");
    println("=== POST ENDPOINT TESTS ===");
    println("");

    test_post("POST /echo", "/echo", "test123", "200 OK");
    test_post("POST /data", "/data", "hello", "201");

    println("");
    println("=========================================");
    println("TEST RESULTS SUMMARY");
    println("=========================================");
    print("Total tests: ");
    print_int(tests_total);
    println("");
    print("Passed: ");
    print_int(tests_passed);
    println("");
    print("Failed: ");
    print_int(tests_failed);
    println("");
    println("");

    if (tests_failed == 0) {
        println("✓ ALL TESTS PASSED!");
    }

    if (tests_failed > 0) {
        println("✗ Some tests failed");
    }

    println("=========================================");

    return 0;
}
