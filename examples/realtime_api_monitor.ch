// CHRONOS REALTIME API MONITOR
// Fetches data from API, parses JSON, displays in terminal

let AF_INET = 2;
let SOCK_STREAM = 1;
let SYS_SOCKET = 41;
let SYS_CONNECT = 42;

let sockaddr: [i8; 16];
let response_buffer: [i8; 8192];
let request_buffer: [i8; 512];

fn htons(port: i32) -> i32 {
    let high = port / 256;
    let low = port % 256;
    return (low * 256) + high;
}

fn setup_sockaddr_ip(a: i32, b: i32, c: i32, d: i32, port: i32) -> i32 {
    let i = 0;
    while (i < 16) {
        sockaddr[i] = 0;
        i++;
    }
    sockaddr[0] = 2;  // AF_INET
    
    let port_be = htons(port);
    sockaddr[2] = port_be / 256;
    sockaddr[3] = port_be % 256;
    
    sockaddr[4] = a;
    sockaddr[5] = b;
    sockaddr[6] = c;
    sockaddr[7] = d;
    
    return 0;
}

fn http_get(host_a: i32, host_b: i32, host_c: i32, host_d: i32, 
            port: i32, path: i32) -> i32 {
    
    let sockfd = syscall6(SYS_SOCKET, AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    if (sockfd < 0) {
        println("[ERROR] Socket creation failed");
        return 0;
    }
    
    setup_sockaddr_ip(host_a, host_b, host_c, host_d, port);
    let conn_result = syscall6(SYS_CONNECT, sockfd, sockaddr, 16, 0, 0, 0);
    if (conn_result < 0) {
        println("[ERROR] Connection failed");
        close(sockfd);
        return 0;
    }
    
    let pos = 0;
    
    // "GET "
    request_buffer[pos] = 71;
    pos++;
    request_buffer[pos] = 69;
    pos++;
    request_buffer[pos] = 84;
    pos++;
    request_buffer[pos] = 32;
    pos++;
    
    strcpy(request_buffer + pos, path);
    pos += strlen(path);
    
    // " HTTP/1.1\r\n"
    request_buffer[pos] = 32;
    pos++;
    request_buffer[pos] = 72;
    pos++;
    request_buffer[pos] = 84;
    pos++;
    request_buffer[pos] = 84;
    pos++;
    request_buffer[pos] = 80;
    pos++;
    request_buffer[pos] = 47;
    pos++;
    request_buffer[pos] = 49;
    pos++;
    request_buffer[pos] = 46;
    pos++;
    request_buffer[pos] = 49;
    pos++;
    request_buffer[pos] = 13;
    pos++;
    request_buffer[pos] = 10;
    pos++;
    
    // "Host: API\r\n"
    request_buffer[pos] = 72;
    pos++;
    request_buffer[pos] = 111;
    pos++;
    request_buffer[pos] = 115;
    pos++;
    request_buffer[pos] = 116;
    pos++;
    request_buffer[pos] = 58;
    pos++;
    request_buffer[pos] = 32;
    pos++;
    request_buffer[pos] = 65;
    pos++;
    request_buffer[pos] = 80;
    pos++;
    request_buffer[pos] = 73;
    pos++;
    request_buffer[pos] = 13;
    pos++;
    request_buffer[pos] = 10;
    pos++;
    
    // "\r\n"
    request_buffer[pos] = 13;
    pos++;
    request_buffer[pos] = 10;
    pos++;
    
    write(sockfd, request_buffer, pos);
    
    let total = 0;
    let received = read(sockfd, response_buffer, 8191);
    if (received > 0) {
        response_buffer[received] = 0;
        total = received;
    }
    
    close(sockfd);
    return total;
}

fn find_json_value(json: i32, key: i32, output: i32) -> i32 {
    let key_len = strlen(key);
    let json_len = strlen(json);
    
    let i = 0;
    while (i < json_len) {
        let match = 1;
        let j = 0;
        while (j < key_len && (i + j) < json_len) {
            if (json[i + j] != key[j]) {
                match = 0;
            }
            j++;
        }
        
        if (match && json[i + key_len] == 34) {
            let k = i + key_len + 1;
            while (k < json_len && json[k] != 58) {
                k++;
            }
            k++;
            
            while (k < json_len && (json[k] == 32 || json[k] == 9)) {
                k++;
            }
            
            let is_string = (json[k] == 34);
            if (is_string) {
                k++;
            }
            
            let out_pos = 0;
            while (k < json_len) {
                let ch = json[k];
                if (is_string && ch == 34) {
                    break;
                }
                if (!is_string && (ch == 44 || ch == 125)) {
                    break;
                }
                output[out_pos] = ch;
                out_pos++;
                k++;
            }
            output[out_pos] = 0;
            return 1;
        }
        i++;
    }
    return 0;
}

fn print_separator() -> i32 {
    println("========================================");
    return 0;
}

fn print_header(title: i32) -> i32 {
    print_separator();
    print("  ");
    println(title);
    print_separator();
    return 0;
}

fn print_field(label: i32, value: i32) -> i32 {
    print("  ");
    print(label);
    print(": ");
    println(value);
    return 0;
}

let value_buffer: [i8; 256];

fn fetch_and_display() -> i32 {
    print_header("FETCHING API DATA");
    
    let bytes = http_get(127, 0, 0, 1, 8000, "/api/status");
    
    if (bytes == 0) {
        println("[ERROR] No data received");
        return 1;
    }
    
    println("[OK] Received data");
    
    let json_start = 0;
    let i = 0;
    while (i < bytes) {
        if (response_buffer[i] == 13 && response_buffer[i+1] == 10 &&
            response_buffer[i+2] == 13 && response_buffer[i+3] == 10) {
            json_start = i + 4;
            break;
        }
        i++;
    }
    
    if (json_start == 0) {
        println("[ERROR] No JSON body found");
        return 1;
    }
    
    print("\n");
    print_header("PARSED DATA");
    
    if (find_json_value(response_buffer + json_start, "status", value_buffer)) {
        print_field("Status", value_buffer);
    }
    
    if (find_json_value(response_buffer + json_start, "count", value_buffer)) {
        print_field("Count", value_buffer);
    }
    
    if (find_json_value(response_buffer + json_start, "message", value_buffer)) {
        print_field("Message", value_buffer);
    }
    
    print("\n");
    return 0;
}

fn main() -> i32 {
    println("");
    print_header("CHRONOS REALTIME API MONITOR");
    println("");
    println("Monitoring API endpoint...");
    println("Press Ctrl+C to stop");
    println("");
    
    let iteration = 0;
    while (iteration < 5) {
        fetch_and_display();
        
        print("Waiting for next update");
        println("...");
        println("");
        
        iteration++;
        
        let delay = 0;
        while (delay < 50000000) {
            delay++;
        }
    }
    
    print_separator();
    println("Monitoring complete");
    print_separator();
    
    return 0;
}
