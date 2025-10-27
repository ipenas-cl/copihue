#!/bin/bash

# ===========================================================================
# BUILD SCRIPT PARA TEMPO BOOTSTRAP EN MACOS ARM64 (Apple Silicon)
# ===========================================================================
# Compila el bootstrap de Tempo en macOS/Darwin ARM64
# Author: Ignacio Peña Sepúlveda
# Date: June 26, 2025
# ===========================================================================

set -e  # Salir si hay errores

echo "╔═════╦═════╦═════╗"
echo "║ 🛡️  ║ ⚖️  ║ ⚡  ║  TEMPO BUILD - macOS ARM64"
echo "║  C  ║  E  ║  G  ║"
echo "╚═════╩═════╩═════╝"
echo ""

# Verificar que estamos en macOS ARM64
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: Este script es solo para macOS"
    exit 1
fi

# Verificar arquitectura
ARCH=$(uname -m)
if [[ "$ARCH" != "arm64" ]]; then
    echo "⚠️  Advertencia: Detectada arquitectura $ARCH, pero compilando para ARM64"
    echo "   Esto funcionará en Rosetta 2 pero es mejor usar la versión x86-64"
fi

# Verificar NASM
if ! command -v nasm &> /dev/null; then
    echo "Error: NASM no está instalado"
    echo "Instala con: brew install nasm"
    exit 1
fi

echo "🔧 Compilando bootstrap.asm para ARM64..."

# NASM no soporta ARM64 nativamente, necesitamos usar un enfoque diferente
# Para ARM64, usaremos el assembler de macOS (as) en lugar de NASM

echo "⚠️  NASM no soporta ARM64 nativamente"
echo "   Necesitamos convertir a sintaxis GNU ARM64 o usar un enfoque híbrido"
echo ""

# Verificar si tenemos el assembler de macOS
if ! command -v as &> /dev/null; then
    echo "Error: Assembler de macOS (as) no encontrado"
    exit 1
fi

echo "🔄 Convirtiendo sintaxis NASM a GNU ARM64..."

# Por ahora, crear un mensaje indicando que necesita implementación adicional
cat > temp_arm64.s << 'EOF'
.section __TEXT,__text,regular,pure_instructions
.globl _start

_start:
    // Banner message
    mov x8, #4                    // write syscall (0x2000004 & 0xFF)
    mov x0, #1                    // stdout
    adr x1, banner                // message
    mov x2, #100                  // approximate length
    svc #0x80                     // system call with high bits

    // Exit
    mov x8, #1                    // exit syscall (0x2000001 & 0xFF)  
    mov x0, #0                    // exit code
    svc #0x80                     // system call with high bits

.section __TEXT,__cstring,cstring_literals
banner:
    .ascii "╔═════╦═════╦═════╗\n"
    .ascii "║ 🛡️  ║ ⚖️  ║ ⚡  ║  TEMPO Bootstrap ARM64\n"
    .ascii "║  C  ║  E  ║  G  ║\n"
    .ascii "╚═════╩═════╩═════╝\n"
    .ascii "\n❌ ARM64 Bootstrap necesita implementación completa\n"
    .ascii "   Por ahora usa la versión x86-64 con Rosetta 2\n\0"
EOF

echo "🔗 Ensamblando con GNU assembler..."

# Ensamblar con el assembler de macOS
as -arch arm64 temp_arm64.s -o bootstrap_arm64.o

echo "🔗 Enlazando..."

# Enlazar para ARM64
ld -arch arm64 -e _start -o tempo-bootstrap-arm64 bootstrap_arm64.o

# Limpiar archivos temporales
rm temp_arm64.s bootstrap_arm64.o

echo "✅ tempo-bootstrap-arm64 compilado!"
echo ""

# Verificar el binario
echo "📊 Información del binario:"
file tempo-bootstrap-arm64
echo ""

echo "🎯 Dependencias:"
otool -L tempo-bootstrap-arm64 2>/dev/null || echo "   (Ejecutable estático)"
echo ""

echo "[T∞] ¡Bootstrap ARM64 básico listo!"
echo ""
echo "⚠️  NOTA IMPORTANTE:"
echo "   Este es un bootstrap básico de demostración"
echo "   Para funcionalidad completa, se necesita:"
echo "   1. Convertir toda la lógica NASM a sintaxis GNU ARM64"
echo "   2. Adaptar las convenciones de llamada ARM64"
echo "   3. Ajustar los números de syscall para ARM64"
echo ""
echo "Por ahora, usa: ../macos/tempo-bootstrap (funciona en Rosetta 2)"