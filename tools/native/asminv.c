// Inventory of the NASM text the code generator emits.
//
//   asminv [--top N] [--list-uncovered] FILE.asm ...
//
// Counts every line of the given files by kind (blank, comment, label,
// directive, instruction), every mnemonic, every instruction shape
// ("mov r64,m64") and every memory-operand form, and says which shapes
// compiler/native/x64.c encodes. "Encodes" means x64_encode returned bytes;
// whether those bytes equal nasm's is what tests/x64 checks. Output is Markdown.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compiler/native/x64.h"

#define MAXN 8192

typedef struct { char key[96]; long n; int covered; char why[64]; } Cnt;
static Cnt shapes[MAXN]; static int nshapes;
static Cnt mnems[MAXN];  static int nmnems;
static Cnt dirs[256];    static int ndirs;
static Cnt mforms[256];  static int nmforms;
static Cnt unparsed[MAXN]; static int nunparsed;

static long n_lines, n_blank, n_comment, n_label, n_dir, n_insn, n_unparsed;
static long n_covered, n_branch_sym, n_branch_sym_cov;

static Cnt* bump(Cnt* t, int* n, int max, const char* key) {
    for (int i = 0; i < *n; i++) if (!strcmp(t[i].key, key)) { t[i].n++; return &t[i]; }
    if (*n >= max) return NULL;
    snprintf(t[*n].key, sizeof t[*n].key, "%s", key);
    t[*n].n = 1;
    return &t[(*n)++];
}

// equ constants of the file being read
static struct { char name[128]; int64_t v; } eq[4096]; static int neq;
static int const_lookup(const char* name, int64_t* v, void* ctx) {
    (void)ctx;
    for (int i = 0; i < neq; i++) if (!strcmp(eq[i].name, name)) { *v = eq[i].v; return 1; }
    return 0;
}

static int cmp_cnt(const void* a, const void* b) {
    const Cnt* x = a; const Cnt* y = b;
    if (y->n != x->n) return y->n > x->n ? 1 : -1;
    return strcmp(x->key, y->key);
}

static void memform(const XOp* o, char* out, size_t n) {
    char t[64] = "[";
    if (o->rip) strcat(t, "rip+");
    if (o->has_sym) strcat(t, "sym");
    if (o->base >= 0) {
        if (t[1]) strcat(t, "+");
        strcat(t, (o->base == 4 || o->base == 12) ? "base(sp/r12)" : (o->base == 5 || o->base == 13) ? "base(bp/r13)" : "base");
    }
    if (o->index >= 0) { strcat(t, "+index*"); char s[4]; snprintf(s, sizeof s, "%d", o->scale); strcat(t, s); }
    if (o->has_sym) { if (o->disp) strcat(t, "+addend"); }
    else if (o->base < 0 && o->index < 0) strcat(t, "abs");
    else if (o->disp) strcat(t, (o->disp >= -128 && o->disp <= 127) ? "+disp8" : "+disp32");
    strcat(t, "]");
    snprintf(out, n, "%s", t);
}

static void process_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { perror(path); exit(1); }
    char line[1024];
    neq = 0;
    while (fgets(line, sizeof line, f)) {
        XLine xl; x64_classify(line, &xl);
        if (xl.kind == XL_EQU && neq < 4096) {
            snprintf(eq[neq].name, sizeof eq[neq].name, "%s", xl.name); eq[neq].v = xl.val; neq++;
        }
    }
    rewind(f);
    x64_set_consts(const_lookup, NULL);
    while (fgets(line, sizeof line, f)) {
        XLine xl; x64_classify(line, &xl);
        n_lines++;
        if (xl.has_label) n_label++;
        switch (xl.kind) {
        case XL_BLANK: n_blank++; continue;
        case XL_COMMENT: n_comment++; continue;
        case XL_LABEL: continue;
        case XL_EQU: case XL_DIRECTIVE: n_dir++; bump(dirs, &ndirs, 256, xl.dir); continue;
        default: break;
        }
        n_insn++;
        XInsn in; char err[128];
        if (x64_parse(xl.text, &in, err, sizeof err) != 0) {
            n_unparsed++;
            char k[160]; snprintf(k, sizeof k, "%.60s  -- %.30s", xl.text, err);
            bump(unparsed, &nunparsed, MAXN, k);
            continue;
        }
        char key[96];
        x64_shape(&in, key, sizeof key);
        XEnc e; const char* why = "";
        int len = x64_encode(&in, &e, &why);
        Cnt* c = bump(shapes, &nshapes, MAXN, key);
        if (c) { c->covered = len > 0; if (!len) snprintf(c->why, sizeof c->why, "%s", why); }
        if (len > 0) n_covered++;
        char mn[48]; snprintf(mn, sizeof mn, "%s%s%s", in.lock ? "lock " : "", in.rep ? "rep " : "", in.mnem);
        bump(mnems, &nmnems, MAXN, mn);
        if (in.nops == 1 && in.op[0].kind == X_IMM && in.op[0].has_sym &&
            (in.mnem[0] == 'j' || !strcmp(in.mnem, "call"))) {
            n_branch_sym++;
            if (len > 0) n_branch_sym_cov++;
        }
        for (int k = 0; k < in.nops; k++) if (in.op[k].kind == X_MEM) {
            char mf[64]; memform(&in.op[k], mf, sizeof mf);
            bump(mforms, &nmforms, 256, mf);
        }
    }
    fclose(f);
}

int main(int argc, char** argv) {
    int top = 60, list_unc = 0, nfiles = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--top") && i + 1 < argc) top = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--list-uncovered")) list_unc = 1;
        else { process_file(argv[i]); nfiles++; }
    }
    if (!nfiles) { fprintf(stderr, "usage: asminv [--top N] [--list-uncovered] FILE.asm ...\n"); return 2; }

    printf("Files: %d. Lines: %ld (blank %ld, comment %ld, label %ld, directive %ld, instruction %ld).\n\n",
           nfiles, n_lines, n_blank, n_comment, n_label, n_dir, n_insn);
    printf("Instructions the parser could not read: %ld.\n", n_unparsed);
    printf("Instructions the encoder returns bytes for: %ld of %ld (%.2f%%).\n\n", n_covered, n_insn,
           n_insn ? 100.0 * (double)n_covered / (double)n_insn : 0.0);
    printf("Branches and calls to a symbol: %ld, encoded %ld.\n\n", n_branch_sym, n_branch_sym_cov);

    qsort(dirs, (size_t)ndirs, sizeof dirs[0], cmp_cnt);
    printf("### Directives\n\n| Directive | Count |\n|---|---:|\n");
    for (int i = 0; i < ndirs; i++) printf("| `%s` | %ld |\n", dirs[i].key, dirs[i].n);

    qsort(mnems, (size_t)nmnems, sizeof mnems[0], cmp_cnt);
    printf("\n### Mnemonics (%d distinct)\n\n| Mnemonic | Count | Share | Cumulative |\n|---|---:|---:|---:|\n", nmnems);
    long cum = 0;
    for (int i = 0; i < nmnems; i++) {
        cum += mnems[i].n;
        printf("| `%s` | %ld | %.2f%% | %.2f%% |\n", mnems[i].key, mnems[i].n,
               100.0 * (double)mnems[i].n / (double)n_insn, 100.0 * (double)cum / (double)n_insn);
    }

    qsort(shapes, (size_t)nshapes, sizeof shapes[0], cmp_cnt);
    printf("\n### Shapes (%d distinct), most frequent %d\n\n| Shape | Count | Share | Cumulative | Encoder |\n|---|---:|---:|---:|---|\n",
           nshapes, top);
    cum = 0;
    for (int i = 0; i < nshapes; i++) {
        cum += shapes[i].n;
        if (i < top)
            printf("| `%s` | %ld | %.2f%% | %.2f%% | %s |\n", shapes[i].key, shapes[i].n,
                   100.0 * (double)shapes[i].n / (double)n_insn, 100.0 * (double)cum / (double)n_insn,
                   shapes[i].covered ? "yes" : "no");
    }
    if (list_unc) {
        printf("\n### Shapes the encoder does not cover\n\n| Shape | Count | Reason |\n|---|---:|---|\n");
        for (int i = 0; i < nshapes; i++)
            if (!shapes[i].covered) printf("| `%s` | %ld | %s |\n", shapes[i].key, shapes[i].n, shapes[i].why);
    }

    qsort(mforms, (size_t)nmforms, sizeof mforms[0], cmp_cnt);
    long nm = 0; for (int i = 0; i < nmforms; i++) nm += mforms[i].n;
    printf("\n### Memory operands (%ld)\n\n| Form | Count | Share |\n|---|---:|---:|\n", nm);
    for (int i = 0; i < nmforms; i++)
        printf("| `%s` | %ld | %.2f%% |\n", mforms[i].key, mforms[i].n, 100.0 * (double)mforms[i].n / (double)nm);

    if (nunparsed) {
        qsort(unparsed, (size_t)nunparsed, sizeof unparsed[0], cmp_cnt);
        printf("\n### Unparsed lines\n\n");
        for (int i = 0; i < nunparsed && i < 40; i++) printf("- %ld x `%s`\n", unparsed[i].n, unparsed[i].key);
    }
    return 0;
}
