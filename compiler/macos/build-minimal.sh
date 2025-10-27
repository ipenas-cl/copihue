#!/bin/bash
# Build Minimal Tempo Compiler

echo "🔨 Building Tempo Minimal Compiler..."

# Clean previous builds
rm -f tempo-minimal.o tempo-compiler stage1

# Assemble
nasm -f macho64 tempo-minimal.asm -o tempo-minimal.o
if [ $? -ne 0 ]; then
    echo "❌ Assembly failed"
    exit 1
fi

# Link
ld -macos_version_min 10.7 -no_pie -arch x86_64 -o tempo-compiler tempo-minimal.o
if [ $? -ne 0 ]; then
    echo "❌ Linking failed"
    exit 1
fi

echo "✅ Built tempo-compiler"

# Test
echo "🧪 Testing compiler..."
./tempo-compiler hello.ch

if [ -f stage1 ]; then
    echo "📦 Generated stage1"
    # Rename stage1 to tempo.app for consistency
    mv stage1 tempo.app
    echo "✅ Renamed to tempo.app"
    echo "🚀 Running tempo.app..."
    ./tempo.app
    echo ""
    echo "🎉 SUCCESS! Tempo compiler works!"
    echo "[T∞] 100% Assembly, 0% C dependencies"
else
    echo "❌ No stage1 generated"
fi