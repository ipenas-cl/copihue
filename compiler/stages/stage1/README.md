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

# Chronos Compiler Stage 1

Compilador de Chronos escrito en Chronos mínimo (compilable por stage0).

## 🎯 Objetivo

Stage1 es un compilador más completo que stage0, pero todavía simple. Está escrito en un subset de Chronos que nuestro bootstrap en assembly puede compilar.

## 📚 Características Nuevas vs Stage0

### Lexer Mejorado
- ✅ Todos los operadores (`+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`)
- ✅ Comentarios de línea (`//`)
- ✅ Mejor manejo de errores con línea y columna
- ✅ Keywords adicionales (`type`, `let`, `if`, `else`, `while`, `for`, `return`)

### Parser Completo
- ✅ Expresiones con precedencia correcta
- ✅ Declaraciones de variables (`let`)
- ✅ Estructuras de control (`if`, `else`, `while`)
- ✅ Llamadas a funciones
- ✅ Operadores binarios y unarios
- ✅ Asignaciones

### Generador de Código
- ✅ Genera assembly x86_64 optimizado
- ✅ Manejo de expresiones complejas
- ✅ Control flow (if/else, while)
- ✅ Stack management correcto

## 🔧 Compilación

```bash
# Primero necesitas stage0
cd ../stage0
make

# Compilar stage1 con stage0
./tempo0 ../stage1/compiler.ch ../stage1/compiler.s

# Ensamblar y linkear
nasm -f elf64 ../stage1/compiler.s -o ../stage1/compiler.o
ld -o ../stage1/tempo1 ../stage1/compiler.o

# Ahora tienes tempo1!
```

## 📝 Ejemplos de Código Soportado

### Hello World
```tempo
function main() {
    print("Hello from Stage 1!");
}
```

### Variables y Expresiones
```tempo
function main() {
    let x = 10;
    let y = 20;
    let sum = x + y;
    
    print("The sum is:");
    print_number(sum);
}
```

### Control Flow
```tempo
function factorial(n: i32) -> i32 {
    if (n <= 1) {
        return 1;
    } else {
        return n * factorial(n - 1);
    }
}

function main() {
    let result = factorial(5);
    print_number(result); // 120
}
```

### Loops
```tempo
function main() {
    let i = 0;
    while (i < 10) {
        print_number(i);
        i = i + 1;
    }
}
```

## 🏗️ Arquitectura

### AST (Abstract Syntax Tree)
```
Program
├── Function("main")
│   └── Block
│       ├── Let("x", Literal(10))
│       ├── Let("y", Literal(20))
│       └── Call("print", [Binary("+", Var("x"), Var("y"))])
```

### Fases del Compilador
1. **Lexer**: Source → Tokens
2. **Parser**: Tokens → AST
3. **Type Checker**: AST → Typed AST (básico por ahora)
4. **Code Gen**: AST → Assembly

## 🚀 Próximos Pasos (Stage 2)

Stage 2 será el compilador completo con:
- Sistema de tipos completo
- Type inference
- Análisis WCET
- Optimizaciones
- Generics
- Traits/Interfaces
- Pattern matching
- Memory pools

## 📊 Comparación de Stages

| Feature | Stage 0 | Stage 1 | Stage 2 |
|---------|---------|---------|---------|
| Líneas de código | 500 | 2000 | 10000+ |
| Lenguaje | Assembly | Chronos básico | Chronos completo |
| Features | print() | Variables, if, while | Todo |
| Optimizaciones | Ninguna | Básicas | Avanzadas |
| Error handling | Crash | Línea/columna | Full diagnostics |
| Type system | No | Básico | Completo con inference |

## 🎓 Para el Curso de Compiladores

### Lección 1: Bootstrap
- Por qué assembly
- Tokenizer mínimo
- Parser recursivo

### Lección 2: Self-hosting
- Compilar el compilador
- Subset del lenguaje
- Evolución incremental

### Lección 3: AST y Parsing
- Precedencia de operadores
- Recursive descent
- Error recovery

### Lección 4: Code Generation
- Stack machines
- Register allocation básico
- Calling conventions

## 🐛 Limitaciones Actuales

1. **No hay type checking real** - Todo es i64 o string
2. **No hay structs** - Solo tipos primitivos
3. **No hay arrays dinámicos** - Solo estáticos
4. **No hay heap allocation** - Todo en stack
5. **No hay optimizaciones** - Código directo

Estas limitaciones son intencionales - Stage1 debe ser simple enough para que stage0 lo compile!

---

*"El arte de bootstrapping: Hacer mucho con poco"*