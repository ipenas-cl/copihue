// Test POST requests to HTTP API v2

let AF_INET = 2;
let SOCK_STREAM = 1;
let SYS_SOCKET = 41;
let SYS_CONNECT = 42;

let sockaddr: [i8; 16];
let buffer: [i8; 4096];
let request_buffer: [i8; 1024];
let len_str: [i8; 10];

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

fn test_post(path_str: i32, body_str: i32) -> i32 {
    println("========================================");
    println("Testing POST: ");
    println(path_str);
    println("========================================");

    // Create socket
    let sockfd = syscall6(SYS_SOCKET, AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    if (sockfd < 0) {
        println("[ERROR] Socket creation failed");
        return 1;
    }

    // Connect
    setup_sockaddr(6666);
    let conn_result = syscall6(SYS_CONNECT, sockfd, sockaddr, 16, 0, 0, 0);
    if (conn_result < 0) {
        println("[ERROR] Connection failed - make sure server is running");
        close(sockfd);
        return 1;
    }

    // Build POST request
    let pos = 0;

    // "POST "
    request_buffer[pos] = 80; pos = pos + 1;   // P
    request_buffer[pos] = 79; pos = pos + 1;   // O
    request_buffer[pos] = 83; pos = pos + 1;   // S
    request_buffer[pos] = 84; pos = pos + 1;   // T
    request_buffer[pos] = 32; pos = pos + 1;   // space

    // Copy path
    strcpy(request_buffer + pos, path_str);
    pos = pos + strlen(path_str);

    // " HTTP/1.0\r\n"
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

    // "Content-Type: application/json\r\n"
    strcpy(request_buffer + pos, "Content-Type: application/json\r\n");
    pos = pos + 33;

    // "Content-Length: XX\r\n\r\n"
    strcpy(request_buffer + pos, "Content-Length: ");
    pos = pos + 16;

    // Calculate and write body length
    let body_len = strlen(body_str);
    let len_pos = 0;
    let num = body_len;

    // Special case: if body_len is 0
    if (num == 0) {
        request_buffer[pos] = 48;  // '0'
        pos = pos + 1;
    }

    // Collect digits
    while (num > 0) {
        let digit_div = num / 10;
        let digit = num - digit_div * 10;
        len_str[len_pos] = 48 + digit;
        len_pos = len_pos + 1;
        num = digit_div;
    }

    // Write in reverse
    len_pos = len_pos - 1;
    while (len_pos >= 0) {
        request_buffer[pos] = len_str[len_pos];
        pos = pos + 1;
        len_pos = len_pos - 1;
    }

    // "\r\n\r\n"
    request_buffer[pos] = 13; pos = pos + 1;   // \r
    request_buffer[pos] = 10; pos = pos + 1;   // \n
    request_buffer[pos] = 13; pos = pos + 1;   // \r
    request_buffer[pos] = 10; pos = pos + 1;   // \n

    // Copy body
    strcpy(request_buffer + pos, body_str);
    pos = pos + body_len;

    println("[SENDING]");
    println("Request size: ");
    print_int(pos);
    println(" bytes");
    println("");
    println("Body: ");
    println(body_str);
    println("");

    // Null-terminate for debug
    request_buffer[pos] = 0;
    println("[DEBUG REQUEST]");
    println(request_buffer);
    println("");

    // Send complete request
    write(sockfd, request_buffer, pos);

    // Receive response
    let received = read(sockfd, buffer, 4095);
    buffer[received] = 0;

    println("[RESPONSE]");
    println(buffer);
    println("");

    close(sockfd);
    return 0;
}

fn main() -> i32 {
    println("");
    println("========================================");
    println("CHRONOS HTTP API - POST TESTS");
    println("========================================");
    println("");
    println("NOTE: Start server with: ./http_api_v2 &");
    println("      Wait 2 seconds, then run this test");
    println("");

    test_post("/echo", "test123");

    println("========================================");
    println("POST TEST COMPLETED");
    println("========================================");

    return 0;
}
