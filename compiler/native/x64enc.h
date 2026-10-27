/* x64enc: codificador x86-64 directo para el subconjunto de NASM que emite
 * el generador hosted de Copihue. Ver docs/DESIGN_NATIVE.md.
 *
 * No lo usa el compilador por defecto: es la primera porción del paso 1
 * (escribir el ELF sin nasm ni ld). Lo ejercitan tools/native/asminv y
 * tests/native.
 */
#ifndef X64ENC_H
#define X64ENC_H

#include <stddef.h>
#include <stdint.h>

enum { NX_NONE = 0, NX_REG, NX_IMM, NX_MEM, NX_SYM };
enum { NX_GPR = 0, NX_XMM = 1 };

typedef struct {
    int kind;            /* NX_* */
    int rclass;          /* NX_GPR o NX_XMM (solo NX_REG) */
    int reg;             /* 0..15 */
    int size;            /* bits de registro (8/16/32/64/128) o de memoria
                            declarada (0 = no declarada) */
    int high8;           /* ah/ch/dh/bh */
    int base, index;     /* -1 = ninguno; para memoria */
    int scale;           /* 1, 2, 4, 8 */
    int64_t val;         /* inmediato o desplazamiento constante */
    char sym[64];        /* símbolo ("" = ninguno) */
    int symneg;          /* el símbolo entra restando */
} NxOperand;

typedef struct {
    char prefix[8];      /* "", "lock", "rep" */
    char mnem[16];
    int nops;
    NxOperand op[3];
} NxInsn;

/* Tipo de línea de ensamblador. */
enum { NX_L_EMPTY, NX_L_LABEL, NX_L_INSN, NX_L_DIRECTIVE, NX_L_BAD };

/* Analiza una línea de texto NASM. Rellena `ins` si es una instrucción.
 * `name` recibe la etiqueta (NX_L_LABEL, o línea "etiqueta: instr") o la
 * directiva. Devuelve NX_L_*. Una etiqueta seguida de instrucción devuelve
 * NX_L_INSN y deja la etiqueta en `label` si no es NULL. */
int nx_parse_line(const char* line, NxInsn* ins, char* label, size_t labelsz);

/* Resolución de símbolos constantes (equ). Devuelve 0 si lo conoce. */
typedef int (*NxResolve)(void* ctx, const char* sym, int64_t* value);

/* Codifica `ins`. Escribe hasta 15 bytes en `out`. Devuelve su longitud, o
 * -1 si la forma no está en el subconjunto implementado (`why` recibe el
 * motivo, si no es NULL). Una instrucción con símbolo solo se acepta si
 * `resolve` lo resuelve a una constante. */
int nx_encode(const NxInsn* ins, uint8_t* out, NxResolve resolve, void* ctx,
              const char** why);

/* Nombre estable de la forma, p. ej. "mov r64,m64[b+d]". Es la clave del
 * inventario. */
void nx_form(const NxInsn* ins, char* buf, size_t sz);

#endif
