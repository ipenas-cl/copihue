// Assembler for the hosted NASM text: labels, jump relaxation, call. See asm.h.
#include "asm.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { IT_BYTES, IT_BRANCH, IT_ALIGN };

typedef struct {
    int kind;
    int line;
    size_t poff;        // IT_BYTES: bytes in the pool
    int n;              // IT_BYTES: length; IT_ALIGN: padding after layout
    int n1;             // IT_BYTES: length in nasm's first pass (a later `equ` is unknown there)
    char* rsym;         // IT_BYTES: symbol of the relocation (resolved name), or NULL
    XReloc rel;
    // IT_BRANCH
    char mnem[16];
    char* target;
    int is_call, is_jmp, shortk, islong, forcelong;
    long tidx;          // item index of the target label
    // IT_ALIGN
    int align, fill;
} Item;

typedef struct Sym {
    char* name;
    int sec;
    long item;          // .text: index of the item that follows the label
    int global, line;
    struct Sym* next;
} Sym;

typedef struct { char* name; int64_t v; int line; } Equ;

typedef struct {
    char* err; size_t errsz;
    Item* it; long nit, cap;
    uint8_t* pool; size_t npool, cappool;
    Sym** tab; size_t ntab;
    Equ* equ; int nequ, capequ;
    char** glob; int nglob, capglob;
    long* addr;
} Ctx;

static Ctx* G;          // for the equ callback

static int fail(Ctx* c, int line, const char* fmt, ...) {
    char msg[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (c->err && c->errsz) snprintf(c->err, c->errsz, "line %d: %s", line, msg);
    return -1;
}

static char* trim(char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char* e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static char* dup(const char* s) {
    size_t n = strlen(s) + 1;
    char* p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static unsigned hash(const char* s) {
    unsigned h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static Sym* sym_find(Ctx* c, const char* name) {
    for (Sym* s = c->tab[hash(name) % c->ntab]; s; s = s->next) if (!strcmp(s->name, name)) return s;
    return NULL;
}

static int equ_find(Ctx* c, const char* name, int64_t* v) {
    for (int i = 0; i < c->nequ; i++) if (!strcmp(c->equ[i].name, name)) { *v = c->equ[i].v; return 1; }
    return 0;
}

// A constant used before the line that defines it is unknown to nasm in its
// first pass; it then picks the large immediate / displacement forms. cur_line
// is the line being parsed; fwd_mode makes such a constant look huge, which
// gives the size of that first-pass encoding. saw_fwd tells the caller that
// the line uses one.
static int cur_line, fwd_mode, saw_fwd;

static int const_cb(const char* name, int64_t* v, void* ctx) {
    (void)ctx;
    for (int i = 0; i < G->nequ; i++) {
        if (strcmp(G->equ[i].name, name)) continue;
        int fwd = G->equ[i].line >= cur_line;
        if (fwd) saw_fwd = 1;
        *v = fwd && fwd_mode ? 0x40000000 : G->equ[i].v;
        return 1;
    }
    return 0;
}

static Item* add_item(Ctx* c, int kind, int line) {
    if (c->nit == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 1024;
        c->it = realloc(c->it, (size_t)c->cap * sizeof *c->it);
        if (!c->it) { fprintf(stderr, "asm: out of memory\n"); exit(1); }
    }
    Item* it = &c->it[c->nit++];
    memset(it, 0, sizeof *it);
    it->kind = kind; it->line = line;
    return it;
}

static void pool_put(Ctx* c, Item* it, const uint8_t* b, int n) {
    if (c->npool + (size_t)n > c->cappool) {
        c->cappool = (c->npool + (size_t)n) * 2 + 4096;
        c->pool = realloc(c->pool, c->cappool);
        if (!c->pool) { fprintf(stderr, "asm: out of memory\n"); exit(1); }
    }
    it->poff = c->npool; it->n = it->n1 = n;
    memcpy(c->pool + c->npool, b, (size_t)n);
    c->npool += (size_t)n;
}

// The text after the directive word on a line, without comment.
static char* dir_args(char* line, const XLine* x) {
    char* p = line;
    if (x->has_label) { char* q = strchr(p, ':'); if (q) p = q + 1; }
    while (isspace((unsigned char)*p)) p++;
    while (*p && !isspace((unsigned char)*p)) p++;
    char* sc = strchr(p, ';');
    if (sc) *sc = 0;
    return trim(p);
}

static int parse_int(const char* s, int64_t* v) {
    char* e;
    if (!*s) return 0;
    *v = strtoll(s, &e, 0);
    return *e == 0;
}

// Full name of a label reference: `.x` belongs to the last non-local label.
static int qualify(Ctx* c, int line, const char* parent, const char* name, char* out, size_t n) {
    if (name[0] == '.') {
        if (!parent[0]) return fail(c, line, "local label '%s' with no label before it", name);
        if (snprintf(out, n, "%s%s", parent, name) >= (int)n) return fail(c, line, "label name too long");
    } else if (snprintf(out, n, "%s", name) >= (int)n) return fail(c, line, "label name too long");
    return 0;
}

static int define_label(Ctx* c, int line, const char* full, int sec) {
    if (sym_find(c, full)) return fail(c, line, "label '%s' defined twice", full);
    int64_t v;
    if (equ_find(c, full, &v)) return fail(c, line, "'%s' is both a label and an equ", full);
    Sym* s = calloc(1, sizeof *s);
    s->name = dup(full); s->sec = sec; s->item = c->nit; s->line = line;
    unsigned h = hash(full) % c->ntab;
    s->next = c->tab[h]; c->tab[h] = s;
    return 0;
}

static int is_branch_mnem(const char* m) {
    XEnc e;
    return x64_encode_branch(m, 0, 0, &e) > 0;
}

static int is_data_dir(const char* d) {
    static const char* const D[] = {"db", "dw", "dd", "dq", "resb", "resw", "resd", "resq", "align", NULL};
    for (int i = 0; D[i]; i++) if (!strcmp(d, D[i])) return 1;
    return 0;
}

// Pass 1: classify lines, collect equs; 2: build items (called with the equ table full).
static int collect_equs(Ctx* c, const char* src) {
    char* copy = dup(src);
    int line = 0;
    for (char* p = copy; p; ) {
        char* nl = strchr(p, '\n');
        if (nl) *nl = 0;
        line++;
        char* raw = dup(p);
        XLine x;
        x64_classify(raw, &x);
        if (x.kind == XL_EQU) {
            char* q = strstr(raw, x.name);
            int64_t v, old;
            q = q ? q + strlen(x.name) : NULL;
            while (q && isspace((unsigned char)*q)) q++;
            if (!q || strncmp(q, "equ", 3) || !parse_int(trim(q + 3), &v)) {
                free(raw); free(copy);
                return fail(c, line, "equ '%s' needs a plain integer value", x.name);
            }
            if (equ_find(c, x.name, &old)) { free(raw); free(copy); return fail(c, line, "equ '%s' defined twice", x.name); }
            if (c->nequ == c->capequ) { c->capequ = c->capequ ? c->capequ * 2 : 64; c->equ = realloc(c->equ, (size_t)c->capequ * sizeof *c->equ); }
            c->equ[c->nequ].name = dup(x.name); c->equ[c->nequ].v = v; c->equ[c->nequ].line = line; c->nequ++;
        }
        free(raw);
        p = nl ? nl + 1 : NULL;
    }
    free(copy);
    return 0;
}

static int read_lines(Ctx* c, const char* src) {
    char* copy = dup(src);
    char parent[128] = "";
    int sec = ASM_SEC_TEXT, line = 0, rc = 0;
    for (char* p = copy; p && rc == 0; ) {
        char* nl = strchr(p, '\n');
        if (nl) *nl = 0;
        line++;
        char* raw = dup(p);
        char* work = dup(p);
        XLine x;
        x64_classify(work, &x);
        p = nl ? nl + 1 : NULL;
        if (x.kind == XL_BLANK || x.kind == XL_COMMENT) goto next;
        if (x.kind == XL_EQU) goto next;
        if (x.has_label) {
            char full[256];
            if (qualify(c, line, parent, x.label, full, sizeof full)) { rc = -1; goto next; }
            if (x.label[0] != '.') snprintf(parent, sizeof parent, "%s", x.label);
            if (define_label(c, line, full, sec)) { rc = -1; goto next; }
        }
        if (x.kind == XL_LABEL) goto next;
        if (x.kind == XL_DIRECTIVE) {
            if (!strncmp(x.dir, "section ", 8)) {
                const char* s = x.dir + 8;
                if (!strcmp(s, ".text")) sec = ASM_SEC_TEXT;
                else if (!strcmp(s, ".data")) sec = ASM_SEC_DATA;
                else if (!strcmp(s, ".bss")) sec = ASM_SEC_BSS;
                else rc = fail(c, line, "section '%s' not supported (only .text, .data, .bss, no attributes)", s);
                goto next;
            }
            char* args = dir_args(raw, &x);
            if (!strcmp(x.dir, "global")) {
                if (!*args || strpbrk(args, " \t,")) { rc = fail(c, line, "global takes one name"); goto next; }
                if (c->nglob == c->capglob) { c->capglob = c->capglob ? c->capglob * 2 : 8; c->glob = realloc(c->glob, (size_t)c->capglob * sizeof *c->glob); }
                c->glob[c->nglob++] = dup(args);
                goto next;
            }
            if (sec != ASM_SEC_TEXT) {
                if (!is_data_dir(x.dir)) rc = fail(c, line, "directive '%s' not supported", x.dir);
                goto next;               // contents of .data/.bss are not encoded (M3)
            }
            if (!strcmp(x.dir, "db") || !strcmp(x.dir, "dw") || !strcmp(x.dir, "dd") || !strcmp(x.dir, "dq")) {
                int w = x.dir[1] == 'b' ? 1 : x.dir[1] == 'w' ? 2 : x.dir[1] == 'd' ? 4 : 8;
                int64_t lo = w == 1 ? -128 : w == 2 ? -32768 : w == 4 ? INT32_MIN : INT64_MIN;
                int64_t hi = w == 1 ? 255 : w == 2 ? 65535 : w == 4 ? 4294967295LL : INT64_MAX;
                uint8_t buf[512]; int n = 0;
                char* a = args;
                if (!*a) { rc = fail(c, line, "%s with no values", x.dir); goto next; }
                while (*a) {
                    char* comma = strchr(a, ',');
                    if (comma) *comma = 0;
                    int64_t v;
                    if (!parse_int(trim(a), &v) || v < lo || v > hi) { rc = fail(c, line, "%s in .text takes plain integers in %lld..%lld", x.dir, (long long)lo, (long long)hi); goto next; }
                    if (n + w > (int)sizeof buf) { rc = fail(c, line, "%s too long", x.dir); goto next; }
                    for (int k = 0; k < w; k++) buf[n++] = (uint8_t)((uint64_t)v >> (8 * k));
                    if (!comma) break;
                    a = comma + 1;
                    if (!*trim(a)) { rc = fail(c, line, "%s with a trailing comma", x.dir); goto next; }
                }
                Item* it = add_item(c, IT_BYTES, line);
                pool_put(c, it, buf, n);
                goto next;
            }
            if (!strcmp(x.dir, "align")) {
                int64_t n, fill = 0x90;
                char* comma = strchr(args, ',');
                if (comma) *comma = 0;
                if (!parse_int(trim(args), &n) || n < 1 || n > 4096 || (n & (n - 1))) { rc = fail(c, line, "align needs a power of two up to 4096"); goto next; }
                if (comma) {
                    char* f = trim(comma + 1);
                    if (strncmp(f, "db", 2) || !isspace((unsigned char)f[2]) || !parse_int(trim(f + 2), &fill) || fill < -128 || fill > 255) {
                        rc = fail(c, line, "align in .text takes only `align N` or `align N, db B`"); goto next;
                    }
                }
                Item* it = add_item(c, IT_ALIGN, line);
                it->align = (int)n; it->fill = (int)(fill & 0xFF);
                goto next;
            }
            rc = fail(c, line, "directive '%s' not supported in .text", x.dir);
            goto next;
        }
        // XL_INSN
        if (sec != ASM_SEC_TEXT) { rc = fail(c, line, "instruction outside .text"); goto next; }
        {
            XInsn in; char perr[96];
            G = c; x64_set_consts(const_cb, NULL);
            cur_line = line; saw_fwd = 0; fwd_mode = 0;
            if (x64_parse(x.text, &in, perr, sizeof perr)) { rc = fail(c, line, "%s: '%s'", perr, x.text); goto next; }
            if (is_branch_mnem(in.mnem) && in.nops == 1 && in.op[0].kind == X_IMM && in.op[0].has_sym) {
                const XOp* o = &in.op[0];
                if (in.lock || in.rep || o->imm != 0 || o->sym_neg) { rc = fail(c, line, "branch to a label with prefix or offset not supported: '%s'", x.text); goto next; }
                char full[256];
                if (qualify(c, line, parent, o->sym, full, sizeof full)) { rc = -1; goto next; }
                Item* it = add_item(c, IT_BRANCH, line);
                snprintf(it->mnem, sizeof it->mnem, "%s", in.mnem);
                it->is_call = !strcmp(in.mnem, "call");
                it->is_jmp = !strcmp(in.mnem, "jmp");
                it->shortk = o->shortk;
                it->forcelong = o->neark;
                it->islong = it->is_call || o->neark;
                if (it->is_call && (o->shortk || o->neark)) { rc = fail(c, line, "call takes no short/near"); goto next; }
                it->target = dup(full);
                goto next;
            }
            XEnc e; const char* why = NULL;
            if (x64_encode(&in, &e, &why) <= 0) { rc = fail(c, line, "instruction not covered (%s): '%s'", why ? why : "?", x.text); goto next; }
            Item* it = add_item(c, IT_BYTES, line);
            pool_put(c, it, e.b, e.n);
            if (saw_fwd) {
                // nasm's first pass does not know the constant. Measured against nasm (see
                // DESIGN_NATIVE.md, M2): a memory operand then gets the disp32 form, while an
                // immediate keeps the size it will have (`sub rsp, F` stays imm8).
                int mem = 0;
                for (int k = 0; k < in.nops; k++) if (in.op[k].kind == X_MEM) mem = 1;
                XInsn in1; XEnc e1; char perr1[96];
                fwd_mode = 1;
                if (mem && x64_parse(x.text, &in1, perr1, sizeof perr1) == 0 && x64_encode(&in1, &e1, &why) > 0) it->n1 = e1.n;
                fwd_mode = 0;
            }
            if (e.has_rel) {
                char full[256];
                if (qualify(c, line, parent, e.rel.sym, full, sizeof full)) { rc = -1; goto next; }
                it->rel = e.rel;
                it->rsym = dup(full);
            }
        }
    next:
        free(raw); free(work);
    }
    free(copy);
    return rc;
}

static int branch_size(const Item* it) {
    if (!it->islong) return 2;
    return (it->is_call || it->is_jmp) ? 5 : 6;
}

// Lay out .text the way nasm does. Every pass walks the items in order and
// decides each jmp/jcc again: rel8 when the distance fits, measured from the
// short form, with the target at the address it had in the previous pass, or
// in this pass when the label is already behind (nasm defines labels as it
// goes). In the first pass a label not seen yet is unknown, so a jump to it is
// short; one to a label behind is measured like in any other pass. In that
// pass an instruction with a memory operand that names a later `equ` has its
// disp32 form (n1). The sizes can go both ways between passes (a long jump may
// become short again); it stops when a pass leaves every address as it was.
// Returns the number of passes, or -1 when it does not settle.
static int relax(Ctx* c) {
    long n = c->nit;
    long* prev = calloc((size_t)n + 1, sizeof *prev);
    int pass, rc = -1;
    for (pass = 1; pass <= 1000; pass++) {
        long a = 0;
        for (long i = 0; i < n; i++) {
            Item* it = &c->it[i];
            c->addr[i] = a;
            if (it->kind == IT_BYTES) a += pass == 1 ? it->n1 : it->n;
            else if (it->kind == IT_BRANCH) {
                if (!it->is_call && !it->forcelong) {
                    if (it->shortk || (pass == 1 && it->tidx > i)) it->islong = 0;   // pass 1: a label not seen yet is unknown, be optimistic
                    else {
                        long t = it->tidx <= i ? c->addr[it->tidx] : prev[it->tidx];
                        long rel = t - (a + 2);
                        it->islong = rel < -128 || rel > 127;                    }
                }
                a += branch_size(it);
            } else { it->n = (int)((-a) & (it->align - 1)); a += it->n; }
        }
        c->addr[n] = a;
        if (pass > 1 && !memcmp(prev, c->addr, ((size_t)n + 1) * sizeof *prev)) { rc = pass; break; }
        memcpy(prev, c->addr, ((size_t)n + 1) * sizeof *prev);
    }
    free(prev);
    return rc;
}

static long sym_addr(Ctx* c, const Sym* s) { return c->addr[s->item]; }

static void reloc_push(AsmOut* o, size_t* cap, const AsmReloc* r) {
    if (o->nrel == *cap) { *cap = *cap ? *cap * 2 : 64; o->rel = realloc(o->rel, *cap * sizeof *o->rel); }
    o->rel[o->nrel++] = *r;
}

static int assemble(Ctx* c, AsmOut* out) {
    // Every `global` must be defined; every branch target must be a .text label.
    for (int i = 0; i < c->nglob; i++) {
        Sym* s = sym_find(c, c->glob[i]);
        if (!s) return fail(c, 0, "global '%s' is not defined", c->glob[i]);
        s->global = 1;
    }
    for (long i = 0; i < c->nit; i++) {
        Item* it = &c->it[i];
        if (it->kind != IT_BRANCH) continue;
        Sym* s = sym_find(c, it->target);
        if (!s) return fail(c, it->line, "undefined label '%s'", it->target);
        if (s->sec != ASM_SEC_TEXT) return fail(c, it->line, "'%s' is not a label of .text", it->target);
        it->tidx = s->item;
    }
    c->addr = calloc((size_t)c->nit + 1, sizeof *c->addr);
    out->relax_rounds = relax(c);
    if (out->relax_rounds < 0) return fail(c, 0, "jump relaxation does not settle");
    for (long i = 0; i < c->nit; i++) {
        Item* it = &c->it[i];
        if (it->kind != IT_BRANCH || !it->shortk) continue;
        long rel = c->addr[it->tidx] - (c->addr[i] + 2);
        if (rel < -128 || rel > 127) return fail(c, it->line, "short jump to '%s' out of range", it->target);
    }
    out->ntext = (size_t)c->addr[c->nit];
    out->text = calloc(out->ntext ? out->ntext : 1, 1);
    size_t relcap = 0;
    for (long i = 0; i < c->nit; i++) {
        Item* it = &c->it[i];
        uint8_t* d = out->text + c->addr[i];
        if (it->kind == IT_ALIGN) { memset(d, it->fill, (size_t)it->n); continue; }
        if (it->kind == IT_BRANCH) {
            long size = branch_size(it);
            long rel = sym_addr(c, sym_find(c, it->target)) - (c->addr[i] + size);
            XEnc e;
            if (x64_encode_branch(it->mnem, !it->islong, (int32_t)rel, &e) != size) return fail(c, it->line, "branch to '%s' does not encode", it->target);
            memcpy(d, e.b, (size_t)size);
            out->br = realloc(out->br, (out->nbr + 1) * sizeof *out->br);
            out->br[out->nbr++] = (AsmBranch){it->line, c->addr[i], it->islong, it->is_call};
            if (it->is_call) out->calls++;
            else if (it->is_jmp) { if (it->islong) out->jmp_long++; else out->jmp_short++; }
            else { if (it->islong) out->jcc_long++; else out->jcc_short++; }
            if (it->islong && !it->is_call) {
                long rs = sym_addr(c, sym_find(c, it->target)) - (c->addr[i] + 2);
                if (rs >= -128 && rs <= 127) out->long_fits++;
            }
            continue;
        }
        memcpy(d, c->pool + it->poff, (size_t)it->n);
        if (!it->rsym) continue;
        Sym* s = sym_find(c, it->rsym);
        if (!s) return fail(c, it->line, "undefined symbol '%s'", it->rsym);
        const XReloc* r = &it->rel;
        long field = c->addr[i] + r->off;
        if (s->sec == ASM_SEC_TEXT && r->type == XR_PC32) {
            int64_t v = sym_addr(c, s) + r->addend - field;
            if (v < INT32_MIN || v > INT32_MAX) return fail(c, it->line, "displacement to '%s' out of range", it->rsym);
            for (int k = 0; k < 4; k++) out->text[field + k] = (uint8_t)((uint64_t)v >> (8 * k));
            continue;
        }
        AsmReloc ar;
        memset(&ar, 0, sizeof ar);
        ar.off = (int)field; ar.type = r->type; ar.sec = s->sec;
        if (s->sec == ASM_SEC_TEXT) {
            snprintf(ar.sym, sizeof ar.sym, ".text");
            ar.addend = r->addend + sym_addr(c, s);
        } else {
            snprintf(ar.sym, sizeof ar.sym, "%s", s->name);
            ar.addend = r->addend;
        }
        reloc_push(out, &relcap, &ar);
    }
    return 0;
}

int asm_assemble(const char* src, AsmOut* out, char* err, size_t errsz) {
    Ctx c;
    memset(&c, 0, sizeof c);
    memset(out, 0, sizeof *out);
    c.err = err; c.errsz = errsz;
    c.ntab = 4099;
    c.tab = calloc(c.ntab, sizeof *c.tab);
    G = &c;
    int rc = collect_equs(&c, src);
    if (rc == 0) rc = read_lines(&c, src);
    if (rc == 0) rc = assemble(&c, out);
    x64_set_consts(NULL, NULL);
    for (size_t h = 0; h < c.ntab; h++)
        for (Sym* s = c.tab[h]; s; ) { Sym* n = s->next; free(s->name); free(s); s = n; }
    free(c.tab);
    for (long i = 0; i < c.nit; i++) { free(c.it[i].rsym); free(c.it[i].target); }
    free(c.it); free(c.pool); free(c.addr);
    for (int i = 0; i < c.nequ; i++) free(c.equ[i].name);
    free(c.equ);
    for (int i = 0; i < c.nglob; i++) free(c.glob[i]);
    free(c.glob);
    if (rc) { asm_free(out); return -1; }
    return 0;
}

void asm_free(AsmOut* o) {
    free(o->text); free(o->rel); free(o->br);
    memset(o, 0, sizeof *o);
}
