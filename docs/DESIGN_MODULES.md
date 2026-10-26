# Módulos y enlace (v0.33.0)

Un archivo sin `module` es el espacio plano de hoy. `import "path.lap";` de un archivo sin `module` sigue metiendo sus nombres en ese espacio, sin visibilidad. Los tests de `import`, el duplicado y `-c`/`extern` no cambian. `module` y `pub` son opt-in.

## Sintaxis

```copihue
module mem;

import "other.lap";

pub fn alloc(n: i64) -> i64 { return n; }
fn hidden() -> i64 { return 1; }
pub let base: i64 = 1;
pub struct Cell { n: i64 }

fn use_other() -> i64 { return other::value() + alloc(base); }
```

- `module nombre;` es la primera declaración del archivo. Un ítem antes es `module must be the first declaration`. Un segundo `module` en el mismo archivo es `a file has one module`.
- `import`, `pub`, `module` y `::` son contextuales, no palabras clave. `module` solo abre un módulo cuando el siguiente token es un identificador y después hay `;`.
- `pub` va delante de `fn`, `let`, `struct`, `data`, `extern`, `packed`, `interrupt`, `syscall`, `bits`, `naked` o `align`. Fuera de un módulo es `pub is only valid inside a module` y el ítem queda en el espacio plano, visible como hoy.
- Dentro de un módulo lo que no es `pub` es privado.
- El calificador es `::` (`mem::alloc`, `mem::base`, `mem::Cell`). `.` sigue siendo el campo. `:` sigue siendo el de tipos y de campos.

## Nombres

- El nombre del módulo es un identificador, único en una compilación. El segundo archivo con el mismo `module` es `module 'mem' is already defined at file:line:col`.
- Un archivo es un módulo. No se parte un módulo en varios archivos de la misma compilación.
- Dentro del módulo, un nombre sin calificar ve solo los ítems de ese módulo (privados y `pub`) y los builtins (`print_int`, `sizeof`, …). No ve el espacio plano ni otros módulos.
- Fuera, `Mod::item` si el módulo se cargó (por `import` o porque el archivo va en la línea de comandos) y el ítem es `pub`. Cargar un módulo lo deja visible en toda la compilación; `import` no reexporta y no copia nombres. Un ítem privado es `item 'hidden' of module 'mem' is private`. Un módulo que no existe es `unknown module 'mem'`. Un ítem que no existe es `module 'mem' has no item 'name'`.
- El mismo nombre corto puede existir en dos módulos y en el espacio plano. El símbolo de enlace de `mod::item` es `mod__item`. El del espacio plano es el nombre tal cual (`add`, `main`, `_start`).
- `fn main` y `fn _start` son el entry solo en el espacio plano. `pub fn main` dentro de un módulo es `mod__main`, no el entry.
- Dos declaraciones con el mismo símbolo de enlace chocan: `link symbol 'mem__add' is already used`. `__syscall_rsp` y `__syscall_user_rsp` no se renombran: son del compilador.
- Los structs de un módulo se registran con el mismo símbolo (`mem__Cell`). `let x: Cell` dentro de `mem` y `let y: mem::Cell` fuera nombran ese registro. `sizeof` y `offsetof` igual.

## Una compilación, un objeto

Una invocación de `copihue`, con todos los `import` y todos los archivos de la línea de comandos, sigue siendo un solo ensamblado: un ELF, o un objeto con `-c`. `pub` no es visibilidad ELF. Una función o un global definidos, privados o no, salen como `global` de NASM. El nombre es `mod__item` o el nombre plano.

`--freestanding`, `--flat` y `--boot` aceptan `module`, `pub` e `import` con las mismas reglas. `import` dentro de `section` sigue prohibido.

## `extern` que ya existe

`extern nombre;` y `extern fn` siguen significando «este símbolo lo define otro objeto». Sin `-c`, `extern fn` sigue siendo un error. En el espacio plano el símbolo es el nombre escrito: `extern fn add` es `add`, `extern bias` es `bias`. Los objetos de `tests/link` no cambian.

Dentro de `module mem`, `extern fn alloc` y `extern slot` son `mem__alloc` y `mem__slot`. `pub` en ese `extern` solo decide si otra parte de la misma compilación puede escribir `mem::alloc`. No cambia el `extern` de NASM. Para llamar a `mem::alloc` definido en otro `-c`, el objeto que llama declara el mismo `module mem` y el `extern fn alloc`, y `ld` une los dos objetos. Un archivo con `module` no puede contener también el `fn main` plano: `main` va en otro archivo de esa misma invocación `-c`, o en el espacio plano de otro objeto usando el símbolo `mem__alloc`.

## Errores

Si el archivo ya declaró `module`, el diagnóstico es `modulo:archivo:linea:col: error: mensaje`, con la línea y el caret. Sin módulo sigue `archivo:linea:col: error: mensaje`. El lexer corre antes de ver `module`, así que un literal sin cerrar no lleva el nombre del módulo.

El ciclo se detecta por la ruta, como hoy. Sin módulo el texto no cambia (`import of 'path' is a cycle`). Con módulo, el prefijo lleva el módulo del importador y el mensaje nombra el módulo del archivo que ya está en la pila: `import of 'path' (module a) is a cycle`.
