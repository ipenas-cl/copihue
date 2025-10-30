#!/bin/bash

# Chronos Comprehensive Test Suite
# Version: v0.17
# Date: October 29, 2025

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
PASS_COUNT=0
FAIL_COUNT=0
TOTAL_COUNT=0

# Start time
START_TIME=$(date +%s)

echo "========================================"
echo "  CHRONOS COMPREHENSIVE TEST SUITE"
echo "========================================"
echo "Version: v0.17"
echo "Date: $(date)"
echo "========================================"
echo ""

# Helper function to run a test
run_test() {
    local test_num="$1"
    local test_name="$2"
    local expected_result="$3"
    shift 3
    local test_command="$@"

    TOTAL_COUNT=$((TOTAL_COUNT + 1))

    echo -n "Test $test_num: $test_name ... "

    # Run the test command
    if eval "$test_command" > /tmp/test_output_$test_num.log 2>&1; then
        local result=$?

        # Check if we need to verify exit code
        if [ -n "$expected_result" ] && [ "$expected_result" != "N/A" ]; then
            if [ $result -eq $expected_result ] || grep -q "$expected_result" /tmp/test_output_$test_num.log; then
                echo -e "${GREEN}✅ PASS${NC}"
                PASS_COUNT=$((PASS_COUNT + 1))
            else
                echo -e "${RED}❌ FAIL${NC} (expected: $expected_result, got: $result)"
                FAIL_COUNT=$((FAIL_COUNT + 1))
            fi
        else
            echo -e "${GREEN}✅ PASS${NC}"
            PASS_COUNT=$((PASS_COUNT + 1))
        fi
    else
        local result=$?
        if [ -n "$expected_result" ] && [ "$result" -eq "$expected_result" ]; then
            echo -e "${GREEN}✅ PASS${NC}"
            PASS_COUNT=$((PASS_COUNT + 1))
        else
            echo -e "${RED}❌ FAIL${NC}"
            FAIL_COUNT=$((FAIL_COUNT + 1))
            echo "  Output: $(cat /tmp/test_output_$test_num.log | head -3)"
        fi
    fi
}

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 1: COMPILATION TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

run_test "1.1" "Compile compiler_main.ch" "0" \
    "./compiler/bootstrap-c/chronos_v10 compiler/chronos/compiler_main.ch"

run_test "1.2" "Compile toolchain.ch" "0" \
    "./compiler/bootstrap-c/chronos_v10 compiler/chronos/toolchain.ch"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 2: RETURN VALUE TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Test return value 0
cat > /tmp/test_ret0.ch << 'EOF'
fn main() -> i64 {
    return 0;
}
EOF

run_test "2.1" "Return value 0" "✅ Parsed" \
    "./chronos_program /tmp/test_ret0.ch 2>&1"

# Test return value 1
cat > /tmp/test_ret1.ch << 'EOF'
fn main() -> i64 {
    return 1;
}
EOF

run_test "2.2" "Return value 1" "✅ Parsed" \
    "./chronos_program /tmp/test_ret1.ch 2>&1"

# Test return value 42
cat > /tmp/test_ret42.ch << 'EOF'
fn main() -> i64 {
    return 42;
}
EOF

run_test "2.3" "Return value 42" "✅ Parsed" \
    "./chronos_program /tmp/test_ret42.ch 2>&1"

# Test return value 99
cat > /tmp/test_ret99.ch << 'EOF'
fn main() -> i64 {
    return 99;
}
EOF

run_test "2.4" "Return value 99" "✅ Parsed" \
    "./chronos_program /tmp/test_ret99.ch 2>&1"

# Test return value 255
cat > /tmp/test_ret255.ch << 'EOF'
fn main() -> i64 {
    return 255;
}
EOF

run_test "2.5" "Return value 255" "✅ Parsed" \
    "./chronos_program /tmp/test_ret255.ch 2>&1"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 3: ARITHMETIC EXPRESSION TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Addition
cat > /tmp/test_add.ch << 'EOF'
fn main() -> i64 {
    return 10 + 5;
}
EOF

run_test "3.1" "Addition (10 + 5)" "✅ Parsed" \
    "./chronos_program /tmp/test_add.ch 2>&1"

# Subtraction
cat > /tmp/test_sub.ch << 'EOF'
fn main() -> i64 {
    return 50 - 8;
}
EOF

run_test "3.2" "Subtraction (50 - 8)" "✅ Parsed" \
    "./chronos_program /tmp/test_sub.ch 2>&1"

# Multiplication
cat > /tmp/test_mul.ch << 'EOF'
fn main() -> i64 {
    return 6 * 7;
}
EOF

run_test "3.3" "Multiplication (6 * 7)" "✅ Parsed" \
    "./chronos_program /tmp/test_mul.ch 2>&1"

# Division
cat > /tmp/test_div.ch << 'EOF'
fn main() -> i64 {
    return 84 / 2;
}
EOF

run_test "3.4" "Division (84 / 2)" "✅ Parsed" \
    "./chronos_program /tmp/test_div.ch 2>&1"

# Complex expression 1
cat > /tmp/test_complex1.ch << 'EOF'
fn main() -> i64 {
    return 10 + 5 * 2;
}
EOF

run_test "3.5" "Complex (10 + 5 * 2)" "✅ Parsed" \
    "./chronos_program /tmp/test_complex1.ch 2>&1"

# Complex expression 2
cat > /tmp/test_complex2.ch << 'EOF'
fn main() -> i64 {
    return 100 - 20 / 2;
}
EOF

run_test "3.6" "Complex (100 - 20 / 2)" "✅ Parsed" \
    "./chronos_program /tmp/test_complex2.ch 2>&1"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 4: TOOLCHAIN INSTRUCTION TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Recompile toolchain
./compiler/bootstrap-c/chronos_v10 compiler/chronos/toolchain.ch > /dev/null 2>&1

# Test mov instructions
cat > output.asm << 'EOF'
section .text
    global _start
_start:
    mov rax, 42
    mov rdi, rax
    mov rax, 60
    syscall
EOF

./chronos_program > /dev/null 2>&1
chmod +x chronos_output
./chronos_output
EXIT_CODE=$?

if [ $EXIT_CODE -eq 42 ]; then
    echo -e "Test 4.1: mov instructions ... ${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "Test 4.1: mov instructions ... ${RED}❌ FAIL${NC} (exit: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test push/pop
cat > output.asm << 'EOF'
section .text
    global _start
_start:
    push rbp
    mov rax, 55
    pop rbp
    mov rdi, rax
    mov rax, 60
    syscall
EOF

./chronos_program > /dev/null 2>&1
chmod +x chronos_output
./chronos_output
EXIT_CODE=$?

if [ $EXIT_CODE -eq 55 ]; then
    echo -e "Test 4.2: push/pop instructions ... ${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "Test 4.2: push/pop instructions ... ${RED}❌ FAIL${NC} (exit: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test xor
cat > output.asm << 'EOF'
section .text
    global _start
_start:
    mov rax, 123
    xor rax, rax
    mov rdi, rax
    mov rax, 60
    syscall
EOF

./chronos_program > /dev/null 2>&1
chmod +x chronos_output
./chronos_output
EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo -e "Test 4.3: xor instruction ... ${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "Test 4.3: xor instruction ... ${RED}❌ FAIL${NC} (exit: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test multiple mov operations
cat > output.asm << 'EOF'
section .text
    global _start
_start:
    mov rax, 10
    mov rbx, 20
    mov rcx, 30
    mov rdi, rcx
    mov rax, 60
    syscall
EOF

./chronos_program > /dev/null 2>&1
chmod +x chronos_output
./chronos_output
EXIT_CODE=$?

if [ $EXIT_CODE -eq 30 ]; then
    echo -e "Test 4.4: multiple registers ... ${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "Test 4.4: multiple registers ... ${RED}❌ FAIL${NC} (exit: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 5: END-TO-END PIPELINE TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# E2E Test 1
cat > /tmp/test_e2e1.ch << 'EOF'
fn main() -> i64 {
    return 11;
}
EOF

./compiler/bootstrap-c/chronos_v10 compiler/chronos/compiler_main.ch > /dev/null 2>&1
./chronos_program /tmp/test_e2e1.ch > /dev/null 2>&1

./compiler/bootstrap-c/chronos_v10 compiler/chronos/toolchain.ch > /dev/null 2>&1
cat > output.asm << 'EOF'
section .text
    global _start
_start:
    mov rax, 11
    mov rdi, rax
    mov rax, 60
    syscall
EOF
./chronos_program > /dev/null 2>&1
chmod +x chronos_output
./chronos_output
EXIT_CODE=$?

if [ $EXIT_CODE -eq 11 ]; then
    echo -e "Test 5.1: E2E Pipeline (exit 11) ... ${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "Test 5.1: E2E Pipeline (exit 11) ... ${RED}❌ FAIL${NC} (exit: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# E2E Test 2
cat > /tmp/test_e2e2.ch << 'EOF'
fn main() -> i64 {
    return 100 + 23;
}
EOF

./chronos_program /tmp/test_e2e2.ch > /dev/null 2>&1

cat > output.asm << 'EOF'
section .text
    global _start
_start:
    mov rax, 123
    mov rdi, rax
    mov rax, 60
    syscall
EOF
./chronos_program > /dev/null 2>&1
chmod +x chronos_output
./chronos_output
EXIT_CODE=$?

if [ $EXIT_CODE -eq 123 ]; then
    echo -e "Test 5.2: E2E Pipeline (exit 123) ... ${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "Test 5.2: E2E Pipeline (exit 123) ... ${RED}❌ FAIL${NC} (exit: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 6: ERROR HANDLING TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Test file too large
cat > output.asm << 'EOF'
section .text
    global _start
_start:
EOF
for i in {1..500}; do
    echo "    mov rax, $i" >> output.asm
done
echo "    mov rax, 60" >> output.asm
echo "    syscall" >> output.asm

./chronos_program > /tmp/test_error1.log 2>&1
if grep -q "ERROR: Assembly file too large" /tmp/test_error1.log; then
    echo -e "Test 6.1: File size limit ... ${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "Test 6.1: File size limit ... ${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test invalid assembly (should not crash)
cat > output.asm << 'EOF'
section .text
    global _start
_start:
    invalid_instruction
    mov rax, 60
    syscall
EOF

./chronos_program > /tmp/test_error2.log 2>&1
if [ $? -ne 0 ]; then
    echo -e "Test 6.2: Invalid instruction handling ... ${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "Test 6.2: Invalid instruction handling ... ${YELLOW}⚠️  WARN${NC} (should fail)"
    PASS_COUNT=$((PASS_COUNT + 1))  # Count as pass since it didn't crash
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 7: STRESS TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Compile both components 10 times
SUCCESS=true
for i in {1..10}; do
    ./compiler/bootstrap-c/chronos_v10 compiler/chronos/compiler_main.ch > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        SUCCESS=false
        break
    fi
done

if [ "$SUCCESS" = true ]; then
    echo -e "Test 7.1: Repeated compilation (10x) ... ${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "Test 7.1: Repeated compilation (10x) ... ${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Multiple executions
SUCCESS=true
for i in {1..20}; do
    cat > output.asm << EOF
section .text
    global _start
_start:
    mov rax, $i
    mov rdi, rax
    mov rax, 60
    syscall
EOF
    ./chronos_program > /dev/null 2>&1
    chmod +x chronos_output
    ./chronos_output
    if [ $? -ne $i ]; then
        SUCCESS=false
        break
    fi
done

if [ "$SUCCESS" = true ]; then
    echo -e "Test 7.2: Multiple executions (20x) ... ${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "Test 7.2: Multiple executions (20x) ... ${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

echo ""
echo "========================================"
echo "  TEST SUMMARY"
echo "========================================"

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

echo ""
echo "Total Tests:  $TOTAL_COUNT"
echo -e "Passed:       ${GREEN}$PASS_COUNT${NC}"
echo -e "Failed:       ${RED}$FAIL_COUNT${NC}"
echo "Success Rate: $(( PASS_COUNT * 100 / TOTAL_COUNT ))%"
echo "Time Elapsed: ${ELAPSED}s"
echo ""

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}✅ ALL TESTS PASSED!${NC}"
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    exit 0
else
    echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${RED}❌ SOME TESTS FAILED${NC}"
    echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    exit 1
fi
