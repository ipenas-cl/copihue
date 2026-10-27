/* x64enc: ver x64enc.h */
#include "x64enc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- registros ---------- */

typedef struct { const char* name; int cls, reg, size, high8; } RegName;

static const RegName g_regs[] = {
    {"rax",0,0,64,0},{"rcx",0,1,64,0},{"rdx",0,2,64,0},{"rbx",0,3,64,0},
    {"rsp",0,4,64,0},{"rbp",0,5,64,0},{"rsi",0,6,64,0},{"rdi",0,7,64,0},
    {"r8",0,8,64,0},{"r9",0,9,64,0},{"r10",0,10,64,0},{"r11",0,11,64,0},
    {"r12",0,12,64,0},{"r13",0,13,64,0},{"r14",0,14,64,0},{"r15",0,15,64,0},
    {"eax",0,0,32,0},{"ecx",0,1,32,0},{"edx",0,2,32,0},{"ebx",0,3,32,0},
    {"esp",0,4,32,0},{"ebp",0,5,32,0},{"esi",0,6,32,0},{"edi",0,7,32,0},
    {"r8d",0,8,32,0},{"r9d",0,9,32,0},{"r10d",0,10,32,0},{"r11d",0,11,32,0},
    {"r12d",0,12,32,0},{"r13d",0,13,32,0},{"r14d",0,14,32,0},{"r15d",0,15,32,0},
    {"ax",0,0,16,0},{"cx",0,1,16,0},{"dx",0,2,16,0},{"bx",0,3,16,0},
    {"sp",0,4,16,0},{"bp",0,5,16,0},{"si",0,6,16,0},{"di",0,7,16,0},
    {"r8w",0,8,16,0},{"r9w",0,9,16,0},{"r10w",0,10,16,0},{"r11w",0,11,16,0},
    {"r12w",0,12,16,0},{"r13w",0,13,16,0},{"r14w",0,14,16,0},{"r15w",0,15,16,0},
    {"al",0,0,8,0},{"cl",0,1,8,0},{"dl",0,2,8,0},{"bl",0,3,8,0},
    {"spl",0,4,8,0},{"bpl",0,5,8,0},{"sil",0,6,8,0},{"dil",0,7,8,0},
    {"r8b",0,8,8,0},{"r9b",0,9,8,0},{"r10b",0,10,8,0},{"r11b",0,11,8,0},
    {"r12b",0,12,8,0},{"r13b",0,13,8,0},{"r14b",0,14,8,0},{"r15b",0,15,8,0},
    {"ah",0,4,8,1},{"ch",0,5,8,1},{"dh",0,6,8,1},{"bh",0,7,8,1},
    {"xmm0",1,0,128,0},{"xmm1",1,1,128,0},{"xmm2",1,2,128,0},{"xmm3",1,3,128,0},
    {"xmm4",1,4,128,0},{"xmm5",1,5,128,0},{"xmm6",1,6,128,0},{"xmm7",1,7,128,0},
    {"xmm8",1,8,128,0},{"xmm9",1,9,128,0},{"xmm10",1,10,128,0},{"xmm11",1,11,128,0},
    {"xmm12",1,12,128,0},{"xmm13",1,13,128,0},{"xmm14",1,14,128,0},{"xmm15",1,15,128,0},
};

static const RegName* find_reg(const char* s) {
    for (size_t i = 0; i < sizeof g_regs / sizeof g_regs[0]; i++)
        if (!strcmp(g_regs[i].name, s)) return &g_regs[i];
    return NULL;
}

/* ---------- análisis de líneas ---------- */

static char* trim(char* s) {
    while (isspace((unsigned char)*s)) s++;
    char* e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static int is_ident_start(int c) { return isalpha(c) || c == '_' || c == '.' || c == '$'; }
static int is_ident(int c) { return isalnum(c) || c == '_' || c == '.' || c == '$' || c == '@' || c == '#' || c == '~'; }

/* Número NASM: decimal, 0x..., 0b... Devuelve 0 si `s` entero lo es. */
static int parse_number(const char* s, int64_t* v) {
    char* end;
    if (!(isdigit((unsigned char)*s))) return 0;
    *v = (int64_t)strtoull(s, &end, 0);
    return *end == 0;
}

static NxOperand blank(void) {
    NxOperand o;
    memset(&o, 0, sizeof o);
    o.base = o.index = -1;
    o.scale = 1;
    return o;
}

/* Suma un término `[+-] (reg | reg*N | N | sym)` a un operando. */
static int add_term(NxOperand* o, char* t, int sign, int in_mem) {
    t = trim(t);
    if (!*t) return 0;
    char* star = strchr(t, '*');
    if (star) {
        *star = 0;
        char* a = trim(t);
        char* b = trim(star + 1);
        const RegName* r = find_reg(a);
        int64_t sc;
        if (!r) { const char* tmp = a; a = b; b = (char*)tmp; r = find_reg(a); }
        if (!r || !in_mem || sign < 0 || !parse_number(b, &sc)) return 0;
        if (sc != 1 && sc != 2 && sc != 4 && sc != 8) return 0;
        if (o->index >= 0) return 0;
        o->index = r->reg; o->scale = (int)sc;
        return 1;
    }
    const RegName* r = find_reg(t);
    if (r) {
        if (!in_mem || sign < 0 || r->cls != NX_GPR || r->size != 64) return 0;
        if (o->base < 0) o->base = r->reg;
        else if (o->index < 0) { o->index = r->reg; o->scale = 1; }
        else return 0;
        return 1;
    }
    int64_t v;
    if (parse_number(t, &v)) { o->val += sign * v; return 1; }
    if (is_ident_start((unsigned char)*t)) {
        for (char* p = t; *p; p++) if (!is_ident((unsigned char)*p)) return 0;
        if (o->sym[0]) return 0;
        if (strlen(t) >= sizeof o->sym) return 0;
        strcpy(o->sym, t);
        o->symneg = sign < 0;
        return 1;
    }
    return 0;
}

/* Divide `s` en términos por + y -, conservando el signo. */
static int parse_expr(NxOperand* o, char* s, int in_mem) {
    int sign = 1;
    char* start = s;
    int any = 0;
    s = trim(s);
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    start = s;
    for (;; s++) {
        if (*s == '+' || *s == '-' || *s == 0) {
            if (s == start) return 0;
            char save = *s;
            *s = 0;
            if (!add_term(o, start, sign, in_mem)) return 0;
            any = 1;
            if (save == 0) break;
            sign = save == '-' ? -1 : 1;
            start = s + 1;
        }
    }
    return any;
}

static int parse_operand(char* s, NxOperand* o) {
    static const struct { const char* w; int bits; } sizes[] = {
        {"byte", 8}, {"word", 16}, {"dword", 32}, {"qword", 64}, {"xmmword", 128},
    };
    s = trim(s);
    *o = blank();
    int size = 0;
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        size_t n = strlen(sizes[i].w);
        if (!strncmp(s, sizes[i].w, n) && (isspace((unsigned char)s[n]) || s[n] == '[')) {
            size = sizes[i].bits; s = trim(s + n); break;
        }
    }
    if (*s == '[') {
        size_t n = strlen(s);
        if (s[n - 1] != ']') return 0;
        s[n - 1] = 0;
        s++;
        char* t = trim(s);
        int rel = 0;
        if (!strncmp(t, "rel", 3) && isspace((unsigned char)t[3])) { rel = 1; t = trim(t + 3); }
        if (!parse_expr(o, t, 1)) return 0;
        if (rel) { o->base = -2; }   /* rel: RIP-relativo; solo con símbolo */
        o->kind = NX_MEM; o->size = size;
        return 1;
    }
    if (size) return 0;
    const RegName* r = find_reg(s);
    if (r) {
        o->kind = NX_REG; o->rclass = r->cls; o->reg = r->reg; o->size = r->size; o->high8 = r->high8;
        return 1;
    }
    if (!parse_expr(o, s, 0)) return 0;
    o->kind = o->sym[0] ? NX_SYM : NX_IMM;
    return 1;
}

int nx_parse_line(const char* line, NxInsn* ins, char* label, size_t labelsz) {
    char buf[512];
    if (label && labelsz) label[0] = 0;
    memset(ins, 0, sizeof *ins);
    if (strlen(line) >= sizeof buf) return NX_L_BAD;
    strcpy(buf, line);
    char* sc = strchr(buf, ';');
    if (sc) *sc = 0;
    char* s = trim(buf);
    if (!*s) return NX_L_EMPTY;

    /* etiqueta: identificador seguido de ':' */
    if (is_ident_start((unsigned char)*s)) {
        char* p = s;
        while (is_ident((unsigned char)*p)) p++;
        if (*p == ':') {
            if (label && (size_t)(p - s) < labelsz) { memcpy(label, s, p - s); label[p - s] = 0; }
            s = trim(p + 1);
            if (!*s) return NX_L_LABEL;
            /* "str_0: db ..." es dato, no instrucción */
            if (!strncmp(s, "db", 2) || !strncmp(s, "dq", 2) || !strncmp(s, "dd", 2) ||
                !strncmp(s, "dw", 2) || !strncmp(s, "resb", 4) || !strncmp(s, "resw", 4) ||
                !strncmp(s, "resd", 4) || !strncmp(s, "resq", 4))
                return NX_L_DIRECTIVE;
        }
    }

    /* directivas */
    static const char* dirs[] = { "global ", "section ", "db ", "dq ", "dd ", "dw ", "resb ", "resw ", "resd ", "resq ",
                                  "align ", "bits ", "org ", "extern ", "times ", NULL };
    for (int i = 0; dirs[i]; i++)
        if (!strncmp(s, dirs[i], strlen(dirs[i]))) {
            if (label && labelsz) snprintf(label, labelsz, "%.*s", (int)strcspn(s, " "), s);
            return NX_L_DIRECTIVE;
        }
    {   /* "nombre equ valor" */
        char* p = s;
        while (is_ident((unsigned char)*p)) p++;
        if (p != s && isspace((unsigned char)*p)) {
            char* q = trim(p);
            if (!strncmp(q, "equ", 3) && isspace((unsigned char)q[3])) {
                if (label && (size_t)(p - s) < labelsz) { memcpy(label, s, p - s); label[p - s] = 0; }
                return NX_L_DIRECTIVE;
            }
        }
    }

    /* instrucción: [prefijo] mnemónico [operandos] */
    char* p = s;
    for (int pass = 0; pass < 2; pass++) {
        char* w = p;
        while (is_ident((unsigned char)*p)) p++;
        if (p == w) return NX_L_BAD;
        size_t n = (size_t)(p - w);
        if (n >= sizeof ins->mnem) return NX_L_BAD;
        char word[16];
        memcpy(word, w, n); word[n] = 0;
        if (pass == 0 && (!strcmp(word, "lock") || !strcmp(word, "rep") || !strcmp(word, "repe") || !strcmp(word, "repne"))) {
            strcpy(ins->prefix, word);
            while (isspace((unsigned char)*p)) p++;
            continue;
        }
        strcpy(ins->mnem, word);
        break;
    }
    p = trim(p);
    if (!*p) return NX_L_INSN;

    /* operandos separados por coma fuera de corchetes */
    char* start = p;
    int depth = 0;
    for (char* q = p;; q++) {
        if (*q == '[') depth++;
        else if (*q == ']') depth--;
        if ((*q == ',' && depth == 0) || *q == 0) {
            char save = *q;
            *q = 0;
            if (ins->nops >= 3) return NX_L_BAD;
            if (!parse_operand(start, &ins->op[ins->nops])) return NX_L_BAD;
            ins->nops++;
            if (!save) break;
            start = q + 1;
        }
    }
    return NX_L_INSN;
}

/* ---------- nombre de forma ---------- */

static int is_branch(const char* m) {
    return m[0] == 'j' || !strcmp(m, "call") || !strcmp(m, "loop");
}

static void form_op(const NxInsn* ins, const NxOperand* o, char* b, size_t n) {
    switch (o->kind) {
    case NX_REG:
        if (o->rclass == NX_XMM) snprintf(b, n, "xmm");
        else snprintf(b, n, "r%d%s", o->size, o->high8 ? "h" : "");
        break;
    case NX_IMM: snprintf(b, n, "imm"); break;
    case NX_SYM:
        snprintf(b, n, "%s", is_branch(ins->mnem) ? "label" : (o->val ? "imm(sym+d)" : "imm(sym)"));
        break;
    case NX_MEM: {
        char sh[48] = "";
        if (o->base == -2) strcat(sh, "rip");
        if (o->base >= 0) strcat(sh, "b");
        if (o->index >= 0) strcat(sh, sh[0] ? "+i*" : "i*"), strcat(sh, o->scale == 1 ? "1" : o->scale == 2 ? "2" : o->scale == 4 ? "4" : "8");
        if (o->sym[0]) strcat(sh, sh[0] ? (o->symneg ? "-s" : "+s") : "s");
        if (o->val) strcat(sh, sh[0] ? "+d" : "d");
        if (!sh[0]) strcpy(sh, "0");
        if (o->size) snprintf(b, n, "m%d[%s]", o->size, sh);
        else snprintf(b, n, "m?[%s]", sh);
        break;
    }
    default: snprintf(b, n, "?");
    }
}

void nx_form(const NxInsn* ins, char* buf, size_t sz) {
    char tmp[3][64];
    size_t off = 0;
    off += (size_t)snprintf(buf + off, sz - off, "%s%s%s", ins->prefix, ins->prefix[0] ? " " : "", ins->mnem);
    for (int i = 0; i < ins->nops && off < sz; i++) {
        form_op(ins, &ins->op[i], tmp[i], sizeof tmp[i]);
        off += (size_t)snprintf(buf + off, sz - off, "%s%s", i ? "," : " ", tmp[i]);
    }
}
