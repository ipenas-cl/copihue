#!/bin/bash
# Build Tempo compiler for macOS

echo "Building Tempo compiler..."

# Assemble
nasm -f macho64 bootstrap.asm -o bootstrap.o

# Link without C libraries
ld -o tempo-compiler bootstrap.o \
   -lSystem \
   -syslibroot `xcrun -sdk macosx --show-sdk-path` \
   -e _main \
   -arch x86_64

# Clean up
rm bootstrap.o

# Make executable
chmod +x tempo-compiler

echo "✅ Built tempo-compiler"