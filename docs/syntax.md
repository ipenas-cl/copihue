# CHRONOS LANGUAGE SYNTAX REFERENCE

## Overview

Chronos es un lenguaje de programación de sistemas diseñado para:
- **Determinismo**: Comportamiento predecible y verificable
- **Minimalismo**: Sin imports, sin macros complejas
- **Performance**: Compilación directa a assembly x64
- **Self-hosting**: Compilador escrito en sí mismo

## Versión Actual

**Chronos v0.17** - OPTIMIZING COMPILER
- Operadores de incremento/decremento: `++`, `--`
- Operadores de asignación compuesta: `+=`, `-=`, `*=`, `/=`, `%=`
- String literal indexing: `"Hello"[0]`
- Todas las features anteriores incluidas (%, &&, ||, !, arrays locales tipados)

---

## 1. TIPOS DE DATOS

### Tipos Primitivos

```chronos
// Integers
let x: i8 = 127;        // 8-bit signed (-128 a 127)
let y: i32 = 1000;      // 32-bit signed (default)
let z: i64 = 10000;     // 64-bit signed
let w: u32 = 4000;      // 32-bit unsigned
```

### Arrays

```chronos
// Global arrays con tipo explícito ✅
let global_array: [i8; 256];
let buffer: [i32; 1024];

// Local arrays - SOLO sin tipo explícito ✅
fn foo() -> i32 {
    let local = [0, 1, 2, 3];  // Array literal, tipo inferido
    return local[0];
}

// ❌ NO SOPORTADO: Arrays locales con tipo explícito
fn bar() -> i32 {
    let arr: [i32; 10];  // Parse error
    return 0;
}
```

### Punteros

```chronos
fn process_buffer(buf: i32, size: i32) -> i32 {
    // buf es un puntero (i32 = dirección)
    let first_byte = buf[0];
    return first_byte;
}
```

---

## 2. VARIABLES Y CONSTANTES

### Declaración

```chronos
// Variables locales
let x = 42;
let name: i32 = 100;

// Variables globales
let global_counter = 0;
let shared_buffer: [i8; 4096];
```

### Asignación

```chronos
// Asignación simple
x = x + 1;
buffer[0] = 65;  // 'A'
global_counter = global_counter + 1;

// Operadores de incremento/decremento ✅
x++;  // Equivalente a: x = x + 1
x--;  // Equivalente a: x = x - 1

// Operadores de asignación compuesta ✅
x += 5;   // Equivalente a: x = x + 5
x -= 3;   // Equivalente a: x = x - 3
x *= 2;   // Equivalente a: x = x * 2
x /= 4;   // Equivalente a: x = x / 4
x %= 10;  // Equivalente a: x = x % 10
```

---

## 3. OPERADORES

### Aritméticos

```chronos
let sum = a + b;      // Suma
let diff = a - b;     // Resta
let prod = a * b;     // Multiplicación
let quot = a / b;     // División
let remainder = a % b; // Modulo (resto de división) ✅
```

### Comparación

```chronos
if (x == y) { }   // Igual
if (x != y) { }   // Diferente
if (x < y) { }    // Menor
if (x > y) { }    // Mayor
if (x <= y) { }   // Menor o igual
if (x >= y) { }   // Mayor o igual
```

### Lógicos

```chronos
// Operador AND (&&) ✅ con short-circuit evaluation
if (x > 0 && y > 0) {
    println("Ambos son positivos");
}

// Operador OR (||) ✅ con short-circuit evaluation
if (x > 0 || y > 0) {
    println("Al menos uno es positivo");
}

// Expresiones complejas
if ((x > 0 && x < 100) || y == 0) {
    println("En rango o y es cero");
}
```

---

## 4. ESTRUCTURAS DE CONTROL

### If-Else

```chronos
// If básico
if (x > 0) {
    println("Positivo");
}

// If-Else ✅ SOPORTADO
if (x > 0) {
    println("Positivo");
} else {
    println("No positivo");
}

// If-Else anidado
if (x > 0) {
    println("Positivo");
} else {
    if (x < 0) {
        println("Negativo");
    } else {
        println("Cero");
    }
}
```

### While Loop

```chronos
let i = 0;
while (i < 10) {
    print_int(i);
    i = i + 1;
}
```

### For Loop ✅ SOPORTADO

```chronos
// For loop estándar
for (let i = 0; i < 10; i = i + 1) {
    print_int(i);
    println("");
}

// For loop con paso diferente
for (let i = 0; i < 100; i = i + 5) {
    print_int(i);
}
```

---

## 5. FUNCIONES

### Declaración

```chronos
// Sin parámetros
fn greet() -> i32 {
    println("Hello!");
    return 0;
}

// Con parámetros (REQUIEREN tipos explícitos)
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// Con punteros
fn process(buffer: i32, size: i32) -> i32 {
    let first = buffer[0];
    return first;
}
```

### Llamadas

```chronos
let result = add(5, 3);
process(my_buffer, 256);
```

---

## 6. STRINGS Y CARACTERES

### String Literals

```chronos
// ✅ String literals son indexables directamente
let ch = "Hello"[0];  // ✓ OK: 72 ('H')
let ch2 = "Hello"[1]; // ✓ OK: 101 ('e')

// ✅ Usar en expresiones
if ("Hello"[0] == 72) {
    println("Primera letra es 'H'");
}

// ✅ Asignar a variable primero
let msg = "Hello";
// NOTA: msg es un puntero, NO indexable
// Para indexar, usar literal directamente o copiar a array

// ✅ Copiar a buffer para indexar múltiples veces
let buffer: [i8; 256];
strcpy(buffer, "Hello");
let ch3 = buffer[0];  // ✓ OK: 72 ('H')

// ✅ Declarar como array con tipo
let msg_arr: [i8; 6] = "Hello";
let ch4 = msg_arr[0];  // ✓ OK: 72 ('H')

// ✅ Para imprimir directamente
println("Hello");  // ✓ OK
```

### Operaciones de String

```chronos
// strlen - longitud de string
let len = strlen("Hello");  // 5

// strcmp - comparar strings
if (strcmp(str1, str2) == 0) {
    // son iguales
}

// strcpy - copiar string
strcpy(dest, source);
```

---

## 7. ARRAYS Y MEMORIA

### Acceso a Arrays

```chronos
let value = array[index];
array[index] = 42;
```

### Address-of Operator

```chronos
// Obtener dirección de elemento de array
let ptr = array + index;     // Puntero aritmético
let addr = buffer + offset;  // ✓ Funciona

// NOTA: & operator puede tener issues en algunos contextos
```

### Bounds Checking

```chronos
// Chronos hace bounds checking en runtime
let arr: [i8; 10];
let x = arr[15];  // Runtime error: "Array bounds error"
```

---

## 8. SYSCALLS Y I/O

### Syscalls Linux

```chronos
// syscall6 - hasta 6 argumentos
let result = syscall6(
    syscall_num,
    arg1, arg2, arg3,
    arg4, arg5, arg6
);

// Ejemplo: read
let SYS_READ = 0;
let bytes = syscall6(SYS_READ, fd, buffer, size, 0, 0, 0);
```

### I/O Básico

```chronos
println("Hello, World!");    // Print con newline
print("Hello");             // Print sin newline
print_int(42);              // Print integer
```

### File I/O

```chronos
// read(fd, buffer, size)
let bytes_read = read(fd, buffer, 1024);

// write(fd, buffer, size)
let bytes_written = write(fd, buffer, len);

// close(fd)
close(fd);
```

---

## 9. NETWORKING

### Socket Operations

```chronos
let SYS_SOCKET = 41;
let AF_INET = 2;
let SOCK_STREAM = 1;

// Crear socket
let sockfd = syscall6(SYS_SOCKET, AF_INET, SOCK_STREAM, 0, 0, 0, 0);

// Bind
let SYS_BIND = 49;
let result = syscall6(SYS_BIND, sockfd, sockaddr, 16, 0, 0, 0);

// Listen
let SYS_LISTEN = 50;
syscall6(SYS_LISTEN, sockfd, backlog, 0, 0, 0, 0);

// Accept
let SYS_ACCEPT = 43;
let clientfd = syscall6(SYS_ACCEPT, sockfd, 0, 0, 0, 0, 0);
```

---

## 10. MEJORES PRÁCTICAS

### ✅ DO

```chronos
// Usar global arrays para buffers grandes
let tcp_buffer: [i8; 4096];

// Inicializar variables explícitamente
let counter = 0;

// Usar nombres descriptivos
let bytes_received = read(fd, buffer, size);

// Validar resultados de syscalls
if (sockfd < 0) {
    println("[ERROR] Socket creation failed");
    return 1;
}
```

### ❌ DON'T

```chronos
// NO indexar variables de string (solo literals funcionan)
let msg = "hello";  // msg es un puntero
let ch = msg[0];  // ⚠️ No funcionará
// Usar: "hello"[0] o copiar a array typed

// NO usar estructuras locales
fn foo() -> i32 {
    let s: MyStruct;  // ⚠️ Parse error
    // Usar struct global en su lugar
}
```

---

## 11. LIMITACIONES CONOCIDAS

### Sintaxis No Soportada

| Feature | Status | Workaround |
|---------|--------|------------|
| String variable indexing | ⚠️ Limitado | Solo `"literal"[i]` funciona, no `var[i]` |
| Struct locals | ❌ No soportado | Use global structs |
| Pattern matching | ❌ No soportado | Use if/else chains |
| Generics | ❌ No soportado | Manual code duplication |
| Closures | ❌ No soportado | Use function pointers |

### Sintaxis Soportada ✅

| Feature | Status | Ejemplo |
|---------|--------|---------|
| `else` clause | ✅ Funciona | `if (x) { } else { }` |
| `for` loop | ✅ Funciona | `for (let i = 0; i < 10; i = i + 1)` |
| `while` loop | ✅ Funciona | `while (i < 10) { i = i + 1; }` |
| `%` modulo operator | ✅ Funciona | `let remainder = x % y;` |
| `&&` logical AND | ✅ Funciona | `if (x > 0 && y > 0)` |
| `||` logical OR | ✅ Funciona | `if (x > 0 || y > 0)` |
| `!` logical NOT | ✅ Funciona | `if (!x)` or `if (!(x > 10))` |
| `++` increment | ✅ Funciona | `x++;` |
| `--` decrement | ✅ Funciona | `x--;` |
| `+=` add assign | ✅ Funciona | `x += 5;` |
| `-=` sub assign | ✅ Funciona | `x -= 3;` |
| `*=` mul assign | ✅ Funciona | `x *= 2;` |
| `/=` div assign | ✅ Funciona | `x /= 4;` |
| `%=` mod assign | ✅ Funciona | `x %= 10;` |
| String literal indexing | ✅ Funciona | `let ch = "Hello"[0];` |
| Arrays globales con tipo | ✅ Funciona | `let arr: [i32; 10];` |
| Arrays locales con tipo | ✅ Funciona | `let arr: [i32; 5];` (dentro de funciones) |
| Arrays locales sin tipo | ✅ Funciona | `let arr = [1, 2, 3];` |
| Punteros | ✅ Funciona | `let ptr: *i32 = &x;` |

### Runtime Checks

- ✅ Array bounds checking
- ✅ Null pointer detection
- ✅ Stack overflow protection (limited)

---

## 12. EJEMPLOS COMPLETOS

### Hello World

```chronos
fn main() -> i32 {
    println("Hello, Chronos!");
    return 0;
}
```

### TCP Echo Server

```chronos
let AF_INET = 2;
let SOCK_STREAM = 1;
let SYS_SOCKET = 41;
let buffer: [i8; 1024];

fn main() -> i32 {
    let sockfd = syscall6(SYS_SOCKET, AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    if (sockfd < 0) {
        println("Socket failed");
        return 1;
    }

    // Bind, listen, accept...

    let bytes = read(clientfd, buffer, 1024);
    write(clientfd, buffer, bytes);

    close(clientfd);
    close(sockfd);
    return 0;
}
```

### HTTP Request

```chronos
fn send_get_request(sockfd: i32, path: i32) -> i32 {
    write(sockfd, "GET ", 4);
    write(sockfd, path, strlen(path));
    write(sockfd, " HTTP/1.0\r\n\r\n", 13);

    let received = read(sockfd, buffer, 4095);
    buffer[received] = 0;
    println(buffer);

    return 0;
}
```

---

## Versión

**Chronos v0.17** - Última actualización: 2025-10-27

Para más información:
- [Standard Library Reference](stdlib.md)
- [HTTP API Documentation](http-api.md)
- [Compiler Documentation](../compiler/README.md)
