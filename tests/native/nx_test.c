/* nx_test: codifica con x64enc las instrucciones de un archivo .asm y escribe
 * los bytes, para compararlos con `nasm -f bin` (tests/native/run.sh).
 *
 *   nx_test casos.asm salida.bin        codifica todo el archivo
 *   nx_test casos.asm --at OFFSET       imprime la línea que cae en OFFSET
 *   nx_test --one "mov rax, rbx"        imprime los bytes en hexadecimal
 *
 * Las líneas `nombre equ N` definen constantes (se leen todas antes de
 * codificar, como una equ al final de la función). Termina con código 1 si una
 * línea no se puede codificar.
 */
#include "../../compiler/native/x64enc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXEQU 1024
static struct { char name[96]; int64_t v; } g_equ[MAXEQU];
static int g_nequ;

static int resolve(void* ctx, const char* sym, int64_t* v) {
    (void)ctx;
    for (int i = 0; i < g_nequ; i++)
        if (!strcmp(g_equ[i].name, sym)) { *v = g_equ[i].v; return 0; }
    return -1;
}

static void load_equ(FILE* fp) {
    char line[512];
    while (fgets(line, sizeof line, fp)) {
        char nm[96]; long long v;
        if (sscanf(line, "%95s equ %lld", nm, &v) == 2 && g_nequ < MAXEQU) {
            snprintf(g_equ[g_nequ].name, sizeof g_equ[g_nequ].name, "%s", nm);
            g_equ[g_nequ++].v = v;
        }
    }
    rewind(fp);
}

int main(int argc, char** argv) {
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "--one")) {
        NxInsn in; uint8_t b[16]; const char* why = NULL;
        if (argc == 4) {
            FILE* ef = fopen(argv[3], "r");
            if (ef) { load_equ(ef); fclose(ef); }
        }
        if (nx_parse_line(argv[2], &in, NULL, 0) != NX_L_INSN) { puts("no es una instruccion"); return 1; }
        int n = nx_encode(&in, b, resolve, NULL, &why);
        if (n < 0) { printf("no codificable: %s\n", why); return 1; }
        for (int i = 0; i < n; i++) printf("%02x", b[i]);
        putchar('\n');
        return 0;
    }
    if (argc < 3) { fprintf(stderr, "uso: nx_test casos.asm salida.bin | --at OFFSET\n"); return 2; }
    FILE* fp = fopen(argv[1], "r");
    if (!fp) { perror(argv[1]); return 2; }
    load_equ(fp);
    int at = argc == 4 && !strcmp(argv[2], "--at");
    long target = at ? atol(argv[3]) : -1;
    FILE* fo = at ? NULL : fopen(argv[2], "wb");
    if (!at && !fo) { perror(argv[2]); return 2; }
    char line[512];
    long off = 0, nenc = 0, nbad = 0, lineno = 0;
    while (fgets(line, sizeof line, fp)) {
        lineno++;
        line[strcspn(line, "\r\n")] = 0;
        NxInsn in; uint8_t b[16]; const char* why = NULL;
        if (nx_parse_line(line, &in, NULL, 0) != NX_L_INSN) continue;
        int n = nx_encode(&in, b, resolve, NULL, &why);
        if (n < 0) {
            fprintf(stderr, "%s:%ld: no codificable: %s  [%s]\n", argv[1], lineno, why, line);
            nbad++;
            continue;
        }
        if (at && target >= off && target < off + n) { printf("%ld: %s\n", lineno, line); return 0; }
        if (fo) fwrite(b, 1, (size_t)n, fo);
        off += n; nenc++;
    }
    if (fo) fclose(fo);
    fclose(fp);
    if (at) return 1;
    fprintf(stderr, "nx_test: %ld instrucciones codificadas, %ld rechazadas, %ld bytes\n", nenc, nbad, off);
    return nbad ? 1 : 0;
}
