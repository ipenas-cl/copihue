// Minimal ELF64 relocatable reader for the tests: the four sections the
// assembler produces (.text, .data, .bss, .rodata), their relocations and the
// symbol table. Reads what `nasm -f elf64` writes; not a general ELF reader.
#ifndef COPIHUE_NOBJ_H
#define COPIHUE_NOBJ_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int off, type; char sym[128]; int secsym; int64_t addend; } NRel;
typedef struct {
    int present;
    const uint8_t* b;           // NULL for .bss (NOBITS)
    size_t n;                   // sh_size
    uint64_t align, flags;
    NRel* rel; size_t nrel;
} NSec;
typedef struct { char name[128]; int bind, type, shndx; uint64_t value, size; } NSym;
typedef struct {
    uint8_t* raw; size_t rawn;
    NSec s[4];                  // 0 .text, 1 .data, 2 .bss, 3 .rodata (same order as ASM_SEC_*)
    NSym* sym; size_t nsym;
    int sec_of_index[64];       // ELF section index -> 0..3, or -1
} NObj;

static uint64_t nobj_rd(const uint8_t* p, int n) {
    uint64_t v = 0;
    for (int i = n - 1; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

static int nobj_read_file(const char* path, uint8_t** buf, size_t* n) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    *buf = malloc((size_t)l + 1);
    *n = fread(*buf, 1, (size_t)l, f);
    fclose(f);
    return 1;
}

static int nobj_which(const char* nm) {
    static const char* const N[4] = {".text", ".data", ".bss", ".rodata"};
    for (int i = 0; i < 4; i++) if (!strcmp(nm, N[i])) return i;
    return -1;
}

static int nobj_load(const char* path, NObj* o) {
    memset(o, 0, sizeof *o);
    for (int i = 0; i < 64; i++) o->sec_of_index[i] = -1;
    if (!nobj_read_file(path, &o->raw, &o->rawn) || o->rawn < 64) return 0;
    const uint8_t* e = o->raw;
    uint64_t shoff = nobj_rd(e + 0x28, 8);
    int shnum = (int)nobj_rd(e + 0x3C, 2), shstr = (int)nobj_rd(e + 0x3E, 2);
    if (shnum > 64) return 0;
#define SH(i, off, n) nobj_rd(e + shoff + (size_t)(i) * 64 + (off), n)
    const char* names = (const char*)e + SH(shstr, 0x18, 8);
    int isym = -1;
    for (int i = 0; i < shnum; i++) {
        const char* nm = names + SH(i, 0, 4);
        int w = nobj_which(nm);
        if (w >= 0) {
            NSec* s = &o->s[w];
            s->present = 1;
            s->n = (size_t)SH(i, 0x20, 8);
            s->b = SH(i, 4, 4) == 8 ? NULL : e + SH(i, 0x18, 8);     // SHT_NOBITS
            s->align = SH(i, 0x30, 8);
            s->flags = SH(i, 8, 8);
            o->sec_of_index[i] = w;
        } else if (!strcmp(nm, ".symtab")) isym = i;
    }
    const uint8_t* sp = NULL;
    const char* strt = NULL;
    if (isym >= 0) {
        sp = e + SH(isym, 0x18, 8);
        strt = (const char*)e + SH((int)SH(isym, 0x28, 4), 0x18, 8);
        o->nsym = (size_t)SH(isym, 0x20, 8) / 24;
        o->sym = calloc(o->nsym ? o->nsym : 1, sizeof *o->sym);
        for (size_t i = 0; i < o->nsym; i++) {
            const uint8_t* s = sp + i * 24;
            NSym* y = &o->sym[i];
            snprintf(y->name, sizeof y->name, "%s", strt + nobj_rd(s, 4));
            y->bind = (int)(nobj_rd(s + 4, 1) >> 4); y->type = (int)(nobj_rd(s + 4, 1) & 15);
            y->shndx = (int)nobj_rd(s + 6, 2); y->value = nobj_rd(s + 8, 8); y->size = nobj_rd(s + 16, 8);
        }
    }
    for (int i = 0; i < shnum; i++) {
        const char* nm = names + SH(i, 0, 4);
        if (strncmp(nm, ".rela", 5) || !sp) continue;
        int w = nobj_which(nm + 5);
        if (w < 0) continue;
        const uint8_t* rp = e + SH(i, 0x18, 8);
        size_t rn = (size_t)SH(i, 0x20, 8) / 24;
        NSec* s = &o->s[w];
        s->rel = calloc(rn ? rn : 1, sizeof *s->rel); s->nrel = rn;
        for (size_t k = 0; k < rn; k++) {
            NRel* r = &s->rel[k];
            r->off = (int)nobj_rd(rp + k * 24, 8);
            uint64_t info = nobj_rd(rp + k * 24 + 8, 8);
            r->type = (int)(info & 0xFFFFFFFF);
            r->addend = (int64_t)nobj_rd(rp + k * 24 + 16, 8);
            const NSym* y = &o->sym[info >> 32];
            if (y->type == 3) {                                  // STT_SECTION
                r->secsym = 1;
                snprintf(r->sym, sizeof r->sym, "%s", names + SH(y->shndx, 0, 4));
            } else snprintf(r->sym, sizeof r->sym, "%s", y->name);
        }
    }
    return 1;
#undef SH
}

static void nobj_free(NObj* o) {
    for (int i = 0; i < 4; i++) free(o->s[i].rel);
    free(o->sym); free(o->raw);
    memset(o, 0, sizeof *o);
}

#endif
