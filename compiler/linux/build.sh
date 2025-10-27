#!/bin/bash
# Build Tempo Compiler for Linux

echo "🔨 Building Tempo Compiler for Linux..."

# Assemble
nasm -f elf64 tempo-compiler.asm -o tempo-compiler.o

# Link  
ld -o tempo-compiler tempo-compiler.o

echo "✅ Built tempo-compiler for Linux"

# Test if we can
if [ -f tempo-compiler ]; then
    echo "🧪 Testing Linux compiler..."
    ./tempo-compiler hello.ch
    
    if [ -f stage1 ]; then
        echo "📦 Generated stage1"
        chmod +x stage1
        echo "🚀 Running stage1..."
        ./stage1
    fi
fi