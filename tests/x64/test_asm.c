// Whole-program comparison of compiler/native/asm.c against `nasm -f elf64`.
//
//   test_asm WORKDIR files FILE.asm ...   each file is one program
//   test_asm WORKDIR cases FILE.asm       blocks separated by a line `;---`, each one a program
//   test_asm WORKDIR reject FILE.asm      blocks separated by `;---`; asm.c must refuse every one
//
// For each program nasm assembles the same text and what asm.c returns must
// equal what its object holds: the bytes of .text, .data and .rodata, the size
// of .bss, and the relocations of .text, .data and .rodata (offset, type,
// symbol, addend). The alignment nasm gives each section is compared too, but
// a difference there only counts and is printed (not checked against nasm 2.16).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compiler/native/asm.h"
#include "nobj.h"

static int nasm_major;

static void detect_nasm(void) {
    FILE* p = popen("nasm -v 2>&1", "r");
    char l[256] = "";
    if (p) { if (!fgets(l, sizeof l, p)) l[0] = 0; pclose(p); }
    const char* v = strstr(l, "version ");
    nasm_major = v ? atoi(v + 8) : 0;
    printf("nasm: %s", l[0] ? l : "(not found)\n");
}

// NASM 3 shortens `movsxd rax, eax` to cdqe; the encoder follows NASM 2.x (the CI's).
// For nasm >= 3 each register-register movsxd is handed to nasm as its 2.x bytes.
static void for_nasm(const char* text, char* out, size_t outsz) {
    size_t o = 0;
    const char* p = text;
    while (*p && o + 80 < outsz) {
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        static char line[65536];
        snprintf(line, sizeof line, "%.*s", (int)(len < sizeof line - 1 ? len : sizeof line - 1), p);
        int done = 0;
        if (nasm_major >= 3) {
            XLine x; static char tmp[65536];
            snprintf(tmp, sizeof tmp, "%s", line);
            x64_classify(tmp, &x);
            if (x.kind == XL_INSN && !x.has_label && !strncmp(x.text, "movsxd ", 7)) {
                XInsn in; XEnc enc; char err[64]; const char* why;
                x64_set_consts(NULL, NULL);
                if (!x64_parse(x.text, &in, err, sizeof err) && in.op[1].kind == X_REG && x64_encode(&in, &enc, &why) > 0) {
                    o += (size_t)snprintf(out + o, outsz - o, "db");
                    for (int i = 0; i < enc.n; i++) o += (size_t)snprintf(out + o, outsz - o, "%s 0x%02X", i ? "," : "", enc.b[i]);
                    o += (size_t)snprintf(out + o, outsz - o, "\n");
                    done = 1;
                }
            }
        }
        if (!done) o += (size_t)snprintf(out + o, outsz - o, "%s\n", line);
        if (!nl) break;
        p = nl + 1;
    }
    out[o] = 0;
}

static long tot_js, tot_jl, tot_cs, tot_cl, tot_calls, tot_fits, max_rounds;
static long files_ok, files_bad, files_long;
// Totals over the programs that are identical, per section: bytes and relocations.
static long tot_bytes[ASM_NSEC], tot_rel[ASM_NSEC], tot_progs_with[ASM_NSEC], align_diff;

static const char* const SEC_NAME[ASM_NSEC] = {".text", ".data", ".bss", ".rodata"};

// Compare one section; returns 1 and prints when they differ.
static int cmp_section(const char* name, int k, const AsmSection* a, const NSec* n) {
    size_t an = a->size, nn = n->present ? n->n : 0;
    if (k == ASM_SEC_BSS) {
        if (an != nn) { printf("FAIL %s: .bss is %zu bytes (nasm %zu)\n", name, an, nn); return 1; }
        return 0;
    }
    size_t m = an < nn ? an : nn, d = 0;
    while (d < m && a->bytes[d] == n->b[d]) d++;
    if (an != nn || d < m) {
        printf("FAIL %s: %s is %zu bytes (nasm %zu); first difference at offset %zu\n", name, SEC_NAME[k], an, nn, d);
        return 1;
    }
    return 0;
}

static int cmp_relocs(const char* name, int k, const AsmSection* a, const NSec* n) {
    size_t nn = n->present ? n->nrel : 0;
    if (a->nrel != nn) { printf("FAIL %s: %zu relocations in %s, nasm %zu\n", name, a->nrel, SEC_NAME[k], nn); return 1; }
    for (size_t i = 0; i < a->nrel; i++) {
        const AsmReloc* r = &a->rel[i]; const NRel* q = &n->rel[i];
        const char* why = NULL;
        if (r->off != q->off) why = "offset";
        else if (r->type != q->type) why = "type";
        else if (!q->secsym || strcmp(q->sym, r->sym)) why = "symbol";
        else if (r->addend != q->addend) why = "addend";
        if (why) {
            printf("FAIL %s: relocation %zu of %s differs in %s (asm.c +%d type %d %s%+lld, nasm +%d type %d %s%+lld)\n", name, i, SEC_NAME[k], why,
                   r->off, r->type, r->sym, (long long)r->addend, q->off, q->type, q->sym, (long long)q->addend);
            return 1;
        }
    }
    return 0;
}

// Returns 0 when asm.c and nasm agree on `text`.
static int compare(const char* work, const char* name, const char* text) {
    char p[600], cmd[1400];
    snprintf(p, sizeof p, "%s/prog.asm", work);
    FILE* f = fopen(p, "w");
    if (!f) { perror(p); return 1; }
    size_t sz = strlen(text) * 3 + 64;
    char* nt = malloc(sz);
    for_nasm(text, nt, sz);
    fputs(nt, f); fclose(f); free(nt);
    snprintf(cmd, sizeof cmd, "nasm -f elf64 -w-all '%s/prog.asm' -o '%s/prog.o' 2>'%s/prog.err'", work, work, work);
    if (system(cmd) != 0) {
        char first[300] = "";
        snprintf(p, sizeof p, "%s/prog.err", work);
        FILE* ef = fopen(p, "r");
        if (ef) { if (!fgets(first, sizeof first, ef)) first[0] = 0; fclose(ef); }
        printf("FAIL %s: nasm refuses the program (see %s/prog.err): %s", name, work, first);
        if (!strchr(first, '\n')) printf("\n");
        return 1;
    }
    snprintf(p, sizeof p, "%s/prog.o", work);
    NObj n;
    if (!nobj_load(p, &n)) { printf("FAIL %s: cannot read nasm object\n", name); return 1; }
    AsmOut a; char err[256];
    if (asm_assemble(text, &a, err, sizeof err)) { printf("FAIL %s: asm.c refuses: %s\n", name, err); nobj_free(&n); return 1; }
    int bad = 0;
    const AsmSection* at = &a.sec[ASM_SEC_TEXT];
    if (cmp_section(name, ASM_SEC_TEXT, at, &n.s[ASM_SEC_TEXT])) {
        size_t m = at->size < n.s[0].n ? at->size : n.s[0].n, d = 0;
        while (d < m && at->bytes[d] == n.s[0].b[d]) d++;
        bad = 1;
        for (size_t i = a.nbr; i-- > 0;)
            if (a.br[i].addr <= (long)d) {
                printf("  last branch before it: line %d at %ld, asm.c %s\n", a.br[i].line, a.br[i].addr, a.br[i].islong ? "long" : "short");
                break;
            }
        // Which jumps does nasm lay out differently? Read its listing.
        snprintf(cmd, sizeof cmd, "nasm -f elf64 -w-all -l '%s/prog.lst' '%s/prog.asm' -o '%s/prog2.o' >/dev/null 2>&1", work, work, work);
        snprintf(p, sizeof p, "%s/prog.lst", work);
        if (system(cmd) == 0) {
            FILE* lf = fopen(p, "r");
            char l[1024]; int shown = 0;
            while (lf && fgets(l, sizeof l, lf) && shown < 8) {
                int ln; unsigned ad; char by[256];
                if (sscanf(l, "%d %x %255s", &ln, &ad, by) != 3) continue;
                size_t bl = strlen(by);
                if (bl != 4 && bl != 10 && bl != 12) continue;
                int nlong = bl > 4;
                if (strspn(by, "0123456789ABCDEF") != bl) continue;
                if (!(by[0] == 'E' && by[1] == '9') && !(by[0] == '0' && by[1] == 'F') && !(by[0] == '7') && !(by[0] == 'E' && by[1] == 'B')) continue;
                for (size_t i = 0; i < a.nbr; i++)
                    if (a.br[i].line == ln && !a.br[i].is_call && a.br[i].islong != nlong) {
                        printf("  line %d: nasm %s, asm.c %s\n", ln, nlong ? "long" : "short", a.br[i].islong ? "long" : "short");
                        shown++;
                    }
            }
            if (lf) fclose(lf);
        }
    }
    for (int k = 1; k < ASM_NSEC && !bad; k++) bad = cmp_section(name, k, &a.sec[k], &n.s[k]);
    for (int k = 0; k < ASM_NSEC && !bad; k++) bad = cmp_relocs(name, k, &a.sec[k], &n.s[k]);
    if (!bad) {
        for (int k = 0; k < ASM_NSEC; k++) {
            const NSec* q = &n.s[k];
            // sh_addralign of a section nasm wrote (it writes .data/.bss even when empty)
            if (q->present && a.sec[k].present && (uint64_t)a.sec[k].align != q->align) { align_diff++; printf("  note %s: %s alignment %d (nasm %llu)\n", name, SEC_NAME[k], a.sec[k].align, (unsigned long long)q->align); }
            tot_bytes[k] += (long)a.sec[k].size; tot_rel[k] += (long)a.sec[k].nrel;
            if (a.sec[k].size) tot_progs_with[k]++;
        }
        tot_js += a.jmp_short; tot_jl += a.jmp_long; tot_cs += a.jcc_short; tot_cl += a.jcc_long;
        tot_calls += a.calls; tot_fits += a.long_fits;
        if (a.relax_rounds > max_rounds) max_rounds = a.relax_rounds;
        if (a.jmp_long + a.jcc_long) files_long++;
    }
    asm_free(&a); nobj_free(&n);
    return bad;
}

static char* slurp(const char* path) {
    uint8_t* b; size_t n;
    if (!nobj_read_file(path, &b, &n)) return NULL;
    b[n] = 0;
    return (char*)b;
}

// Split at lines that start with ";---"; calls fn for each non-empty block.
static int each_block(char* text, int (*fn)(int idx, const char* block, void* ctx), void* ctx) {
    int idx = 0, bad = 0;
    char* p = text;
    for (;;) {
        char* sep = p;
        char* end = NULL;
        while (*sep) {
            if (!strncmp(sep, ";---", 4) && (sep == p || sep[-1] == '\n')) { end = sep; break; }
            char* nl = strchr(sep, '\n');
            if (!nl) { sep = sep + strlen(sep); break; }
            sep = nl + 1;
        }
        char saved = end ? *end : 0;
        if (end) *end = 0;
        idx++;
        bad += fn(idx, p, ctx);
        if (!end) break;
        *end = saved;
        char* nl = strchr(end, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    return bad;
}

static int case_fn(int idx, const char* block, void* ctx) {
    char name[64];
    int code = 0;                                   // a block of comments only is not a program
    for (const char* p = block; *p; p += strcspn(p, "\n") + (p[strcspn(p, "\n")] ? 1 : 0)) {
        const char* q = p;
        while (*q == ' ' || *q == '\t') q++;
        if (*q && *q != ';' && *q != '\n') code = 1;
    }
    if (!code) return 0;
    snprintf(name, sizeof name, "case %d", idx);
    return compare((const char*)ctx, name, block);
}

static int reject_fn(int idx, const char* block, void* ctx) {
    (void)ctx;
    AsmOut a; char err[256];
    if (asm_assemble(block, &a, err, sizeof err) == 0) {
        printf("FAIL reject %d: asm.c accepts:\n%s\n", idx, block);
        asm_free(&a);
        return 1;
    }
    printf("  reject %2d: %s\n", idx, err);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: test_asm WORKDIR files|cases|reject FILE...\n"); return 2; }
    const char* work = argv[1]; const char* mode = argv[2];
    detect_nasm();
    int bad = 0;
    if (!strcmp(mode, "files")) {
        for (int i = 3; i < argc; i++) {
            char* t = slurp(argv[i]);
            if (!t) { perror(argv[i]); return 2; }
            if (compare(work, argv[i], t)) { bad++; files_bad++; } else files_ok++;
            free(t);
        }
        printf("programs: %ld identical to nasm (.text, .data, .rodata bytes, .bss size, relocations), %ld different\n", files_ok, files_bad);
        printf("branches to a label (only counted for identical programs):\n");
        printf("  jmp:  %ld short, %ld long\n  jcc:  %ld short, %ld long\n  call: %ld (always rel32)\n", tot_js, tot_jl, tot_cs, tot_cl, tot_calls);
        long tj = tot_js + tot_jl + tot_cs + tot_cl;
        if (tj) printf("  jmp+jcc: %ld, nasm leaves %ld long (%.2f%%) and %ld short (%.2f%%)\n", tj, tot_jl + tot_cl, 100.0 * (double)(tot_jl + tot_cl) / (double)tj, tot_js + tot_cs, 100.0 * (double)(tot_js + tot_cs) / (double)tj);
        printf("  programs with at least one long jmp/jcc: %ld; long ones whose final displacement fits rel8: %ld\n", files_long, tot_fits);
        printf("  most layout passes in one program: %ld\n", max_rounds);
        printf("sections, summed over the identical programs:\n");
        for (int k = 0; k < ASM_NSEC; k++)
            printf("  %-7s %9ld bytes%s, %6ld relocations, in %ld programs\n", SEC_NAME[k], tot_bytes[k], k == ASM_SEC_BSS ? " (size)" : "", tot_rel[k], tot_progs_with[k]);
        printf("  section alignment different from nasm: %ld (informative, see test_asm.c)\n", align_diff);
    } else {
        char* t = slurp(argv[3]);
        if (!t) { perror(argv[3]); return 2; }
        if (!strcmp(mode, "cases")) { bad = each_block(t, case_fn, (void*)work); printf("cases: %s\n", bad ? "FAIL" : "all identical to nasm"); }
        else if (!strcmp(mode, "reject")) { bad = each_block(t, reject_fn, NULL); printf("reject: %s\n", bad ? "FAIL" : "every block refused"); }
        else { fprintf(stderr, "unknown mode %s\n", mode); return 2; }
        free(t);
    }
    return bad ? 1 : 0;
}
