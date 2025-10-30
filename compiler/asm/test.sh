#!/bin/bash
# Test script for Chronos compiler

set -e

echo "Testing Chronos Compiler..."
echo ""

# Create test template
echo "Creating test template..."
cat > test.chronos << 'EOF'
Program hello
  Print "Hello from Chronos!"
  Print "This compiler is written in pure assembly!"
EOF

echo "Template created: test.chronos"
echo ""

# Compile template
echo "Compiling with Chronos..."
./chronos test.chronos

echo ""

# Assemble generated code
echo "Assembling output.s..."
as -o output.o output.s

# Link
echo "Linking..."
ld -o hello output.o

echo ""

# Run the generated program
echo "Running generated program:"
echo "---"
./hello
echo "---"

echo ""
echo "✓ Test successful!"

# Cleanup
echo "Cleaning up..."
rm -f test.chronos output.s output.o hello

echo "Done."
