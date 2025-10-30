# Chronos - Lenguaje Basado en Prompts y Templates

**Versión:** 0.0.1
**Fecha:** 29 de octubre de 2025
**Idea:** ¿Y si programar fuera como escribir prompts de IA?

---

## La Inspiración

### Programación Tradicional (Imperativa)

```rust
fn divide(a: i32, b: i32) -> Result<i32, String> {
    if b == 0 {
        return Err("Division by zero".to_string());
    }
    Ok(a / b)
}
```

**Problema:** Describes **CÓMO** hacer las cosas, no **QUÉ** quieres.

### Prompts de IA (Actual)

```
Create a function that divides two numbers.

Requirements:
- Takes two integers as input
- Returns the result or an error
- Must handle division by zero gracefully
- Should be deterministic
```

**Ventaja:** Describes **QUÉ** quieres, la IA decide **CÓMO**.

---

## ¿Y si combinamos ambos?

### Chronos Prompt-Based Language

```chronos
Function: divide
  Purpose: Divide two integers safely

  Inputs:
    - a: integer (dividend)
    - b: integer (divisor)

  Output:
    - result: integer or error

  Requirements:
    - b must not be zero
    - result must be deterministic
    - overflow must be handled

  Implementation:
    When b is zero:
      Return error "Division by zero"
    Otherwise:
      Return a divided by b
```

**¿Qué hace el compilador?**
1. Lee la especificación
2. Verifica que cumple los requisitos
3. Genera código determinista
4. Garantiza safety

---

## Conceptos Clave

### 1. **Templates = Estructura Garantizada**

En lugar de sintaxis libre (error-prone), usamos **plantillas predefinidas**:

```chronos
Template: Function
  Name: <identifier>
  Purpose: <description>
  Inputs: <parameter_list>
  Output: <return_type>
  Requirements: <contract_list>
  Implementation: <body>
```

**Beneficio:** Imposible escribir código mal formado.

### 2. **Declarativo = QUÉ, no CÓMO**

```chronos
# Declaras QUÉ quieres
Sort: my_array
  By: value ascending
  Stable: yes

# El compilador elige CÓMO (merge sort, quick sort, etc)
```

### 3. **Natural Language-ish**

```chronos
Function: process_data
  When: data is valid
    - Parse the input
    - Transform to uppercase
    - Save to database

  When: data is invalid
    - Log the error
    - Return error message
```

**Más cercano a cómo pensamos.**

---

## Ejemplos Completos

### Ejemplo 1: Hello World

**Versión tradicional:**
```rust
fn main() {
    println!("Hello, World!");
}
```

**Versión Chronos (prompt-based):**
```chronos
Program: hello_world
  Description: Print greeting to console

  Actions:
    - Print "Hello, World!"
```

**Compilador genera:**
- Función main
- Syscall a write
- Exit limpio

---

### Ejemplo 2: Fibonacci

**Versión tradicional:**
```rust
fn fib(n: i32) -> i32 {
    if n <= 1 {
        return n;
    }
    fib(n-1) + fib(n-2)
}
```

**Versión Chronos:**
```chronos
Function: fibonacci
  Purpose: Calculate nth Fibonacci number

  Input:
    - n: non-negative integer

  Output:
    - result: integer

  Logic:
    When n is 0 or 1:
      Return n

    Otherwise:
      Return fibonacci(n-1) + fibonacci(n-2)

  Constraints:
    - Must be deterministic
    - No mutation allowed
    - Stack depth limited to 1000
```

**Compilador puede:**
- Verificar no-mutation
- Limitar recursión
- Optimizar (memoization automática)
- Generar código determinista

---

### Ejemplo 3: Web Server

**Versión Chronos:**
```chronos
Service: web_server
  Description: HTTP server for user data

  Port: 8080

  Endpoint: GET /users/:id
    Input:
      - id: integer from URL

    Output:
      - user data as JSON
      - or 404 if not found

    Logic:
      - Query database for user with id
      - When found:
          Return user as JSON with status 200
      - When not found:
          Return error with status 404

    Requirements:
      - Response time < 100ms (p99)
      - Must handle 1000 req/s
      - Database connection pooled

  Endpoint: POST /users
    Input:
      - user data as JSON from body

    Validation:
      - email must be valid format
      - age must be between 0 and 150
      - name must not be empty

    Logic:
      - Validate input
      - When valid:
          - Save to database
          - Return created user with status 201
      - When invalid:
          - Return validation errors with status 400
```

**Compilador genera:**
- Router con endpoints
- Validation automática
- Error handling
- Logging
- Metrics

---

## Sistema de Templates

### Template: Function

```yaml
Template: Function
  Required:
    - Name: identifier
    - Purpose: string
    - Inputs: parameter_list
    - Output: type_spec
    - Logic: implementation

  Optional:
    - Requirements: contract_list
    - Constraints: constraint_list
    - Effects: effect_list
    - WCET: time_bound
```

### Template: Data Structure

```chronos
Data: User
  Description: Represents a user in the system

  Fields:
    - id: integer (unique, auto-increment)
    - name: string (required, max 100 chars)
    - email: string (required, valid email format)
    - age: integer (optional, range 0-150)
    - created_at: timestamp (auto, immutable)

  Constraints:
    - email must be unique
    - id cannot be changed after creation

  Indexes:
    - Primary: id
    - Unique: email
```

**Compilador genera:**
- Struct definition
- Validation functions
- Serialization/deserialization
- Database schema

### Template: State Machine

```chronos
StateMachine: order_processing
  Description: Order lifecycle

  States:
    - Pending (initial)
    - Confirmed
    - Shipped
    - Delivered (final)
    - Cancelled (final)

  Transitions:
    From Pending:
      - To Confirmed: when payment received
      - To Cancelled: when user cancels

    From Confirmed:
      - To Shipped: when order dispatched
      - To Cancelled: when out of stock

    From Shipped:
      - To Delivered: when customer receives

    From Delivered:
      - (final state, no transitions)

    From Cancelled:
      - (final state, no transitions)

  Events:
    - payment_received
    - user_cancelled
    - order_dispatched
    - delivery_confirmed
    - out_of_stock

  Invariants:
    - Cannot transition from final state
    - Must record timestamp of each transition
    - Payment must be refunded if cancelled
```

**Compilador genera:**
- Enum para estados
- Funciones de transición
- Verificación de invariantes
- Event log

---

## Ventajas para Determinismo

### 1. **Contratos Explícitos**

```chronos
Function: process
  Requirements:
    - input must be non-null
    - buffer must have capacity >= input.length

  Guarantees:
    - result is same length as input
    - no allocation occurs
    - execution time <= O(n)
```

**Compilador puede verificar** en compile-time.

### 2. **Effects Declarados**

```chronos
Function: read_config
  Effects:
    - IO: reads from filesystem
    - Error: may fail if file not found

  Pure: no  # Explicit: this has side effects
```

```chronos
Function: add
  Effects: none
  Pure: yes  # Explicit: pure function
```

**Beneficio:** No side effects ocultos.

### 3. **Bounds Explícitos**

```chronos
Function: process_array
  Input:
    - data: array of integers (size 1..1000)

  Constraints:
    - array size must be known at compile time
    - no dynamic allocation

  WCET: 500 microseconds
```

**Verificable en compile-time.**

### 4. **Orden de Evaluación Declarado**

```chronos
Expression: total
  Compute:
    - First: calculate subtotal
    - Then: add tax to subtotal
    - Finally: round to 2 decimals

  Order: strict sequential (left-to-right)
```

**No ambigüedad.**

---

## Compilador como Asistente IA

### Approach 1: Template Expansion

```chronos
Function: divide
  Inputs: a, b (integers)
  Output: result (integer or error)
  Logic: ...

↓ [Compiler expands template]

fn divide(a: i32, b: i32) -> Result<i32, String> {
    if b == 0 {
        return Err("Division by zero");
    }
    Ok(a / b)
}
```

### Approach 2: Verification

```chronos
Function: process
  Requirements:
    - input >= 0
    - result < 1000

↓ [Compiler verifies]

✓ Requirements satisfiable
✓ No violation possible
✓ Generated code is safe
```

### Approach 3: Optimization

```chronos
Sort: data
  By: value
  Method: any deterministic

↓ [Compiler chooses]

Using merge sort (stable, deterministic, O(n log n))
```

### Approach 4: Correction

```chronos
Function: fibonacci
  Logic:
    When n is 0: return 0
    When n is 1: return 1
    # BUG: forgot recursive case

↓ [Compiler suggests]

⚠️  Warning: Not all inputs handled
💡 Suggestion: Add case for n > 1
```

---

## Syntax Comparison

### Traditional Code

```rust
struct User {
    id: i32,
    name: String,
    email: String,
}

impl User {
    fn new(id: i32, name: String, email: String) -> Result<User, String> {
        if name.is_empty() {
            return Err("Name cannot be empty".to_string());
        }
        if !email.contains('@') {
            return Err("Invalid email".to_string());
        }
        Ok(User { id, name, email })
    }
}
```

### Chronos Template-Based

```chronos
Data: User
  Fields:
    - id: integer
    - name: string (non-empty)
    - email: string (valid email format)

  Validation:
    - name must not be empty
    - email must contain @

  Constructor:
    Inputs: id, name, email
    Validates all fields
    Returns: User instance or validation error
```

**Mucho más claro QUÉ hace, menos CÓMO.**

---

## Interacción con IA

### Programa escribiendo programa

```bash
$ chronos ai "Create a function to validate email addresses"

Generated template:

Function: validate_email
  Purpose: Check if string is valid email format

  Input:
    - email: string

  Output:
    - valid: boolean

  Logic:
    Check if email:
      - Contains exactly one @
      - Has characters before @
      - Has domain after @
      - Domain has at least one dot

    Return true if all checks pass, false otherwise

Accept? (y/n/edit):
```

### Refinamiento iterativo

```bash
$ chronos ai "Add requirement: email must be max 254 chars"

Updated template:

Function: validate_email
  ...
  Constraints:
    - email length <= 254 characters  [NEW]
  ...

Apply? (y/n):
```

---

## Templates Predefinidos

### Template Categories

1. **Control Flow**
   - Function
   - Procedure
   - When/Otherwise (if)
   - Loop
   - ForEach
   - While
   - Match

2. **Data**
   - Data (struct)
   - Enum
   - Union
   - Array
   - Map

3. **Contracts**
   - Requires
   - Ensures
   - Invariant
   - Effect

4. **Concurrency**
   - Task
   - Channel
   - Atomic
   - Lock

5. **Real-Time**
   - RTTask (with WCET)
   - Interrupt
   - Timer

6. **System**
   - Service
   - Endpoint
   - StateMachine
   - Protocol

---

## Example: Complete Program

```chronos
Program: todo_app
  Description: Simple TODO list application

  Data: Todo
    Fields:
      - id: integer (unique)
      - title: string (required, max 100 chars)
      - completed: boolean (default: false)
      - created_at: timestamp (auto)

  Storage: todos
    Type: in-memory list
    Capacity: 1000 items

  Function: add_todo
    Purpose: Create a new TODO item

    Input:
      - title: string

    Output:
      - todo: Todo instance
      - or error if validation fails

    Logic:
      - Validate title is not empty
      - Generate unique ID
      - Create Todo with:
          - id: generated
          - title: from input
          - completed: false
          - created_at: now
      - Add to storage
      - Return created todo

    Requirements:
      - Title must not be empty
      - Storage must not be full

  Function: list_todos
    Purpose: Get all TODO items

    Output:
      - todos: list of Todo

    Logic:
      - Fetch all todos from storage
      - Sort by created_at descending
      - Return list

  Function: toggle_todo
    Purpose: Mark TODO as completed or incomplete

    Input:
      - id: integer

    Output:
      - success or error

    Logic:
      - Find todo with given id
      - When found:
          - Toggle completed status
          - Return success
      - When not found:
          - Return error "Todo not found"

  Main:
    Actions:
      - Add todo "Buy milk"
      - Add todo "Write code"
      - List all todos
      - Toggle first todo
      - List all todos
```

**Compilador genera:**
- Struct definitions
- Storage management
- All functions
- Main entry point
- Error handling
- Validation

---

## Ventajas del Approach

### 1. **Accesibilidad Extrema**

No necesitas conocer sintaxis compleja:
```chronos
Function: greet
  Input: name (string)
  Output: greeting (string)
  Logic:
    Return "Hello, " + name + "!"
```

Cualquiera entiende esto.

### 2. **Imposible Cometer Errores Sintácticos**

Las plantillas garantizan estructura válida.

### 3. **Verificación Automática**

```chronos
Function: process
  Requirements:
    - input > 0

  Logic:
    Return input * 2

  Ensures:
    - result > input  # Compiler verifies!
```

### 4. **Optimización Automática**

```chronos
Sort: data
  Method: any deterministic

# Compiler elige el mejor algoritmo para el contexto
```

### 5. **Determinismo Natural**

Las plantillas fuerzan:
- Contratos explícitos
- Effects declarados
- Bounds definidos
- Orden de evaluación claro

### 6. **Generación de Código Perfecta**

Una vez que el template es válido, el código generado es correcto.

### 7. **IDE Support Trivial**

```
Template: Function
         ↓
IDE muestra campos requeridos
     ↓
Autocompletado perfecto
     ↓
Validación en tiempo real
```

---

## Desafíos

### 1. **Expresividad**

¿Puede expresar algoritmos complejos?

```chronos
# ¿Cómo expresas quicksort?
Function: quicksort
  ...
  # Necesitas recursión, particionado, etc
```

**Solución:** Templates más sofisticados o fallback a código tradicional.

### 2. **Performance**

¿El código generado es eficiente?

**Solución:** Compilador inteligente, optimizaciones agresivas.

### 3. **Debugging**

¿Debuggeas el template o el código generado?

**Solución:** Ambos (mostrar mapeo template ↔ código).

### 4. **Learning Curve**

Aunque más simple, aún hay curva de aprendizaje.

**Solución:** Tutoriales, examples, AI assistant.

---

## Implementación

### Fase 1: Template Parser

```rust
struct Template {
    name: String,
    fields: HashMap<String, Value>,
}

fn parse_template(source: &str) -> Result<Template> {
    // YAML-like parsing
}
```

### Fase 2: Validator

```rust
fn validate(template: &Template) -> Result<()> {
    // Check required fields
    // Verify constraints
    // Type check
}
```

### Fase 3: Code Generator

```rust
fn generate(template: &Template) -> String {
    match template.name {
        "Function" => generate_function(template),
        "Data" => generate_struct(template),
        // ...
    }
}
```

### Fase 4: Compiler Integration

```
template.ch → Parser → Validator → Generator → AST → Compiler → Binary
```

---

## Ejemplo de Compilación

**Input (template.ch):**
```chronos
Function: add
  Inputs: a, b (integers)
  Output: sum (integer)
  Logic: Return a + b
```

**Generated (intermediate):**
```rust
fn add(a: i32, b: i32) -> i32 {
    a + b
}
```

**Compiled (assembly):**
```asm
add:
    movl %edi, %eax
    addl %esi, %eax
    ret
```

---

## Conclusión

**¿Es viable?**

✅ **Sí, 100%**

**Ventajas:**
- Extremadamente accesible
- Determinismo natural
- Verificación automática
- Sin errores sintácticos
- IA-friendly

**Desventajas:**
- Necesita diseño cuidadoso de templates
- Compilador más complejo
- Curva de aprendizaje (diferente)

---

## Próximos Pasos

1. **Diseñar templates core** (Function, Data, Control Flow)
2. **Implementar parser de templates**
3. **Code generator básico**
4. **Compilar "Hello World" desde template**
5. **Iterar y refinar**

---

**¿Esto es lo que tenías en mente?**

¿O estabas pensando en algo más específico con los prompts/templates?
