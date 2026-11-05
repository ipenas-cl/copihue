// Assembler for the hosted NASM text: labels, jump relaxation, call, data sections. See asm.h.
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
    int ext_t;          // the target is an `extern` name: always the long form, with a relocation
    long tidx;          // item index of the target label
    // IT_ALIGN
    int align, fill;
} Item;

typedef struct Sym {
    char* name;
    int sec;
    long item;          // .text: index of the item that follows the label
    long off;           // other sections: offset of the label
    int global, line;
    struct Sym* next;
} Sym;

typedef struct { char* name; int64_t v; int line; } Equ;

// A relocation whose field is in .data or .rodata; the target is resolved after .text is laid out.
typedef struct { int in; size_t off; int type; char* sym; int64_t addend; int line; } DReloc;

typedef struct {
    char* err; size_t errsz;
    Item* it; long nit, cap;
    uint8_t* pool; size_t npool, cappool;
    uint8_t* dbuf[ASM_NSEC]; size_t dcap[ASM_NSEC];   // .data and .rodata contents
    size_t dpos[ASM_NSEC];                             // bytes so far in .data, .rodata, .bss
    int salign[ASM_NSEC], present[ASM_NSEC];
    DReloc* dr; size_t ndr, capdr;
    Sym** tab; size_t ntab;
    Equ* equ; int nequ, capequ;
    char** glob; int nglob, capglob;
    char** ext; int next, capext;
    int obj;            // assembling a relocatable object
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

static int ext_find(const Ctx* c, const char* name) {
    for (int i = 0; i < c->next; i++) if (!strcmp(c->ext[i], name)) return 1;
    return 0;
}

static void ext_add(Ctx* c, const char* name) {
    if (c->next == c->capext) {
        c->capext = c->capext ? c->capext * 2 : 8;
        c->ext = realloc(c->ext, (size_t)c->capext * sizeof *c->ext);
    }
    c->ext[c->next++] = dup(name);
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

// A decimal or 0x hexadecimal integer with an optional leading '-' (nasm reads
// a leading 0 as decimal, so no octal). *big is set when the value is above
// INT64_MAX (only a 64-bit datum takes it); *v then holds its two's complement.
static int parse_num(const char* s, int64_t* v, int* big) {
    int neg = 0, hex = 0;
    if (*s == '-') { neg = 1; s++; }
    if (s[0] == '0' && s[1] == 'x') { hex = 1; s += 2; }
    if (!*s) return 0;
    uint64_t u = 0;
    for (; *s; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (hex && ((*s | 32) >= 'a' && (*s | 32) <= 'f')) d = (*s | 32) - 'a' + 10;
        else return 0;
        uint64_t base = hex ? 16 : 10;
        if (u > (UINT64_MAX - (uint64_t)d) / base) return 0;
        u = u * base + (uint64_t)d;
    }
    *big = 0;
    if (neg) {
        if (u > (uint64_t)INT64_MAX + 1) return 0;
        *v = (int64_t)(0 - u);
    } else {
        *big = u > (uint64_t)INT64_MAX;
        *v = (int64_t)u;
    }
    return 1;
}

static int parse_int(const char* s, int64_t* v) {
    int big;
    return parse_num(s, v, &big) && !big;
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
    s->off = sec == ASM_SEC_TEXT ? 0 : (long)c->dpos[sec];
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

// ---- data directives (M3) ----------------------------------------------------

typedef struct { int64_t v; char sym[256]; int has_sym; } DVal;

static int ident_start(int ch) { return isalpha(ch) || ch == '_' || ch == '.'; }
static int ident_part(int ch) { return isalnum(ch) || ch == '_' || ch == '.' || ch == '$' || ch == '@' || ch == '#' || ch == '~'; }

// One value of db/dw/dd/dq: an integer, an `equ` constant, or a label with an
// optional +/- integer (dd and dq only). `s` is trimmed.
static int parse_dval(Ctx* c, int line, const char* parent, const char* dir, int w, char* s, DVal* out) {
    memset(out, 0, sizeof *out);
    int64_t v;
    if (ident_start((unsigned char)*s)) {
        char* e = s;
        while (ident_part((unsigned char)*e)) e++;
        char save = *e;
        *e = 0;
        char name[256];
        snprintf(name, sizeof name, "%s", s);
        *e = save;
        char* rest = trim(e);
        int64_t off = 0;
        if (*rest) {
            int sign = *rest == '-' ? -1 : *rest == '+' ? 1 : 0;
            if (!sign || !parse_int(trim(rest + 1), &off)) return fail(c, line, "%s takes a label with at most one +/- integer: '%s'", dir, s);
            off *= sign;
        }
        int64_t cv;
        if (equ_find(c, name, &cv)) v = cv + off;
        else {
            if (w != 4 && w != 8) return fail(c, line, "%s cannot hold the address of a label (only dd and dq)", dir);
            if (qualify(c, line, parent, name, out->sym, sizeof out->sym)) return -1;
            out->has_sym = 1;
            out->v = off;
            return 0;
        }
    } else {
        int big;
        if (!parse_num(s, &v, &big)) return fail(c, line, "%s takes integers (decimal or 0x), labels and constants, not '%s'", dir, s);
        if (big && w != 8) return fail(c, line, "%s value '%s' out of range", dir, s);
    }
    int64_t lo = w == 1 ? -128 : w == 2 ? -32768 : w == 4 ? INT32_MIN : INT64_MIN;
    int64_t hi = w == 1 ? 255 : w == 2 ? 65535 : w == 4 ? 4294967295LL : INT64_MAX;
    if (w < 8 && (v < lo || v > hi)) return fail(c, line, "%s value '%s' out of range", dir, s);
    out->v = v;
    return 0;
}

#define DATA_LIMIT ((size_t)1 << 30)

static void dput(Ctx* c, int sec, const uint8_t* b, size_t n, int fill) {
    if (c->dpos[sec] + n > c->dcap[sec]) {
        c->dcap[sec] = (c->dpos[sec] + n) * 2 + 4096;
        c->dbuf[sec] = realloc(c->dbuf[sec], c->dcap[sec]);
        if (!c->dbuf[sec]) { fprintf(stderr, "asm: out of memory\n"); exit(1); }
    }
    if (b) memcpy(c->dbuf[sec] + c->dpos[sec], b, n);
    else memset(c->dbuf[sec] + c->dpos[sec], fill, n);
    c->dpos[sec] += n;
}

static int data_room(Ctx* c, int line, int sec, size_t n) {
    if (n > DATA_LIMIT || c->dpos[sec] + n > DATA_LIMIT) return fail(c, line, "section is larger than 1 GiB");
    return 0;
}

static void note_align(Ctx* c, int sec, int64_t n) {
    if (n > c->salign[sec]) c->salign[sec] = (int)n;
}

// db dw dd dq resb resw resd resq align times, in any section. `args` is the text after the directive.
static int data_directive(Ctx* c, int sec, int line, const char* dir, char* args, const char* parent) {
    int64_t reps = 1;
    if (!strcmp(dir, "times")) {
        char* p = args;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = 0;
        if (!parse_int(args, &reps) || reps < 0 || reps > (1 << 24)) return fail(c, line, "times needs a count from 0 to 16777216");
        args = trim(p);
        p = args;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = 0;
        dir = args;
        args = trim(p);
        if (!is_data_dir(dir) || !strcmp(dir, "align")) return fail(c, line, "times takes only db dw dd dq resb resw resd resq (not '%s')", dir);
    }
    if (!strcmp(dir, "align")) {
        int64_t n, fill = 0x90, resk = 0;
        int has_fill = 0;
        char* comma = strchr(args, ',');
        if (comma) *comma = 0;
        if (!parse_int(trim(args), &n) || n < 1 || n > 4096 || (n & (n - 1))) return fail(c, line, "align needs a power of two up to 4096");
        if (comma) {
            char* f = trim(comma + 1);
            has_fill = 1;
            if (!strncmp(f, "db", 2) && isspace((unsigned char)f[2]) && parse_int(trim(f + 2), &fill) && fill >= -128 && fill <= 255) resk = 0;
            else if (!strncmp(f, "resb", 4) && isspace((unsigned char)f[4]) && parse_int(trim(f + 4), &resk) && resk == 1) resk = 1;
            else return fail(c, line, "align takes only `align N`, `align N, db B` or `align N, resb 1`");
        }
        note_align(c, sec, n);
        if (sec == ASM_SEC_TEXT) {
            if (resk) return fail(c, line, "align with resb in .text");
            Item* it = add_item(c, IT_ALIGN, line);
            it->align = (int)n; it->fill = (int)(fill & 0xFF);
            return 0;
        }
        size_t pad = (size_t)((0 - c->dpos[sec]) & (size_t)(n - 1));
        if (sec == ASM_SEC_BSS) {
            if (has_fill && !resk) return fail(c, line, "align with db in .bss (use `align N, resb 1`)");
            c->dpos[sec] += pad;
            return 0;
        }
        if (resk) return fail(c, line, "align with resb outside .bss");
        dput(c, sec, NULL, pad, (int)(fill & 0xFF));
        return 0;
    }
    if (dir[0] == 'r') {                       // resb resw resd resq
        int w = dir[3] == 'b' ? 1 : dir[3] == 'w' ? 2 : dir[3] == 'd' ? 4 : 8;
        int64_t n;
        if (sec != ASM_SEC_BSS) return fail(c, line, "%s only in .bss", dir);
        if (!parse_int(args, &n) || n < 0 || n > (int64_t)DATA_LIMIT) return fail(c, line, "%s needs a plain integer count", dir);
        size_t total = (size_t)n * (size_t)w * (size_t)reps;
        if ((size_t)reps && total / (size_t)reps != (size_t)n * (size_t)w) return fail(c, line, "section is larger than 1 GiB");
        if (data_room(c, line, sec, total)) return -1;
        c->dpos[sec] += total;
        return 0;
    }
    int w = dir[1] == 'b' ? 1 : dir[1] == 'w' ? 2 : dir[1] == 'd' ? 4 : 8;
    if (sec == ASM_SEC_BSS) return fail(c, line, "%s with contents in .bss (use resb/resw/resd/resq)", dir);
    if (!*args) return fail(c, line, "%s with no values", dir);
    DVal* vals = NULL;
    size_t nv = 0, capv = 0;
    int rc = 0;
    for (char* a = args; rc == 0; ) {
        char* comma = strchr(a, ',');
        if (comma) *comma = 0;
        char* t = trim(a);
        if (!*t) { rc = fail(c, line, "%s with an empty value", dir); break; }
        if (nv == capv) { capv = capv ? capv * 2 : 64; vals = realloc(vals, capv * sizeof *vals); }
        rc = parse_dval(c, line, parent, dir, w, t, &vals[nv++]);
        if (!comma) break;
        a = comma + 1;
    }
    if (rc == 0 && (size_t)reps * nv * (size_t)w > DATA_LIMIT) rc = fail(c, line, "section is larger than 1 GiB");
    if (rc == 0 && sec == ASM_SEC_TEXT && (size_t)reps * nv > (1u << 20)) rc = fail(c, line, "too many values in .text");
    for (int64_t r = 0; r < reps && rc == 0; r++) {
        for (size_t i = 0; i < nv; i++) {
            const DVal* d = &vals[i];
            uint8_t buf[8];
            for (int k = 0; k < w; k++) buf[k] = d->has_sym ? 0 : (uint8_t)((uint64_t)d->v >> (8 * k));
            int type = w == 8 ? XR_64 : XR_32;
            if (sec == ASM_SEC_TEXT) {
                Item* it = add_item(c, IT_BYTES, line);
                pool_put(c, it, buf, w);
                if (d->has_sym) {
                    it->rel = (XReloc){0, type, "", d->v};
                    it->rsym = dup(d->sym);
                }
            } else {
                if (d->has_sym) {
                    if (c->ndr == c->capdr) { c->capdr = c->capdr ? c->capdr * 2 : 64; c->dr = realloc(c->dr, c->capdr * sizeof *c->dr); }
                    c->dr[c->ndr++] = (DReloc){sec, c->dpos[sec], type, dup(d->sym), d->v, line};
                }
                dput(c, sec, buf, (size_t)w, 0);
            }
        }
    }
    free(vals);
    return rc;
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
                else if (!strcmp(s, ".rodata")) sec = ASM_SEC_RODATA;
                else { rc = fail(c, line, "section '%s' not supported (only .text, .data, .rodata, .bss, no attributes)", s); goto next; }
                c->present[sec] = 1;
                goto next;
            }
            char* args = dir_args(raw, &x);
            if (!strcmp(x.dir, "global")) {
                if (!*args || strpbrk(args, " \t,")) { rc = fail(c, line, "global takes one name"); goto next; }
                if (c->nglob == c->capglob) { c->capglob = c->capglob ? c->capglob * 2 : 8; c->glob = realloc(c->glob, (size_t)c->capglob * sizeof *c->glob); }
                c->glob[c->nglob++] = dup(args);
                goto next;
            }
            if (!strcmp(x.dir, "extern")) {
                if (!c->obj) { rc = fail(c, line, "directive 'extern' not supported (an executable has nothing to link it with; only a -c object)"); goto next; }
                if (!*args || strpbrk(args, " \t,") || args[0] == '.') { rc = fail(c, line, "extern takes one name"); goto next; }
                if (ext_find(c, args)) { rc = fail(c, line, "extern '%s' declared twice", args); goto next; }
                ext_add(c, args);
                goto next;
            }
            if (!is_data_dir(x.dir) && strcmp(x.dir, "times")) { rc = fail(c, line, "directive '%s' not supported", x.dir); goto next; }
            rc = data_directive(c, sec, line, x.dir, args, parent);
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

static const char* const SEC_NAME[ASM_NSEC] = {".text", ".data", ".bss", ".rodata"};

// A relocation like nasm's: against the section symbol of the target, with the
// target's offset in that section added to the addend.
static void reloc_push(Ctx* c, AsmOut* o, size_t* caps, int in, long off, int type, const Sym* s, int64_t addend) {
    AsmSection* sc = &o->sec[in];
    if (sc->nrel == caps[in]) { caps[in] = caps[in] ? caps[in] * 2 : 64; sc->rel = realloc(sc->rel, caps[in] * sizeof *sc->rel); }
    AsmReloc* r = &sc->rel[sc->nrel++];
    memset(r, 0, sizeof *r);
    r->in = in; r->off = (int)off; r->type = type; r->sec = s->sec;
    snprintf(r->sym, sizeof r->sym, "%s", SEC_NAME[s->sec]);
    r->addend = addend + (s->sec == ASM_SEC_TEXT ? sym_addr(c, s) : s->off);
}

// A relocation against a name declared `extern`: the symbol is the name itself.
static void reloc_push_ext(AsmOut* o, size_t* caps, int in, long off, int type, const char* name, int64_t addend) {
    AsmSection* sc = &o->sec[in];
    if (sc->nrel == caps[in]) { caps[in] = caps[in] ? caps[in] * 2 : 64; sc->rel = realloc(sc->rel, caps[in] * sizeof *sc->rel); }
    AsmReloc* r = &sc->rel[sc->nrel++];
    memset(r, 0, sizeof *r);
    r->in = in; r->off = (int)off; r->type = type; r->sec = ASM_SEC_UNDEF;
    snprintf(r->sym, sizeof r->sym, "%s", name);
    r->addend = addend;
}

static int sym_cmp(const void* a, const void* b) {
    const AsmSym* x = a; const AsmSym* y = b;
    if (x->sec != y->sec) return x->sec < y->sec ? -1 : 1;
    if (x->off != y->off) return x->off < y->off ? -1 : 1;
    return strcmp(x->name, y->name);
}

static int assemble(Ctx* c, AsmOut* out) {
    // Every `global` must be defined; every branch target must be a .text label.
    for (int i = 0; i < c->nglob; i++) {
        Sym* s = sym_find(c, c->glob[i]);
        if (!s && c->obj) continue;      // nasm -f elf64 drops a global that nothing defines or uses
        if (!s) return fail(c, 0, "global '%s' is not defined", c->glob[i]);
        s->global = 1;
    }
    for (int i = 0; i < c->next; i++)
        if (sym_find(c, c->ext[i])) return fail(c, sym_find(c, c->ext[i])->line, "'%s' is declared extern and also defined", c->ext[i]);
    for (long i = 0; i < c->nit; i++) {
        Item* it = &c->it[i];
        if (it->kind != IT_BRANCH) continue;
        Sym* s = sym_find(c, it->target);
        if (!s && ext_find(c, it->target)) {
            // A jump or call to another object: near form, the displacement is the linker's.
            if (it->shortk) return fail(c, it->line, "short jump to the extern '%s'", it->target);
            it->ext_t = 1; it->forcelong = 1; it->islong = 1;
            continue;
        }
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
    AsmSection* tx = &out->sec[ASM_SEC_TEXT];
    tx->size = (size_t)c->addr[c->nit];
    tx->bytes = calloc(tx->size ? tx->size : 1, 1);
    size_t relcap[ASM_NSEC] = {0};
    for (long i = 0; i < c->nit; i++) {
        Item* it = &c->it[i];
        uint8_t* d = tx->bytes + c->addr[i];
        if (it->kind == IT_ALIGN) { memset(d, it->fill, (size_t)it->n); continue; }
        if (it->kind == IT_BRANCH) {
            long size = branch_size(it);
            long rel = it->ext_t ? 0 : sym_addr(c, sym_find(c, it->target)) - (c->addr[i] + size);
            XEnc e;
            if (x64_encode_branch(it->mnem, !it->islong, (int32_t)rel, &e) != size) return fail(c, it->line, "branch to '%s' does not encode", it->target);
            memcpy(d, e.b, (size_t)size);
            out->br = realloc(out->br, (out->nbr + 1) * sizeof *out->br);
            out->br[out->nbr++] = (AsmBranch){it->line, c->addr[i], it->islong, it->is_call};
            if (it->is_call) out->calls++;
            else if (it->is_jmp) { if (it->islong) out->jmp_long++; else out->jmp_short++; }
            else { if (it->islong) out->jcc_long++; else out->jcc_short++; }
            if (it->ext_t) reloc_push_ext(out, relcap, ASM_SEC_TEXT, c->addr[i] + size - 4, XR_PC32, it->target, -4);
            else if (it->islong && !it->is_call) {
                long rs = sym_addr(c, sym_find(c, it->target)) - (c->addr[i] + 2);
                if (rs >= -128 && rs <= 127) out->long_fits++;
            }
            continue;
        }
        memcpy(d, c->pool + it->poff, (size_t)it->n);
        if (!it->rsym) continue;
        Sym* s = sym_find(c, it->rsym);
        const XReloc* r = &it->rel;
        long field = c->addr[i] + r->off;
        if (!s && ext_find(c, it->rsym)) { reloc_push_ext(out, relcap, ASM_SEC_TEXT, field, r->type, it->rsym, r->addend); continue; }
        if (!s) return fail(c, it->line, "undefined symbol '%s'", it->rsym);
        if (s->sec == ASM_SEC_TEXT && r->type == XR_PC32) {
            int64_t v = sym_addr(c, s) + r->addend - field;
            if (v < INT32_MIN || v > INT32_MAX) return fail(c, it->line, "displacement to '%s' out of range", it->rsym);
            for (int k = 0; k < 4; k++) tx->bytes[field + k] = (uint8_t)((uint64_t)v >> (8 * k));
            continue;
        }
        reloc_push(c, out, relcap, ASM_SEC_TEXT, field, r->type, s, r->addend);
    }
    // .data and .rodata: contents were laid out while reading; only the labels they name are left.
    for (int k = 0; k < ASM_NSEC; k++) {
        AsmSection* sc = &out->sec[k];
        sc->present = c->present[k];
        sc->align = c->salign[k];
        if (k == ASM_SEC_TEXT) continue;
        sc->size = c->dpos[k];
        if (k == ASM_SEC_BSS) continue;
        sc->bytes = calloc(sc->size ? sc->size : 1, 1);
        if (sc->size) memcpy(sc->bytes, c->dbuf[k], sc->size);
    }
    for (size_t i = 0; i < c->ndr; i++) {
        const DReloc* r = &c->dr[i];
        Sym* s = sym_find(c, r->sym);
        if (!s && ext_find(c, r->sym)) { reloc_push_ext(out, relcap, r->in, (long)r->off, r->type, r->sym, r->addend); continue; }
        if (!s) return fail(c, r->line, "undefined symbol '%s'", r->sym);
        reloc_push(c, out, relcap, r->in, (long)r->off, r->type, s, r->addend);
    }
    // Symbol table.
    size_t ns = 0;
    for (size_t h = 0; h < c->ntab; h++) for (Sym* s = c->tab[h]; s; s = s->next) ns++;
    out->sym = calloc(ns ? ns : 1, sizeof *out->sym);
    for (size_t h = 0; h < c->ntab; h++)
        for (Sym* s = c->tab[h]; s; s = s->next) {
            AsmSym* y = &out->sym[out->nsym++];
            y->name = dup(s->name); y->sec = s->sec; y->global = s->global; y->line = s->line;
            y->off = s->sec == ASM_SEC_TEXT ? sym_addr(c, s) : s->off;
        }
    qsort(out->sym, out->nsym, sizeof *out->sym, sym_cmp);
    // The undefined symbols of the object: the externs some code or data uses
    // (nasm leaves an unused one out).
    out->ext = calloc(c->next ? (size_t)c->next : 1, sizeof *out->ext);
    for (int i = 0; i < c->next; i++) {
        int used = 0;
        for (int k = 0; k < ASM_NSEC && !used; k++)
            for (size_t r = 0; r < out->sec[k].nrel && !used; r++)
                used = out->sec[k].rel[r].sec == ASM_SEC_UNDEF && !strcmp(out->sec[k].rel[r].sym, c->ext[i]);
        if (used) out->ext[out->next++] = dup(c->ext[i]);
    }
    return 0;
}

const AsmSym* asm_find_sym(const AsmOut* o, const char* name) {
    for (size_t i = 0; i < o->nsym; i++) if (!strcmp(o->sym[i].name, name)) return &o->sym[i];
    return NULL;
}

static int assemble_text(const char* src, AsmOut* out, char* err, size_t errsz, int obj) {
    Ctx c;
    memset(&c, 0, sizeof c);
    memset(out, 0, sizeof *out);
    c.err = err; c.errsz = errsz; c.obj = obj;
    c.ntab = 4099;
    c.tab = calloc(c.ntab, sizeof *c.tab);
    G = &c;
    c.salign[ASM_SEC_TEXT] = 16; c.salign[ASM_SEC_DATA] = 4; c.salign[ASM_SEC_BSS] = 4; c.salign[ASM_SEC_RODATA] = 4;
    c.present[ASM_SEC_TEXT] = 1;
    int rc = collect_equs(&c, src);
    if (rc == 0) rc = read_lines(&c, src);
    if (rc == 0) rc = assemble(&c, out);
    x64_set_consts(NULL, NULL);
    for (size_t h = 0; h < c.ntab; h++)
        for (Sym* s = c.tab[h]; s; ) { Sym* n = s->next; free(s->name); free(s); s = n; }
    free(c.tab);
    for (long i = 0; i < c.nit; i++) { free(c.it[i].rsym); free(c.it[i].target); }
    free(c.it); free(c.pool); free(c.addr);
    for (int k = 0; k < ASM_NSEC; k++) free(c.dbuf[k]);
    for (size_t i = 0; i < c.ndr; i++) free(c.dr[i].sym);
    free(c.dr);
    for (int i = 0; i < c.nequ; i++) free(c.equ[i].name);
    free(c.equ);
    for (int i = 0; i < c.nglob; i++) free(c.glob[i]);
    free(c.glob);
    for (int i = 0; i < c.next; i++) free(c.ext[i]);
    free(c.ext);
    if (rc) { asm_free(out); return -1; }
    return 0;
}

int asm_assemble(const char* src, AsmOut* out, char* err, size_t errsz) {
    return assemble_text(src, out, err, errsz, 0);
}

int asm_assemble_obj(const char* src, AsmOut* out, char* err, size_t errsz) {
    return assemble_text(src, out, err, errsz, 1);
}

void asm_free(AsmOut* o) {
    for (int k = 0; k < ASM_NSEC; k++) { free(o->sec[k].bytes); free(o->sec[k].rel); }
    free(o->br);
    for (size_t i = 0; i < o->nsym; i++) free(o->sym[i].name);
    free(o->sym);
    for (size_t i = 0; i < o->next; i++) free(o->ext[i]);
    free(o->ext);
    memset(o, 0, sizeof *o);
}
