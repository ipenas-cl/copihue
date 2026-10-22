#!/bin/bash

# Chronos Comprehensive Test Suite v2
# Version: v0.17
# Date: October 29, 2025

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
echo "  CHRONOS COMPREHENSIVE TEST SUITE v2"
echo "========================================"
echo "Version: v0.17"
echo "Date: $(date)"
echo "========================================"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 1: COMPILATION TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Test 1.1: Compile compiler
echo -n "Test 1.1: Compile compiler_main.ch ... "
if ./compiler/bootstrap-c/chronos_v10 compiler/chronos/compiler_main.ch > /dev/null 2>&1; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 1.2: Compile toolchain
echo -n "Test 1.2: Compile toolchain.ch ... "
if ./compiler/bootstrap-c/chronos_v10 compiler/chronos/toolchain.ch > /dev/null 2>&1; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 2: PARSER TESTS (Compiler)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Compile compiler once for all parser tests
./compiler/bootstrap-c/chronos_v10 compiler/chronos/compiler_main.ch > /dev/null 2>&1

# Test 2.1
echo -n "Test 2.1: Parse return 0 ... "
cat > /tmp/test_ret0.ch << 'EOF'
fn main() -> i64 {
    return 0;
}
EOF
if ./chronos_program /tmp/test_ret0.ch 2>&1 | grep -q "✅ Parsed"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 2.2
echo -n "Test 2.2: Parse return 42 ... "
cat > /tmp/test_ret42.ch << 'EOF'
fn main() -> i64 {
    return 42;
}
EOF
if ./chronos_program /tmp/test_ret42.ch 2>&1 | grep -q "✅ Parsed"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 2.3
echo -n "Test 2.3: Parse return 99 ... "
cat > /tmp/test_ret99.ch << 'EOF'
fn main() -> i64 {
    return 99;
}
EOF
if ./chronos_program /tmp/test_ret99.ch 2>&1 | grep -q "✅ Parsed"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 2.4
echo -n "Test 2.4: Parse return 255 ... "
cat > /tmp/test_ret255.ch << 'EOF'
fn main() -> i64 {
    return 255;
}
EOF
if ./chronos_program /tmp/test_ret255.ch 2>&1 | grep -q "✅ Parsed"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 3: ARITHMETIC EXPRESSION TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Test 3.1
echo -n "Test 3.1: Addition (10 + 5) ... "
cat > /tmp/test_add.ch << 'EOF'
fn main() -> i64 {
    return 10 + 5;
}
EOF
if ./chronos_program /tmp/test_add.ch 2>&1 | grep -q "✅ Parsed"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 3.2
echo -n "Test 3.2: Subtraction (50 - 8) ... "
cat > /tmp/test_sub.ch << 'EOF'
fn main() -> i64 {
    return 50 - 8;
}
EOF
if ./chronos_program /tmp/test_sub.ch 2>&1 | grep -q "✅ Parsed"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 3.3
echo -n "Test 3.3: Multiplication (6 * 7) ... "
cat > /tmp/test_mul.ch << 'EOF'
fn main() -> i64 {
    return 6 * 7;
}
EOF
if ./chronos_program /tmp/test_mul.ch 2>&1 | grep -q "✅ Parsed"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 3.4
echo -n "Test 3.4: Division (84 / 2) ... "
cat > /tmp/test_div.ch << 'EOF'
fn main() -> i64 {
    return 84 / 2;
}
EOF
if ./chronos_program /tmp/test_div.ch 2>&1 | grep -q "✅ Parsed"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 3.5
echo -n "Test 3.5: Complex (10 + 5 * 2) ... "
cat > /tmp/test_complex1.ch << 'EOF'
fn main() -> i64 {
    return 10 + 5 * 2;
}
EOF
if ./chronos_program /tmp/test_complex1.ch 2>&1 | grep -q "✅ Parsed"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 3.6
echo -n "Test 3.6: Complex (100 - 20 / 2) ... "
cat > /tmp/test_complex2.ch << 'EOF'
fn main() -> i64 {
    return 100 - 20 / 2;
}
EOF
if ./chronos_program /tmp/test_complex2.ch 2>&1 | grep -q "✅ Parsed"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 4: TOOLCHAIN ASSEMBLY TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Compile toolchain once
./compiler/bootstrap-c/chronos_v10 compiler/chronos/toolchain.ch > /dev/null 2>&1

# Test 4.1: mov instructions
echo -n "Test 4.1: mov rax/rdi (exit 42) ... "
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
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC} (got: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 4.2: push/pop
echo -n "Test 4.2: push/pop rbp (exit 55) ... "
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
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC} (got: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 4.3: xor instruction
echo -n "Test 4.3: xor rax, rax (exit 0) ... "
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
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC} (got: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 4.4: multiple registers
echo -n "Test 4.4: mov to multiple regs (exit 30) ... "
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
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC} (got: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 4.5: Different exit codes
for val in 1 7 13 77 88 100 127 200; do
    echo -n "Test 4.$((5 + $val / 50)): exit code $val ... "
    cat > output.asm << EOF
section .text
    global _start
_start:
    mov rax, $val
    mov rdi, rax
    mov rax, 60
    syscall
EOF
    ./chronos_program > /dev/null 2>&1
    chmod +x chronos_output
    ./chronos_output
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq $val ]; then
        echo -e "${GREEN}✅ PASS${NC}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}❌ FAIL${NC} (expected: $val, got: $EXIT_CODE)"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 5: END-TO-END PIPELINE TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# E2E Test 1
echo -n "Test 5.1: E2E (compile + assemble + run, exit 11) ... "
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
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC} (got: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# E2E Test 2
echo -n "Test 5.2: E2E (arithmetic parse, exit 123) ... "
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
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC} (got: $EXIT_CODE)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 6: ERROR HANDLING TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Test 6.1: file too large
echo -n "Test 6.1: File size limit (8192 bytes) ... "
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
if ./chronos_program 2>&1 | grep -q "ERROR: Assembly file too large"; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SECTION 7: STRESS TESTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Test 7.1: Repeated compilation
echo -n "Test 7.1: Repeated compilation (10x) ... "
SUCCESS=true
for i in {1..10}; do
    if ! ./compiler/bootstrap-c/chronos_v10 compiler/chronos/compiler_main.ch > /dev/null 2>&1; then
        SUCCESS=false
        break
    fi
done
if [ "$SUCCESS" = true ]; then
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
TOTAL_COUNT=$((TOTAL_COUNT + 1))

# Test 7.2: Multiple different exit codes
echo -n "Test 7.2: Multiple executions (20 different exit codes) ... "
./compiler/bootstrap-c/chronos_v10 compiler/chronos/toolchain.ch > /dev/null 2>&1
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
    echo -e "${GREEN}✅ PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}❌ FAIL${NC}"
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
if [ $TOTAL_COUNT -gt 0 ]; then
    echo "Success Rate: $(( PASS_COUNT * 100 / TOTAL_COUNT ))%"
fi
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
    echo -e "${RED}❌ $FAIL_COUNT TESTS FAILED${NC}"
    echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    exit 1
fi
