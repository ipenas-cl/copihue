#!/bin/bash
# Build script for Chronos compiler
# Assembles and links all modules

set -e

echo "Building Chronos Compiler (Assembly)..."

# Assemble each module
echo "  [1/7] Assembling main.s..."
as -o main.o main.s

echo "  [2/7] Assembling io.s..."
as -o io.o io.s

echo "  [3/7] Assembling parser.s..."
as -o parser.o parser.s

echo "  [4/7] Assembling symbol_table.s..."
as -o symbol_table.o symbol_table.s

echo "  [5/7] Assembling expr.s..."
as -o expr.o expr.s

echo "  [6/7] Assembling codegen.s..."
as -o codegen.o codegen.s

echo "  [7/7] Assembling memory.s..."
as -o memory.o memory.s

# Link all modules
echo "  Linking..."
ld -o chronos main.o io.o parser.o symbol_table.o expr.o codegen.o memory.o

# Cleanup object files
echo "  Cleaning up..."
rm -f *.o

echo "✓ Build successful: ./chronos"
echo ""
echo "Usage: ./chronos <input.chronos>"
