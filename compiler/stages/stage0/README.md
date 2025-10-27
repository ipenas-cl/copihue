<div align="center">

╔═════╦═════╦═════╗  
║ 🛡️  ║ ⚖️  ║ ⚡  ║  
║  C  ║  E  ║  G  ║  
╚═════╩═════╩═════╝  
╔═════════════════╗  
║ wcet [T∞] bound ║  
╚═════════════════╝  

**Author:** Ignacio Peña Sepúlveda  
**Date:** June 25, 2025

</div>

---

# Chronos Bootstrap Stage 0

El compilador más pequeño del mundo - **500 líneas de assembly puro**.

## ¿Qué es esto?

Este es el bootstrap inicial de Chronos. Un compilador minimalista escrito completamente en assembly x86_64 que puede compilar un subset básico de Chronos, suficiente para compilar el siguiente stage.

## Características

- ✅ Tokenizer completo
- ✅ Parser recursivo descendente  
- ✅ Generador de código assembly
- ✅ Sin dependencias externas
- ✅ < 500 líneas de código
- ✅ Compila en < 1 segundo

## Subset de Chronos soportado

```tempo
function main() {
    print("Hello, world!");
}
```

Por ahora solo soporta:
- Funciones sin parámetros
- Statement `print` con strings
- Eso es todo! (suficiente para bootstrap)

## Compilar y ejecutar

```bash
# Compilar el bootstrap
make

# Esto genera:
# - tempo0: El compilador bootstrap
# - hello: Programa de prueba compilado

# Compilar un archivo .ch manualmente
./tempo0 programa.ch programa.s
nasm -f elf64 programa.s -o programa.o
ld -o programa programa.o
./programa
```

## Arquitectura

```
hello.ch → [TOKENIZER] → [PARSER] → [AST] → [CODEGEN] → hello.s
```

### Tokenizer
- Reconoce keywords, identificadores, números, strings
- Skip whitespace y comentarios
- Tracking de líneas para errores

### Parser  
- Parseo recursivo descendente
- Construcción de AST simple
- Validación sintáctica básica

### Code Generator
- Genera assembly x86_64 para Linux
- Optimizaciones: ninguna (es bootstrap!)
- Output: archivo .s listo para NASM

## Próximos pasos

Una vez que este bootstrap funciona, lo usamos para compilar `stage1.ch`:

```tempo
// stage1.ch - Compilador más completo escrito en Chronos
type Token = Keyword(string) | Identifier(string) | Number(i64) | ...

function tokenize(source: string) -> Array<Token> {
    // Tokenizer completo en Chronos
}

function parse(tokens: Array<Token>) -> AST {
    // Parser completo en Chronos  
}

function codegen(ast: AST) -> string {
    // Generador optimizado
}
```

## Por qué assembly?

1. **Sin dependencias**: No necesitas nada más que un assembler
2. **Determinístico**: Control total sobre cada ciclo
3. **Educacional**: Entiendes TODO lo que pasa
4. **Bragging rights**: "Mi compilador está en assembly"

## Hacks interesantes

- El tokenizer no usa heap, todo en buffers estáticos
- El parser genera AST directamente en memoria lineal
- Zero allocations = zero problemas
- Tiempo de compilación constante (no depende del input!)

---

*"From assembly to revolution in 500 lines"*