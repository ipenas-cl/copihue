// Test simple de HTTP

let AF_INET = 2;
let SOCK_STREAM = 1;
let SYS_SOCKET = 41;
let SYS_CONNECT = 42;

let sockaddr: [i8; 16];
let buffer: [i8; 4096];

fn htons(port: i32) -> i32 {
    let high = port / 256;
    let low = port % 256;
    return (low * 256) + high;
}

fn main() -> i32 {
    println("Test HTTP Client");
    
    // Create socket
    let sockfd = syscall6(SYS_SOCKET, AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    if (sockfd < 0) {
        println("ERROR: Socket failed");
        return 1;
    }
    println("Socket created");
    
    // Setup sockaddr
    let i = 0;
    while (i < 16) {
        sockaddr[i] = 0;
        i++;
    }
    sockaddr[0] = 2;  // AF_INET
    let port_be = htons(8000);
    sockaddr[2] = port_be / 256;
    sockaddr[3] = port_be % 256;
    sockaddr[4] = 127;  // 127.0.0.1
    sockaddr[5] = 0;
    sockaddr[6] = 0;
    sockaddr[7] = 1;
    
    // Connect
    let conn = syscall6(SYS_CONNECT, sockfd, sockaddr, 16, 0, 0, 0);
    if (conn < 0) {
        println("ERROR: Connect failed");
        close(sockfd);
        return 1;
    }
    println("Connected!");
    
    // Send GET request
    let request = "GET /api/status HTTP/1.1\r\nHost: localhost\r\n\r\n";
    write(sockfd, request, strlen(request));
    println("Request sent");
    
    // Read response
    let bytes = read(sockfd, buffer, 4095);
    buffer[bytes] = 0;
    
    println("Response:");
    println(buffer);
    
    close(sockfd);
    return 0;
}
