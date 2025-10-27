// CHRONOS TCP/IP LIBRARY - PURE CHRONOS
// Network programming using syscalls directly
// Zero C dependencies for networking

// ============================================
// CONSTANTS
// ============================================

// Address families
let AF_INET = 2;

// Socket types
let SOCK_STREAM = 1;    // TCP

// Protocol
let IPPROTO_TCP = 6;

// Syscall numbers
let SYS_SOCKET = 41;
let SYS_CONNECT = 42;
let SYS_ACCEPT = 43;
let SYS_SENDTO = 44;
let SYS_RECVFROM = 45;
let SYS_BIND = 49;
let SYS_LISTEN = 50;

// ============================================
// SOCKET ADDRESS STRUCTURE
// ============================================

let sockaddr: [i8; 16];   // struct sockaddr_in
let tcp_buffer: [i8; 1024];  // Shared buffer for TCP operations

// ============================================
// HELPERS
// ============================================

fn htons(hostshort: i32) -> i32 {
    // Manual modulo since % operator not implemented yet
    let low_div = hostshort / 256;
    let low = hostshort - low_div * 256;

    let high_div = (hostshort / 256) / 256;
    let high = (hostshort / 256) - high_div * 256;

    return (low * 256) + high;
}

fn setup_sockaddr(port: i32) -> i32 {
    // Clear
    let i = 0;
    while (i < 16) {
        sockaddr[i] = 0;
        i = i + 1;
    }

    // sin_family = AF_INET
    sockaddr[0] = 2;
    sockaddr[1] = 0;

    // sin_port (big-endian)
    let port_be = htons(port);
    let port_be_div = port_be / 256;
    sockaddr[2] = port_be_div;
    sockaddr[3] = port_be - port_be_div * 256;

    // sin_addr = 127.0.0.1 (localhost)
    sockaddr[4] = 127;
    sockaddr[5] = 0;
    sockaddr[6] = 0;
    sockaddr[7] = 1;

    return 0;
}

// ============================================
// SOCKET API - PURE CHRONOS
// ============================================

fn socket_create() -> i32 {
    // socket(AF_INET, SOCK_STREAM, 0)
    return syscall6(SYS_SOCKET, AF_INET, SOCK_STREAM, 0, 0, 0, 0);
}

fn socket_bind(sockfd: i32, port: i32) -> i32 {
    setup_sockaddr(port);
    // bind(sockfd, &sockaddr, 16)
    return syscall6(SYS_BIND, sockfd, sockaddr, 16, 0, 0, 0);
}

fn socket_listen(sockfd: i32, backlog: i32) -> i32 {
    // listen(sockfd, backlog)
    return syscall6(SYS_LISTEN, sockfd, backlog, 0, 0, 0, 0);
}

fn socket_accept(sockfd: i32) -> i32 {
    // accept(sockfd, NULL, NULL)
    return syscall6(SYS_ACCEPT, sockfd, 0, 0, 0, 0, 0);
}

fn socket_connect(sockfd: i32, port: i32) -> i32 {
    setup_sockaddr(port);
    // connect(sockfd, &sockaddr, 16)
    return syscall6(SYS_CONNECT, sockfd, sockaddr, 16, 0, 0, 0);
}

fn socket_send(sockfd: i32, buffer: i32, length: i32) -> i32 {
    // send = write for sockets
    return write(sockfd, buffer, length);
}

fn socket_recv(sockfd: i32, buffer: i32, length: i32) -> i32 {
    // recv = read for sockets
    return read(sockfd, buffer, length);
}

// ============================================
// TCP SERVER
// ============================================

fn tcp_server(port: i32) -> i32 {
    println("========================================");
    println("CHRONOS TCP SERVER - PURE CHRONOS");
    println("========================================");
    println("");

    // 1. Create socket
    println("[1] Creating socket...");
    let sockfd = socket_create();
    if (sockfd < 0) {
        println("[ERROR] Failed to create socket");
        return 1;
    }
    println("[OK] Socket created: fd=");
    print_int(sockfd);
    println("");
    println("");

    // 2. Bind
    println("[2] Binding to port ");
    print_int(port);
    println("...");
    let bind_result = socket_bind(sockfd, port);
    if (bind_result < 0) {
        println("[ERROR] Failed to bind");
        return 1;
    }
    println("[OK] Bind successful");
    println("");

    // 3. Listen
    println("[3] Listening (backlog=5)...");
    let listen_result = socket_listen(sockfd, 5);
    if (listen_result < 0) {
        println("[ERROR] Failed to listen");
        return 1;
    }
    println("[OK] Listening on 127.0.0.1:");
    print_int(port);
    println("");
    println("");

    // 4. Accept
    println("[4] Waiting for connections...");
    println("(Connect with: nc 127.0.0.1 ");
    print_int(port);
    println(")");
    let clientfd = socket_accept(sockfd);
    if (clientfd < 0) {
        println("[ERROR] Failed to accept");
        return 1;
    }
    println("[OK] Connection accepted: client_fd=");
    print_int(clientfd);
    println("");
    println("");

    // 5. Receive data
    println("[5] Receiving data...");
    let received = socket_recv(clientfd, tcp_buffer, 1023);
    if (received > 0) {
        tcp_buffer[received] = 0;  // null terminate
        println("[OK] Received ");
        print_int(received);
        println(" bytes:");
        println(tcp_buffer);
        println("");
    }

    // 6. Send response
    println("[6] Sending response...");
    let response = "Hello from Chronos TCP server!\n";
    let sent = socket_send(clientfd, response, 32);
    println("[OK] Sent ");
    print_int(sent);
    println(" bytes");
    println("");

    // 7. Close
    close(clientfd);
    close(sockfd);

    println("========================================");
    println("SERVER COMPLETED SUCCESSFULLY");
    println("========================================");

    return 0;
}

// ============================================
// TCP CLIENT
// ============================================

fn tcp_client(port: i32) -> i32 {
    println("========================================");
    println("CHRONOS TCP CLIENT - PURE CHRONOS");
    println("========================================");
    println("");

    // 1. Create socket
    println("[1] Creating socket...");
    let sockfd = socket_create();
    if (sockfd < 0) {
        println("[ERROR] Failed to create socket");
        return 1;
    }
    println("[OK] Socket created: fd=");
    print_int(sockfd);
    println("");
    println("");

    // 2. Connect
    println("[2] Connecting to 127.0.0.1:");
    print_int(port);
    println("...");
    let connect_result = socket_connect(sockfd, port);
    if (connect_result < 0) {
        println("[ERROR] Failed to connect");
        return 1;
    }
    println("[OK] Connected");
    println("");

    // 3. Send data
    println("[3] Sending message...");
    let message = "Hello from Chronos TCP client!\n";
    let sent = socket_send(sockfd, message, 32);
    println("[OK] Sent ");
    print_int(sent);
    println(" bytes");
    println("");

    // 4. Receive response
    println("[4] Receiving response...");
    let received = socket_recv(sockfd, tcp_buffer, 1023);
    if (received > 0) {
        tcp_buffer[received] = 0;
        println("[OK] Received ");
        print_int(received);
        println(" bytes:");
        println(tcp_buffer);
        println("");
    }

    // 5. Close
    close(sockfd);

    println("========================================");
    println("CLIENT COMPLETED SUCCESSFULLY");
    println("========================================");

    return 0;
}

// ============================================
// MAIN
// ============================================

fn main() -> i32 {
    // TCP Server on port 8080
    return tcp_server(8080);

    // To test client, change to:
    // return tcp_client(8080);
}
