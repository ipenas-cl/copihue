// Classification of one line of the assembly the code generator emits.
#include "x64.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ident_char(int c) { return isalnum(c) || c == '_' || c == '.' || c == '$' || c == '@' || c == '#' || c == '~'; }

static char* trim(char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char* e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static int is_directive_word(const char* w) {
    static const char* const D[] = {"section", "segment", "global", "extern", "align", "alignb", "db", "dw", "dd",
        "dq", "resb", "resw", "resd", "resq", "times", "equ", "bits", "default", "org", "incbin", "use64",
        "use32", "use16", "%define", "%macro", "%endmacro", "%include", "%if", "%endif", "%assign", NULL};
    for (int i = 0; D[i]; i++) if (!strcmp(w, D[i])) return 1;
    return 0;
}

void x64_classify(char* line, XLine* out) {
    memset(out, 0, sizeof *out);
    char* nl = strpbrk(line, "\r\n");
    if (nl) *nl = 0;
    char* p = line;
    while (isspace((unsigned char)*p)) p++;
    if (!*p) { out->kind = XL_BLANK; return; }
    if (*p == ';') { out->kind = XL_COMMENT; return; }
    char* q = p;
    while (ident_char((unsigned char)*q)) q++;
    if (*q == ':' && q > p) {
        out->has_label = 1;
        snprintf(out->label, sizeof out->label, "%.*s", (int)(q - p), p);
        p = q + 1;
        while (isspace((unsigned char)*p)) p++;
        if (!*p || *p == ';') { out->kind = XL_LABEL; return; }
    }
    char w[128];
    int i = 0;
    while (p[i] && !isspace((unsigned char)p[i]) && i < 127) { w[i] = p[i]; i++; }
    w[i] = 0;
    char* r = p + i;
    while (isspace((unsigned char)*r)) r++;
    if (!strncmp(r, "equ", 3) && isspace((unsigned char)r[3])) {
        out->kind = XL_EQU;
        snprintf(out->name, sizeof out->name, "%s", w);
        char* v = r + 3;
        char* sc = strchr(v, ';');
        if (sc) *sc = 0;
        out->val = strtoll(trim(v), NULL, 0);
        snprintf(out->dir, sizeof out->dir, "equ");
        return;
    }
    if (is_directive_word(w)) {
        out->kind = XL_DIRECTIVE;
        if (!strcmp(w, "section") || !strcmp(w, "segment")) {
            char* sc = strchr(r, ';');
            if (sc) *sc = 0;
            snprintf(out->dir, sizeof out->dir, "section %s", trim(r));
        } else snprintf(out->dir, sizeof out->dir, "%s", w);
        return;
    }
    char* sc = strchr(p, ';');
    if (sc) *sc = 0;
    out->kind = XL_INSN;
    out->text = trim(p);
}
