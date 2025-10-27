# Chronos Compiler for macOS

Este directorio contiene el compilador de Chronos para macOS.

## Archivos

- `tempo-compiler` - Compilador binario para macOS (x86_64)
- `compiler/` - Código fuente del compilador (en desarrollo)

## Arquitectura

El compilador genera binarios Mach-O nativos sin usar ninguna herramienta de C:
- 100% ensamblador
- Genera binarios ejecutables directamente
- No requiere linker externo

## Estado actual

v0.0.1 - Versión inicial con funcionalidad básica:
- ✅ Genera binarios Mach-O válidos
- ✅ Compila programas simples con `print_line`
- 🚧 Parser completo en desarrollo
- 🚧 Soporte para más características del lenguaje

## Uso

```bash
tempo programa.ch
./stage1
```