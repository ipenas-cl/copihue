/* x64enc: codificación de instrucciones. Ver x64enc.h y docs/DESIGN_NATIVE.md.
 *
 * Reglas para coincidir byte a byte con `nasm -f bin`:
 *  - reg,reg usa la forma "r/m, r" (89, 01, 31...); reg,mem la forma "r, r/m".
 *  - ALU con inmediato: 83 /n ib si cabe en int8; si no, la forma corta del
 *    acumulador (05, 3D...) para rax/eax/ax; si no 81 /n id. Para al, siempre
 *    la forma corta (04, 3C...).
 *  - mov r64, imm: B8+r de 32 bits si cabe sin signo, C7 /0 si cabe con signo,
 *    B8+r de 64 bits en otro caso.
 *  - Los desplazamientos de memoria usan disp8 si caben; una `equ` conocida al
 *    final de la función se trata con su valor final (nasm también la encoge).
 */
#include "x64enc.h"

#include <string.h>

typedef struct {
    uint8_t b[16];
    int n;
    const char* why;
} Out;

static int fail(Out* o, const char* why) { o->why = why; return -1; }

/* ---------- operandos ---------- */

static int is_r(const NxOperand* o, int size) {
    return o->kind == NX_REG && o->rclass == NX_GPR && o->size == size && !(size == 8 && o->high8);
}
static int is_xmm(const NxOperand* o) { return o->kind == NX_REG && o->rclass == NX_XMM; }
static int is_m(const NxOperand* o) { return o->kind == NX_MEM; }
static int is_i(const NxOperand* o) { return o->kind == NX_IMM; }

/* Reemplaza los símbolos por su valor. Devuelve 0 o -1. */
static int fold(Out* o, NxOperand* x, NxResolve resolve, void* ctx) {
    if (!x->sym[0]) return 0;
    int64_t v;
    if (!resolve || resolve(ctx, x->sym, &v) != 0) return fail(o, "simbolo no constante (necesita etiqueta o relocacion)");
    x->val += x->symneg ? -v : v;
    x->sym[0] = 0;
    if (x->kind == NX_SYM) x->kind = NX_IMM;
    return 0;
}

static int fits8(int64_t v) { return v >= -128 && v <= 127; }
static int fits32(int64_t v) { return v >= INT32_MIN && v <= INT32_MAX; }

/* ---------- emisor genérico ---------- */

typedef struct {
    int lockrep;      /* 0 o F0/F2/F3 */
    int p66;          /* prefijo de tamaño de operando */
    int mand;         /* prefijo obligatorio SSE (66/F3), 0 si ninguno */
    int w;            /* REX.W */
    int regf;         /* campo reg (0..15) o /dígito */
    const NxOperand* rm;
    uint8_t opc[3]; int nopc;
    int64_t imm; int immsz;   /* bytes de inmediato: 0, 1, 2, 4, 8 */
    int byteregs;     /* algún registro de 8 bits que exige REX (spl, sil...) */
    int high8;        /* algún ah/ch/dh/bh: prohíbe REX */
} Enc;

static void put(Out* o, int b) { o->b[o->n++] = (uint8_t)b; }

static int need_rex_byte(const NxOperand* x) {
    return x->kind == NX_REG && x->rclass == NX_GPR && x->size == 8 && !x->high8 && x->reg >= 4 && x->reg <= 7;
}

static int emit_modrm(Out* o, Enc* e) {
    const NxOperand* m = e->rm;
    int rex = 0x40 | (e->w ? 8 : 0) | ((e->regf & 8) ? 4 : 0);
    int modrm, sib = -1, dispsz = 0;
    int64_t disp = 0;

    if (m->kind == NX_REG) {
        if (m->reg & 8) rex |= 1;
        modrm = 0xC0 | ((e->regf & 7) << 3) | (m->reg & 7);
    } else {
        int base = m->base, index = m->index, scale = m->scale;
        if (base == -2) return fail(o, "memoria [rel ...] necesita relocacion");
        disp = m->val;
        if (!fits32(disp)) return fail(o, "desplazamiento fuera de int32");
        if (index >= 0 && base < 0 && scale == 1) { base = index; index = -1; }
        if (index >= 0 && (index & 15) == 4) return fail(o, "rsp como indice");
        if (index >= 0 && base < 0 && scale == 2) return fail(o, "indice*2 sin base (nasm lo reescribe)");
        if (base < 0 && index < 0) {            /* [disp32] absoluto */
            modrm = ((e->regf & 7) << 3) | 4; sib = 0x25; dispsz = 4;
        } else if (base < 0) {                  /* [index*s + disp32] */
            modrm = ((e->regf & 7) << 3) | 4;
            sib = (scale == 2 ? 1 : scale == 4 ? 2 : 3) << 6 | ((index & 7) << 3) | 5;
            if (index & 8) rex |= 2;
            dispsz = 4;
        } else {
            int mod;
            if (disp == 0 && (base & 7) != 5) mod = 0, dispsz = 0;
            else if (fits8(disp)) mod = 1, dispsz = 1;
            else mod = 2, dispsz = 4;
            if (base & 8) rex |= 1;
            if (index >= 0 || (base & 7) == 4) {
                int ss = scale == 1 ? 0 : scale == 2 ? 1 : scale == 4 ? 2 : 3;
                int idx = index >= 0 ? (index & 7) : 4;
                if (index >= 0 && (index & 8)) rex |= 2;
                modrm = (mod << 6) | ((e->regf & 7) << 3) | 4;
                sib = (ss << 6) | (idx << 3) | (base & 7);
            } else {
                modrm = (mod << 6) | ((e->regf & 7) << 3) | (base & 7);
            }
        }
    }

    if (e->lockrep) put(o, e->lockrep);
    if (e->p66) put(o, 0x66);
    if (e->mand) put(o, e->mand);
    if (rex != 0x40 || e->byteregs) {
        if (e->high8) return fail(o, "ah/ch/dh/bh con REX");
        put(o, rex);
    }
    for (int i = 0; i < e->nopc; i++) put(o, e->opc[i]);
    put(o, modrm);
    if (sib >= 0) put(o, sib);
    for (int i = 0; i < dispsz; i++) put(o, (int)((disp >> (8 * i)) & 0xFF));
    for (int i = 0; i < e->immsz; i++) put(o, (int)((e->imm >> (8 * i)) & 0xFF));
    return o->n;
}

static void enc_init(Enc* e) { memset(e, 0, sizeof *e); }

/* Marca prefijos y REX de bytes según los operandos. */
static void note_regs(Enc* e, const NxOperand* a, const NxOperand* b) {
    if (need_rex_byte(a) || (b && need_rex_byte(b))) e->byteregs = 1;
    if ((a->kind == NX_REG && a->high8) || (b && b->kind == NX_REG && b->high8)) e->high8 = 1;
}

/* Operación "op1 en r/m, op2 en reg" o la inversa, con opcode fijo. */
static int rm_reg(Out* o, const NxInsn* in, int opc8, int opc, const NxOperand* regop, const NxOperand* rmop, int size, int lockrep) {
    Enc e; enc_init(&e);
    e.lockrep = lockrep;
    e.p66 = size == 16; e.w = size == 64;
    e.regf = regop->reg; e.rm = rmop;
    e.opc[0] = (uint8_t)(size == 8 ? opc8 : opc); e.nopc = 1;
    note_regs(&e, regop, rmop);
    (void)in;
    return emit_modrm(o, &e);
}

/* Operando r/m con opcode de uno o dos bytes y /dígito. */
static int rm_digit(Out* o, int size, int p66, int w, const uint8_t* opc, int nopc, int digit,
                    const NxOperand* rmop, int64_t imm, int immsz, int lockrep) {
    Enc e; enc_init(&e);
    e.lockrep = lockrep; e.p66 = p66; e.w = w;
    e.regf = digit; e.rm = rmop;
    memcpy(e.opc, opc, (size_t)nopc); e.nopc = nopc;
    e.imm = imm; e.immsz = immsz;
    note_regs(&e, rmop, NULL);
    (void)size;
    return emit_modrm(o, &e);
}

/* Registro embebido en el opcode (B8+r, 50+r, 58+r). */
static int reg_in_opc(Out* o, int p66, int w, int base, int reg, int64_t imm, int immsz, int byteregs) {
    int rex = 0x40 | (w ? 8 : 0) | ((reg & 8) ? 1 : 0);
    if (p66) put(o, 0x66);
    if (rex != 0x40 || byteregs) put(o, rex);
    put(o, base + (reg & 7));
    for (int i = 0; i < immsz; i++) put(o, (int)((imm >> (8 * i)) & 0xFF));
    return o->n;
}

/* ---------- tablas ---------- */

static const struct { const char* n; int idx; } k_alu[] = {
    {"add",0},{"or",1},{"adc",2},{"sbb",3},{"and",4},{"sub",5},{"xor",6},{"cmp",7},
};
static const struct { const char* n; int digit; } k_shift[] = {
    {"rol",0},{"ror",1},{"shl",4},{"sal",4},{"shr",5},{"sar",7},
};
static const struct { const char* n; int digit; } k_grp3[] = {   /* F6/F7 */
    {"not",2},{"neg",3},{"mul",4},{"div",6},{"idiv",7},
};
static const struct { const char* n; int cc; } k_cc[] = {
    {"o",0},{"no",1},{"b",2},{"c",2},{"nae",2},{"ae",3},{"nb",3},{"nc",3},{"e",4},{"z",4},{"ne",5},{"nz",5},
    {"be",6},{"na",6},{"a",7},{"nbe",7},{"s",8},{"ns",9},{"p",10},{"np",11},{"l",12},{"nge",12},
    {"ge",13},{"nl",13},{"le",14},{"ng",14},{"g",15},{"nle",15},
};
static const struct { const char* n; const uint8_t b[3]; int len; } k_zero[] = {
    {"syscall",{0x0F,0x05},2},{"ret",{0xC3},1},{"leave",{0xC9},1},{"hlt",{0xF4},1},{"cld",{0xFC},1},
    {"std",{0xFD},1},{"ud2",{0x0F,0x0B},2},{"pause",{0xF3,0x90},2},{"mfence",{0x0F,0xAE,0xF0},3},
    {"lfence",{0x0F,0xAE,0xE8},3},{"sfence",{0x0F,0xAE,0xF8},3},{"pushfq",{0x9C},1},{"popfq",{0x9D},1},
    {"stc",{0xF9},1},{"clc",{0xF8},1},{"cpuid",{0x0F,0xA2},2},{"cqo",{0x48,0x99},2},{"cdq",{0x99},1},
    {"cdqe",{0x48,0x98},2},{"nop",{0x90},1},{"int3",{0xCC},1},{"rdtsc",{0x0F,0x31},2},
};
static const struct { const char* n; int mand; int opc2; } k_sse[] = {   /* xmm, xmm/m128 */
    {"pxor",0x66,0xEF},{"paddb",0x66,0xFC},{"paddw",0x66,0xFD},{"paddd",0x66,0xFE},{"paddq",0x66,0xD4},
    {"punpcklbw",0x66,0x60},{"punpcklwd",0x66,0x61},{"punpckldq",0x66,0x62},{"psadbw",0x66,0xF6},
    {"pcmpeqb",0x66,0x74},{"por",0x66,0xEB},{"pand",0x66,0xDB},
};

static int cc_of(const char* s) {
    for (size_t i = 0; i < sizeof k_cc / sizeof k_cc[0]; i++) if (!strcmp(k_cc[i].n, s)) return k_cc[i].cc;
    return -1;
}

static int op_size(const NxOperand* a, const NxOperand* b) {
    if (a->kind == NX_REG && a->rclass == NX_GPR) return a->size;
    if (b && b->kind == NX_REG && b->rclass == NX_GPR) return b->size;
    if (a->kind == NX_MEM && a->size) return a->size;
    if (b && b->kind == NX_MEM && b->size) return b->size;
    return 0;
}

/* ¿Coincide el tamaño declarado de la memoria con `size`? (0 = no declarado) */
static int mem_ok(const NxOperand* m, int size) { return m->kind != NX_MEM || m->size == 0 || m->size == size; }

/* ---------- instrucciones de dos operandos con inmediato ---------- */

static int imm_range(int64_t v, int size) {
    if (size == 8) return v >= -128 && v <= 255;
    if (size == 16) return v >= -32768 && v <= 65535;
    if (size == 32) return v >= INT32_MIN && v <= (int64_t)UINT32_MAX;
    return fits32(v);
}

static int64_t norm(int64_t v, int size) {         /* imm tal como se firma en su tamaño */
    if (size == 8) return (int8_t)v;
    if (size == 16) return (int16_t)v;
    if (size == 32) return (int32_t)v;
    return v;
}

static int alu_imm(Out* o, int idx, const NxOperand* d, int64_t v, int size, int lockrep) {
    if (!imm_range(v, size)) return fail(o, "inmediato fuera de rango");
    int64_t sv = norm(v, size);
    int p66 = size == 16, w = size == 64;
    int isacc = d->kind == NX_REG && d->reg == 0;
    uint8_t opc[1];
    int immsz = size == 8 ? 1 : size == 16 ? 2 : 4;
    if (size == 8) {
        if (isacc) {   /* op al, imm8 */
            if (lockrep) put(o, lockrep);
            put(o, idx * 8 + 4); put(o, (int)(sv & 0xFF));
            return o->n;
        }
        opc[0] = 0x80;
        return rm_digit(o, size, 0, 0, opc, 1, idx, d, sv, 1, lockrep);
    }
    if (fits8(sv)) { opc[0] = 0x83; return rm_digit(o, size, p66, w, opc, 1, idx, d, sv, 1, lockrep); }
    if (isacc) {
        if (lockrep) put(o, lockrep);
        if (p66) put(o, 0x66);
        if (w) put(o, 0x48);
        put(o, idx * 8 + 5);
        for (int i = 0; i < immsz; i++) put(o, (int)((sv >> (8 * i)) & 0xFF));
        return o->n;
    }
    opc[0] = 0x81;
    return rm_digit(o, size, p66, w, opc, 1, idx, d, sv, immsz, lockrep);
}

int nx_encode(const NxInsn* ins, uint8_t* out, NxResolve resolve, void* ctx, const char** why) {
    Out o; o.n = 0; o.why = NULL;
    NxInsn in = *ins;
    int r = -1;
    const char* m = in.mnem;
    int lk = !strcmp(in.prefix, "lock") ? 0xF0 : !strcmp(in.prefix, "rep") || !strcmp(in.prefix, "repe") ? 0xF3 :
             !strcmp(in.prefix, "repne") ? 0xF2 : 0;
    int n = in.nops;

    /* ramas: necesitan etiquetas y relajación (siguiente porción) */
    if (m[0] == 'j' || !strcmp(m, "call")) {
        if (n == 1 && in.op[0].kind == NX_REG && in.op[0].rclass == NX_GPR && in.op[0].size == 64 &&
            (!strcmp(m, "call") || !strcmp(m, "jmp"))) {
            uint8_t opc = 0xFF;
            r = rm_digit(&o, 64, 0, 0, &opc, 1, !strcmp(m, "call") ? 2 : 4, &in.op[0], 0, 0, lk);
            goto done;
        }
        r = fail(&o, "salto o llamada a etiqueta (pendiente: etiquetas y relajacion)");
        goto done;
    }

    /* equ constante: se pliega; etiqueta: relocacion (pendiente) */
    for (int i = 0; i < n; i++)
        if (fold(&o, &in.op[i], resolve, ctx) < 0) { r = -1; goto done; }

    if (n == 0) {
        for (size_t i = 0; i < sizeof k_zero / sizeof k_zero[0]; i++)
            if (!strcmp(k_zero[i].n, m)) {
                if (lk && !(!strcmp(m, "nop"))) { /* rep sin cadena: no es nuestro caso */ }
                for (int j = 0; j < k_zero[i].len; j++) put(&o, k_zero[i].b[j]);
                r = o.n; goto done;
            }
        static const struct { const char* n; const uint8_t b[2]; int len; } str[] = {
            {"movsb",{0xA4},1},{"movsq",{0x48,0xA5},2},{"movsd",{0xA5},1},{"stosb",{0xAA},1},
            {"stosq",{0x48,0xAB},2},{"stosd",{0xAB},1},{"stosw",{0x66,0xAB},2},{"movsw",{0x66,0xA5},2},
        };
        for (size_t i = 0; i < sizeof str / sizeof str[0]; i++)
            if (!strcmp(str[i].n, m)) {
                if (lk) put(&o, lk);
                for (int j = 0; j < str[i].len; j++) put(&o, str[i].b[j]);
                r = o.n; goto done;
            }
        r = fail(&o, "mnemonico sin operandos no implementado"); goto done;
    }

    const NxOperand *a = &in.op[0], *b = n > 1 ? &in.op[1] : NULL, *c = n > 2 ? &in.op[2] : NULL;
    int size = op_size(a, b);

    /* ---- ALU: add or adc sbb and sub xor cmp ---- */
    for (size_t i = 0; i < sizeof k_alu / sizeof k_alu[0]; i++) {
        if (strcmp(k_alu[i].n, m)) continue;
        int idx = k_alu[i].idx;
        if (n != 2) { r = fail(&o, "ALU: se esperaban 2 operandos"); goto done; }
        if (!size) { r = fail(&o, "ALU: tamano de operando indeterminado"); goto done; }
        if ((a->kind == NX_REG && a->rclass != NX_GPR) || (b->kind == NX_REG && b->rclass != NX_GPR)) { r = fail(&o, "ALU: registro no entero"); goto done; }
        if (is_i(b) && (a->kind == NX_REG || is_m(a))) {
            if (is_m(a) && !a->size) { r = fail(&o, "ALU: memoria sin tamano"); goto done; }
            r = alu_imm(&o, idx, a, b->val, size, lk); goto done;
        }
        if (a->kind == NX_REG && b->kind == NX_REG && a->size == b->size)
            { r = rm_reg(&o, &in, idx * 8, idx * 8 + 1, b, a, size, lk); goto done; }
        if (a->kind == NX_REG && is_m(b) && mem_ok(b, size))
            { r = rm_reg(&o, &in, idx * 8 + 2, idx * 8 + 3, a, b, size, lk); goto done; }
        if (is_m(a) && b->kind == NX_REG && mem_ok(a, size))
            { r = rm_reg(&o, &in, idx * 8, idx * 8 + 1, b, a, size, lk); goto done; }
        r = fail(&o, "ALU: combinacion de operandos no implementada"); goto done;
    }

    /* ---- mov ---- */
    if (!strcmp(m, "mov") && n == 2) {
        if (a->kind == NX_REG && a->rclass == NX_GPR && b->kind == NX_REG && b->rclass == NX_GPR && a->size == b->size)
            { r = rm_reg(&o, &in, 0x88, 0x89, b, a, size, lk); goto done; }
        if (a->kind == NX_REG && a->rclass == NX_GPR && is_m(b) && mem_ok(b, a->size))
            { r = rm_reg(&o, &in, 0x8A, 0x8B, a, b, size, lk); goto done; }
        if (is_m(a) && b->kind == NX_REG && b->rclass == NX_GPR && mem_ok(a, b->size))
            { r = rm_reg(&o, &in, 0x88, 0x89, b, a, size, lk); goto done; }
        if (a->kind == NX_REG && a->rclass == NX_GPR && is_i(b)) {
            int64_t v = b->val; int s = a->size, br = need_rex_byte(a);
            if (a->high8) { r = fail(&o, "mov ah/ch/dh/bh no implementado"); goto done; }
            if (s == 8) { if (!imm_range(v, 8)) { r = fail(&o, "inmediato fuera de rango"); goto done; } r = reg_in_opc(&o, 0, 0, 0xB0, a->reg, v, 1, br); goto done; }
            if (s == 16) { if (!imm_range(v, 16)) { r = fail(&o, "inmediato fuera de rango"); goto done; } r = reg_in_opc(&o, 1, 0, 0xB8, a->reg, v, 2, 0); goto done; }
            if (s == 32) { if (!imm_range(v, 32)) { r = fail(&o, "inmediato fuera de rango"); goto done; } r = reg_in_opc(&o, 0, 0, 0xB8, a->reg, v, 4, 0); goto done; }
            if (v >= 0 && v <= (int64_t)UINT32_MAX) { r = reg_in_opc(&o, 0, 0, 0xB8, a->reg, v, 4, 0); goto done; }
            if (fits32(v)) { uint8_t opc = 0xC7; r = rm_digit(&o, 64, 0, 1, &opc, 1, 0, a, v, 4, lk); goto done; }
            r = reg_in_opc(&o, 0, 1, 0xB8, a->reg, v, 8, 0); goto done;
        }
        if (is_m(a) && is_i(b)) {
            int s = a->size;
            if (!s) { r = fail(&o, "mov m, imm sin tamano"); goto done; }
            if (!imm_range(b->val, s)) { r = fail(&o, "inmediato fuera de rango"); goto done; }
            uint8_t opc = s == 8 ? 0xC6 : 0xC7;
            r = rm_digit(&o, s, s == 16, s == 64, &opc, 1, 0, a, norm(b->val, s), s == 8 ? 1 : s == 16 ? 2 : 4, lk);
            goto done;
        }
        r = fail(&o, "mov: combinacion de operandos no implementada"); goto done;
    }

    /* ---- test ---- */
    if (!strcmp(m, "test") && n == 2 && size) {
        if (a->kind == NX_REG && b->kind == NX_REG && a->size == b->size && a->rclass == NX_GPR)
            { r = rm_reg(&o, &in, 0x84, 0x85, b, a, size, lk); goto done; }
        if (is_m(a) && b->kind == NX_REG && b->rclass == NX_GPR && mem_ok(a, size))
            { r = rm_reg(&o, &in, 0x84, 0x85, b, a, size, lk); goto done; }
        r = fail(&o, "test: combinacion de operandos no implementada"); goto done;
    }

    /* ---- xchg / xadd / cmpxchg (memoria, registro) y registro,registro ---- */
    if (!strcmp(m, "xchg") && n == 2 && a->kind == NX_REG && b->kind == NX_REG && a->rclass == NX_GPR &&
        a->size == b->size && a->size >= 32 && (a->reg == 0 || b->reg == 0) && !(a->reg == 0 && b->reg == 0)) {
        /* xchg con el acumulador: 90+r (nasm la prefiere a 87 /r) */
        int other = a->reg == 0 ? b->reg : a->reg;
        r = reg_in_opc(&o, 0, a->size == 64, 0x90, other, 0, 0, 0);
        goto done;
    }
    if ((!strcmp(m, "xchg") || !strcmp(m, "xadd") || !strcmp(m, "cmpxchg")) && n == 2 && size) {
        int x = !strcmp(m, "xchg");
        int two = !x;
        int o8 = x ? 0x86 : (!strcmp(m, "xadd") ? 0xC0 : 0xB0);
        int o16 = x ? 0x87 : o8 + 1;
        const NxOperand* rmop = a; const NxOperand* rg = b;
        if (x && is_m(b) && a->kind == NX_REG) { rmop = b; rg = a; }
        if ((is_m(rmop) || rmop->kind == NX_REG) && rg->kind == NX_REG && rg->rclass == NX_GPR && mem_ok(rmop, size) &&
            (rmop->kind == NX_MEM || rmop->size == rg->size)) {
            Enc e; enc_init(&e);
            e.lockrep = lk; e.p66 = size == 16; e.w = size == 64; e.regf = rg->reg; e.rm = rmop;
            if (two) { e.opc[0] = 0x0F; e.opc[1] = (uint8_t)(size == 8 ? o8 : o16); e.nopc = 2; }
            else { e.opc[0] = (uint8_t)(size == 8 ? o8 : o16); e.nopc = 1; }
            note_regs(&e, rg, rmop);
            r = emit_modrm(&o, &e); goto done;
        }
        r = fail(&o, "xchg/xadd/cmpxchg: combinacion no implementada"); goto done;
    }

    /* ---- push / pop ---- */
    if (!strcmp(m, "push") && n == 1) {
        if (is_r(a, 64)) { r = reg_in_opc(&o, 0, 0, 0x50, a->reg, 0, 0, 0); goto done; }
        if (is_i(a)) {
            if (!fits32(a->val)) { r = fail(&o, "inmediato fuera de rango"); goto done; }
            if (fits8(a->val)) { put(&o, 0x6A); put(&o, (int)(a->val & 0xFF)); r = o.n; goto done; }
            put(&o, 0x68); for (int i = 0; i < 4; i++) put(&o, (int)((a->val >> (8 * i)) & 0xFF));
            r = o.n; goto done;
        }
        r = fail(&o, "push: operando no implementado"); goto done;
    }
    if (!strcmp(m, "pop") && n == 1) {
        if (is_r(a, 64)) { r = reg_in_opc(&o, 0, 0, 0x58, a->reg, 0, 0, 0); goto done; }
        r = fail(&o, "pop: operando no implementado"); goto done;
    }

    /* ---- lea ---- */
    if (!strcmp(m, "lea") && n == 2 && a->kind == NX_REG && a->rclass == NX_GPR && is_m(b) && a->size >= 16) {
        r = rm_reg(&o, &in, 0x8D, 0x8D, a, b, a->size, 0); goto done;
    }

    /* ---- inc dec ---- */
    if ((!strcmp(m, "inc") || !strcmp(m, "dec")) && n == 1 && size) {
        int d = !strcmp(m, "dec");
        if (is_m(a) && !a->size) { r = fail(&o, "inc/dec memoria sin tamano"); goto done; }
        uint8_t opc = size == 8 ? 0xFE : 0xFF;
        r = rm_digit(&o, size, size == 16, size == 64, &opc, 1, d, a, 0, 0, lk); goto done;
    }

    /* ---- not neg mul div idiv ---- */
    for (size_t i = 0; i < sizeof k_grp3 / sizeof k_grp3[0]; i++) {
        if (strcmp(k_grp3[i].n, m) || n != 1) continue;
        if (!size || (is_m(a) && !a->size)) { r = fail(&o, "tamano indeterminado"); goto done; }
        uint8_t opc = size == 8 ? 0xF6 : 0xF7;
        r = rm_digit(&o, size, size == 16, size == 64, &opc, 1, k_grp3[i].digit, a, 0, 0, lk); goto done;
    }

    /* ---- imul ---- */
    if (!strcmp(m, "imul")) {
        if (n == 2 && a->kind == NX_REG && a->rclass == NX_GPR && a->size >= 16 && (b->kind == NX_REG || is_m(b))) {
            if ((b->kind == NX_REG && b->size != a->size) || (is_m(b) && !mem_ok(b, a->size))) { r = fail(&o, "imul: tamanos"); goto done; }
            Enc e; enc_init(&e);
            e.p66 = a->size == 16; e.w = a->size == 64; e.regf = a->reg; e.rm = b;
            e.opc[0] = 0x0F; e.opc[1] = 0xAF; e.nopc = 2;
            r = emit_modrm(&o, &e); goto done;
        }
        const NxOperand* src = NULL; const NxOperand* im = NULL;
        if (n == 3 && a->kind == NX_REG && a->rclass == NX_GPR && a->size >= 16 && (b->kind == NX_REG || is_m(b)) && is_i(c)) { src = b; im = c; }
        else if (n == 2 && a->kind == NX_REG && a->rclass == NX_GPR && a->size >= 16 && is_i(b)) { src = a; im = b; }
        if (src) {
            int s = a->size;
            if (!imm_range(im->val, s == 16 ? 16 : 32) || (s == 64 && !fits32(im->val))) { r = fail(&o, "imul: inmediato fuera de rango"); goto done; }
            int64_t sv = norm(im->val, s == 16 ? 16 : 32);
            Enc e; enc_init(&e);
            e.p66 = s == 16; e.w = s == 64; e.regf = a->reg; e.rm = src;
            e.opc[0] = fits8(sv) ? 0x6B : 0x69; e.nopc = 1;
            e.imm = sv; e.immsz = fits8(sv) ? 1 : (s == 16 ? 2 : 4);
            r = emit_modrm(&o, &e); goto done;
        }
        r = fail(&o, "imul: combinacion no implementada"); goto done;
    }

    /* ---- desplazamientos ---- */
    for (size_t i = 0; i < sizeof k_shift / sizeof k_shift[0]; i++) {
        if (strcmp(k_shift[i].n, m) || n != 2 || !size) continue;
        if (is_m(a) && !a->size) { r = fail(&o, "shift memoria sin tamano"); goto done; }
        int d = k_shift[i].digit;
        uint8_t opc[1];
        if (b->kind == NX_REG && b->rclass == NX_GPR && b->size == 8 && b->reg == 1 && !b->high8) {
            opc[0] = size == 8 ? 0xD2 : 0xD3;
            r = rm_digit(&o, size, size == 16, size == 64, opc, 1, d, a, 0, 0, lk); goto done;
        }
        if (is_i(b)) {
            if (b->val < 0 || b->val > 255) { r = fail(&o, "shift: cuenta fuera de rango"); goto done; }
            if (b->val == 1) { opc[0] = size == 8 ? 0xD0 : 0xD1; r = rm_digit(&o, size, size == 16, size == 64, opc, 1, d, a, 0, 0, lk); goto done; }
            opc[0] = size == 8 ? 0xC0 : 0xC1;
            r = rm_digit(&o, size, size == 16, size == 64, opc, 1, d, a, b->val, 1, lk); goto done;
        }
        r = fail(&o, "shift: combinacion no implementada"); goto done;
    }

    /* ---- movzx movsx movsxd ---- */
    if ((!strcmp(m, "movzx") || !strcmp(m, "movsx")) && n == 2 && a->kind == NX_REG && a->rclass == NX_GPR && a->size >= 16) {
        int z = !strcmp(m, "movzx");
        int ssz = b->kind == NX_REG ? b->size : b->size;
        if ((b->kind == NX_REG && (b->rclass != NX_GPR || b->high8)) || (ssz != 8 && ssz != 16) || ssz >= a->size) { r = fail(&o, "movzx/movsx: operando no implementado"); goto done; }
        Enc e; enc_init(&e);
        e.p66 = a->size == 16; e.w = a->size == 64; e.regf = a->reg; e.rm = b;
        e.opc[0] = 0x0F; e.opc[1] = (uint8_t)((z ? 0xB6 : 0xBE) + (ssz == 16 ? 1 : 0)); e.nopc = 2;
        note_regs(&e, a, b);
        r = emit_modrm(&o, &e); goto done;
    }
    if (!strcmp(m, "movsxd") && n == 2 && is_r(a, 64) && is_r(b, 32) && a->reg == 0 && b->reg == 0) {
        put(&o, 0x48); put(&o, 0x98);           /* nasm lo reduce a cdqe */
        r = o.n; goto done;
    }
    if (!strcmp(m, "movsxd") && n == 2 && is_r(a, 64) && ((b->kind == NX_REG && is_r(b, 32)) || (is_m(b) && mem_ok(b, 32)))) {
        Enc e; enc_init(&e);
        e.w = 1; e.regf = a->reg; e.rm = b; e.opc[0] = 0x63; e.nopc = 1;
        r = emit_modrm(&o, &e); goto done;
    }

    /* ---- setcc ---- */
    if (m[0] == 's' && m[1] == 'e' && m[2] == 't' && n == 1) {
        int cc = cc_of(m + 3);
        if (cc >= 0 && (is_r(a, 8) || (is_m(a) && mem_ok(a, 8)))) {
            uint8_t opc[2] = {0x0F, (uint8_t)(0x90 + cc)};
            r = rm_digit(&o, 8, 0, 0, opc, 2, 0, a, 0, 0, 0); goto done;
        }
    }

    /* ---- call/jmp registro: arriba. SSE ---- */
    for (size_t i = 0; i < sizeof k_sse / sizeof k_sse[0]; i++) {
        if (strcmp(k_sse[i].n, m) || n != 2) continue;
        if (is_xmm(a) && (is_xmm(b) || (is_m(b) && b->size == 0))) {
            Enc e; enc_init(&e);
            e.mand = k_sse[i].mand; e.regf = a->reg; e.rm = b; e.opc[0] = 0x0F; e.opc[1] = (uint8_t)k_sse[i].opc2; e.nopc = 2;
            r = emit_modrm(&o, &e); goto done;
        }
    }
    if ((!strcmp(m, "movdqu") || !strcmp(m, "movdqa")) && n == 2) {
        int mand = !strcmp(m, "movdqu") ? 0xF3 : 0x66;
        Enc e; enc_init(&e);
        e.mand = mand; e.opc[0] = 0x0F; e.nopc = 2;
        if (is_xmm(a) && (is_xmm(b) || (is_m(b) && b->size == 0))) { e.opc[1] = 0x6F; e.regf = a->reg; e.rm = b; r = emit_modrm(&o, &e); goto done; }
        if (is_m(a) && a->size == 0 && is_xmm(b)) { e.opc[1] = 0x7F; e.regf = b->reg; e.rm = a; r = emit_modrm(&o, &e); goto done; }
    }
    if (!strcmp(m, "pshufd") && n == 3 && is_xmm(a) && (is_xmm(b) || is_m(b)) && is_i(c) && c->val >= 0 && c->val <= 255) {
        Enc e; enc_init(&e);
        e.mand = 0x66; e.regf = a->reg; e.rm = b; e.opc[0] = 0x0F; e.opc[1] = 0x70; e.nopc = 2; e.imm = c->val; e.immsz = 1;
        r = emit_modrm(&o, &e); goto done;
    }
    if (!strcmp(m, "psrldq") && n == 2 && is_xmm(a) && is_i(b) && b->val >= 0 && b->val <= 255) {
        Enc e; enc_init(&e);
        e.mand = 0x66; e.regf = 3; e.rm = a; e.opc[0] = 0x0F; e.opc[1] = 0x73; e.nopc = 2; e.imm = b->val; e.immsz = 1;
        r = emit_modrm(&o, &e); goto done;
    }
    if (!strcmp(m, "movd") && n == 2) {
        Enc e; enc_init(&e);
        e.mand = 0x66; e.opc[0] = 0x0F; e.nopc = 2;
        if (is_xmm(a) && (is_r(b, 32) || (is_m(b) && mem_ok(b, 32)))) { e.opc[1] = 0x6E; e.regf = a->reg; e.rm = b; r = emit_modrm(&o, &e); goto done; }
        if ((is_r(a, 32) || (is_m(a) && mem_ok(a, 32))) && is_xmm(b)) { e.opc[1] = 0x7E; e.regf = b->reg; e.rm = a; r = emit_modrm(&o, &e); goto done; }
    }
    if (!strcmp(m, "movq") && n == 2) {
        Enc e; enc_init(&e);
        e.mand = 0x66; e.w = 1; e.opc[0] = 0x0F; e.nopc = 2;
        if (is_xmm(a) && is_r(b, 64)) { e.opc[1] = 0x6E; e.regf = a->reg; e.rm = b; r = emit_modrm(&o, &e); goto done; }
        if (is_r(a, 64) && is_xmm(b)) { e.opc[1] = 0x7E; e.regf = b->reg; e.rm = a; r = emit_modrm(&o, &e); goto done; }
    }

    r = fail(&o, "mnemonico o forma no implementado");

done:
    if (r > 0 && out) memcpy(out, o.b, (size_t)o.n);
    if (r < 0 && why) *why = o.why ? o.why : "no implementado";
    return r;
}
