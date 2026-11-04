// nasm version detection and the movsxd adaptation shared by the native tests.
#ifndef COPIHUE_NASMFIX_H
#define COPIHUE_NASMFIX_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compiler/native/x64.h"

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

#endif
