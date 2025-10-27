#!/bin/bash
# Build script for Tempo Full Compiler (100% Assembly)

echo "🔨 Building Tempo Full Compiler..."

# Assemble the compiler
nasm -f macho64 tempo-full-compiler.asm -o tempo-full-compiler.o

# Link (minimal, no C libraries)
ld -macosx_version_min 10.7 -lSystem -o tempo-compiler tempo-full-compiler.o

# Make executable
chmod +x tempo-compiler

echo "✅ Built tempo-compiler"

# Test with hello world
if [ -f "../../examples/hello.ch" ]; then
    echo "🧪 Testing with hello.ch..."
    ./tempo-compiler ../../examples/hello.ch
    if [ -f stage1 ]; then
        echo "📦 Generated stage1"
        echo "🚀 Running stage1..."
        ./stage1
    fi
fi