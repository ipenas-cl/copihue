/* nxinv: inventario de instrucciones y formas de operandos de ensamblador
 * NASM generado por Copihue (modo hosted). Alimenta docs/DESIGN_NATIVE.md.
 *
 *   nxinv [--cases] archivo.asm ...
 *
 * Sin opciones imprime el inventario: totales, y por forma las veces que
 * aparece (total / en el runtime / en código de usuario), en cuántos archivos
 * y si x64enc la codifica. Con --cases imprime, sin repetir, cada instrucción
 * que x64enc codifica (más las líneas `equ` que usa) para el test contra nasm.
 */
#include "../../compiler/native/x64enc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXF 4096
#define MAXEQU 256

typedef struct {
    char form[96];
    char example[160];
    long total, runtime, user;
    int files;
    int lastfile;
    int covered;
    char why[80];
} Form;

static Form g_forms[MAXF];
static int g_nforms;

static struct { char name[64]; int64_t v; } g_equ[MAXEQU];
static int g_nequ;

static int resolve(void* ctx, const char* sym, int64_t* v) {
    (void)ctx;
    for (int i = 0; i < g_nequ; i++)
        if (!strcmp(g_equ[i].name, sym)) { *v = g_equ[i].v; return 0; }
    return -1;
}

static Form* form_get(const char* name) {
    for (int i = 0; i < g_nforms; i++)
        if (!strcmp(g_forms[i].form, name)) return &g_forms[i];
    if (g_nforms >= MAXF) { fprintf(stderr, "nxinv: demasiadas formas\n"); exit(2); }
    Form* f = &g_forms[g_nforms++];
    memset(f, 0, sizeof *f);
    snprintf(f->form, sizeof f->form, "%s", name);
    f->lastfile = -1;
    return f;
}

/* Líneas ya impresas por --cases. */
static char** g_seen; static int g_nseen, g_capseen;
static int seen_before(const char* s) {
    for (int i = 0; i < g_nseen; i++) if (!strcmp(g_seen[i], s)) return 1;
    if (g_nseen == g_capseen) {
        g_capseen = g_capseen ? g_capseen * 2 : 1024;
        g_seen = realloc(g_seen, (size_t)g_capseen * sizeof *g_seen);
    }
    g_seen[g_nseen++] = strdup(s);
    return 0;
}

static int cmp_forms(const void* a, const void* b) {
    const Form *x = a, *y = b;
    if (x->total != y->total) return x->total < y->total ? 1 : -1;
    return strcmp(x->form, y->form);
}

int main(int argc, char** argv) {
    int cases = 0, first = 1;
    if (argc > 1 && !strcmp(argv[1], "--cases")) { cases = 1; first = 2; }
    long n_insn = 0, n_rt = 0, n_user = 0, n_cov = 0, n_cov_rt = 0, n_cov_user = 0;
    long n_dir = 0, n_label = 0, n_bad = 0, n_empty = 0;
    int files_total = 0, files_full = 0;
    char badsamples[8][160]; int nbad_s = 0;

    for (int fi = first; fi < argc; fi++) {
        FILE* fp = fopen(argv[fi], "r");
        if (!fp) { perror(argv[fi]); return 2; }
        files_total++;
        /* primera pasada: constantes equ del archivo */
        g_nequ = 0;
        char line[1024];
        while (fgets(line, sizeof line, fp)) {
            char nm[64]; long long v;
            if (sscanf(line, "%63s equ %lld", nm, &v) == 2 && g_nequ < MAXEQU) {
                snprintf(g_equ[g_nequ].name, sizeof g_equ[g_nequ].name, "%s", nm);
                g_equ[g_nequ++].v = v;
            }
        }
        rewind(fp);
        int in_runtime = 1;          /* _start y todo `__x` es runtime */
        int file_uncovered = 0;
        while (fgets(line, sizeof line, fp)) {
            line[strcspn(line, "\r\n")] = 0;
            NxInsn ins; char lab[128];
            int k = nx_parse_line(line, &ins, lab, sizeof lab);
            if (k == NX_L_EMPTY) { n_empty++; continue; }
            if (k == NX_L_DIRECTIVE) { n_dir++; continue; }
            if (k == NX_L_LABEL || (k == NX_L_INSN && lab[0])) {
                n_label++;
                if (lab[0] != '.') in_runtime = !strncmp(lab, "__", 2) || !strcmp(lab, "_start");
                if (k == NX_L_LABEL) continue;
            }
            if (k == NX_L_BAD) {
                n_bad++;
                if (nbad_s < 8) snprintf(badsamples[nbad_s++], sizeof badsamples[0], "%s", line);
                continue;
            }
            char fname[96];
            nx_form(&ins, fname, sizeof fname);
            Form* f = form_get(fname);
            uint8_t code[16];
            const char* why = NULL;
            int len = nx_encode(&ins, code, resolve, NULL, &why);
            n_insn++;
            f->total++;
            if (in_runtime) { f->runtime++; n_rt++; } else { f->user++; n_user++; }
            if (!f->example[0]) {
                char* s = line; while (*s == ' ') s++;
                snprintf(f->example, sizeof f->example, "%s", s);
            }
            if (f->lastfile != fi) { f->lastfile = fi; f->files++; }
            if (len > 0) {
                f->covered = 1; n_cov++;
                if (in_runtime) n_cov_rt++; else n_cov_user++;
                if (cases) {
                    char* s = line; while (*s == ' ') s++;
                    char* sc = strchr(s, ';'); if (sc) *sc = 0;
                    while (*s && s[strlen(s) - 1] == ' ') s[strlen(s) - 1] = 0;
                    /* Una constante equ cambia de valor de un programa a otro
                     * (__frame_main). Para que nasm vea cada valor, el símbolo
                     * lleva el valor en el nombre: __frame_main -> __frame_main_V64. */
                    char text[256];
                    snprintf(text, sizeof text, "%s", s);
                    for (int i = 0; i < ins.nops; i++)
                        if (ins.op[i].sym[0]) {
                            int64_t v;
                            if (resolve(NULL, ins.op[i].sym, &v) != 0) continue;
                            char nn[96], eq[160], rep[256];
                            snprintf(nn, sizeof nn, "%s_V%lld", ins.op[i].sym, (long long)v);
                            snprintf(eq, sizeof eq, "%s equ %lld", nn, (long long)v);
                            char* at = strstr(text, ins.op[i].sym);
                            if (at) {
                                snprintf(rep, sizeof rep, "%.*s%s%s", (int)(at - text), text, nn, at + strlen(ins.op[i].sym));
                                snprintf(text, sizeof text, "%s", rep);
                            }
                            if (!seen_before(eq)) printf("%s\n", eq);
                        }
                    if (!seen_before(text)) printf("%s\n", text);
                }
            } else {
                file_uncovered = 1;
                if (!f->why[0] && why) snprintf(f->why, sizeof f->why, "%s", why);
            }
        }
        fclose(fp);
        if (!file_uncovered) files_full++;
    }
    if (cases) return 0;

    qsort(g_forms, (size_t)g_nforms, sizeof g_forms[0], cmp_forms);
    printf("archivos: %d\n", files_total);
    printf("lineas: instrucciones=%ld directivas=%ld etiquetas=%ld vacias=%ld ilegibles=%ld\n",
           n_insn, n_dir, n_label, n_empty, n_bad);
    printf("instrucciones: total=%ld runtime=%ld usuario=%ld\n", n_insn, n_rt, n_user);
    int cf = 0; for (int i = 0; i < g_nforms; i++) cf += g_forms[i].covered;
    printf("formas distintas: %d (codificadas: %d)\n", g_nforms, cf);
    if (n_insn) {
        printf("cobertura por ocurrencia: %.2f%% (%ld de %ld)\n", 100.0 * n_cov / n_insn, n_cov, n_insn);
        if (n_rt) printf("  runtime: %.2f%% (%ld de %ld)\n", 100.0 * n_cov_rt / n_rt, n_cov_rt, n_rt);
        if (n_user) printf("  usuario: %.2f%% (%ld de %ld)\n", 100.0 * n_cov_user / n_user, n_cov_user, n_user);
    }
    if (g_nforms) printf("cobertura por forma distinta: %.2f%% (%d de %d)\n", 100.0 * cf / g_nforms, cf, g_nforms);
    printf("archivos que se ensamblarian enteros: %d de %d\n", files_full, files_total);
    for (int i = 0; i < nbad_s; i++) printf("ilegible: %s\n", badsamples[i]);
    /* totales por mnemónico (con prefijo lock/rep si lo hay) */
    {
        static struct { char m[32]; long total, covered; int forms; } mn[256];
        int nm = 0;
        for (int i = 0; i < g_nforms; i++) {
            char key[32];
            const char* sp = strchr(g_forms[i].form, ' ');
            size_t len = sp ? (size_t)(sp - g_forms[i].form) : strlen(g_forms[i].form);
            if (sp && (!strncmp(g_forms[i].form, "lock", len) || !strncmp(g_forms[i].form, "rep", len))) {
                const char* sp2 = strchr(sp + 1, ' ');
                len = sp2 ? (size_t)(sp2 - g_forms[i].form) : strlen(g_forms[i].form);
            }
            snprintf(key, sizeof key, "%.*s", (int)len, g_forms[i].form);
            int k = 0;
            while (k < nm && strcmp(mn[k].m, key)) k++;
            if (k == nm && nm < 256) { snprintf(mn[nm].m, sizeof mn[nm].m, "%s", key); nm++; }
            mn[k].total += g_forms[i].total;
            mn[k].forms++;
            if (g_forms[i].covered) mn[k].covered += g_forms[i].total;
        }
        for (int i = 0; i < nm; i++)          /* orden por total */
            for (int j = i + 1; j < nm; j++)
                if (mn[j].total > mn[i].total) { __typeof__(mn[0]) t = mn[i]; mn[i] = mn[j]; mn[j] = t; }
        printf("\nmnemonicos: %d\n%9s %7s %9s  %s\n", nm, "total", "formas", "cubiertas", "mnemonico");
        for (int i = 0; i < nm; i++) printf("%9ld %7d %9ld  %s\n", mn[i].total, mn[i].forms, mn[i].covered, mn[i].m);
    }
    printf("\n%9s %9s %9s %5s  %-3s %-34s %s\n", "total", "runtime", "usuario", "arch", "cod", "forma", "ejemplo / motivo");
    for (int i = 0; i < g_nforms; i++) {
        Form* f = &g_forms[i];
        printf("%9ld %9ld %9ld %5d  %-3s %-34s %s%s%s\n", f->total, f->runtime, f->user, f->files,
               f->covered ? "si" : "no", f->form, f->example, f->covered ? "" : "   <- ", f->covered ? "" : f->why);
    }
    return 0;
}
