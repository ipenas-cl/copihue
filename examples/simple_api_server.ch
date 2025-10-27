// SIMPLE API SERVER FOR TESTING
// Serves JSON data on port 8000

let AF_INET = 2;
let SOCK_STREAM = 1;
let SOL_SOCKET = 1;
let SO_REUSEADDR = 2;
let SYS_SOCKET = 41;
let SYS_BIND = 49;
let SYS_LISTEN = 50;
let SYS_ACCEPT = 43;
let SYS_SETSOCKOPT = 54;

let sockaddr: [i8; 16];
let buffer: [i8; 4096];
let response: [i8; 2048];

let request_count = 0;

fn htons(port: i32) -> i32 {
    let high = port / 256;
    let low = port % 256;
    return (low * 256) + high;
}

fn setup_sockaddr(port: i32) -> i32 {
    let i = 0;
    while (i < 16) {
        sockaddr[i] = 0;
        i++;
    }
    sockaddr[0] = 2;  // AF_INET
    let port_be = htons(port);
    sockaddr[2] = port_be / 256;
    sockaddr[3] = port_be % 256;
    // 0.0.0.0 (bind to all interfaces)
    return 0;
}

fn int_to_str(num: i32, buf: i32) -> i32 {
    if (num == 0) {
        buf[0] = 48;  // '0'
        buf[1] = 0;
        return 1;
    }
    
    let temp: [i32; 20];
    let count = 0;
    let n = num;
    
    while (n > 0) {
        temp[count] = (n % 10) + 48;  // Convert to ASCII
        n /= 10;
        count++;
    }
    
    // Reverse
    let i = 0;
    while (i < count) {
        buf[i] = temp[count - i - 1];
        i++;
    }
    buf[count] = 0;
    return count;
}

fn build_json_response() -> i32 {
    let pos = 0;
    
    // HTTP headers
    strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");
    pos = strlen(response);
    
    // JSON body: {
    response[pos] = 123;
    pos++;
    
    // "status"
    response[pos] = 34;
    pos++;
    strcpy(response + pos, "status");
    pos += 6;
    response[pos] = 34;
    pos++;
    response[pos] = 58;
    pos++;
    
    // "ok"
    response[pos] = 34;
    pos++;
    strcpy(response + pos, "ok");
    pos += 2;
    response[pos] = 34;
    pos++;
    response[pos] = 44;
    pos++;
    
    // "count"
    response[pos] = 34;
    pos++;
    strcpy(response + pos, "count");
    pos += 5;
    response[pos] = 34;
    pos++;
    response[pos] = 58;
    pos++;
    
    let num_buf: [i8; 20];
    int_to_str(request_count, num_buf);
    strcpy(response + pos, num_buf);
    pos += strlen(num_buf);
    response[pos] = 44;
    pos++;
    
    // "message"
    response[pos] = 34;
    pos++;
    strcpy(response + pos, "message");
    pos += 7;
    response[pos] = 34;
    pos++;
    response[pos] = 58;
    pos++;
    response[pos] = 34;
    pos++;
    strcpy(response + pos, "Chronos API Server v0.16");
    pos += 25;
    response[pos] = 34;
    pos++;
    
    // }
    response[pos] = 125;
    pos++;
    response[pos] = 0;
    
    return pos;
}

fn main() -> i32 {
    println("========================================");
    println("  CHRONOS API SERVER");
    println("  Listening on http://127.0.0.1:8000");
    println("  Endpoint: /api/status");
    println("========================================");
    println("");
    
    // Create socket
    let sockfd = syscall6(SYS_SOCKET, AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    if (sockfd < 0) {
        println("[ERROR] Socket creation failed");
        return 1;
    }
    
    // Set SO_REUSEADDR
    let optval = 1;
    syscall6(SYS_SETSOCKOPT, sockfd, SOL_SOCKET, SO_REUSEADDR, optval, 4, 0);
    
    // Bind
    setup_sockaddr(8000);
    let bind_result = syscall6(SYS_BIND, sockfd, sockaddr, 16, 0, 0, 0);
    if (bind_result < 0) {
        println("[ERROR] Bind failed");
        close(sockfd);
        return 1;
    }
    
    // Listen
    let listen_result = syscall6(SYS_LISTEN, sockfd, 10, 0, 0, 0, 0);
    if (listen_result < 0) {
        println("[ERROR] Listen failed");
        close(sockfd);
        return 1;
    }
    
    println("[OK] Server started successfully");
    println("[INFO] Waiting for requests...");
    println("");
    
    // Accept loop
    while (1) {
        let clientfd = syscall6(SYS_ACCEPT, sockfd, 0, 0, 0, 0, 0);
        if (clientfd < 0) {
            println("[ERROR] Accept failed");
            break;
        }
        
        request_count++;
        
        // Read request
        let bytes = read(clientfd, buffer, 4095);
        buffer[bytes] = 0;
        
        print("[");
        print_int(request_count);
        print("] Request received - ");
        print_int(bytes);
        println(" bytes");
        
        // Build and send JSON response
        let response_len = build_json_response();
        write(clientfd, response, response_len);
        
        close(clientfd);
        
        print("    Response sent (");
        print_int(response_len);
        println(" bytes)");
        println("");
    }
    
    close(sockfd);
    return 0;
}
