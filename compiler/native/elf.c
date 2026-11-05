// ELF64 executable writer. See elf.h.
#include "elf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum { EHDR_SIZE = 64, PHDR_SIZE = 56 };

static int elf_fail(char* err, size_t n, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (err && n) vsnprintf(err, n, fmt, ap);
    va_end(ap);
    return -1;
}

static uint64_t elf_up(uint64_t v, uint64_t a) { return a > 1 ? (v + a - 1) / a * a : v; }

static void elf_w(uint8_t* p, uint64_t v, int n) {
    for (int i = 0; i < n; i++) p[i] = (uint8_t)(v >> (8 * i));
}

int elf_layout(const AsmOut* a, ElfLayout* L, char* err, size_t errsz) {
    memset(L, 0, sizeof *L);
    const AsmSection* tx = &a->sec[ASM_SEC_TEXT];
    const AsmSection* ro = &a->sec[ASM_SEC_RODATA];
    const AsmSection* da = &a->sec[ASM_SEC_DATA];
    const AsmSection* bs = &a->sec[ASM_SEC_BSS];
    for (int k = 0; k < ASM_NSEC; k++)
        if (a->sec[k].align > ELF_PAGE) return elf_fail(err, errsz, "section alignment %d is larger than a page", a->sec[k].align);
    if (tx->size == 0) return elf_fail(err, errsz, "the program has no code");

    const AsmSym* st = asm_find_sym(a, "_start");
    if (!st) return elf_fail(err, errsz, "the program has no _start");
    if (st->sec != ASM_SEC_TEXT) return elf_fail(err, errsz, "_start is not in .text");
    if (!st->global) return elf_fail(err, errsz, "_start is not declared global");

    uint64_t cur = ELF_TEXT_OFF;
    L->off[ASM_SEC_TEXT] = cur;
    L->base[ASM_SEC_TEXT] = ELF_BASE + cur;
    cur += tx->size;
    if (ro->size) {
        cur = elf_up(cur, (uint64_t)ro->align);
        L->off[ASM_SEC_RODATA] = cur;
        L->base[ASM_SEC_RODATA] = ELF_BASE + cur;
        cur += ro->size;
    }
    L->s1_filesz = cur;
    L->file_size = cur;
    L->nphdr = 1;
    if (da->size || bs->size) {
        // The data segment starts on the next page of memory but at the next
        // byte of the file: vaddr = offset (mod page).
        uint64_t off = da->size ? elf_up(cur, (uint64_t)da->align) : cur;
        uint64_t va = elf_up(ELF_BASE + cur, ELF_PAGE) + off % ELF_PAGE;
        uint64_t end = va;
        L->nphdr = 2;
        L->s2_off = off; L->s2_va = va;
        if (da->size) {
            L->off[ASM_SEC_DATA] = off;
            L->base[ASM_SEC_DATA] = va;
            end = va + da->size;
            L->s2_filesz = da->size;
            L->file_size = off + da->size;
        }
        if (bs->size) {
            end = elf_up(end, (uint64_t)bs->align);
            L->base[ASM_SEC_BSS] = end;
            end += bs->size;
        }
        L->s2_memsz = end - va;
    }
    L->entry = L->base[ASM_SEC_TEXT] + (uint64_t)st->off;
    L->load_end = L->file_size;
    return 0;
}

enum { SYM_SIZE = 24, SHDR_SIZE = 64 };

typedef struct { uint8_t* b; size_t n, cap; } ElfBuf;

static int elf_buf_put(ElfBuf* b, const void* p, size_t n) {
    if (b->n + n > b->cap) {
        size_t c = b->cap ? b->cap * 2 : 256;
        while (c < b->n + n) c *= 2;
        uint8_t* q = realloc(b->b, c);
        if (!q) return -1;
        b->b = q; b->cap = c;
    }
    if (n) memcpy(b->b + b->n, p, n);
    b->n += n;
    return 0;
}

// Appends a NUL-terminated string; returns its offset (or -1).
static long elf_str(ElfBuf* b, const char* s) {
    long at = (long)b->n;
    return elf_buf_put(b, s, strlen(s) + 1) ? -1 : at;
}

static int elf_is_symbol(const AsmOut* a, const AsmSym* y) {
    return !strchr(y->name, '.') && a->sec[y->sec].size > 0;
}

// .symtab and .strtab for the final layout. Returns 0 or -1 (out of memory).
// *first_global is the index of the first GLOBAL symbol (sh_info of .symtab).
static int elf_symtab(const AsmOut* a, const ElfLayout* L, const int* shndx, ElfBuf* sy, ElfBuf* st, uint32_t* first_global) {
    uint8_t zero[SYM_SIZE] = {0};
    if (elf_buf_put(sy, zero, SYM_SIZE) || elf_str(st, "") < 0) return -1;
    uint32_t count = 1;
    for (int pass = 0; pass < 2; pass++) {      // locals, then globals
        if (pass == 1) *first_global = count;
        for (size_t i = 0; i < a->nsym; i++) {
            const AsmSym* y = &a->sym[i];
            if (!elf_is_symbol(a, y) || (y->global != 0) != (pass == 1)) continue;
            // Size: up to the next symbol of the section at a higher offset, else the section's end.
            uint64_t end = a->sec[y->sec].size;
            for (size_t j = i + 1; j < a->nsym && a->sym[j].sec == y->sec; j++)
                if (elf_is_symbol(a, &a->sym[j]) && a->sym[j].off > y->off) { end = (uint64_t)a->sym[j].off; break; }
            uint8_t e[SYM_SIZE] = {0};
            long nm = elf_str(st, y->name);
            if (nm < 0) return -1;
            elf_w(e, (uint64_t)nm, 4);
            e[4] = (uint8_t)(((y->global ? 1 : 0) << 4) | (y->sec == ASM_SEC_TEXT ? 2 : 1));   // GLOBAL/LOCAL, FUNC/OBJECT
            elf_w(e + 6, (uint64_t)shndx[y->sec], 2);
            elf_w(e + 8, L->base[y->sec] + (uint64_t)y->off, 8);
            elf_w(e + 16, end - (uint64_t)y->off, 8);
            if (elf_buf_put(sy, e, SYM_SIZE)) return -1;
            count++;
        }
    }
    return 0;
}

static void elf_shdr(uint8_t* p, uint32_t name, uint32_t type, uint64_t flags, uint64_t addr, uint64_t off, uint64_t size, uint32_t link, uint32_t info, uint64_t align, uint64_t entsize) {
    elf_w(p, name, 4); elf_w(p + 4, type, 4); elf_w(p + 8, flags, 8); elf_w(p + 16, addr, 8);
    elf_w(p + 24, off, 8); elf_w(p + 32, size, 8); elf_w(p + 40, link, 4); elf_w(p + 44, info, 4);
    elf_w(p + 48, align, 8); elf_w(p + 56, entsize, 8);
}

// Patch one relocation with the final addresses (no relocation is kept).
static int elf_patch(const AsmOut* a, const ElfLayout* L, const AsmReloc* r, uint8_t* field, uint64_t place, char* err, size_t errsz) {
    int64_t s = (int64_t)L->base[r->sec] + r->addend;
    switch (r->type) {
    case XR_64: elf_w(field, (uint64_t)s, 8); return 0;
    case XR_32:
        if (s < 0 || s > 0xFFFFFFFFLL) break;
        elf_w(field, (uint64_t)s, 4); return 0;
    case XR_32S:
        if (s < INT32_MIN || s > INT32_MAX) break;
        elf_w(field, (uint64_t)s, 4); return 0;
    case XR_PC32: {
        int64_t v = s - (int64_t)place;
        if (v < INT32_MIN || v > INT32_MAX) break;
        elf_w(field, (uint64_t)v, 4); return 0;
    }
    default:
        return elf_fail(err, errsz, "relocation type %d in %s at offset %d is not supported", r->type, r->in == ASM_SEC_TEXT ? ".text" : ".data", r->off);
    }
    (void)a;
    return elf_fail(err, errsz, "relocation to %s (type %d) at offset %d does not fit its field", r->sym, r->type, r->off);
}

int elf_build(const AsmOut* a, uint8_t** img, size_t* n, ElfLayout* L, char* err, size_t errsz) {
    ElfLayout loc;
    if (!L) L = &loc;
    if (elf_layout(a, L, err, errsz)) return -1;

    // Tail: section header numbers, .symtab, .strtab, .shstrtab, section header table.
    static const char* const NAME[ASM_NSEC] = {".text", ".data", ".bss", ".rodata"};
    static const uint64_t FLAGS[ASM_NSEC] = {6, 3, 3, 2};     // AX, WA, WA, A
    static const int ORDER[ASM_NSEC] = {ASM_SEC_TEXT, ASM_SEC_RODATA, ASM_SEC_DATA, ASM_SEC_BSS};
    int shndx[ASM_NSEC] = {0};
    long shname[ASM_NSEC] = {0};
    int nsh = 1;
    ElfBuf sy = {0}, st = {0}, hs = {0};
    uint32_t first_global = 1;
    if (elf_str(&hs, "") < 0) goto oom;
    long n_symtab = elf_str(&hs, ".symtab"), n_strtab = elf_str(&hs, ".strtab"), n_shstr = elf_str(&hs, ".shstrtab");
    if (n_symtab < 0 || n_strtab < 0 || n_shstr < 0) goto oom;
    for (int i = 0; i < ASM_NSEC; i++) {
        int k = ORDER[i];
        if (!a->sec[k].size) continue;
        shndx[k] = nsh++;
        if ((shname[k] = elf_str(&hs, NAME[k])) < 0) goto oom;
    }
    if (elf_symtab(a, L, shndx, &sy, &st, &first_global)) goto oom;
    int i_symtab = nsh, i_strtab = nsh + 1, i_shstr = nsh + 2;
    nsh += 3;
    L->file_size = elf_up(L->load_end, 8);
    uint64_t o_symtab = L->file_size;
    uint64_t o_strtab = o_symtab + sy.n;
    uint64_t o_shstr = o_strtab + st.n;
    uint64_t o_sh = elf_up(o_shstr + hs.n, 8);
    L->file_size = o_sh + (uint64_t)nsh * SHDR_SIZE;

    uint8_t* f = calloc((size_t)L->file_size, 1);
    if (!f) goto oom;
    memcpy(f + o_symtab, sy.b, sy.n);
    memcpy(f + o_strtab, st.b, st.n);
    memcpy(f + o_shstr, hs.b, hs.n);
    uint8_t* sh = f + o_sh;                         // entry 0 stays all zero
    for (int i = 0; i < ASM_NSEC; i++) {
        int k = ORDER[i];
        if (!shndx[k]) continue;
        const AsmSection* s = &a->sec[k];
        // .bss takes no file bytes; its offset is where the loaded part of the file ends.
        uint64_t off = k == ASM_SEC_BSS ? L->load_end : L->off[k];
        elf_shdr(sh + (size_t)shndx[k] * SHDR_SIZE, (uint32_t)shname[k], k == ASM_SEC_BSS ? 8 : 1, FLAGS[k], L->base[k], off, s->size, 0, 0, (uint64_t)s->align, 0);
    }
    elf_shdr(sh + (size_t)i_symtab * SHDR_SIZE, (uint32_t)n_symtab, 2, 0, 0, o_symtab, sy.n, (uint32_t)i_strtab, first_global, 8, SYM_SIZE);
    elf_shdr(sh + (size_t)i_strtab * SHDR_SIZE, (uint32_t)n_strtab, 3, 0, 0, o_strtab, st.n, 0, 0, 1, 0);
    elf_shdr(sh + (size_t)i_shstr * SHDR_SIZE, (uint32_t)n_shstr, 3, 0, 0, o_shstr, hs.n, 0, 0, 1, 0);
    free(sy.b); free(st.b); free(hs.b);
    elf_w(f + 40, o_sh, 8);                         // e_shoff
    elf_w(f + 60, (uint64_t)nsh, 2);                // e_shnum
    elf_w(f + 62, (uint64_t)i_shstr, 2);            // e_shstrndx

    // ELF header.
    memcpy(f, "\x7f" "ELF\2\1\1", 7);              // 64-bit, little endian, version 1, System V ABI
    elf_w(f + 16, 2, 2);                           // ET_EXEC
    elf_w(f + 18, 62, 2);                          // EM_X86_64
    elf_w(f + 20, 1, 4);
    elf_w(f + 24, L->entry, 8);
    elf_w(f + 32, EHDR_SIZE, 8);                   // e_phoff
    elf_w(f + 52, EHDR_SIZE, 2);                   // e_ehsize
    elf_w(f + 54, PHDR_SIZE, 2);
    elf_w(f + 56, (uint64_t)L->nphdr, 2);
    elf_w(f + 58, SHDR_SIZE, 2);                   // e_shentsize

    // Program headers: PT_LOAD R+X, then PT_LOAD R+W.
    uint8_t* p = f + EHDR_SIZE;
    elf_w(p, 1, 4); elf_w(p + 4, 5, 4);
    elf_w(p + 8, 0, 8); elf_w(p + 16, ELF_BASE, 8); elf_w(p + 24, ELF_BASE, 8);
    elf_w(p + 32, L->s1_filesz, 8); elf_w(p + 40, L->s1_filesz, 8); elf_w(p + 48, ELF_PAGE, 8);
    if (L->nphdr == 2) {
        p += PHDR_SIZE;
        elf_w(p, 1, 4); elf_w(p + 4, 6, 4);
        elf_w(p + 8, L->s2_off, 8); elf_w(p + 16, L->s2_va, 8); elf_w(p + 24, L->s2_va, 8);
        elf_w(p + 32, L->s2_filesz, 8); elf_w(p + 40, L->s2_memsz, 8); elf_w(p + 48, ELF_PAGE, 8);
    }

    // Section contents, then the relocations patched in place.
    for (int k = 0; k < ASM_NSEC; k++) {
        const AsmSection* s = &a->sec[k];
        if (k == ASM_SEC_BSS || !s->size) continue;
        memcpy(f + L->off[k], s->bytes, s->size);
    }
    for (int k = 0; k < ASM_NSEC; k++) {
        const AsmSection* s = &a->sec[k];
        if (k == ASM_SEC_BSS) continue;
        for (size_t i = 0; i < s->nrel; i++) {
            const AsmReloc* r = &s->rel[i];
            if ((size_t)r->off + (r->type == XR_64 ? 8 : 4) > s->size) { free(f); return elf_fail(err, errsz, "relocation outside its section"); }
            if (elf_patch(a, L, r, f + L->off[k] + r->off, L->base[k] + (uint64_t)r->off, err, errsz)) { free(f); return -1; }
        }
    }
    *img = f;
    *n = (size_t)L->file_size;
    return 0;
oom:
    free(sy.b); free(st.b); free(hs.b);
    return elf_fail(err, errsz, "out of memory");
}

int elf_write_exec(const AsmOut* a, const char* path, char* err, size_t errsz) {
    uint8_t* img;
    size_t n;
    if (elf_build(a, &img, &n, NULL, err, errsz)) return -1;
    FILE* fp = fopen(path, "wb");
    if (!fp) { free(img); return elf_fail(err, errsz, "cannot write '%s'", path); }
    int ok = fwrite(img, 1, n, fp) == n;
    ok = fclose(fp) == 0 && ok;
    free(img);
    if (!ok) return elf_fail(err, errsz, "error writing '%s'", path);
    if (chmod(path, 0755) != 0) return elf_fail(err, errsz, "cannot set the mode of '%s'", path);
    return 0;
}
