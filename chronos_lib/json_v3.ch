// CHRONOS JSON LIBRARY V3 - PURE CHRONOS
// JSON builder using only direct buffer writes
// Zero C dependencies

// ============================================
// GLOBAL BUFFERS
// ============================================

let json_buffer: [i8; 1024];
let temp_digits: [i8; 20];

// ============================================
// LOW-LEVEL BUFFER OPERATIONS
// ============================================

// Write a single char
fn buf_write_char(pos: i32, ch: i32) -> i32 {
    json_buffer[pos] = ch;
    return pos + 1;
}

// Write quote
fn buf_quote(pos: i32) -> i32 {
    json_buffer[pos] = 34;  // '"'
    return pos + 1;
}

// Write colon
fn buf_colon(pos: i32) -> i32 {
    json_buffer[pos] = 58;  // ':'
    return pos + 1;
}

// Write comma
fn buf_comma(pos: i32) -> i32 {
    json_buffer[pos] = 44;  // ','
    return pos + 1;
}

// ============================================
// MANUAL STRING WRITERS (for common keys)
// ============================================

// Write "status"
fn write_status_key(pos: i32) -> i32 {
    json_buffer[pos] = 115;  // 's'
    json_buffer[pos + 1] = 116;  // 't'
    json_buffer[pos + 2] = 97;   // 'a'
    json_buffer[pos + 3] = 116;  // 't'
    json_buffer[pos + 4] = 117;  // 'u'
    json_buffer[pos + 5] = 115;  // 's'
    return pos + 6;
}

// Write "code"
fn write_code_key(pos: i32) -> i32 {
    json_buffer[pos] = 99;   // 'c'
    json_buffer[pos + 1] = 111;  // 'o'
    json_buffer[pos + 2] = 100;  // 'd'
    json_buffer[pos + 3] = 101;  // 'e'
    return pos + 4;
}

// Write "message"
fn write_message_key(pos: i32) -> i32 {
    json_buffer[pos] = 109;  // 'm'
    json_buffer[pos + 1] = 101;  // 'e'
    json_buffer[pos + 2] = 115;  // 's'
    json_buffer[pos + 3] = 115;  // 's'
    json_buffer[pos + 4] = 97;   // 'a'
    json_buffer[pos + 5] = 103;  // 'g'
    json_buffer[pos + 6] = 101;  // 'e'
    return pos + 7;
}

// Write "error"
fn write_error_key(pos: i32) -> i32 {
    json_buffer[pos] = 101;  // 'e'
    json_buffer[pos + 1] = 114;  // 'r'
    json_buffer[pos + 2] = 114;  // 'r'
    json_buffer[pos + 3] = 111;  // 'o'
    json_buffer[pos + 4] = 114;  // 'r'
    return pos + 5;
}

// Write "OK"
fn write_ok_value(pos: i32) -> i32 {
    json_buffer[pos] = 79;   // 'O'
    json_buffer[pos + 1] = 75;   // 'K'
    return pos + 2;
}

// Write "success"
fn write_success_value(pos: i32) -> i32 {
    json_buffer[pos] = 115;  // 's'
    json_buffer[pos + 1] = 117;  // 'u'
    json_buffer[pos + 2] = 99;   // 'c'
    json_buffer[pos + 3] = 99;   // 'c'
    json_buffer[pos + 4] = 101;  // 'e'
    json_buffer[pos + 5] = 115;  // 's'
    json_buffer[pos + 6] = 115;  // 's'
    return pos + 7;
}

// ============================================
// NUMBER TO STRING
// ============================================

fn write_number(pos: i32, value: i32) -> i32 {
    if (value == 0) {
        json_buffer[pos] = 48;  // '0'
        return pos + 1;
    }

    // Collect digits
    let temp_pos = 0;
    let num = value;

    while (num > 0) {
        let digit_div = num / 10;
        let digit = num - digit_div * 10;
        temp_digits[temp_pos] = 48 + digit;
        temp_pos = temp_pos + 1;
        num = digit_div;
    }

    // Write in reverse
    temp_pos = temp_pos - 1;
    while (temp_pos >= 0) {
        json_buffer[pos] = temp_digits[temp_pos];
        pos = pos + 1;
        temp_pos = temp_pos - 1;
    }

    return pos;
}

// ============================================
// JSON BUILDERS
// ============================================

// Build: {"status":"OK","code":200}
fn json_status_ok(code: i32) -> i32 {
    let pos = 0;

    // {
    pos = buf_write_char(pos, 123);

    // "status"
    pos = buf_quote(pos);
    pos = write_status_key(pos);
    pos = buf_quote(pos);
    pos = buf_colon(pos);

    // "OK"
    pos = buf_quote(pos);
    pos = write_ok_value(pos);
    pos = buf_quote(pos);
    pos = buf_comma(pos);

    // "code"
    pos = buf_quote(pos);
    pos = write_code_key(pos);
    pos = buf_quote(pos);
    pos = buf_colon(pos);

    // 200
    pos = write_number(pos, code);

    // }
    pos = buf_write_char(pos, 125);
    json_buffer[pos] = 0;

    return pos;
}

// Build: {"status":"success"}
fn json_status_success() -> i32 {
    let pos = 0;

    // {"status":"success"}
    pos = buf_write_char(pos, 123);
    pos = buf_quote(pos);
    pos = write_status_key(pos);
    pos = buf_quote(pos);
    pos = buf_colon(pos);
    pos = buf_quote(pos);
    pos = write_success_value(pos);
    pos = buf_quote(pos);
    pos = buf_write_char(pos, 125);
    json_buffer[pos] = 0;

    return pos;
}

// Build: {"error":"<custom>"}
fn json_error_notfound() -> i32 {
    let pos = 0;

    // {"error":"Not found"}
    pos = buf_write_char(pos, 123);
    pos = buf_quote(pos);
    pos = write_error_key(pos);
    pos = buf_quote(pos);
    pos = buf_colon(pos);
    pos = buf_quote(pos);

    // "Not found"
    json_buffer[pos] = 78;   // 'N'
    json_buffer[pos + 1] = 111;  // 'o'
    json_buffer[pos + 2] = 116;  // 't'
    json_buffer[pos + 3] = 32;   // ' '
    json_buffer[pos + 4] = 102;  // 'f'
    json_buffer[pos + 5] = 111;  // 'o'
    json_buffer[pos + 6] = 117;  // 'u'
    json_buffer[pos + 7] = 110;  // 'n'
    json_buffer[pos + 8] = 100;  // 'd'
    pos = pos + 9;

    pos = buf_quote(pos);
    pos = buf_write_char(pos, 125);
    json_buffer[pos] = 0;

    return pos;
}

// ============================================
// TESTING
// ============================================

fn main() -> i32 {
    println("========================================");
    println("CHRONOS JSON BUILDER V3 - PURE CHRONOS");
    println("========================================");
    println("");

    // Test 1
    println("Test 1: Status OK");
    json_status_ok(200);
    println(json_buffer);
    println("");

    // Test 2
    println("Test 2: Status success");
    json_status_success();
    println(json_buffer);
    println("");

    // Test 3
    println("Test 3: Error not found");
    json_error_notfound();
    println(json_buffer);
    println("");

    // Test 4: Custom
    println("Test 4: Custom response");
    let pos = 0;
    pos = buf_write_char(pos, 123);  // {
    pos = buf_quote(pos);
    pos = write_code_key(pos);
    pos = buf_quote(pos);
    pos = buf_colon(pos);
    pos = write_number(pos, 42);
    pos = buf_comma(pos);
    pos = buf_quote(pos);
    pos = write_message_key(pos);
    pos = buf_quote(pos);
    pos = buf_colon(pos);
    pos = buf_quote(pos);
    pos = write_success_value(pos);
    pos = buf_quote(pos);
    pos = buf_write_char(pos, 125);  // }
    json_buffer[pos] = 0;
    println(json_buffer);
    println("");

    println("========================================");
    println("JSON BUILDER V3 WORKING!");
    println("========================================");

    return 0;
}
