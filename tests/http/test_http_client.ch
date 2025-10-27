// Simple HTTP API client for testing

let AF_INET = 2;
let SOCK_STREAM = 1;
let SYS_SOCKET = 41;
let SYS_CONNECT = 42;

let sockaddr: [i8; 16];
let buffer: [i8; 4096];
let request_buffer: [i8; 512];

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

fn test_endpoint(path_str: i32) -> i32 {
    println("========================================");
    println("Testing endpoint: ");
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
        println("[ERROR] Connection failed");
        close(sockfd);
        return 1;
    }

    // Build request in global buffer
    let pos = 0;

    // "GET "
    request_buffer[pos] = 71; pos = pos + 1;  // G
    request_buffer[pos] = 69; pos = pos + 1;  // E
    request_buffer[pos] = 84; pos = pos + 1;  // T
    request_buffer[pos] = 32; pos = pos + 1;  // space

    // Copy path using strcpy to a temp position, then copy byte by byte
    let path_start = pos;
    strcpy(request_buffer + pos, path_str);
    pos = pos + strlen(path_str);

    // " HTTP/1.0\r\n\r\n"
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
    println("CHRONOS API CLIENT - Testing endpoints");
    println("");

    test_endpoint("/status");

    return 0;
}
