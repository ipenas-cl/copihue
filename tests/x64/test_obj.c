// Relocatable objects (-c) written by compiler/native/elf.c, against the object
// `nasm -f elf64` makes from the same text.
//
//   test_obj WORKDIR files FILE.asm ...   each file is one program
//   test_obj WORKDIR cases FILE.asm       blocks separated by a line `;---`, each one a program
//   test_obj WORKDIR obj OBJ FILE.asm     an object already written (by `copihue -c`) for that program
//   test_obj WORKDIR reject               programs the object writer must refuse
//
// Nothing here links or runs anything (the development machine may not be
// Linux): tests/x64/run_obj.sh links and runs on Linux. What is compared with
// nasm's object, with the reader in nobj.h:
//   - the bytes of .text, .data and .rodata and the size of .bss;
//   - every relocation in nasm's .rela.* tables: offset, type, addend and the
//     symbol it names (the section symbol of the target for a label of the
//     program, global or not; the extern's own name for an undefined one);
//   - the symbols: every label without a dot (bind, section, value), the undefined
//     externs; type FUNC in .text and OBJECT elsewhere; size up to the next label.
// What nasm writes and this does not: STT_FILE, the symbols of `equ` constants
// and the `parent.child` labels. What is checked on the file alone: ET_REL,
// the section headers (no overlap, alignment, sh_link/sh_info of every .rela
// and of .symtab, entsize), the symbol order (null, sections, LOCAL, GLOBAL)
// and that the file ends with the section header table.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compiler/native/asm.h"
#include "../../compiler/native/elf.h"
#include "nasmfix.h"
#include "nobj.h"

static const char* const SEC_NAME[ASM_NSEC] = {".text", ".data", ".bss", ".rodata"};

static long n_ok, n_bad, n_relocs, n_ext, n_syms, tot_file;

#define FAILF(...) do { printf("FAIL %s: ", name); printf(__VA_ARGS__); printf("\n"); return 1; } while (0)

typedef struct { uint32_t name, type, link, info; uint64_t flags, addr, off, size, align, entsize; } Sh;

static const char* str_at(const uint8_t* tab, uint64_t tabn, uint64_t at) {
    if (at >= tabn) return NULL;
    for (uint64_t i = at; i < tabn; i++) if (!tab[i]) return (const char*)tab + at;
    return NULL;
}

// The structure of the file by itself. Fills sh (shnum entries, caller frees) for the comparison.
static int verify_structure(const char* name, const uint8_t* f, size_t n, Sh** shout, int* shnum_out) {
    if (n < 64) FAILF("file of %zu bytes", n);
    if (memcmp(f, "\x7f" "ELF", 4) || f[4] != 2 || f[5] != 1 || f[6] != 1) FAILF("e_ident is not ELF64, little endian, version 1");
    for (int i = 7; i < 16; i++) if (f[i]) FAILF("e_ident[%d] is not 0", i);
    if (nobj_rd(f + 16, 2) != 1) FAILF("e_type is not ET_REL");
    if (nobj_rd(f + 18, 2) != 62) FAILF("e_machine is not EM_X86_64");
    if (nobj_rd(f + 20, 4) != 1) FAILF("e_version");
    if (nobj_rd(f + 24, 8) != 0 || nobj_rd(f + 32, 8) != 0 || nobj_rd(f + 48, 4) != 0) FAILF("e_entry / e_phoff / e_flags are not 0");
    if (nobj_rd(f + 52, 2) != 64 || nobj_rd(f + 54, 2) != 0 || nobj_rd(f + 56, 2) != 0) FAILF("e_ehsize / e_phentsize / e_phnum");
    if (nobj_rd(f + 58, 2) != 64) FAILF("e_shentsize is not 64");
    uint64_t shoff = nobj_rd(f + 40, 8);
    int shnum = (int)nobj_rd(f + 60, 2), shstrndx = (int)nobj_rd(f + 62, 2);
    if (shoff % 8 || shoff < 64 || shoff + 64 * (uint64_t)shnum != n) FAILF("the section header table (offset %llu, %d entries) does not end the %zu-byte file", (unsigned long long)shoff, shnum, n);
    if (shnum < 4 || shstrndx != shnum - 1) FAILF("e_shnum %d, e_shstrndx %d", shnum, shstrndx);
    Sh* sh = calloc((size_t)shnum, sizeof *sh);
    for (int i = 0; i < shnum; i++) {
        const uint8_t* p = f + shoff + 64 * (uint64_t)i;
        sh[i] = (Sh){(uint32_t)nobj_rd(p, 4), (uint32_t)nobj_rd(p + 4, 4), (uint32_t)nobj_rd(p + 40, 4), (uint32_t)nobj_rd(p + 44, 4),
                     nobj_rd(p + 8, 8), nobj_rd(p + 16, 8), nobj_rd(p + 24, 8), nobj_rd(p + 32, 8), nobj_rd(p + 48, 8), nobj_rd(p + 56, 8)};
    }
#define SFAIL(...) do { printf("FAIL %s: ", name); printf(__VA_ARGS__); printf("\n"); free(sh); return 1; } while (0)
    for (int i = 0; i < 64; i++) if (f[shoff + i]) SFAIL("section 0 is not the null section");
    const Sh* ss = &sh[shstrndx];
    if (ss->type != 3 || ss->off + ss->size > shoff || ss->size == 0) SFAIL(".shstrtab is not a string table inside the file");
    const uint8_t* shs = f + ss->off;
    if (shs[0] || shs[ss->size - 1]) SFAIL(".shstrtab does not start and end with NUL");
    int isym = -1;
    for (int i = 1; i < shnum; i++) {
        const Sh* h = &sh[i];
        const char* nm = str_at(shs, ss->size, h->name);
        if (!nm) SFAIL("section %d has no name", i);
        if (h->type != 8 && h->off + h->size > shoff) SFAIL("%s goes into the section header table", nm);
        if (h->type != 8 && h->type != 0 && h->off < 64) SFAIL("%s overlaps the ELF header", nm);
        if (h->align > 1 && h->off % h->align) SFAIL("%s at offset %llu is not aligned to %llu", nm, (unsigned long long)h->off, (unsigned long long)h->align);
        if (h->addr != 0) SFAIL("%s has an address (%llx) in a relocatable object", nm, (unsigned long long)h->addr);
        if (h->type == 2) { if (isym >= 0) SFAIL("two symbol tables"); isym = i; }
        for (int j = 1; j < i; j++) {
            const Sh* g = &sh[j];
            if (h->type == 8 || g->type == 8 || !h->size || !g->size) continue;
            if (h->off < g->off + g->size && g->off < h->off + h->size) SFAIL("%s and %s overlap in the file", nm, str_at(shs, ss->size, g->name));
        }
    }
    if (isym < 0) SFAIL("no .symtab");
    const Sh* sy = &sh[isym];
    if (sy->entsize != 24 || sy->size % 24 || sy->align != 8 || sy->link >= (uint32_t)shnum || sh[sy->link].type != 3) SFAIL(".symtab header: entsize %llu size %llu align %llu link %u", (unsigned long long)sy->entsize, (unsigned long long)sy->size, (unsigned long long)sy->align, sy->link);
    uint64_t ns = sy->size / 24;
    const Sh* st = &sh[sy->link];
    if (st->size == 0 || f[st->off] || f[st->off + st->size - 1]) SFAIL(".strtab does not start and end with NUL");
    // Symbols: null, section symbols, LOCAL ..., then GLOBAL ...; sh_info is the first GLOBAL.
    for (int i = 0; i < 24; i++) if (f[sy->off + (unsigned)i]) SFAIL("symbol 0 is not the null symbol");
    if (sy->info < 1 || sy->info > ns) SFAIL(".symtab sh_info %u out of range (%llu symbols)", sy->info, (unsigned long long)ns);
    for (uint64_t i = 1; i < ns; i++) {
        const uint8_t* p = f + sy->off + 24 * i;
        int info = (int)nobj_rd(p + 4, 1), shi = (int)nobj_rd(p + 6, 2), bind = info >> 4, type = info & 15;
        if ((i < sy->info) != (bind == 0)) SFAIL("symbol %llu (bind %d) is on the wrong side of sh_info %u", (unsigned long long)i, bind, sy->info);
        if (shi >= shnum && shi < 0xff00) SFAIL("symbol %llu: section index %d", (unsigned long long)i, shi);
        if (type == 3 && (bind != 0 || nobj_rd(p, 4) != 0 || nobj_rd(p + 8, 8) != 0)) SFAIL("section symbol %llu is not LOCAL, nameless and at 0", (unsigned long long)i);
        if (type != 3 && !str_at(f + st->off, st->size, nobj_rd(p, 4))) SFAIL("symbol %llu has no name", (unsigned long long)i);
        if (shi == 0 && type != 0) SFAIL("undefined symbol %llu has a type", (unsigned long long)i);
        if (shi > 0 && shi < shnum && type != 3 && nobj_rd(p + 8, 8) > sh[shi].size) SFAIL("symbol %llu is past the end of its section", (unsigned long long)i);
    }
    // Every .rela: links to .symtab, applies to a section that is not itself a table, entries in range.
    for (int i = 1; i < shnum; i++) {
        const Sh* h = &sh[i];
        if (h->type != 4) continue;
        const char* nm = str_at(shs, ss->size, h->name);
        if (h->link != (uint32_t)isym || h->info == 0 || h->info >= (uint32_t)shnum || h->entsize != 24 || h->size % 24 || h->align != 8 || h->flags != 0x40)
            SFAIL("%s header: link %u info %u entsize %llu align %llu flags %llx", nm, h->link, h->info, (unsigned long long)h->entsize, (unsigned long long)h->align, (unsigned long long)h->flags);
        const char* tn = str_at(shs, ss->size, sh[h->info].name);
        if (strncmp(nm, ".rela", 5) || !tn || strcmp(nm + 5, tn)) SFAIL("%s does not apply to %s", nm, tn ? tn : "?");
        for (uint64_t k = 0; k < h->size / 24; k++) {
            const uint8_t* p = f + h->off + 24 * k;
            uint64_t info = nobj_rd(p + 8, 8);
            if ((info >> 32) == 0 || (info >> 32) >= ns) SFAIL("%s entry %llu names symbol %llu", nm, (unsigned long long)k, (unsigned long long)(info >> 32));
            if (nobj_rd(p, 8) >= sh[h->info].size) SFAIL("%s entry %llu is outside %s", nm, (unsigned long long)k, tn);
        }
    }
#undef SFAIL
    *shout = sh; *shnum_out = shnum;
    return 0;
}

static int ends_with_dot(const char* s) { return strchr(s, '.') != NULL; }

static int verify(const char* name, const uint8_t* f, size_t n) {
    Sh* sh = NULL; int shnum = 0;
    if (verify_structure(name, f, n, &sh, &shnum)) return 1;
    free(sh);
    tot_file += (long)n;
    return 0;
}

// Compare the object read from `mine` with nasm's.
static int compare(const char* name, const NObj* m, const NObj* o) {
    for (int k = 0; k < ASM_NSEC; k++) {
        const NSec* a = &m->s[k];
        const NSec* b = &o->s[k];
        int ha = a->present && a->n, hb = b->present && b->n;
        if (ha != hb) FAILF("%s: mine %s, nasm %s", SEC_NAME[k], ha ? "has it" : "does not", hb ? "has it" : "does not");
        if (!ha) continue;
        if (a->n != b->n) FAILF("%s: %zu bytes, nasm has %zu", SEC_NAME[k], a->n, b->n);
        if (k != ASM_SEC_BSS && memcmp(a->b, b->b, a->n)) {
            size_t d = 0;
            while (a->b[d] == b->b[d]) d++;
            FAILF("%s differs from nasm's at offset %zu", SEC_NAME[k], d);
        }
        if (a->align != b->align) FAILF("%s: sh_addralign %llu, nasm has %llu", SEC_NAME[k], (unsigned long long)a->align, (unsigned long long)b->align);
        if (a->nrel != b->nrel) FAILF("%s: %zu relocations, nasm has %zu", SEC_NAME[k], a->nrel, b->nrel);
        for (size_t i = 0; i < a->nrel; i++) {
            const NRel* x = &a->rel[i];
            const NRel* y = &b->rel[i];
            if (x->off != y->off || x->type != y->type || x->addend != y->addend || x->secsym != y->secsym || strcmp(x->sym, y->sym))
                FAILF("%s relocation %zu: +%d type %d %s %s %+lld; nasm: +%d type %d %s %s %+lld", SEC_NAME[k], i, x->off, x->type, x->secsym ? "section" : "symbol", x->sym, (long long)x->addend,
                      y->off, y->type, y->secsym ? "section" : "symbol", y->sym, (long long)y->addend);
            n_relocs++;
        }
    }
    // Symbols. nasm's: labels without a dot in a section with bytes, and the undefined names.
    size_t exp_def = 0, exp_und = 0, got_def = 0, got_und = 0;
    for (size_t j = 0; j < o->nsym; j++) {
        const NSym* y = &o->sym[j];
        if (!y->name[0] || y->type == 3 || y->type == 4) continue;
        if (y->shndx == 0) { exp_und++; continue; }
        if (y->shndx >= 64 || o->sec_of_index[y->shndx] < 0 || ends_with_dot(y->name) || !o->s[o->sec_of_index[y->shndx]].n) continue;
        exp_def++;
    }
    for (size_t i = 0; i < m->nsym; i++) {
        const NSym* y = &m->sym[i];
        if (!y->name[0] || y->type == 3) continue;
        const NSym* z = NULL;
        for (size_t j = 0; j < o->nsym; j++) if (!strcmp(o->sym[j].name, y->name) && o->sym[j].type != 3 && o->sym[j].type != 4) { z = &o->sym[j]; break; }
        if (!z) FAILF("symbol %s is not in nasm's object", y->name);
        if (y->shndx == 0) {
            if (z->shndx != 0 || y->bind != 1 || z->bind != 1) FAILF("extern %s: shndx %d bind %d, nasm has shndx %d bind %d", y->name, y->shndx, y->bind, z->shndx, z->bind);
            got_und++; n_ext++;
            continue;
        }
        if (z->shndx == 0 || z->shndx >= 64 || o->sec_of_index[z->shndx] < 0) FAILF("symbol %s: nasm does not define it in a section", y->name);
        int k = m->sec_of_index[y->shndx], kz = o->sec_of_index[z->shndx];
        if (k < 0 || k != kz) FAILF("symbol %s is in section %d, nasm has %d", y->name, k, kz);
        if (y->value != z->value) FAILF("symbol %s at %llu, nasm has %llu", y->name, (unsigned long long)y->value, (unsigned long long)z->value);
        if (y->bind != z->bind) FAILF("symbol %s: bind %d, nasm has %d", y->name, y->bind, z->bind);
        if (y->type != (k == ASM_SEC_TEXT ? 2 : 1)) FAILF("symbol %s: type %d", y->name, y->type);
        uint64_t next = o->s[kz].n;
        for (size_t j = 0; j < o->nsym; j++) {
            const NSym* w = &o->sym[j];
            if (w->name[0] && w->shndx == z->shndx && w->type != 3 && w->type != 4 && !ends_with_dot(w->name) && w->value > z->value && w->value < next) next = w->value;
        }
        if (y->size != next - z->value) FAILF("symbol %s: size %llu, expected %llu", y->name, (unsigned long long)y->size, (unsigned long long)(next - z->value));
        got_def++; n_syms++;
    }
    if (got_def != exp_def) FAILF("%zu defined symbols, nasm's object has %zu labels without a dot", got_def, exp_def);
    if (got_und != exp_und) FAILF("%zu undefined symbols, nasm's object has %zu", got_und, exp_und);
    return 0;
}

static int check(const char* work, const char* name, const char* text, const char* objfile) {
    char p[600], cmd[1400];
    snprintf(p, sizeof p, "%s/prog.asm", work);
    FILE* f = fopen(p, "w");
    if (!f) { perror(p); return 1; }
    size_t sz = strlen(text) * 3 + 64;
    char* nt = malloc(sz);
    for_nasm(text, nt, sz);
    fputs(nt, f); fclose(f); free(nt);
    snprintf(cmd, sizeof cmd, "nasm -f elf64 -w-all '%s/prog.asm' -o '%s/nasm.o' 2>'%s/prog.err'", work, work, work);
    if (system(cmd) != 0) { printf("FAIL %s: nasm refuses the program (see %s/prog.err)\n", name, work); n_bad++; return 1; }
    snprintf(p, sizeof p, "%s/nasm.o", work);
    NObj o;
    if (!nobj_load(p, &o)) { printf("FAIL %s: cannot read nasm object\n", name); n_bad++; return 1; }
    AsmOut a; char err[256];
    if (asm_assemble_obj(text, &a, err, sizeof err)) { printf("FAIL %s: asm.c refuses: %s\n", name, err); nobj_free(&o); n_bad++; return 1; }
    uint8_t* img; size_t n;
    if (elf_build_obj(&a, &img, &n, err, sizeof err)) { printf("FAIL %s: elf_build_obj: %s\n", name, err); asm_free(&a); nobj_free(&o); n_bad++; return 1; }
    int bad = verify(name, img, n);
    char mine[600];
    snprintf(mine, sizeof mine, "%s/mine.o", work);
    if (!bad) {
        if (!objfile && elf_write_obj(&a, mine, err, sizeof err)) { printf("FAIL %s: elf_write_obj: %s\n", name, err); bad = 1; }
        const char* path = objfile ? objfile : mine;
        uint8_t* disk; size_t dn;
        if (!bad && !nobj_read_file(path, &disk, &dn)) { printf("FAIL %s: cannot read %s\n", name, path); bad = 1; }
        else if (!bad) {
            if (dn != n || memcmp(disk, img, n)) { printf("FAIL %s: %s is not what elf_build_obj produces (%zu vs %zu bytes)\n", name, path, dn, n); bad = 1; }
            free(disk);
        }
        NObj m;
        if (!bad && !nobj_load(path, &m)) { printf("FAIL %s: the reader cannot read %s\n", name, path); bad = 1; }
        else if (!bad) { bad = compare(name, &m, &o); nobj_free(&m); }
    }
    free(img); asm_free(&a); nobj_free(&o);
    if (bad) n_bad++; else n_ok++;
    return bad;
}

static char* slurp(const char* path) {
    uint8_t* b; size_t n;
    if (!nobj_read_file(path, &b, &n)) return NULL;
    b[n] = 0;
    return (char*)b;
}

// Programs the object writer (or asm.c) must refuse, with a word of the reason.
static int reject(void) {
    static const struct { const char* why; const char* text; } R[] = {
        {"no code and no data", "section .text\n"},
        {"extern", "extern f\nf:\n    ret\n"},
        {"extern", "extern f\nextern f\nsection .text\n    ret\n"},
        {"short jump", "extern f\nsection .text\n    jmp short f\n"},
        {"undefined", "section .text\n    call g\n"},
        {"undefined", "section .text\n    mov rax, [g]\n"},
        {"extern takes one name", "extern a, b\n"},
        {"extern takes one name", "extern\n"},
    };
    int bad = 0;
    for (size_t i = 0; i < sizeof R / sizeof *R; i++) {
        AsmOut a; char err[256] = "";
        int rc = asm_assemble_obj(R[i].text, &a, err, sizeof err);
        if (rc == 0) {
            uint8_t* img; size_t n;
            if (elf_build_obj(&a, &img, &n, err, sizeof err) == 0) { printf("FAIL reject %zu: accepted\n", i + 1); free(img); bad = 1; asm_free(&a); continue; }
            asm_free(&a);
        }
        if (!strstr(err, R[i].why)) { printf("FAIL reject %zu: wrong reason '%s' (expected '%s')\n", i + 1, err, R[i].why); bad = 1; }
        else printf("  reject %zu: %s\n", i + 1, err);
    }
    printf(bad ? "reject: FAIL\n" : "reject: every program refused\n");
    return bad;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: test_obj WORKDIR files|cases|obj|reject ...\n"); return 2; }
    const char* work = argv[1];
    const char* mode = argv[2];
    detect_nasm();
    if (!strcmp(mode, "reject")) return reject();
    int bad = 0;
    if (!strcmp(mode, "files")) {
        for (int i = 3; i < argc; i++) {
            char* t = slurp(argv[i]);
            if (!t) { printf("FAIL %s: cannot read\n", argv[i]); bad = 1; n_bad++; continue; }
            bad |= check(work, argv[i], t, NULL);
            free(t);
        }
    } else if (!strcmp(mode, "obj")) {
        if (argc != 5) { fprintf(stderr, "usage: test_obj WORKDIR obj OBJ FILE.asm\n"); return 2; }
        char* t = slurp(argv[4]);
        if (!t) { printf("FAIL %s: cannot read\n", argv[4]); return 1; }
        bad = check(work, argv[3], t, argv[3]);
        free(t);
    } else if (!strcmp(mode, "cases")) {
        char* t = slurp(argv[3]);
        if (!t) { printf("FAIL %s: cannot read\n", argv[3]); return 1; }
        int idx = 0;
        for (char* p = t; p && *p; ) {
            char* q = p;
            char* blk_end = NULL;
            while (q && *q) {
                if (!strncmp(q, ";---", 4)) { blk_end = q; break; }
                q = strchr(q, '\n');
                if (q) q++;
            }
            char* next = blk_end ? strchr(blk_end, '\n') : NULL;
            if (blk_end) *blk_end = 0;
            idx++;
            char nm[64];
            snprintf(nm, sizeof nm, "obj case %d", idx);
            if (strspn(p, " \t\r\n") != strlen(p)) bad |= check(work, nm, p, NULL);
            p = next ? next + 1 : NULL;
        }
        free(t);
    } else { fprintf(stderr, "unknown mode %s\n", mode); return 2; }
    printf("obj: %ld objects verified, %ld failed; %ld relocations, %ld defined symbols and %ld externs compared with nasm, %ld bytes of ELF\n", n_ok, n_bad, n_relocs, n_syms, n_ext, tot_file);
    return bad || n_bad ? 1 : 0;
}
