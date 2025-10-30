# Chronos - Sintaxis del Lenguaje
**Versión:** 0.0.1
**Sintaxis:** Template/Prompt-Based (estilo YAML/declarativo)

---

## Filosofía de Sintaxis

Chronos usa una sintaxis **declarativa tipo plantilla**, inspirada en prompts de IA y YAML:
- ✅ Legible como lenguaje natural
- ✅ Indentación significativa (2 espacios)
- ✅ Sin punto y coma ni llaves
- ✅ Keywords en inglés, estructura clara

---

## 1. Estructura Básica

### Programa Mínimo
```chronos
Program hello
  Print "Hello, World!"
```

### Programa con Variables
```chronos
Program test
  Variables:
    x: i32 = 42
    y: i64 = 100
    z: bool = true

  Print "Testing variables:"
  Print x
  Print y
  Print z
```

---

## 2. Variables (FASE 1 - Implementando)

### Declaración
```chronos
Variables:
  name: type = value
  age: i32 = 25
  height: i64 = 180
  active: bool = true
```

### Tipos Primitivos
- `i32` - Entero 32 bits con signo
- `i64` - Entero 64 bits con signo
- `bool` - Booleano (true/false o 1/0)

### Scope
Las variables declaradas en `Variables:` están disponibles en todo el programa.

---

## 3. Print Statement (IMPLEMENTADO)

### Print String Literal
```chronos
Print "Hello, World!"
```

### Print Variable
```chronos
Print x
```

### Comportamiento
- Siempre agrega newline al final
- String literals entre comillas dobles `"`
- Variables sin comillas

---

## 4. Expresiones Aritméticas (FASE 2 - Planeado)

```chronos
Program arithmetic
  Variables:
    a: i32 = 10
    b: i32 = 20
    result: i32 = a + b * 2

  Print result  # Imprime 50
```

### Operadores Planeados
- `+` Suma
- `-` Resta
- `*` Multiplicación
- `/` División
- `%` Módulo

### Orden de Evaluación
**SIEMPRE left-to-right, con precedencia estándar:**
1. Paréntesis `()`
2. Multiplicación `*`, División `/`, Módulo `%`
3. Suma `+`, Resta `-`

---

## 5. Control de Flujo (FASE 3 - Planeado)

### If/Else
```chronos
Program control
  Variables:
    x: i32 = 10

  If x > 5:
    Print "x is large"
  Else:
    Print "x is small"
```

### While Loop
```chronos
Program loop
  Variables:
    i: i32 = 0

  While i < 10:
    Print i
    i = i + 1
```

### For Loop
```chronos
Program for_loop
  Variables:
    i: i32

  For i in 0 to 10:
    Print i
```

---

## 6. Funciones (FASE 4 - Planeado)

```chronos
Program functions

Function: add
  Parameters:
    a: i32
    b: i32
  Returns: i32
  Implementation:
    return a + b

Function: main
  Variables:
    result: i32 = add(10, 20)
  Print result
```

---

## 7. Structs (FASE 5 - Planeado)

```chronos
Program structs

Struct: Point
  Fields:
    x: i32
    y: i32

Function: create_point
  Returns: Point owns
  Implementation:
    return Point { x: 10, y: 20 }

Function: main
  Variables:
    p: Point = create_point()
  Print p.x
  Print p.y
```

---

## 8. Ownership (FASE 5 - Planeado)

```chronos
Function: use_point
  Parameters:
    p: Point borrows
  Implementation:
    Print p.x
    Print p.y

Function: consume_point
  Parameters:
    p: Point owns
  Implementation:
    Print p.x
    # p se destruye al final
```

### Keywords de Ownership
- `owns` - Toma ownership (mueve el valor)
- `borrows` - Préstamo inmutable (referencia)
- `borrows mut` - Préstamo mutable

---

## 9. Real-Time (FASE 6 - Planeado)

```chronos
Program realtime

Function: sensor_read
  WCET: 50 microseconds
  Returns: i32
  Implementation:
    return read_adc()

RTTask: control_loop
  Period: 10 milliseconds
  Priority: 255
  Implementation:
    loop:
      value = sensor_read()
      actuate(value)
```

---

## 10. Concurrency (FASE 7 - Planeado)

```chronos
Program concurrent

Mutex: shared_data
  Protocol: PriorityInheritance
  Type: i32

Thread: worker1
  Priority: 10
  Implementation:
    loop:
      lock(shared_data)
      shared_data = shared_data + 1
      unlock(shared_data)

Thread: worker2
  Priority: 20
  Implementation:
    loop:
      lock(shared_data)
      shared_data = shared_data + 2
      unlock(shared_data)
```

---

## 11. Hardware (FASE 9 - Planeado)

```chronos
Program hardware

MMIO: UART
  Address: 0x40000000
  Registers:
    DATA: offset 0x00, read_write
    STATUS: offset 0x04, read_only
    CONTROL: offset 0x08, write_only

Interrupt: timer_irq
  Vector: 16
  Priority: 10
  Handler:
    flag = true
```

---

## Reglas de Sintaxis

### Indentación
- **2 espacios** por nivel
- NO tabs, NO 4 espacios
- Indentación indica jerarquía y scope

### Keywords
- Comienzan con mayúscula: `Program`, `Variables`, `Print`, `Function`, etc.
- Tipos en minúscula: `i32`, `i64`, `bool`
- Operadores y valores: minúscula

### Nombres
- Variables/funciones: `snake_case` (ej: `my_variable`, `read_sensor`)
- Tipos/Structs: `PascalCase` (ej: `Point`, `Vector2D`)
- Constantes: `UPPER_SNAKE_CASE` (ej: `MAX_SIZE`, `PI`)

### Comentarios
```chronos
# Esto es un comentario
Print "Hello"  # Comentario al final de línea
```

---

## Estado Actual (v0.0.1)

### ✅ Implementado
- Estructura básica `Program`
- `Print` con string literals
- Parsing de `Variables:` section
- Tipos: `i32`, `i64`, `bool`
- Declaración de variables con valores iniciales

### 🔄 En Desarrollo
- `Print` de variables (infraestructura lista, falta conversión int→string)
- Stack allocation para variables

### ⏳ Próximas Fases
- Expresiones aritméticas
- Control de flujo
- Funciones
- Structs y ownership
- Real-time y concurrency
