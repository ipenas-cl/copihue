// Inventory of the data in the NASM text the code generator emits (M3).
//
//   datainv FILE.asm ...
//
// Counts, per section, every data directive by form ("db int", "dq sym",
// "align N, resb"...), the labels of each data section, and every instruction
// operand that names a label of .data/.rodata/.bss, by instruction shape and
// the section it points to. It reads the text with x64_classify / x64_parse,
// independent of compiler/native/asm.c. Whether asm.c accepts a form is what
// tests/x64/run_asm.sh checks, program by program, against nasm. Output is Markdown.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compiler/native/x64.h"

#define MAXN 4096
#define LINE 262144

typedef struct { char key[128]; long n; } Cnt;
static Cnt forms[MAXN];  static int nforms;
static Cnt opnds[MAXN];  static int nopnds;
static long secs[4], labels[4], lines_total, dir_total, eq_total, unknown_dir;

static const char* const SEC_NAME[4] = {".text", ".data", ".rodata", ".bss"};

static void bump(Cnt* t, int* n, const char* key) {
    for (int i = 0; i < *n; i++) if (!strcmp(t[i].key, key)) { t[i].n++; return; }
    if (*n >= MAXN) return;
    snprintf(t[*n].key, sizeof t[*n].key, "%s", key);
    t[*n].n = 1;
    (*n)++;
}

static int cmp_cnt(const void* a, const void* b) {
    const Cnt* x = a; const Cnt* y = b;
    if (y->n != x->n) return y->n > x->n ? 1 : -1;
    return strcmp(x->key, y->key);
}

static int sec_index(const char* s) {
    for (int i = 0; i < 4; i++) if (!strcmp(s, SEC_NAME[i])) return i;
    return -1;
}

// label -> section (0 text, 1 data, 2 rodata, 3 bss) for the file being read
typedef struct { char name[128]; int sec; } Lab;
static Lab* lab; static size_t nlab, caplab;
static int* hash_tab; static size_t hash_n;

static unsigned hash_of(const char* s) {
    unsigned h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static void lab_reset(void) {
    nlab = 0;
    if (!hash_tab) { hash_n = 1 << 16; hash_tab = malloc(hash_n * sizeof *hash_tab); }
    for (size_t i = 0; i < hash_n; i++) hash_tab[i] = -1;
}

static void lab_add(const char* name, int sec) {
    if (nlab == caplab) { caplab = caplab ? caplab * 2 : 4096; lab = realloc(lab, caplab * sizeof *lab); }
    snprintf(lab[nlab].name, sizeof lab[nlab].name, "%s", name);
    lab[nlab].sec = sec;
    size_t h = hash_of(name) & (hash_n - 1);
    while (hash_tab[h] >= 0) h = (h + 1) & (hash_n - 1);
    hash_tab[h] = (int)nlab++;
}

static int lab_sec(const char* name) {
    size_t h = hash_of(name) & (hash_n - 1);
    while (hash_tab[h] >= 0) {
        if (!strcmp(lab[hash_tab[h]].name, name)) return lab[hash_tab[h]].sec;
        h = (h + 1) & (hash_n - 1);
    }
    return -1;
}

static void qualify(const char* parent, const char* name, char* out, size_t n) {
    if (name[0] == '.') snprintf(out, n, "%s%s", parent, name);
    else snprintf(out, n, "%s", name);
}

static char* trim(char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char* e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

// The text after the directive word, without comment.
static char* args_of(char* raw, const XLine* x) {
    char* p = raw;
    if (x->has_label) { char* q = strchr(p, ':'); if (q) p = q + 1; }
    while (isspace((unsigned char)*p)) p++;
    while (*p && !isspace((unsigned char)*p)) p++;
    char* sc = strchr(p, ';');
    if (sc) *sc = 0;
    return trim(p);
}

// "int", "sym", "sym+N" for one value of db/dw/dd/dq.
static const char* value_kind(const char* v) {
    if (isdigit((unsigned char)*v) || *v == '-') return "int";
    if (strpbrk(v, "+-")) return "sym+N";
    return "sym";
}

static void data_form(int sec, const XLine* x, char* args) {
    char key[128];
    const char* d = x->dir;
    if (!strcmp(d, "db") || !strcmp(d, "dw") || !strcmp(d, "dd") || !strcmp(d, "dq")) {
        int n = 0, ints = 0, syms = 0;
        for (char* a = args; a; ) {
            char* c = strchr(a, ',');
            if (c) *c = 0;
            char* v = trim(a);
            n++;
            if (!strcmp(value_kind(v), "int")) ints++; else syms++;
            a = c ? c + 1 : NULL;
        }
        snprintf(key, sizeof key, "%s %s %s%s", SEC_NAME[sec], d, syms ? (ints ? "mixed" : "sym") : "int", n > 1 ? " list" : "");
    } else if (!strncmp(d, "res", 3)) {
        snprintf(key, sizeof key, "%s %s N", SEC_NAME[sec], d);
    } else if (!strcmp(d, "align")) {
        char* c = strchr(args, ',');
        if (!c) snprintf(key, sizeof key, "%s align N", SEC_NAME[sec]);
        else snprintf(key, sizeof key, "%s align N, %.4s", SEC_NAME[sec], trim(c + 1));
    } else {
        snprintf(key, sizeof key, "%s %s", SEC_NAME[sec], d);
        if (strcmp(d, "global") && strcmp(d, "equ")) unknown_dir++;
    }
    bump(forms, &nforms, key);
}

static void read_file(const char* path, char* line) {
    FILE* f = fopen(path, "r");
    if (!f) { perror(path); exit(1); }
    lab_reset();
    char parent[128] = "";
    int sec = 0;
    // pass 1: labels and their sections
    while (fgets(line, LINE, f)) {
        XLine x; x64_classify(line, &x);
        if (x.kind == XL_DIRECTIVE && !strncmp(x.dir, "section ", 8)) { int s = sec_index(x.dir + 8); if (s >= 0) sec = s; }
        if (!x.has_label) continue;
        char full[256];
        qualify(parent, x.label, full, sizeof full);
        if (x.label[0] != '.') snprintf(parent, sizeof parent, "%s", x.label);
        lab_add(full, sec);
        labels[sec]++;
    }
    rewind(f);
    parent[0] = 0; sec = 0;
    while (fgets(line, LINE, f)) {
        char* raw = strdup(line);
        XLine x; x64_classify(line, &x);
        lines_total++;
        if (x.has_label && x.label[0] != '.') snprintf(parent, sizeof parent, "%s", x.label);
        if (x.kind == XL_EQU) { eq_total++; free(raw); continue; }
        if (x.kind == XL_DIRECTIVE) {
            dir_total++;
            if (!strncmp(x.dir, "section ", 8)) { int s = sec_index(x.dir + 8); if (s >= 0) sec = s; secs[sec]++; free(raw); continue; }
            data_form(sec, &x, args_of(raw, &x));
        } else if (x.kind == XL_INSN) {
            XInsn in; char err[96];
            if (!x64_parse(x.text, &in, err, sizeof err)) {
                for (int k = 0; k < in.nops; k++) {
                    const XOp* o = &in.op[k];
                    if (!o->has_sym) continue;
                    char full[256];
                    qualify(parent, o->sym, full, sizeof full);
                    int ts = lab_sec(full);
                    if (ts <= 0) continue;
                    char shape[96], key[160];
                    x64_shape(&in, shape, sizeof shape);
                    snprintf(key, sizeof key, "%-24s -> %s", shape, SEC_NAME[ts]);
                    bump(opnds, &nopnds, key);
                }
            }
        }
        free(raw);
    }
    fclose(f);
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: datainv FILE.asm ...\n"); return 2; }
    char* line = malloc(LINE);
    for (int i = 1; i < argc; i++) read_file(argv[i], line);
    free(line);
    printf("Files: %d. Lines: %ld. Directives: %ld (`equ`: %ld). Directives that are not `section`, `global`, `equ` nor a data directive: %ld.\n\n",
           argc - 1, lines_total, dir_total, eq_total, unknown_dir);
    printf("| section | `section` lines | labels |\n|---|---|---|\n");
    for (int i = 0; i < 4; i++) printf("| %s | %ld | %ld |\n", SEC_NAME[i], secs[i], labels[i]);
    qsort(forms, (size_t)nforms, sizeof forms[0], cmp_cnt);
    printf("\nData directive forms (%d):\n\n| form | lines |\n|---|---|\n", nforms);
    for (int i = 0; i < nforms; i++) printf("| `%s` | %ld |\n", forms[i].key, forms[i].n);
    qsort(opnds, (size_t)nopnds, sizeof opnds[0], cmp_cnt);
    long tot = 0;
    for (int i = 0; i < nopnds; i++) tot += opnds[i].n;
    printf("\nInstruction operands that name a data label (%d shapes, %ld occurrences):\n\n| shape -> section | occurrences |\n|---|---|\n", nopnds, tot);
    for (int i = 0; i < nopnds; i++) printf("| `%s` | %ld |\n", opnds[i].key, opnds[i].n);
    return 0;
}
