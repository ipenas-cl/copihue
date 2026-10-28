// Whole-program comparison of compiler/native/asm.c against `nasm -f elf64`.
//
//   test_asm WORKDIR files FILE.asm ...   each file is one program
//   test_asm WORKDIR cases FILE.asm       blocks separated by a line `;---`, each one a program
//   test_asm WORKDIR reject FILE.asm      blocks separated by `;---`; asm.c must refuse every one
//
// For each program nasm assembles the same text and the .text bytes and the
// relocations of its object are compared with what asm.c returns. Relocations
// to .data/.bss are compared by offset, type and section only: their addend
// depends on the layout of the data, which is M3.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compiler/native/asm.h"

static int nasm_major;

static uint64_t rd(const uint8_t* p, int n) {
    uint64_t v = 0;
    for (int i = n - 1; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

typedef struct { int off, type; char sym[128]; int secsym; int64_t addend; } NRel;
typedef struct {
    uint8_t* raw; size_t rawn;
    const uint8_t* text; size_t textn;
    NRel* rel; size_t nrel;
} NObj;

static int read_file(const char* path, uint8_t** buf, size_t* n) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    *buf = malloc((size_t)l + 1);
    *n = fread(*buf, 1, (size_t)l, f);
    fclose(f);
    return 1;
}

// Minimal ELF64 relocatable reader: .text bytes and .rela.text.
static int load_obj(const char* path, NObj* o) {
    memset(o, 0, sizeof *o);
    if (!read_file(path, &o->raw, &o->rawn) || o->rawn < 64) return 0;
    const uint8_t* e = o->raw;
    uint64_t shoff = rd(e + 0x28, 8);
    int shnum = (int)rd(e + 0x3C, 2), shstr = (int)rd(e + 0x3E, 2);
#define SH(i, off, n) rd(e + shoff + (size_t)(i) * 64 + (off), n)
    const char* names = (const char*)e + SH(shstr, 0x18, 8);
    int itext = -1, irela = -1, isym = -1;
    for (int i = 0; i < shnum; i++) {
        const char* nm = names + SH(i, 0, 4);
        if (!strcmp(nm, ".text")) itext = i;
        else if (!strcmp(nm, ".rela.text")) irela = i;
        else if (!strcmp(nm, ".symtab")) isym = i;
    }
    if (itext < 0) return 0;
    o->text = e + SH(itext, 0x18, 8); o->textn = (size_t)SH(itext, 0x20, 8);
    if (irela >= 0 && isym >= 0) {
        const uint8_t* rp = e + SH(irela, 0x18, 8);
        size_t rn = (size_t)SH(irela, 0x20, 8) / 24;
        const uint8_t* sp = e + SH(isym, 0x18, 8);
        const char* strt = (const char*)e + SH((int)SH(isym, 0x28, 4), 0x18, 8);
        o->rel = calloc(rn ? rn : 1, sizeof *o->rel); o->nrel = rn;
        for (size_t i = 0; i < rn; i++) {
            NRel* r = &o->rel[i];
            r->off = (int)rd(rp + i * 24, 8);
            uint64_t info = rd(rp + i * 24 + 8, 8);
            r->type = (int)(info & 0xFFFFFFFF);
            r->addend = (int64_t)rd(rp + i * 24 + 16, 8);
            const uint8_t* s = sp + (size_t)(info >> 32) * 24;
            if ((rd(s + 4, 1) & 15) == 3) {            // STT_SECTION: name of the section
                r->secsym = 1;
                snprintf(r->sym, sizeof r->sym, "%s", names + SH((int)rd(s + 6, 2), 0, 4));
            } else snprintf(r->sym, sizeof r->sym, "%s", strt + rd(s, 4));
        }
    }
    return 1;
#undef SH
}

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
        char line[512];
        snprintf(line, sizeof line, "%.*s", (int)(len < 511 ? len : 511), p);
        int done = 0;
        if (nasm_major >= 3) {
            XLine x; char tmp[512];
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

static long tot_js, tot_jl, tot_cs, tot_cl, tot_calls, tot_fits, max_rounds, tot_rel;
static long files_ok, files_bad, files_long;

static const char* secname(int s) { return s == ASM_SEC_TEXT ? ".text" : s == ASM_SEC_DATA ? ".data" : ".bss"; }

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
    if (system(cmd) != 0) { printf("FAIL %s: nasm refuses the program (see %s/prog.err)\n", name, work); return 1; }
    snprintf(p, sizeof p, "%s/prog.o", work);
    NObj n;
    if (!load_obj(p, &n)) { printf("FAIL %s: cannot read nasm object\n", name); return 1; }
    AsmOut a; char err[256];
    if (asm_assemble(text, &a, err, sizeof err)) { printf("FAIL %s: asm.c refuses: %s\n", name, err); return 1; }
    int bad = 0;
    size_t m = a.ntext < n.textn ? a.ntext : n.textn, d = 0;
    while (d < m && a.text[d] == n.text[d]) d++;
    if (a.ntext != n.textn || d < m) {
        printf("FAIL %s: .text is %zu bytes (nasm %zu); first difference at offset %zu\n", name, a.ntext, n.textn, d);
        for (size_t i = a.nbr; i-- > 0;)
            if (a.br[i].addr <= (long)d) {
                printf("  last branch before it: line %d at %ld, asm.c %s\n", a.br[i].line, a.br[i].addr, a.br[i].islong ? "long" : "short");
                break;
            }
        bad = 1;
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
    if (!bad) {
        if (a.nrel != n.nrel) { printf("FAIL %s: %zu relocations, nasm %zu\n", name, a.nrel, n.nrel); bad = 1; }
        else for (size_t i = 0; i < a.nrel && !bad; i++) {
            const AsmReloc* r = &a.rel[i]; const NRel* q = &n.rel[i];
            const char* why = NULL;
            if (r->off != q->off) why = "offset";
            else if (r->type != q->type) why = "type";
            else if (r->sec != ASM_SEC_TEXT) {                      // data: section only
                if (!q->secsym || strcmp(q->sym, secname(r->sec))) why = "section";
            } else if (!q->secsym || strcmp(q->sym, ".text") || r->addend != q->addend) why = "text target";
            if (why) { printf("FAIL %s: relocation %zu differs in %s (asm.c %s%+lld, nasm %s%+lld)\n", name, i, why, r->sym, (long long)r->addend, q->sym, (long long)q->addend); bad = 1; }
        }
    }
    if (!bad) {
        tot_js += a.jmp_short; tot_jl += a.jmp_long; tot_cs += a.jcc_short; tot_cl += a.jcc_long;
        tot_calls += a.calls; tot_fits += a.long_fits; tot_rel += (long)a.nrel;
        if (a.relax_rounds > max_rounds) max_rounds = a.relax_rounds;
        if (a.jmp_long + a.jcc_long) files_long++;
    }
    asm_free(&a); free(n.raw); free(n.rel);
    return bad;
}

static char* slurp(const char* path) {
    uint8_t* b; size_t n;
    if (!read_file(path, &b, &n)) return NULL;
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
        printf("programs: %ld identical to nasm, %ld different\n", files_ok, files_bad);
        printf("branches to a label (only counted for identical programs):\n");
        printf("  jmp:  %ld short, %ld long\n  jcc:  %ld short, %ld long\n  call: %ld (always rel32)\n", tot_js, tot_jl, tot_cs, tot_cl, tot_calls);
        long tj = tot_js + tot_jl + tot_cs + tot_cl;
        if (tj) printf("  jmp+jcc: %ld, nasm leaves %ld long (%.2f%%) and %ld short (%.2f%%)\n", tj, tot_jl + tot_cl, 100.0 * (double)(tot_jl + tot_cl) / (double)tj, tot_js + tot_cs, 100.0 * (double)(tot_js + tot_cs) / (double)tj);
        printf("  programs with at least one long jmp/jcc: %ld; long ones whose final displacement fits rel8: %ld\n", files_long, tot_fits);
        printf("  most layout passes in one program: %ld; relocations left in .text (all programs): %ld\n", max_rounds, tot_rel);
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
