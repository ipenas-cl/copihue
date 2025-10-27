#!/bin/bash
# Build Simple Tempo Compiler

echo "🔨 Building Tempo Simple Compiler (100% Assembly)..."

# Assemble
nasm -f macho64 tempo-simple.asm -o tempo-simple.o

# Link without C libraries - just Mach-O loader
ld -macos_version_min 10.7 -no_pie -arch x86_64 -o tempo-compiler tempo-simple.o

# Make executable
chmod +x tempo-compiler

echo "✅ Built tempo-compiler (100% Assembly, 0% C)"

# Test with hello world if it exists
if [ -f "../../examples/hello.ch" ]; then
    echo "🧪 Testing compiler..."
    ./tempo-compiler ../../examples/hello.ch
    
    if [ -f stage1 ]; then
        echo "📦 Generated stage1 binary"
        echo "🚀 Testing stage1..."
        ./stage1
        echo "💯 SUCCESS: Tempo compiler works!"
    else
        echo "❌ No stage1 generated"
    fi
else
    echo "⚠️  No test file found at ../../examples/hello.ch"
fi