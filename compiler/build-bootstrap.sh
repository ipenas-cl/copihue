#!/bin/bash
# Construir el bootstrap binario desde hex - Sin C!
# Author: Ignacio Peña Sepúlveda
# Date: June 25, 2025

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║              TEMPO PURE BOOTSTRAP - NO C!                     ║"
echo "║  ╔═════╦═════╦═════╗                                         ║"
echo "║  ║ 🛡️  ║ ⚖️  ║ ⚡  ║    100% Binary Bootstrap                ║"
echo "║  ║  C  ║  E  ║  G  ║                                         ║"
echo "║  ╚═════╩═════╩═════╝                                         ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Convertir hex a binario
xxd -r -p bootstrap.hex > bootstrap0

# Hacer ejecutable
chmod +x bootstrap0

echo "✅ Bootstrap binario creado!"
echo ""
echo "Ahora puedes compilar Tempo sin C:"
echo "  ./bootstrap0 stage1.ch"
echo ""
echo "[T∞] Cero dependencias de C - Tempo puro!"