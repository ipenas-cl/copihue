// Structure check of the executables written by compiler/native/elf.c (M4).
//
//   test_elf WORKDIR files FILE.asm ...   each file is one program
//   test_elf WORKDIR cases FILE.asm       blocks separated by a line `;---`, each one a program
//   test_elf WORKDIR exe ELF FILE.asm     an ELF file already written (by `copihue --native`) for that program
//   test_elf WORKDIR reject               programs the writer must refuse
//
// Nothing here runs the executable (the development machine may not be Linux).
// What is checked, with a reader written for the purpose and the object that
// `nasm -f elf64` makes from the same text as the oracle:
//   - the ELF header and each program header, field by field (ET_EXEC, EM_X86_64,
//     PT_LOAD R+X then R+W, vaddr == offset mod 0x1000, p_align 0x1000,
//     filesz <= memsz, the segments in different pages, zero padding everywhere
//     between pieces);
//   - (M5) the section headers (null, .text, .rodata, .data, .bss as the program
//     has them, .symtab, .strtab, .shstrtab), e_shoff/e_shnum/e_shstrndx, the
//     tables contiguous at the end of the file, and the symbols against those of
//     nasm's object (see verify_tail);
//   - e_entry == address of _start;
//   - the address of every section is read off the program headers (not from the
//     writer's layout) and is aligned as the program asked;
//   - the bytes of .text, .rodata and .data in the file equal nasm's, with the
//     relocations of nasm's object (its own .rela tables) applied at those
//     addresses: the field of each relocation, patched with S + A (or S + A - P).
// In `exe` mode the file must also equal what the library builds in memory.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../../compiler/native/asm.h"
#include "../../compiler/native/elf.h"
#include "nasmfix.h"
#include "nobj.h"

static const char* const SEC_NAME[ASM_NSEC] = {".text", ".data", ".bss", ".rodata"};

static long n_ok, n_bad, n_relocs, n_two_seg, n_ro;
static long tot_file;

#define FAILF(...) do { printf("FAIL %s: ", name); printf(__VA_ARGS__); printf("\n"); return 1; } while (0)

static void put(uint8_t* p, uint64_t v, int n) { for (int i = 0; i < n; i++) p[i] = (uint8_t)(v >> (8 * i)); }

typedef struct { uint64_t type, flags, off, va, pa, filesz, memsz, align; } Ph;

// Zero check of [a, b) in the file.
static int zeros(const uint8_t* f, uint64_t a, uint64_t b) {
    for (uint64_t i = a; i < b; i++) if (f[i]) return 0;
    return 1;
}

typedef struct { uint32_t name, type, link, info; uint64_t flags, addr, off, size, align, entsize; } Sh;

// NUL-terminated string at `at` inside the table [tab, tab + tabn), or NULL.
static const char* str_at(const uint8_t* tab, uint64_t tabn, uint64_t at) {
    if (at >= tabn) return NULL;
    for (uint64_t i = at; i < tabn; i++) if (!tab[i]) return (const char*)tab + at;
    return NULL;
}

// The tail the writer adds after the mapped bytes (M5): .symtab, .strtab,
// .shstrtab (8-aligned) and the section header table, which ends the file.
// `base`/`off` are the addresses and offsets read off the program headers.
// The symbols are compared with those of nasm's object: every label without a
// dot in its name, in a section that has bytes, with its name, bind (GLOBAL only
// if nasm made it global), section and address; type FUNC in .text and OBJECT
// elsewhere; size up to the next such label at a higher offset or the section's end.
static int verify_tail(const char* name, const uint8_t* f, size_t n, const NObj* o, const uint64_t* base, const uint64_t* off, uint64_t end) {
    uint64_t shoff = nobj_rd(f + 40, 8);
    int shnum = (int)nobj_rd(f + 60, 2), shstrndx = (int)nobj_rd(f + 62, 2);
    if (nobj_rd(f + 58, 2) != 64) FAILF("e_shentsize is not 64");
    if (shoff % 8 || shoff < end || shoff + 64 * (uint64_t)shnum != n) FAILF("section header table at %llu (%d entries) does not end the %zu-byte file", (unsigned long long)shoff, shnum, n);
    int want = 4;                                   // null, .symtab, .strtab, .shstrtab
    for (int k = 0; k < ASM_NSEC; k++) if (o->s[k].present && o->s[k].n) want++;
    if (shnum != want) FAILF("e_shnum is %d, expected %d", shnum, want);
    if (shstrndx != shnum - 1) FAILF("e_shstrndx is %d, expected %d", shstrndx, shnum - 1);
    Sh* sh = calloc((size_t)shnum, sizeof *sh);
    for (int i = 0; i < shnum; i++) {
        const uint8_t* p = f + shoff + 64 * (uint64_t)i;
        sh[i] = (Sh){(uint32_t)nobj_rd(p, 4), (uint32_t)nobj_rd(p + 4, 4), (uint32_t)nobj_rd(p + 40, 4), (uint32_t)nobj_rd(p + 44, 4),
                     nobj_rd(p + 8, 8), nobj_rd(p + 16, 8), nobj_rd(p + 24, 8), nobj_rd(p + 32, 8), nobj_rd(p + 48, 8), nobj_rd(p + 56, 8)};
    }
// Print first: the arguments may read `sh` (h->, sy->, ss->), which is freed after.
#define TFAIL(...) do { printf("FAIL %s: ", name); printf(__VA_ARGS__); printf("\n"); free(sh); return 1; } while (0)
    if (!zeros(f, shoff, shoff + 64)) TFAIL("section 0 is not the null section");
    const Sh* ss = &sh[shstrndx];
    if (ss->type != 3 || ss->off + ss->size > shoff) TFAIL(".shstrtab is not a string table inside the file");
    const uint8_t* shs = f + ss->off;
    // The mapped sections, in the order .text, .rodata, .data, .bss.
    static const int ORDER[ASM_NSEC] = {ASM_SEC_TEXT, ASM_SEC_RODATA, ASM_SEC_DATA, ASM_SEC_BSS};
    static const uint64_t FL[ASM_NSEC] = {6, 3, 3, 2};
    int shidx[ASM_NSEC] = {0}, at = 1;
    for (int i = 0; i < ASM_NSEC; i++) {
        int k = ORDER[i];
        const NSec* s = &o->s[k];
        if (!s->present || !s->n) continue;
        const Sh* h = &sh[at];
        const char* nm = str_at(shs, ss->size, h->name);
        if (!nm || strcmp(nm, SEC_NAME[k])) TFAIL("section %d is not named %s", at, SEC_NAME[k]);
        if (h->type != (k == ASM_SEC_BSS ? 8u : 1u)) TFAIL("%s: sh_type %u", SEC_NAME[k], h->type);
        if (h->flags != FL[k]) TFAIL("%s: sh_flags %llx", SEC_NAME[k], (unsigned long long)h->flags);
        if (h->addr != base[k] || h->size != s->n) TFAIL("%s: addr %llx size %llu, expected %llx and %zu", SEC_NAME[k], (unsigned long long)h->addr, (unsigned long long)h->size, (unsigned long long)base[k], s->n);
        if (k != ASM_SEC_BSS && h->off != off[k]) TFAIL("%s: sh_offset %llx, the program headers put it at %llx", SEC_NAME[k], (unsigned long long)h->off, (unsigned long long)off[k]);
        if (k == ASM_SEC_BSS && h->off != end) TFAIL(".bss: sh_offset %llx, expected the end of the mapped bytes %llx", (unsigned long long)h->off, (unsigned long long)end);
        if (h->align != (s->align ? s->align : 1)) TFAIL("%s: sh_addralign %llu, expected %llu", SEC_NAME[k], (unsigned long long)h->align, (unsigned long long)s->align);
        shidx[k] = at++;
    }
    const Sh* sy = &sh[at];
    const Sh* sr = &sh[at + 1];
    const char* n1 = str_at(shs, ss->size, sy->name);
    const char* n2 = str_at(shs, ss->size, sr->name);
    const char* n3 = str_at(shs, ss->size, ss->name);
    if (!n1 || strcmp(n1, ".symtab") || !n2 || strcmp(n2, ".strtab") || !n3 || strcmp(n3, ".shstrtab")) TFAIL("the last three sections are not .symtab, .strtab, .shstrtab");
    if (sy->type != 2 || sy->entsize != 24 || sy->align != 8 || sy->size % 24 || sy->link != (uint32_t)(at + 1)) TFAIL(".symtab header: type %u entsize %llu align %llu size %llu link %u", sy->type, (unsigned long long)sy->entsize, (unsigned long long)sy->align, (unsigned long long)sy->size, sy->link);
    if (sr->type != 3) TFAIL(".strtab: sh_type %u", sr->type);
    if (sy->off != ((end + 7) & ~7ULL) || sr->off != sy->off + sy->size || ss->off != sr->off + sr->size) TFAIL("tables are not contiguous after the mapped bytes (symtab %llu, strtab %llu, shstrtab %llu, mapped end %llu)", (unsigned long long)sy->off, (unsigned long long)sr->off, (unsigned long long)ss->off, (unsigned long long)end);
    if (!zeros(f, end, sy->off)) TFAIL("padding before .symtab is not zero");
    if (!zeros(f, ss->off + ss->size, shoff)) TFAIL("padding before the section headers is not zero");
    if (shs[0] || ss->size == 0 || shs[ss->size - 1]) TFAIL(".shstrtab does not start and end with NUL");
    if (sr->size == 0 || f[sr->off] || f[sr->off + sr->size - 1]) TFAIL(".strtab does not start and end with NUL");

    // Symbols.
    size_t ns = (size_t)(sy->size / 24);
    if (ns == 0 || !zeros(f, sy->off, sy->off + 24)) TFAIL("symbol 0 is not the null symbol");
    if (sy->info < 1 || sy->info > ns) TFAIL(".symtab sh_info %u out of range", sy->info);
    int* seen = calloc(o->nsym ? o->nsym : 1, sizeof *seen);
    size_t expected = 0, nglob = 0;
    for (size_t j = 0; j < o->nsym; j++) {
        const NSym* y = &o->sym[j];
        if (!y->name[0] || y->shndx <= 0 || y->shndx >= 64 || o->sec_of_index[y->shndx] < 0 || strchr(y->name, '.') || !shidx[o->sec_of_index[y->shndx]]) continue;
        expected++;
    }
    for (size_t i = 1; i < ns; i++) {
        const uint8_t* p = f + sy->off + 24 * i;
        const char* nm = str_at(f + sr->off, sr->size, nobj_rd(p, 4));
        int info = (int)nobj_rd(p + 4, 1), shi = (int)nobj_rd(p + 6, 2);
        uint64_t val = nobj_rd(p + 8, 8), size = nobj_rd(p + 16, 8);
        if (!nm || !nm[0]) { free(seen); TFAIL("symbol %zu has no name", i); }
        if (nobj_rd(p + 5, 1) != 0) { free(seen); TFAIL("symbol %s: st_other", nm); }
        if ((i < sy->info) != ((info >> 4) == 0)) { free(seen); TFAIL("symbol %s (bind %d) is on the wrong side of sh_info %u", nm, info >> 4, sy->info); }
        if ((info >> 4) == 1) nglob++;
        const NSym* y = NULL;
        size_t jy = 0;
        for (size_t j = 0; j < o->nsym; j++) if (!strcmp(o->sym[j].name, nm) && o->sym[j].shndx > 0 && o->sym[j].shndx < 64) { y = &o->sym[j]; jy = j; break; }
        if (!y || o->sec_of_index[y->shndx] < 0) { free(seen); TFAIL("symbol %s is not a label of nasm's object", nm); }
        if (seen[jy]++) { free(seen); TFAIL("symbol %s appears twice", nm); }
        int k = o->sec_of_index[y->shndx];
        if (strchr(nm, '.')) { free(seen); TFAIL("symbol %s has a dot: it is a branch label", nm); }
        if (shi != shidx[k]) { free(seen); TFAIL("symbol %s: section index %d, expected %d (%s)", nm, shi, shidx[k], SEC_NAME[k]); }
        if (val != base[k] + y->value) { free(seen); TFAIL("symbol %s at %llx, nasm's label puts it at %llx", nm, (unsigned long long)val, (unsigned long long)(base[k] + y->value)); }
        if ((info >> 4) != (y->bind == 1 ? 1 : 0)) { free(seen); TFAIL("symbol %s: bind %d, nasm has %d", nm, info >> 4, y->bind); }
        if ((info & 15) != (k == ASM_SEC_TEXT ? 2 : 1)) { free(seen); TFAIL("symbol %s: type %d", nm, info & 15); }
        uint64_t next = o->s[k].n;
        for (size_t j = 0; j < o->nsym; j++) {
            const NSym* z = &o->sym[j];
            if (z->name[0] && z->shndx == y->shndx && !strchr(z->name, '.') && z->value > y->value && z->value < next) next = z->value;
        }
        if (size != next - y->value) { free(seen); TFAIL("symbol %s: size %llu, expected %llu", nm, (unsigned long long)size, (unsigned long long)(next - y->value)); }
    }
    if (ns - 1 != expected) { free(seen); TFAIL(".symtab has %zu symbols, nasm's object has %zu labels without a dot", ns - 1, expected); }
    if (nglob == 0) { free(seen); TFAIL("no global symbol (_start)"); }
    free(seen);
    free(sh);
#undef TFAIL
    return 0;
}

static int verify(const char* name, const uint8_t* f, size_t n, const NObj* o) {
    if (n < 64) FAILF("file of %zu bytes", n);
    if (memcmp(f, "\x7f" "ELF", 4) || f[4] != 2 || f[5] != 1 || f[6] != 1) FAILF("e_ident is not ELF64, little endian, version 1");
    for (int i = 7; i < 16; i++) if (f[i]) FAILF("e_ident[%d] is not 0", i);
    if (nobj_rd(f + 16, 2) != 2) FAILF("e_type is not ET_EXEC");
    if (nobj_rd(f + 18, 2) != 62) FAILF("e_machine is not EM_X86_64");
    if (nobj_rd(f + 20, 4) != 1) FAILF("e_version");
    if (nobj_rd(f + 32, 8) != 64) FAILF("e_phoff is not 64");
    if (nobj_rd(f + 48, 4) != 0) FAILF("e_flags");
    if (nobj_rd(f + 52, 2) != 64 || nobj_rd(f + 54, 2) != 56) FAILF("e_ehsize / e_phentsize");
    int nph = (int)nobj_rd(f + 56, 2);
    const NSec* tx = &o->s[ASM_SEC_TEXT];
    const NSec* da = &o->s[ASM_SEC_DATA];
    const NSec* bs = &o->s[ASM_SEC_BSS];
    const NSec* ro = &o->s[ASM_SEC_RODATA];
    int want2 = (da->present && da->n) || (bs->present && bs->n);
    if (nph != (want2 ? 2 : 1)) FAILF("e_phnum is %d, expected %d", nph, want2 ? 2 : 1);
    if (n < 64 + 56 * (size_t)nph) FAILF("file too short for its program headers");
    Ph ph[2];
    for (int i = 0; i < nph; i++) {
        const uint8_t* p = f + 64 + 56 * i;
        ph[i] = (Ph){nobj_rd(p, 4), nobj_rd(p + 4, 4), nobj_rd(p + 8, 8), nobj_rd(p + 16, 8), nobj_rd(p + 24, 8), nobj_rd(p + 32, 8), nobj_rd(p + 40, 8), nobj_rd(p + 48, 8)};
        if (ph[i].type != 1) FAILF("segment %d is not PT_LOAD", i);
        if (ph[i].align != 0x1000) FAILF("segment %d: p_align %llx", i, (unsigned long long)ph[i].align);
        if (ph[i].pa != ph[i].va) FAILF("segment %d: paddr != vaddr", i);
        if ((ph[i].va - ph[i].off) % 0x1000) FAILF("segment %d: vaddr %llx and offset %llx differ mod 0x1000", i, (unsigned long long)ph[i].va, (unsigned long long)ph[i].off);
        if (ph[i].filesz > ph[i].memsz) FAILF("segment %d: filesz > memsz", i);
        if (ph[i].off + ph[i].filesz > n) FAILF("segment %d goes past the end of the file", i);
        if (ph[i].va < 0x400000) FAILF("segment %d below 0x400000", i);
    }
    if (ph[0].flags != 5) FAILF("segment 0 flags %llu, expected R+X (5)", (unsigned long long)ph[0].flags);
    if (ph[0].off != 0 || ph[0].va != 0x400000) FAILF("segment 0 does not map the file start at 0x400000");
    if (ph[0].filesz != ph[0].memsz) FAILF("segment 0: filesz != memsz");
    uint64_t hdr_end = 64 + 56 * (uint64_t)nph;
    uint64_t text_va = ph[0].va + 0x1000, text_off = 0x1000;
    if (text_va != 0x401000) FAILF(".text not at 0x401000");
    if (ph[0].filesz < text_off + tx->n) FAILF("segment 0 does not hold .text");
    if (!zeros(f, hdr_end, text_off)) FAILF("padding between the headers and .text is not zero");
    uint64_t base[ASM_NSEC] = {0}, off[ASM_NSEC] = {0};
    base[ASM_SEC_TEXT] = text_va; off[ASM_SEC_TEXT] = text_off;
    uint64_t pos = text_off + tx->n;
    if (ro->present && ro->n) {                       // the last bytes of segment 0
        off[ASM_SEC_RODATA] = ph[0].filesz - ro->n;
        base[ASM_SEC_RODATA] = ph[0].va + off[ASM_SEC_RODATA];
        if (off[ASM_SEC_RODATA] < pos || off[ASM_SEC_RODATA] - pos >= (ro->align > 1 ? ro->align : 1)) FAILF(".rodata is not right after .text (aligned %llu)", (unsigned long long)ro->align);
        if (base[ASM_SEC_RODATA] % (ro->align ? ro->align : 1)) FAILF(".rodata at %llx is not aligned to %llu", (unsigned long long)base[ASM_SEC_RODATA], (unsigned long long)ro->align);
        if (!zeros(f, pos, off[ASM_SEC_RODATA])) FAILF("padding before .rodata is not zero");
        pos = ph[0].filesz;
        n_ro++;
    } else if (ph[0].filesz != pos) FAILF("segment 0 is %llu bytes, .text ends at %llu", (unsigned long long)ph[0].filesz, (unsigned long long)pos);
    uint64_t end = ph[0].filesz;
    if (nph == 2) {
        if (ph[1].flags != 6) FAILF("segment 1 flags %llu, expected R+W (6)", (unsigned long long)ph[1].flags);
        if (ph[1].va / 0x1000 <= (ph[0].va + ph[0].memsz - 1) / 0x1000) FAILF("segments 0 and 1 share a page of memory");
        if (ph[1].off < ph[0].filesz) FAILF("segment 1 starts inside segment 0 in the file");
        uint64_t seg_end = ph[1].va + ph[1].memsz;
        if (da->present && da->n) {
            if (ph[1].filesz != da->n) FAILF("segment 1 filesz %llu, .data has %zu bytes", (unsigned long long)ph[1].filesz, da->n);
            base[ASM_SEC_DATA] = ph[1].va; off[ASM_SEC_DATA] = ph[1].off;
            if (ph[1].va % (da->align ? da->align : 1)) FAILF(".data at %llx is not aligned to %llu", (unsigned long long)ph[1].va, (unsigned long long)da->align);
            if (ph[1].off - ph[0].filesz >= (da->align > 1 ? da->align : 1)) FAILF("gap of %llu bytes before .data", (unsigned long long)(ph[1].off - ph[0].filesz));
        } else if (ph[1].filesz != 0) FAILF("segment 1 has file bytes but the program has no .data");
        if (!zeros(f, ph[0].filesz, ph[1].off)) FAILF("padding before segment 1 is not zero");
        if (bs->present && bs->n) {
            base[ASM_SEC_BSS] = seg_end - bs->n;
            uint64_t data_end = ph[1].va + ph[1].filesz;
            if (base[ASM_SEC_BSS] < data_end || base[ASM_SEC_BSS] - data_end >= (bs->align > 1 ? bs->align : 1)) FAILF(".bss at %llx does not follow the data (end %llx)", (unsigned long long)base[ASM_SEC_BSS], (unsigned long long)data_end);
            if (base[ASM_SEC_BSS] % (bs->align ? bs->align : 1)) FAILF(".bss at %llx is not aligned to %llu", (unsigned long long)base[ASM_SEC_BSS], (unsigned long long)bs->align);
        } else if (ph[1].memsz != ph[1].filesz) FAILF("segment 1 memsz != filesz without .bss");
        end = ph[1].off + ph[1].filesz;
        n_two_seg++;
    }
    if (n <= end) FAILF("file is %zu bytes: no room after the mapped bytes (%llu) for the symbol and section tables", n, (unsigned long long)end);
    int tail = verify_tail(name, f, n, o, base, off, end);
    if (tail) return tail;

    // Entry point.
    const NSym* st = NULL;
    for (size_t i = 0; i < o->nsym; i++) if (!strcmp(o->sym[i].name, "_start") && o->sym[i].shndx > 0) st = &o->sym[i];
    if (!st || o->sec_of_index[st->shndx] != ASM_SEC_TEXT) FAILF("nasm object has no _start in .text");
    if (nobj_rd(f + 24, 8) != text_va + st->value) FAILF("e_entry %llx, _start is at %llx", (unsigned long long)nobj_rd(f + 24, 8), (unsigned long long)(text_va + st->value));

    // Contents: nasm's bytes with nasm's relocations applied at the addresses read above.
    for (int k = 0; k < ASM_NSEC; k++) {
        const NSec* s = &o->s[k];
        if (k == ASM_SEC_BSS || !s->present || !s->n) continue;
        uint8_t* want = malloc(s->n);
        memcpy(want, s->b, s->n);
        for (size_t i = 0; i < s->nrel; i++) {
            const NRel* r = &s->rel[i];
            uint64_t sv;
            if (r->secsym) {
                int t = nobj_which(r->sym);
                if (t < 0) { free(want); FAILF("relocation against section '%s'", r->sym); }
                sv = base[t];
            } else {
                const NSym* y = NULL;
                for (size_t j = 0; j < o->nsym; j++) if (!strcmp(o->sym[j].name, r->sym) && o->sym[j].shndx > 0) y = &o->sym[j];
                if (!y || o->sec_of_index[y->shndx] < 0) { free(want); FAILF("relocation against '%s', which is not defined", r->sym); }
                sv = base[o->sec_of_index[y->shndx]] + y->value;
            }
            int64_t v = (int64_t)sv + r->addend;
            int w = 4;
            if (r->type == 1) w = 8;
            else if (r->type == 2) v -= (int64_t)(base[k] + (uint64_t)r->off);
            else if (r->type == 10) { if (v < 0 || v > 0xFFFFFFFFLL) { free(want); FAILF("R_X86_64_32 value out of range"); } }
            else if (r->type != 11) { free(want); FAILF("relocation type %d is not expected in a hosted program", r->type); }
            if (w == 4 && r->type != 10 && (v < INT32_MIN || v > INT32_MAX)) { free(want); FAILF("relocation value %lld does not fit 32 bits", (long long)v); }
            put(want + r->off, (uint64_t)v, w);
            n_relocs++;
        }
        if (memcmp(f + off[k], want, s->n)) {
            size_t d = 0;
            while (want[d] == f[off[k] + d]) d++;
            free(want);
            FAILF("%s differs from nasm's (patched) bytes at offset %zu", SEC_NAME[k], d);
        }
        free(want);
        if (k == ASM_SEC_DATA && off[k] + s->n != end) FAILF("the mapped bytes end at %llu, .data ends at %llu", (unsigned long long)end, (unsigned long long)(off[k] + s->n));
    }
    tot_file += (long)n;
    return 0;
}

// Assemble `text` with nasm and with asm.c, build the ELF in memory (and on
// disk) and verify it. `exe` is an existing file to check instead, or NULL.
static int check(const char* work, const char* name, const char* text, const char* exe) {
    char p[600], cmd[1400];
    snprintf(p, sizeof p, "%s/prog.asm", work);
    FILE* f = fopen(p, "w");
    if (!f) { perror(p); return 1; }
    size_t sz = strlen(text) * 3 + 64;
    char* nt = malloc(sz);
    for_nasm(text, nt, sz);
    fputs(nt, f); fclose(f); free(nt);
    snprintf(cmd, sizeof cmd, "nasm -f elf64 -w-all '%s/prog.asm' -o '%s/prog.o' 2>'%s/prog.err'", work, work, work);
    if (system(cmd) != 0) { printf("FAIL %s: nasm refuses the program (see %s/prog.err)\n", name, work); return 1; }
    snprintf(p, sizeof p, "%s/prog.o", work);
    NObj o;
    if (!nobj_load(p, &o)) { printf("FAIL %s: cannot read nasm object\n", name); return 1; }
    AsmOut a; char err[256];
    if (asm_assemble(text, &a, err, sizeof err)) { printf("FAIL %s: asm.c refuses: %s\n", name, err); nobj_free(&o); return 1; }
    uint8_t* img; size_t n;
    if (elf_build(&a, &img, &n, NULL, err, sizeof err)) { printf("FAIL %s: elf_build: %s\n", name, err); asm_free(&a); nobj_free(&o); return 1; }
    int bad = verify(name, img, n, &o);
    if (!bad) {
        snprintf(p, sizeof p, "%s/prog.elf", work);
        const char* path = exe ? exe : p;
        if (!exe && elf_write_exec(&a, p, err, sizeof err)) { printf("FAIL %s: elf_write_exec: %s\n", name, err); bad = 1; }
        uint8_t* disk; size_t dn;
        if (!bad && !nobj_read_file(path, &disk, &dn)) { printf("FAIL %s: cannot read %s\n", name, path); bad = 1; }
        else if (!bad) {
            if (dn != n || memcmp(disk, img, n)) { printf("FAIL %s: %s is not what elf_build produces (%zu vs %zu bytes)\n", name, path, dn, n); bad = 1; }
            free(disk);
            struct stat sb;
            if (!bad && (stat(path, &sb) || (sb.st_mode & 0777) != 0755)) { printf("FAIL %s: mode of %s is %o, expected 755\n", name, path, (unsigned)(sb.st_mode & 0777)); bad = 1; }
        }
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

// Programs the writer must refuse, with a word of the reason.
static int reject(void) {
    static const struct { const char* why; const char* text; } R[] = {
        {"_start", "global f\nf:\n    ret\n"},
        {"not declared global", "_start:\n    ret\n"},
        {"not in .text", "section .text\nglobal _start\nf:\n    ret\nsection .data\n_start: db 1\n"},
        {"no code", "section .data\nglobal _start\n_start: db 1\n"},
    };
    int bad = 0;
    for (size_t i = 0; i < sizeof R / sizeof *R; i++) {
        AsmOut a; char err[256] = "";
        if (asm_assemble(R[i].text, &a, err, sizeof err)) { printf("FAIL reject %zu: asm.c refuses the program first: %s\n", i + 1, err); bad = 1; continue; }
        uint8_t* img; size_t n;
        if (elf_build(&a, &img, &n, NULL, err, sizeof err) == 0) { printf("FAIL reject %zu: the writer accepted it\n", i + 1); free(img); bad = 1; }
        else if (!strstr(err, R[i].why)) { printf("FAIL reject %zu: wrong reason '%s' (expected '%s')\n", i + 1, err, R[i].why); bad = 1; }
        else printf("  reject %zu: %s\n", i + 1, err);
        asm_free(&a);
    }
    // A relocation that does not fit its field: a 32-bit absolute address is fine at 0x400000+, a
    // large offset into .data is not.
    {
        AsmOut a; char err[256] = "";
        const char* t = "section .text\nglobal _start\n_start:\n    mov rax, [d + 0x7fffffff]\nsection .data\nd: db 1\n";
        int rc = asm_assemble(t, &a, err, sizeof err);
        if (rc == 0) {
            uint8_t* img; size_t n;
            if (elf_build(&a, &img, &n, NULL, err, sizeof err) == 0) { printf("FAIL reject: relocation out of range was accepted\n"); free(img); bad = 1; }
            else printf("  reject 5: %s\n", err);
            asm_free(&a);
        } else printf("  reject 5: asm.c refuses it first (%s)\n", err);
    }
    printf(bad ? "reject: FAIL\n" : "reject: every program refused\n");
    return bad;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: test_elf WORKDIR files|cases|exe|reject ...\n"); return 2; }
    const char* work = argv[1];
    const char* mode = argv[2];
    detect_nasm();
    if (!strcmp(mode, "reject")) return reject();
    int bad = 0;
    if (!strcmp(mode, "files")) {
        for (int i = 3; i < argc; i++) {
            char* t = slurp(argv[i]);
            if (!t) { printf("FAIL %s: cannot read\n", argv[i]); bad = 1; continue; }
            bad |= check(work, argv[i], t, NULL);
            free(t);
        }
    } else if (!strcmp(mode, "exe")) {
        if (argc != 5) { fprintf(stderr, "usage: test_elf WORKDIR exe ELF FILE.asm\n"); return 2; }
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
            snprintf(nm, sizeof nm, "elf case %d", idx);
            if (strspn(p, " \t\r\n")  != strlen(p)) bad |= check(work, nm, p, NULL);
            p = next ? next + 1 : NULL;
        }
        free(t);
    } else { fprintf(stderr, "unknown mode %s\n", mode); return 2; }
    printf("elf: %ld programs verified, %ld failed; %ld with a data segment, %ld with .rodata; %ld relocations patched, %ld bytes of ELF\n", n_ok, n_bad, n_two_seg, n_ro, n_relocs, tot_file);
    return bad || n_bad ? 1 : 0;
}
