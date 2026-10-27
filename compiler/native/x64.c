// Direct x86-64 instruction encoder. See x64.h and docs/DESIGN_NATIVE.md.
#include "x64.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------- constants

static XConstFn g_const_fn;
static void* g_const_ctx;

void x64_set_consts(XConstFn fn, void* ctx) { g_const_fn = fn; g_const_ctx = ctx; }

// ---------------------------------------------------------------- registers

typedef struct { const char* name; int num, size, rclass, hi8; } RegDef;

static const char* const R64[16] = {"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
                                    "r8","r9","r10","r11","r12","r13","r14","r15"};
static const char* const R32[8]  = {"eax","ecx","edx","ebx","esp","ebp","esi","edi"};
static const char* const R16[8]  = {"ax","cx","dx","bx","sp","bp","si","di"};
static const char* const R8[8]   = {"al","cl","dl","bl","spl","bpl","sil","dil"};
static const char* const R8H[4]  = {"ah","ch","dh","bh"};

static int reg_lookup(const char* s, int* num, int* size, int* rclass, int* hi8) {
    *hi8 = 0; *rclass = RC_GPR;
    for (int i = 0; i < 16; i++) if (!strcmp(s, R64[i])) { *num = i; *size = 8; return 1; }
    for (int i = 0; i < 8; i++) {
        if (!strcmp(s, R32[i])) { *num = i; *size = 4; return 1; }
        if (!strcmp(s, R16[i])) { *num = i; *size = 2; return 1; }
        if (!strcmp(s, R8[i]))  { *num = i; *size = 1; return 1; }
    }
    for (int i = 0; i < 4; i++)
        if (!strcmp(s, R8H[i])) { *num = 4 + i; *size = 1; *hi8 = 1; return 1; }
    if (s[0] == 'r' && isdigit((unsigned char)s[1])) {
        char* end; long n = strtol(s + 1, &end, 10);
        if (n >= 8 && n <= 15) {
            if (!*end) { *num = (int)n; *size = 8; return 1; }
            if (!strcmp(end, "d")) { *num = (int)n; *size = 4; return 1; }
            if (!strcmp(end, "w")) { *num = (int)n; *size = 2; return 1; }
            if (!strcmp(end, "b") || !strcmp(end, "l")) { *num = (int)n; *size = 1; return 1; }
        }
    }
    if (s[0] == 'x' && s[1] == 'm' && s[2] == 'm' && isdigit((unsigned char)s[3])) {
        char* end; long n = strtol(s + 3, &end, 10);
        if (!*end && n >= 0 && n <= 15) { *num = (int)n; *size = 16; *rclass = RC_XMM; return 1; }
    }
    return 0;
}

// ---------------------------------------------------------------- parser

static void seterr(char* err, size_t n, const char* fmt, const char* a) {
    if (err && n) snprintf(err, n, fmt, a ? a : "");
}

static char* trim(char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char* e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static int is_ident_start(int c) { return isalpha(c) || c == '_' || c == '.' || c == '$' || c == '@'; }

// Parse an unsigned number literal. 0x.., decimal, or 'c'.
static int parse_number(const char* s, int64_t* v) {
    if (s[0] == '\'' && s[1] && s[2] == '\'' && !s[3]) { *v = (unsigned char)s[1]; return 1; }
    if (!isdigit((unsigned char)s[0])) return 0;
    char* end;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        unsigned long long u = strtoull(s + 2, &end, 16);
        if (*end || end == s + 2) return 0;
        *v = (int64_t)u; return 1;
    }
    unsigned long long u = strtoull(s, &end, 10);
    if (*end) return 0;
    *v = (int64_t)u; return 1;
}

typedef struct {
    int nreg;
    struct { int num, size, rclass, hi8, scale; } reg[3];
    int64_t val;
    char sym[96];
    int has_sym, sym_neg;
    int bad;
} Expr;

// Parse `term (+|-) term ...` where a term is a register, reg*N, a number, a
// known constant or one symbol. Returns 0 on failure.
static int parse_expr(const char* text, Expr* x) {
    memset(x, 0, sizeof *x);
    char buf[256];
    if (strlen(text) >= sizeof buf) return 0;
    strcpy(buf, text);
    char* p = buf;
    int sign = 1, first = 1;
    for (;;) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p && !first) return 0;
        if (*p == '+') { p++; continue; }
        if (*p == '-') { sign = -sign; p++; continue; }
        char* t = p;
        if (*p == '\'') { p++; if (*p) p++; if (*p == '\'') p++; }
        else while (*p && !isspace((unsigned char)*p) && *p != '+' && *p != '-') p++;
        char save = *p; *p = 0;
        char term[128];
        if (strlen(t) >= sizeof term) return 0;
        strcpy(term, t);
        *p = save;
        if (!term[0]) return 0;
        // reg*N or N*reg
        char* star = strchr(term, '*');
        int64_t v;
        if (star) {
            *star = 0;
            char* a = trim(term); char* b = trim(star + 1);
            int n, sz, rc, h8; int64_t sc;
            const char* rn; const char* sn;
            if (reg_lookup(a, &n, &sz, &rc, &h8)) { rn = a; sn = b; }
            else if (reg_lookup(b, &n, &sz, &rc, &h8)) { rn = b; sn = a; }
            else return 0;
            (void)rn;
            if (!parse_number(sn, &sc) || (sc != 1 && sc != 2 && sc != 4 && sc != 8)) return 0;
            if (sign < 0 || x->nreg >= 3) return 0;
            x->reg[x->nreg].num = n; x->reg[x->nreg].size = sz; x->reg[x->nreg].rclass = rc;
            x->reg[x->nreg].hi8 = h8; x->reg[x->nreg].scale = (int)sc; x->nreg++;
        } else {
            int n, sz, rc, h8;
            if (reg_lookup(term, &n, &sz, &rc, &h8)) {
                if (sign < 0 || x->nreg >= 3) return 0;
                x->reg[x->nreg].num = n; x->reg[x->nreg].size = sz; x->reg[x->nreg].rclass = rc;
                x->reg[x->nreg].hi8 = h8; x->reg[x->nreg].scale = 0; x->nreg++;
            } else if (parse_number(term, &v)) {
                x->val += sign * v;
            } else if (is_ident_start((unsigned char)term[0])) {
                int64_t cv;
                if (g_const_fn && g_const_fn(term, &cv, g_const_ctx)) x->val += sign * cv;
                else {
                    if (x->has_sym) return 0;
                    x->has_sym = 1; x->sym_neg = sign < 0;
                    snprintf(x->sym, sizeof x->sym, "%s", term);
                }
            } else return 0;
        }
        first = 0; sign = 1;
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p != '+' && *p != '-') return 0;
    }
    return 1;
}

static int size_keyword(const char* w) {
    if (!strcmp(w, "byte")) return 1;
    if (!strcmp(w, "word")) return 2;
    if (!strcmp(w, "dword")) return 4;
    if (!strcmp(w, "qword")) return 8;
    if (!strcmp(w, "oword") || !strcmp(w, "xmmword")) return 16;
    return 0;
}

static int parse_operand(char* s, XOp* o, char* err, size_t errsz) {
    memset(o, 0, sizeof *o);
    o->base = o->index = -1; o->scale = 1;
    s = trim(s);
    // leading keywords
    for (;;) {
        char w[16]; int i = 0;
        while (s[i] && isalpha((unsigned char)s[i]) && i < 15) { w[i] = s[i]; i++; }
        w[i] = 0;
        if (!i || (s[i] && !isspace((unsigned char)s[i]) && s[i] != '[')) break;
        int sz = size_keyword(w);
        if (sz) { o->size = sz; s = trim(s + i); continue; }
        if (!strcmp(w, "short") && s[i]) { o->shortk = 1; s = trim(s + i); continue; }
        if (!strcmp(w, "near") && s[i]) { o->neark = 1; s = trim(s + i); continue; }
        break;
    }
    if (*s == '[') {
        char* close = strrchr(s, ']');
        if (!close || close[1]) { seterr(err, errsz, "memory operand: %s", s); return 0; }
        *close = 0;
        char* inner = trim(s + 1);
        if (!strncmp(inner, "abs ", 4)) { seterr(err, errsz, "abs memory: %s", inner); return 0; }
        if (!strncmp(inner, "rel ", 4)) { o->rip = 1; inner = trim(inner + 4); }
        Expr x;
        if (!parse_expr(inner, &x)) { seterr(err, errsz, "memory expression: %s", inner); return 0; }
        if (x.sym_neg) { seterr(err, errsz, "negated symbol in address: %s", inner); return 0; }
        o->kind = X_MEM;
        if (o->rip && (x.nreg || !x.has_sym)) { seterr(err, errsz, "rel needs a lone symbol: %s", inner); return 0; }
        for (int i = 0; i < x.nreg; i++) {
            if (x.reg[i].rclass != RC_GPR || x.reg[i].size != 8) {
                seterr(err, errsz, "address register: %s", inner); return 0;
            }
            if (x.reg[i].scale > 1) {
                if (o->index >= 0) { seterr(err, errsz, "two index registers: %s", inner); return 0; }
                o->index = x.reg[i].num; o->scale = x.reg[i].scale;
            } else if (o->base < 0) o->base = x.reg[i].num;
            else if (o->index < 0) { o->index = x.reg[i].num; o->scale = 1; }
            else { seterr(err, errsz, "three registers: %s", inner); return 0; }
        }
        // [rax+rsp] -> rsp cannot be an index; swap.
        if (o->index == 4 && o->scale == 1 && o->base >= 0 && o->base != 4) {
            int t = o->base; o->base = o->index; o->index = t;
        }
        o->disp = x.val;
        if (x.has_sym) { o->has_sym = 1; snprintf(o->sym, sizeof o->sym, "%s", x.sym); }
        return 1;
    }
    Expr x;
    if (!parse_expr(s, &x)) { seterr(err, errsz, "operand: %s", s); return 0; }
    if (x.nreg == 1 && x.reg[0].scale == 0 && !x.has_sym && x.val == 0) {
        o->kind = X_REG; o->reg = x.reg[0].num; o->size = x.reg[0].size;
        o->rclass = x.reg[0].rclass; o->hi8 = x.reg[0].hi8;
        return 1;
    }
    if (x.nreg) { seterr(err, errsz, "register in expression: %s", s); return 0; }
    if (x.sym_neg) { seterr(err, errsz, "negated symbol: %s", s); return 0; }
    o->kind = X_IMM; o->imm = x.val;
    if (x.has_sym) { o->has_sym = 1; snprintf(o->sym, sizeof o->sym, "%s", x.sym); }
    return 1;
}

int x64_parse(const char* line, XInsn* in, char* err, size_t errsz) {
    memset(in, 0, sizeof *in);
    char buf[512];
    if (strlen(line) >= sizeof buf) { seterr(err, errsz, "line too long%s", ""); return -1; }
    strcpy(buf, line);
    char* sc = strchr(buf, ';');
    if (sc) *sc = 0;
    char* s = trim(buf);
    if (!*s) { seterr(err, errsz, "empty%s", ""); return -1; }
    for (;;) {
        char w[16]; int i = 0;
        while (s[i] && !isspace((unsigned char)s[i]) && i < 15) { w[i] = s[i]; i++; }
        w[i] = 0;
        if (!strcmp(w, "lock")) in->lock = 1;
        else if (!strcmp(w, "rep") || !strcmp(w, "repe") || !strcmp(w, "repz")) in->rep = 0xF3;
        else if (!strcmp(w, "repne") || !strcmp(w, "repnz")) in->rep = 0xF2;
        else break;
        s = trim(s + i);
    }
    int i = 0;
    while (s[i] && !isspace((unsigned char)s[i]) && i < 15) { in->mnem[i] = (char)tolower((unsigned char)s[i]); i++; }
    in->mnem[i] = 0;
    if (s[i] && !isspace((unsigned char)s[i])) { seterr(err, errsz, "mnemonic: %s", s); return -1; }
    s = trim(s + i);
    if (!*s) return 0;
    // split on top-level commas
    int depth = 0; char* start = s;
    for (char* p = s;; p++) {
        if (*p == '[') depth++;
        else if (*p == ']') depth--;
        if ((*p == ',' && depth == 0) || !*p) {
            char save = *p; *p = 0;
            if (in->nops >= 3) { seterr(err, errsz, "too many operands: %s", line); return -1; }
            if (!parse_operand(start, &in->op[in->nops], err, errsz)) return -1;
            in->nops++;
            if (!save) break;
            start = p + 1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------- encoder core

typedef struct {
    int p66;            // 0x66 operand-size prefix
    int mand;           // mandatory prefix F2/F3 (0 = none)
    int w;              // REX.W
    uint8_t opc[3];
    int nopc;
    int plusr;          // -1: none; else register added to the last opcode byte
    int reg;            // modrm.reg (register or /digit); -1: no modrm
    const XOp* rm;      // register or memory operand
    int force_rex;
    int nimm;           // immediate bytes (0, 1, 2, 4, 8)
    int64_t imm;
    int imm_rel;        // relocation type for the immediate, 0 = none
    const XOp* imm_op;  // the operand that carries the symbol
} Spec;

static void spec_init(Spec* s) { memset(s, 0, sizeof *s); s->plusr = -1; s->reg = -1; }

static int disp_fits8(int64_t d) { return d >= -128 && d <= 127; }

static int emit(XEnc* e, const XInsn* in, const Spec* s0) {
    Spec s2 = *s0;
    const Spec* s = &s2;
    XOp tmp;
    // NASM turns [idx*2] into [idx+idx]
    if (s->rm && s->rm->kind == X_MEM && !s->rm->rip && s->rm->base < 0 && s->rm->index >= 0 && s->rm->scale == 2) {
        tmp = *s->rm; tmp.base = tmp.index; tmp.scale = 1; s2.rm = &tmp;
    }
    memset(e, 0, sizeof *e);
    // XEnc holds one relocation: a symbol in the address and another in the immediate do not fit
    if (s->rm && s->rm->kind == X_MEM && s->rm->has_sym && s->imm_rel) return 0;
    int rex = 0;
    if (s->w) rex |= 8;
    if (s->reg >= 8) rex |= 4;
    if (s->plusr >= 8) rex |= 1;
    if (s->rm) {
        if (s->rm->kind == X_REG) { if (s->rm->reg >= 8) rex |= 1; }
        else {
            if (s->rm->base >= 8) rex |= 1;
            if (s->rm->index >= 8) rex |= 2;
        }
    }
    int need = rex || s->force_rex;
    if (need && s->rm && s->rm->kind == X_REG && s->rm->hi8) return 0;
    if (in->lock) e->b[e->n++] = 0xF0;
    if (in->rep) e->b[e->n++] = (uint8_t)in->rep;
    if (s->mand) e->b[e->n++] = (uint8_t)s->mand;
    if (s->p66) e->b[e->n++] = 0x66;
    if (need) e->b[e->n++] = (uint8_t)(0x40 | rex);
    for (int i = 0; i < s->nopc; i++) e->b[e->n++] = s->opc[i];
    if (s->plusr >= 0) e->b[e->n - 1] = (uint8_t)(e->b[e->n - 1] + (s->plusr & 7));
    if (s->reg >= 0 && s->rm) {
        int r = s->reg & 7;
        const XOp* m = s->rm;
        if (m->kind == X_REG) {
            e->b[e->n++] = (uint8_t)(0xC0 | r << 3 | (m->reg & 7));
        } else {
            int base = m->base, idx = m->index;
            int sc = m->scale == 1 ? 0 : m->scale == 2 ? 1 : m->scale == 4 ? 2 : 3;
            int d32 = m->has_sym;
            int64_t d = m->disp;
            if (idx == 4) return 0;
            if (m->rip) {
                e->b[e->n++] = (uint8_t)(r << 3 | 5);
                e->has_rel = 1; e->rel.off = e->n; e->rel.type = XR_PC32;
                snprintf(e->rel.sym, sizeof e->rel.sym, "%s", m->sym);
                e->rel.addend = d - 4 - s->nimm;
                for (int i = 0; i < 4; i++) e->b[e->n++] = 0;
                goto after_disp;
            }
            if (base < 0 && idx < 0) {
                e->b[e->n++] = (uint8_t)(r << 3 | 4);
                e->b[e->n++] = 0x25;
                d32 = 1;
            } else if (base < 0) {
                e->b[e->n++] = (uint8_t)(r << 3 | 4);
                e->b[e->n++] = (uint8_t)(sc << 6 | (idx & 7) << 3 | 5);
                d32 = 1;
            } else {
                int mod;
                if (!d32 && d == 0 && (base & 7) != 5) mod = 0;
                else if (!d32 && disp_fits8(d)) mod = 1;
                else { mod = 2; d32 = 1; }
                int sib = idx >= 0 || (base & 7) == 4;
                e->b[e->n++] = (uint8_t)(mod << 6 | r << 3 | (sib ? 4 : (base & 7)));
                if (sib) e->b[e->n++] = (uint8_t)(sc << 6 | (idx >= 0 ? (idx & 7) : 4) << 3 | (base & 7));
                if (mod == 1) { e->b[e->n++] = (uint8_t)(int8_t)d; d32 = 0; goto after_disp; }
                if (mod == 0) goto after_disp;
            }
            // disp32
            if (m->has_sym) {
                e->has_rel = 1; e->rel.off = e->n; e->rel.type = XR_32S;
                snprintf(e->rel.sym, sizeof e->rel.sym, "%s", m->sym);
                e->rel.addend = d;
                d = 0;
            } else if (d < INT32_MIN || d > INT32_MAX) return 0;
            for (int i = 0; i < 4; i++) e->b[e->n++] = (uint8_t)((uint64_t)d >> (8 * i));
        after_disp:;
        }
    }
    if (s->nimm) {
        if (s->imm_rel) {
            e->has_rel = 1; e->rel.off = e->n; e->rel.type = s->imm_rel;
            snprintf(e->rel.sym, sizeof e->rel.sym, "%s", s->imm_op->sym);
            e->rel.addend = s->imm_op->imm;
            for (int i = 0; i < s->nimm; i++) e->b[e->n++] = 0;
        } else for (int i = 0; i < s->nimm; i++) e->b[e->n++] = (uint8_t)((uint64_t)s->imm >> (8 * i));
    }
    return e->n;
}

// ---------------------------------------------------------------- helpers

static int is_reg(const XOp* o) { return o->kind == X_REG && o->rclass == RC_GPR; }
static int is_xmm(const XOp* o) { return o->kind == X_REG && o->rclass == RC_XMM; }
static int is_mem(const XOp* o) { return o->kind == X_MEM; }
static int is_imm(const XOp* o) { return o->kind == X_IMM; }
static int rex8(const XOp* o) { return o->kind == X_REG && o->size == 1 && !o->hi8 && o->reg >= 4 && o->reg <= 7; }
static int rex8any(const XOp* a, const XOp* b) {
    return (a && a->kind == X_REG && rex8(a)) || (b && b->kind == X_REG && rex8(b));
}
static int hi8any(const XOp* a, const XOp* b) {
    return (a && a->kind == X_REG && a->hi8) || (b && b->kind == X_REG && b->hi8);
}

static void set_size(Spec* s, int size) {
    if (size == 2) s->p66 = 1;
    if (size == 8) s->w = 1;
}

// operand size of a (reg|mem) vs (reg|mem|imm) pair; 0 when it cannot be told
static int pair_size(const XOp* a, const XOp* b) {
    int sa = a->kind == X_REG ? a->size : a->kind == X_MEM ? a->size : 0;
    int sb = b && (b->kind == X_REG || b->kind == X_MEM) ? b->size : 0;
    // the memory size keyword is optional, but when given it must match
    if (a->kind == X_REG && b && b->kind == X_MEM) return (!sb || sb == sa) ? sa : 0;
    if (b && b->kind == X_REG && a->kind == X_MEM) return (!sa || sa == sb) ? sb : 0;
    if (sa && sb && sa != sb) return 0;
    return sa ? sa : sb;
}

// Wrap an immediate to the operand width the way NASM does.
static int64_t wrap_imm(int64_t v, int size) {
    if (size == 4 && v >= 0x80000000LL && v <= 0xFFFFFFFFLL) return v - 0x100000000LL;
    if (size == 2 && v >= 0x8000 && v <= 0xFFFF) return v - 0x10000;
    if (size == 1 && v >= 0x80 && v <= 0xFF) return v - 0x100;
    return v;
}
static int imm_ok(int64_t v, int size) {
    if (size == 1) return v >= -128 && v <= 255;
    if (size == 2) return v >= -32768 && v <= 65535;
    return v >= INT32_MIN && v <= 0xFFFFFFFFLL && (size == 4 || v <= INT32_MAX);
}

static const char* const CC[] = {"o","no","b","ae","e","ne","be","a","s","ns","p","np","l","ge","le","g"};

// condition code from a suffix, with aliases; -1 when unknown
static int cc_code(const char* s) {
    static const struct { const char* n; int c; } al[] = {
        {"z",4},{"nz",5},{"c",2},{"nc",3},{"nae",2},{"nb",3},{"na",6},{"nbe",7},{"pe",10},{"po",11},
        {"nge",12},{"nl",13},{"ng",14},{"nle",15},{"eq",4}};
    for (int i = 0; i < 16; i++) if (!strcmp(s, CC[i])) return i;
    for (size_t i = 0; i < sizeof al / sizeof al[0]; i++) if (!strcmp(s, al[i].n)) return al[i].c;
    return -1;
}

static const char* const ALU[] = {"add","or","adc","sbb","and","sub","xor","cmp"};
static const char* const SHF[] = {"rol","ror","rcl","rcr","shl","shr","sal","sar"};

// ---------------------------------------------------------------- encode

#define FAIL(msg) do { if (why) *why = (msg); return 0; } while (0)

static int fixed(const char* m, const uint8_t** bytes, int* n, int* prefix66, int* w) {
    static const struct { const char* m; uint8_t b[3]; int n; int p66; int w; } T[] = {
        {"ret",{0xC3},1,0,0}, {"leave",{0xC9},1,0,0}, {"syscall",{0x0F,0x05},2,0,0},
        {"cqo",{0x99},1,0,1}, {"cdq",{0x99},1,0,0}, {"cdqe",{0x98},1,0,1}, {"cld",{0xFC},1,0,0},
        {"std",{0xFD},1,0,0}, {"hlt",{0xF4},1,0,0}, {"ud2",{0x0F,0x0B},2,0,0},
        {"nop",{0x90},1,0,0}, {"int3",{0xCC},1,0,0}, {"pushfq",{0x9C},1,0,0}, {"popfq",{0x9D},1,0,0},
        {"stc",{0xF9},1,0,0}, {"clc",{0xF8},1,0,0}, {"cli",{0xFA},1,0,0}, {"sti",{0xFB},1,0,0},
        {"cpuid",{0x0F,0xA2},2,0,0}, {"rdtsc",{0x0F,0x31},2,0,0}, {"mfence",{0x0F,0xAE,0xF0},3,0,0},
        {"lfence",{0x0F,0xAE,0xE8},3,0,0}, {"sfence",{0x0F,0xAE,0xF8},3,0,0},
        {"movsb",{0xA4},1,0,0}, {"movsw",{0xA5},1,1,0}, {"movsd",{0xA5},1,0,0}, {"movsq",{0xA5},1,0,1},
        {"stosb",{0xAA},1,0,0}, {"stosw",{0xAB},1,1,0}, {"stosd",{0xAB},1,0,0}, {"stosq",{0xAB},1,0,1},
        {"lodsb",{0xAC},1,0,0}, {"lodsq",{0xAD},1,0,1}, {"scasb",{0xAE},1,0,0}, {"scasq",{0xAF},1,0,1},
        {"cmpsb",{0xA6},1,0,0}, {"cmpsq",{0xA7},1,0,1},
    };
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++)
        if (!strcmp(m, T[i].m)) { *bytes = T[i].b; *n = T[i].n; *prefix66 = T[i].p66; *w = T[i].w; return 1; }
    return 0;
}

static int enc_fixed(const XInsn* in, XEnc* e, int* handled) {
    const uint8_t* b; int n, p66, w;
    if (!fixed(in->mnem, &b, &n, &p66, &w)) { *handled = 0; return 0; }
    *handled = 1;
    if (in->nops) return 0;
    Spec s; spec_init(&s);
    s.p66 = p66; s.w = w; s.nopc = n;
    memcpy(s.opc, b, (size_t)n);
    return emit(e, in, &s);
}

int x64_encode(const XInsn* in, XEnc* e, const char** why) {
    const char* m = in->mnem;
    const XOp* a = in->nops > 0 ? &in->op[0] : NULL;
    const XOp* b = in->nops > 1 ? &in->op[1] : NULL;
    const XOp* c = in->nops > 2 ? &in->op[2] : NULL;
    Spec s; spec_init(&s);
    memset(e, 0, sizeof *e);
    if (why) *why = "";

    // pause is `rep nop`
    if (!strcmp(m, "pause") && !in->nops) {
        s.mand = 0xF3; s.nopc = 1; s.opc[0] = 0x90;
        return emit(e, in, &s);
    }
    { int handled; int r = enc_fixed(in, e, &handled); if (handled) { if (!r) FAIL("fixed op with operands"); return r; } }

    // -- ALU: add or adc sbb and sub xor cmp
    for (int n = 0; n < 8; n++) {
        if (strcmp(m, ALU[n]) || in->nops != 2) continue;
        if (hi8any(a, b)) FAIL("ah..bh");
        int sz = pair_size(a, b);
        if (!sz) FAIL("operand size");
        if (sz != 1 && sz != 2 && sz != 4 && sz != 8) FAIL("operand size");
        set_size(&s, sz);
        if (rex8any(a, b)) s.force_rex = 1;
        if ((is_reg(a) || is_mem(a)) && is_reg(b)) {
            s.opc[0] = (uint8_t)(n * 8 + (sz == 1 ? 0 : 1)); s.nopc = 1; s.reg = b->reg; s.rm = a;
            return emit(e, in, &s);
        }
        if (is_reg(a) && is_mem(b)) {
            s.opc[0] = (uint8_t)(n * 8 + (sz == 1 ? 2 : 3)); s.nopc = 1; s.reg = a->reg; s.rm = b;
            return emit(e, in, &s);
        }
        if ((is_reg(a) || is_mem(a)) && is_imm(b)) {
            int64_t v = wrap_imm(b->imm, sz);
            if (b->has_sym) {
                if (sz < 4) FAIL("symbol immediate narrower than 32 bits");
                s.imm_rel = XR_32S; s.imm_op = b; s.nimm = 4; v = 0;
                if (is_reg(a) && a->reg == 0) { s.opc[0] = (uint8_t)(n * 8 + 5); s.nopc = 1; return emit(e, in, &s); }
                s.opc[0] = 0x81; s.nopc = 1; s.reg = n; s.rm = a;
                return emit(e, in, &s);
            }
            if (!imm_ok(b->imm, sz)) FAIL("immediate range");
            int acc = is_reg(a) && a->reg == 0;
            if (sz == 1) {
                if (acc) { s.opc[0] = (uint8_t)(n * 8 + 4); s.nopc = 1; s.nimm = 1; s.imm = v; return emit(e, in, &s); }
                s.opc[0] = 0x80; s.nopc = 1; s.reg = n; s.rm = a; s.nimm = 1; s.imm = v;
                return emit(e, in, &s);
            }
            if (v >= -128 && v <= 127) {
                s.opc[0] = 0x83; s.nopc = 1; s.reg = n; s.rm = a; s.nimm = 1; s.imm = v;
                return emit(e, in, &s);
            }
            s.nimm = sz == 2 ? 2 : 4; s.imm = v;
            if (acc) { s.opc[0] = (uint8_t)(n * 8 + 5); s.nopc = 1; return emit(e, in, &s); }
            s.opc[0] = 0x81; s.nopc = 1; s.reg = n; s.rm = a;
            return emit(e, in, &s);
        }
        FAIL("ALU operand shape");
    }

    // -- mov
    if (!strcmp(m, "mov") && in->nops == 2) {
        if (is_xmm(a) || is_xmm(b)) FAIL("mov with xmm");
        if (hi8any(a, b)) FAIL("ah..bh");
        int sz = pair_size(a, b);
        if (is_reg(a) && is_imm(b)) sz = a->size;
        if (sz != 1 && sz != 2 && sz != 4 && sz != 8) FAIL("operand size");
        if (rex8any(a, b)) s.force_rex = 1;
        if ((is_reg(a) || is_mem(a)) && is_reg(b)) {
            if (is_reg(a) && a->size != b->size) FAIL("operand size mismatch");
            set_size(&s, sz);
            s.opc[0] = sz == 1 ? 0x88 : 0x89; s.nopc = 1; s.reg = b->reg; s.rm = a;
            return emit(e, in, &s);
        }
        if (is_reg(a) && is_mem(b)) {
            set_size(&s, sz);
            s.opc[0] = sz == 1 ? 0x8A : 0x8B; s.nopc = 1; s.reg = a->reg; s.rm = b;
            return emit(e, in, &s);
        }
        if (is_reg(a) && is_imm(b)) {
            s.plusr = a->reg;
            if (sz == 8) {
                if (b->has_sym) {
                    s.w = 1; s.opc[0] = 0xB8; s.nopc = 1; s.nimm = 8; s.imm_rel = XR_64; s.imm_op = b;
                    return emit(e, in, &s);
                }
                int64_t v = b->imm;
                if (v >= 0 && v <= 0xFFFFFFFFLL) { s.opc[0] = 0xB8; s.nopc = 1; s.nimm = 4; s.imm = v; }
                else if (v >= INT32_MIN && v < 0) { s.w = 1; s.plusr = -1; s.opc[0] = 0xC7; s.nopc = 1; s.reg = 0; s.rm = a; s.nimm = 4; s.imm = v; }
                else { s.w = 1; s.opc[0] = 0xB8; s.nopc = 1; s.nimm = 8; s.imm = v; }
                return emit(e, in, &s);
            }
            set_size(&s, sz);
            s.opc[0] = (uint8_t)(sz == 1 ? 0xB0 : 0xB8); s.nopc = 1; s.nimm = sz;
            if (b->has_sym) {
                if (sz != 4) FAIL("symbol immediate narrower than 32 bits");
                s.imm_rel = XR_32; s.imm_op = b;
            } else {
                if (!imm_ok(b->imm, sz)) FAIL("immediate range");
                s.imm = wrap_imm(b->imm, sz);
            }
            return emit(e, in, &s);
        }
        if (is_mem(a) && is_imm(b)) {
            sz = a->size;
            if (sz != 1 && sz != 2 && sz != 4 && sz != 8) FAIL("memory size not given");
            set_size(&s, sz);
            s.opc[0] = sz == 1 ? 0xC6 : 0xC7; s.nopc = 1; s.reg = 0; s.rm = a;
            s.nimm = sz == 8 ? 4 : sz;
            if (b->has_sym) {
                if (sz < 4) FAIL("symbol immediate narrower than 32 bits");
                s.imm_rel = sz == 8 ? XR_32S : XR_32; s.imm_op = b;
            } else {
                if (!imm_ok(b->imm, sz)) FAIL("immediate range");
                s.imm = wrap_imm(b->imm, sz);
            }
            return emit(e, in, &s);
        }
        FAIL("mov operand shape");
    }

    // -- lea
    if (!strcmp(m, "lea") && in->nops == 2 && is_reg(a) && is_mem(b) && a->size >= 2) {
        set_size(&s, a->size);
        s.opc[0] = 0x8D; s.nopc = 1; s.reg = a->reg; s.rm = b;
        return emit(e, in, &s);
    }

    // -- test
    if (!strcmp(m, "test") && in->nops == 2) {
        if (hi8any(a, b)) FAIL("ah..bh");
        int sz = pair_size(a, b);
        if (sz != 1 && sz != 2 && sz != 4 && sz != 8) FAIL("operand size");
        set_size(&s, sz);
        if (rex8any(a, b)) s.force_rex = 1;
        if (is_reg(b) && (is_reg(a) || is_mem(a))) {
            s.opc[0] = sz == 1 ? 0x84 : 0x85; s.nopc = 1; s.reg = b->reg; s.rm = a;
            return emit(e, in, &s);
        }
        if (is_reg(a) && is_mem(b)) {
            s.opc[0] = sz == 1 ? 0x84 : 0x85; s.nopc = 1; s.reg = a->reg; s.rm = b;
            return emit(e, in, &s);
        }
        if ((is_reg(a) || is_mem(a)) && is_imm(b) && !b->has_sym) {
            if (!imm_ok(b->imm, sz)) FAIL("immediate range");
            s.imm = wrap_imm(b->imm, sz);
            s.nimm = sz == 8 ? 4 : sz;
            if (is_reg(a) && a->reg == 0) { s.opc[0] = sz == 1 ? 0xA8 : 0xA9; s.nopc = 1; return emit(e, in, &s); }
            s.opc[0] = sz == 1 ? 0xF6 : 0xF7; s.nopc = 1; s.reg = 0; s.rm = a;
            return emit(e, in, &s);
        }
        FAIL("test operand shape");
    }

    // -- one-operand group: inc dec (FE/FF), not neg mul imul div idiv (F6/F7)
    {
        static const struct { const char* n; int op; int digit; } G[] = {
            {"inc",0xFE,0},{"dec",0xFE,1},{"not",0xF6,2},{"neg",0xF6,3},{"mul",0xF6,4},
            {"imul",0xF6,5},{"div",0xF6,6},{"idiv",0xF6,7}};
        for (size_t i = 0; i < sizeof G / sizeof G[0]; i++) {
            if (strcmp(m, G[i].n) || in->nops != 1) continue;
            if (!(is_reg(a) || is_mem(a))) FAIL("operand shape");
            if (hi8any(a, NULL)) FAIL("ah..bh");
            int sz = a->size;
            if (sz != 1 && sz != 2 && sz != 4 && sz != 8) FAIL("memory size not given");
            set_size(&s, sz);
            if (rex8any(a, NULL)) s.force_rex = 1;
            s.opc[0] = (uint8_t)(G[i].op + (sz == 1 ? 0 : 1)); s.nopc = 1; s.reg = G[i].digit; s.rm = a;
            return emit(e, in, &s);
        }
    }

    // -- imul two and three operands
    if (!strcmp(m, "imul") && (in->nops == 2 || in->nops == 3) && is_reg(a) && a->size >= 2) {
        const XOp* src = b;
        const XOp* imm = in->nops == 3 ? c : (is_imm(b) ? b : NULL);
        if (imm) {
            if (in->nops == 2) src = a;
            if (!(is_reg(src) || is_mem(src)) || !is_imm(imm) || imm->has_sym) FAIL("imul operand shape");
            if (src->size && src->size != a->size) FAIL("operand size mismatch");
            set_size(&s, a->size);
            int64_t v = wrap_imm(imm->imm, a->size);
            if (!imm_ok(imm->imm, a->size)) FAIL("immediate range");
            s.reg = a->reg; s.rm = src; s.nopc = 1;
            if (v >= -128 && v <= 127) { s.opc[0] = 0x6B; s.nimm = 1; }
            else { s.opc[0] = 0x69; s.nimm = a->size == 2 ? 2 : 4; }
            s.imm = v;
            return emit(e, in, &s);
        }
        if (!(is_reg(src) || is_mem(src))) FAIL("imul operand shape");
        if (src->size && src->size != a->size) FAIL("operand size mismatch");
        set_size(&s, a->size);
        s.opc[0] = 0x0F; s.opc[1] = 0xAF; s.nopc = 2; s.reg = a->reg; s.rm = src;
        return emit(e, in, &s);
    }

    // -- shifts and rotates
    for (int n = 0; n < 8; n++) {
        if (strcmp(m, SHF[n]) || in->nops != 2) continue;
        int digit = n == 6 ? 4 : n;   // sal is shl
        if (!(is_reg(a) || is_mem(a))) FAIL("operand shape");
        if (hi8any(a, NULL)) FAIL("ah..bh");
        int sz = a->size;
        if (sz != 1 && sz != 2 && sz != 4 && sz != 8) FAIL("memory size not given");
        set_size(&s, sz);
        if (rex8any(a, NULL)) s.force_rex = 1;
        s.nopc = 1; s.reg = digit; s.rm = a;
        if (is_reg(b) && b->reg == 1 && b->size == 1 && !b->hi8) {
            s.opc[0] = sz == 1 ? 0xD2 : 0xD3;
            return emit(e, in, &s);
        }
        if (is_imm(b) && !b->has_sym) {
            if (b->imm == 1) { s.opc[0] = sz == 1 ? 0xD0 : 0xD1; return emit(e, in, &s); }
            if (b->imm < 0 || b->imm > 255) FAIL("shift count range");
            s.opc[0] = sz == 1 ? 0xC0 : 0xC1; s.nimm = 1; s.imm = b->imm;
            return emit(e, in, &s);
        }
        FAIL("shift count shape");
    }

    // -- push / pop
    if (!strcmp(m, "push") && in->nops == 1) {
        if (is_reg(a) && a->size == 8) { s.opc[0] = 0x50; s.nopc = 1; s.plusr = a->reg; return emit(e, in, &s); }
        if (is_imm(a)) {
            if (a->has_sym) { s.opc[0] = 0x68; s.nopc = 1; s.nimm = 4; s.imm_rel = XR_32S; s.imm_op = a; return emit(e, in, &s); }
            if (a->imm >= -128 && a->imm <= 127) { s.opc[0] = 0x6A; s.nopc = 1; s.nimm = 1; s.imm = a->imm; return emit(e, in, &s); }
            if (a->imm >= INT32_MIN && a->imm <= INT32_MAX) { s.opc[0] = 0x68; s.nopc = 1; s.nimm = 4; s.imm = a->imm; return emit(e, in, &s); }
            FAIL("immediate range");
        }
        if (is_mem(a) && (a->size == 8 || a->size == 0)) { s.opc[0] = 0xFF; s.nopc = 1; s.reg = 6; s.rm = a; return emit(e, in, &s); }
        FAIL("push operand shape");
    }
    if (!strcmp(m, "pop") && in->nops == 1) {
        if (is_reg(a) && a->size == 8) { s.opc[0] = 0x58; s.nopc = 1; s.plusr = a->reg; return emit(e, in, &s); }
        if (is_mem(a) && (a->size == 8 || a->size == 0)) { s.opc[0] = 0x8F; s.nopc = 1; s.reg = 0; s.rm = a; return emit(e, in, &s); }
        FAIL("pop operand shape");
    }

    // -- call / jmp / jcc to a symbol (rel32 with a relocation), or indirect
    {
        int is_call = !strcmp(m, "call"), is_jmp = !strcmp(m, "jmp");
        int cc = (m[0] == 'j' && !is_jmp) ? cc_code(m + 1) : -1;
        if ((is_call || is_jmp || cc >= 0) && in->nops == 1) {
            if (is_imm(a) && a->has_sym) {
                if (is_call) { s.opc[0] = 0xE8; s.nopc = 1; }
                else if (is_jmp) { s.opc[0] = 0xE9; s.nopc = 1; }
                else { s.opc[0] = 0x0F; s.opc[1] = (uint8_t)(0x80 + cc); s.nopc = 2; }
                s.nimm = 4; s.imm_rel = XR_PC32; s.imm_op = a;
                int r = emit(e, in, &s);
                if (r) e->rel.addend = a->imm - 4;
                return r;
            }
            if ((is_call || is_jmp) && (is_reg(a) && a->size == 8)) {
                s.opc[0] = 0xFF; s.nopc = 1; s.reg = is_call ? 2 : 4; s.rm = a;
                return emit(e, in, &s);
            }
            if ((is_call || is_jmp) && is_mem(a) && (a->size == 8 || a->size == 0)) {
                s.opc[0] = 0xFF; s.nopc = 1; s.reg = is_call ? 2 : 4; s.rm = a;
                return emit(e, in, &s);
            }
            FAIL("branch operand shape");
        }
    }

    // -- setcc
    if (m[0] == 's' && m[1] == 'e' && m[2] == 't' && in->nops == 1) {
        int cc = cc_code(m + 3);
        if (cc >= 0 && (is_reg(a) || is_mem(a)) && (a->size == 1)) {
            if (hi8any(a, NULL)) FAIL("ah..bh");
            if (rex8any(a, NULL)) s.force_rex = 1;
            s.opc[0] = 0x0F; s.opc[1] = (uint8_t)(0x90 + cc); s.nopc = 2; s.reg = 0; s.rm = a;
            return emit(e, in, &s);
        }
    }

    // -- cmovcc
    if (m[0] == 'c' && m[1] == 'm' && m[2] == 'o' && m[3] == 'v' && in->nops == 2) {
        int cc = cc_code(m + 4);
        if (cc >= 0 && is_reg(a) && a->size >= 2 && (is_reg(b) || is_mem(b))) {
            set_size(&s, a->size);
            s.opc[0] = 0x0F; s.opc[1] = (uint8_t)(0x40 + cc); s.nopc = 2; s.reg = a->reg; s.rm = b;
            return emit(e, in, &s);
        }
    }

    // -- movzx / movsx / movsxd
    if ((!strcmp(m, "movzx") || !strcmp(m, "movsx")) && in->nops == 2 && is_reg(a) && a->size >= 2) {
        int sx = m[3] == 's';
        if (!(is_reg(b) || is_mem(b))) FAIL("operand shape");
        if (hi8any(b, NULL)) FAIL("ah..bh");
        int ss = b->size;
        if (ss != 1 && ss != 2) FAIL("source size");
        if (ss >= a->size) FAIL("source not narrower");
        set_size(&s, a->size);
        if (rex8any(b, NULL)) s.force_rex = 1;
        s.opc[0] = 0x0F; s.opc[1] = (uint8_t)((sx ? 0xBE : 0xB6) + (ss == 2 ? 1 : 0)); s.nopc = 2;
        s.reg = a->reg; s.rm = b;
        return emit(e, in, &s);
    }
    if (!strcmp(m, "movsxd") && in->nops == 2 && is_reg(a) && a->size == 8 && (is_reg(b) || is_mem(b)) && b->size == 4) {
        // NASM writes movsxd rax, eax as cdqe
        if (is_reg(b) && a->reg == 0 && b->reg == 0) { s.w = 1; s.opc[0] = 0x98; s.nopc = 1; return emit(e, in, &s); }
        s.w = 1; s.opc[0] = 0x63; s.nopc = 1; s.reg = a->reg; s.rm = b;
        return emit(e, in, &s);
    }

    // -- xchg, xadd, cmpxchg
    if (!strcmp(m, "xchg") && in->nops == 2) {
        // NASM puts the first operand in modrm.reg when it is a register
        const XOp* r = is_reg(a) ? a : (is_reg(b) ? b : NULL);
        const XOp* o = r == a ? b : a;
        if (!r || !(is_reg(o) || is_mem(o)) || hi8any(a, b)) FAIL("xchg operand shape");
        int sz = pair_size(a, b);
        if (sz != 1 && sz != 2 && sz != 4 && sz != 8) FAIL("operand size");
        set_size(&s, sz);
        if (rex8any(a, b)) s.force_rex = 1;
        // NASM uses the 90+r short form when one side is the accumulator
        if (sz != 1 && is_reg(a) && is_reg(b) && (a->reg == 0 || b->reg == 0) && !(sz == 4 && a->reg == b->reg)) {
            int other = a->reg == 0 ? b->reg : a->reg;
            if (other == 0) FAIL("xchg rax, rax");
            s.opc[0] = 0x90; s.nopc = 1; s.plusr = other;
            return emit(e, in, &s);
        }
        s.opc[0] = sz == 1 ? 0x86 : 0x87; s.nopc = 1; s.reg = r->reg; s.rm = o;
        return emit(e, in, &s);
    }
    if ((!strcmp(m, "xadd") || !strcmp(m, "cmpxchg")) && in->nops == 2 && is_reg(b) && (is_reg(a) || is_mem(a))) {
        int sz = pair_size(a, b);
        if (sz != 1 && sz != 2 && sz != 4 && sz != 8) FAIL("operand size");
        if (hi8any(a, b)) FAIL("ah..bh");
        set_size(&s, sz);
        if (rex8any(a, b)) s.force_rex = 1;
        s.opc[0] = 0x0F; s.opc[1] = (uint8_t)((m[0] == 'x' ? 0xC0 : 0xB0) + (sz == 1 ? 0 : 1)); s.nopc = 2;
        s.reg = b->reg; s.rm = a;
        return emit(e, in, &s);
    }

    // -- SSE2 integer subset: xmm, xmm/m128
    {
        static const struct { const char* n; int pfx; uint8_t op; } X2[] = {
            {"paddb",0x66,0xFC},{"paddw",0x66,0xFD},{"paddd",0x66,0xFE},{"paddq",0x66,0xD4},
            {"psubb",0x66,0xF8},{"psubd",0x66,0xFA},{"pxor",0x66,0xEF},{"pand",0x66,0xDB},
            {"por",0x66,0xEB},{"punpcklbw",0x66,0x60},{"punpcklwd",0x66,0x61},{"punpckldq",0x66,0x62},
            {"punpcklqdq",0x66,0x6C},{"psadbw",0x66,0xF6},{"pcmpeqb",0x66,0x74}};
        for (size_t i = 0; i < sizeof X2 / sizeof X2[0]; i++) {
            if (strcmp(m, X2[i].n) || in->nops != 2) continue;
            if (!is_xmm(a) || !(is_xmm(b) || (is_mem(b) && (b->size == 16 || b->size == 0)))) FAIL("xmm operand shape");
            s.mand = X2[i].pfx; s.opc[0] = 0x0F; s.opc[1] = X2[i].op; s.nopc = 2; s.reg = a->reg; s.rm = b;
            return emit(e, in, &s);
        }
        if ((!strcmp(m, "movdqu") || !strcmp(m, "movdqa")) && in->nops == 2) {
            int pfx = m[5] == 'u' ? 0xF3 : 0x66;
            if (is_xmm(a) && (is_xmm(b) || is_mem(b))) {
                s.mand = pfx; s.opc[0] = 0x0F; s.opc[1] = 0x6F; s.nopc = 2; s.reg = a->reg; s.rm = b;
                return emit(e, in, &s);
            }
            if (is_mem(a) && is_xmm(b)) {
                s.mand = pfx; s.opc[0] = 0x0F; s.opc[1] = 0x7F; s.nopc = 2; s.reg = b->reg; s.rm = a;
                return emit(e, in, &s);
            }
        }
        if (!strcmp(m, "pshufd") && in->nops == 3 && is_xmm(a) && (is_xmm(b) || is_mem(b)) && is_imm(c) && !c->has_sym) {
            s.mand = 0x66; s.opc[0] = 0x0F; s.opc[1] = 0x70; s.nopc = 2; s.reg = a->reg; s.rm = b;
            s.nimm = 1; s.imm = c->imm;
            return emit(e, in, &s);
        }
        if (!strcmp(m, "psrldq") && in->nops == 2 && is_xmm(a) && is_imm(b) && !b->has_sym) {
            s.mand = 0x66; s.opc[0] = 0x0F; s.opc[1] = 0x73; s.nopc = 2; s.reg = 3; s.rm = a;
            s.nimm = 1; s.imm = b->imm;
            return emit(e, in, &s);
        }
        if ((!strcmp(m, "movd") || !strcmp(m, "movq")) && in->nops == 2) {
            int q = m[3] == 'q';
            if (is_xmm(a) && is_reg(b) && b->size == (q ? 8 : 4)) {
                s.mand = 0x66; s.w = q; s.opc[0] = 0x0F; s.opc[1] = 0x6E; s.nopc = 2; s.reg = a->reg; s.rm = b;
                return emit(e, in, &s);
            }
            if (is_reg(a) && is_xmm(b) && a->size == (q ? 8 : 4)) {
                s.mand = 0x66; s.w = q; s.opc[0] = 0x0F; s.opc[1] = 0x7E; s.nopc = 2; s.reg = b->reg; s.rm = a;
                return emit(e, in, &s);
            }
        }
    }

    FAIL("not covered");
}

// ---------------------------------------------------------------- branches

int x64_branch_size(const char* m, int short_form) {
    if (short_form) return 2;
    if (!strcmp(m, "jmp") || !strcmp(m, "call")) return 5;
    return 6;
}

int x64_encode_branch(const char* m, int short_form, int32_t rel, XEnc* e) {
    memset(e, 0, sizeof *e);
    int is_call = !strcmp(m, "call"), is_jmp = !strcmp(m, "jmp");
    int cc = (m[0] == 'j' && !is_jmp) ? cc_code(m + 1) : -1;
    if (!is_call && !is_jmp && cc < 0) return 0;
    if (short_form) {
        if (is_call || rel < -128 || rel > 127) return 0;
        e->b[e->n++] = is_jmp ? 0xEB : (uint8_t)(0x70 + cc);
        e->b[e->n++] = (uint8_t)(int8_t)rel;
        return e->n;
    }
    if (is_call) e->b[e->n++] = 0xE8;
    else if (is_jmp) e->b[e->n++] = 0xE9;
    else { e->b[e->n++] = 0x0F; e->b[e->n++] = (uint8_t)(0x80 + cc); }
    for (int i = 0; i < 4; i++) e->b[e->n++] = (uint8_t)((uint32_t)rel >> (8 * i));
    return e->n;
}

// ---------------------------------------------------------------- shape

static void shape_op(const XOp* o, char* out, size_t n) {
    switch (o->kind) {
    case X_REG:
        if (o->rclass == RC_XMM) snprintf(out, n, "xmm");
        else snprintf(out, n, "r%d", o->size * 8);
        break;
    case X_IMM:
        snprintf(out, n, o->has_sym ? "sym" : "imm");
        break;
    case X_MEM: {
        char sz[8] = "";
        if (o->size) snprintf(sz, sizeof sz, "%d", o->size * 8);
        snprintf(out, n, "m%s", sz);
        break;
    }
    default: snprintf(out, n, "?");
    }
}

void x64_shape(const XInsn* in, char* out, size_t outsz) {
    size_t k = (size_t)snprintf(out, outsz, "%s%s%s", in->lock ? "lock " : "", in->rep ? "rep " : "", in->mnem);
    for (int i = 0; i < in->nops && k < outsz; i++) {
        char t[16];
        shape_op(&in->op[i], t, sizeof t);
        k += (size_t)snprintf(out + k, outsz - k, "%s%s", i ? "," : " ", t);
    }
}
