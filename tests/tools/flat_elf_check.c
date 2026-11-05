// Reads the ELF64 file written by `copihue --flat ADDR --elf` and the raw image
// of the same program (`--flat ADDR`), and checks the layout the regression
// suite expects of tests/link/parts/flat_{entry,console,note}.lap at 0x10000:
// ELF header, section headers, the section bytes against the raw image, the
// symbols, and the PT_LOAD of .console. Prints the first failed check and exits 1.
//
//   flat_elf_check k.elf k.bin
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t* slurp(const char* path, size_t* n) {
    FILE* f = fopen(path, "rb");
    uint8_t* b;
    long len;
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)len + 1);
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
    fclose(f);
    *n = (size_t)len;
    return b;
}

static size_t g_len;

static void fail(const char* what) {
    fprintf(stderr, "flat_elf_check: %s\n", what);
    exit(1);
}

#define CHECK(c) do { if (!(c)) fail(#c); } while (0)

static uint64_t rd(const uint8_t* d, size_t off, int n) {
    uint64_t v = 0;
    CHECK(off + (size_t)n <= g_len);
    for (int i = n - 1; i >= 0; i--) v = (v << 8) | d[off + (size_t)i];
    return v;
}

typedef struct { char name[64]; uint64_t type, addr, off, size; int present; } Sec;

#define MAXSEC 64
#define MAXSYM 512

int main(int argc, char** argv) {
    size_t rawn;
    uint8_t *d, *raw;
    Sec secs[MAXSEC];
    char symname[MAXSYM][64];
    uint64_t symval[MAXSYM];
    int nsyms = 0;
    const char* want[] = {".text", ".console", ".note", ".data", ".bss"};
    Sec *text = NULL, *console = NULL, *note = NULL, *symtab = NULL, *strtab = NULL;
    uint64_t shoff, phoff;
    unsigned phnum, shnum, shstrndx;

    if (argc != 3) { fprintf(stderr, "usage: flat_elf_check k.elf k.bin\n"); return 2; }
    d = slurp(argv[1], &g_len);
    raw = slurp(argv[2], &rawn);

    CHECK(g_len >= 64 && memcmp(d, "\x7f" "ELF", 4) == 0);
    CHECK(d[4] == 2 && d[5] == 1);                       // ELFCLASS64, little endian
    CHECK(rd(d, 16, 2) == 2 && rd(d, 18, 2) == 62);      // ET_EXEC, EM_X86_64
    uint64_t entry = rd(d, 24, 8);
    phoff = rd(d, 32, 8);
    shoff = rd(d, 40, 8);
    CHECK(rd(d, 54, 2) == 56 && rd(d, 58, 2) == 64);     // e_phentsize, e_shentsize
    phnum = (unsigned)rd(d, 56, 2);
    shnum = (unsigned)rd(d, 60, 2);
    shstrndx = (unsigned)rd(d, 62, 2);
    CHECK(phnum >= 1 && shnum >= 4 && shnum <= MAXSEC && shstrndx < shnum);

    uint64_t shstr_off = rd(d, shoff + (uint64_t)shstrndx * 64 + 24, 8);
    uint64_t shstr_size = rd(d, shoff + (uint64_t)shstrndx * 64 + 32, 8);
    CHECK(shstr_off + shstr_size <= g_len);
    memset(secs, 0, sizeof secs);
    for (unsigned i = 0; i < shnum; i++) {
        size_t sh = (size_t)(shoff + (uint64_t)i * 64);
        uint64_t nm = rd(d, sh, 4);
        CHECK(nm < shstr_size);
        snprintf(secs[i].name, sizeof secs[i].name, "%s", (const char*)d + shstr_off + nm);
        secs[i].type = rd(d, sh + 4, 4);
        secs[i].addr = rd(d, sh + 16, 8);
        secs[i].off = rd(d, sh + 24, 8);
        secs[i].size = rd(d, sh + 32, 8);
        secs[i].present = 1;
    }
    for (size_t w = 0; w < sizeof want / sizeof want[0]; w++) {
        int found = 0;
        for (unsigned i = 0; i < shnum; i++) if (!strcmp(secs[i].name, want[w])) found = 1;
        if (!found) { fprintf(stderr, "flat_elf_check: section %s is missing\n", want[w]); return 1; }
    }
    for (unsigned i = 0; i < shnum; i++) {   // a later section of the same name wins
        if (!strcmp(secs[i].name, ".text")) text = &secs[i];
        if (!strcmp(secs[i].name, ".console")) console = &secs[i];
        if (!strcmp(secs[i].name, ".note")) note = &secs[i];
        if (!strcmp(secs[i].name, ".symtab")) symtab = &secs[i];
        if (!strcmp(secs[i].name, ".strtab")) strtab = &secs[i];
    }
    CHECK(console->addr == 0x12000);
    uint64_t blob = text->off;
    CHECK(blob + rawn <= g_len && memcmp(d + blob, raw, rawn) == 0);   // the image is in the file as is
    Sec* three[] = {text, console, note};
    for (int k = 0; k < 3; k++) {
        Sec* s = three[k];
        CHECK(s->type == 1 && s->size > 0);                              // SHT_PROGBITS
        CHECK(s->off >= blob && s->off - blob + s->size <= rawn);
        CHECK(memcmp(d + s->off, raw + (s->off - blob), s->size) == 0);
    }

    CHECK(symtab && strtab && strtab->off + strtab->size <= g_len);
    for (uint64_t i = 0; i < symtab->size / 24; i++) {
        size_t se = (size_t)(symtab->off + i * 24);
        uint64_t nm = rd(d, se, 4);
        if (nm == 0) continue;
        CHECK(nm < strtab->size && nsyms < MAXSYM);
        snprintf(symname[nsyms], sizeof symname[nsyms], "%s", (const char*)d + strtab->off + nm);
        symval[nsyms++] = rd(d, se + 8, 8);
    }
#define SYM(var, nm) uint64_t var = 0; do { int f_ = 0; \
        for (int i_ = 0; i_ < nsyms; i_++) if (!strcmp(symname[i_], nm)) { var = symval[i_]; f_ = 1; } \
        if (!f_) { fprintf(stderr, "flat_elf_check: symbol %s is missing\n", nm); return 1; } } while (0)
    SYM(s_start, "_start");
    SYM(s_putc, "console__putc");
    SYM(s_mark, "console__mark");
    SYM(s_banner, "banner");
    SYM(s_hidden, "console__hidden");
    (void)s_hidden;
    CHECK(entry == s_start && s_start == text->addr && text->addr == 0x10000);
    CHECK(s_putc == console->addr);
    CHECK(s_mark > console->addr);
    CHECK(s_banner == note->addr);

    CHECK(console->off + (s_mark - console->addr) + 8 <= g_len);
    CHECK(rd(d, (size_t)(console->off + (s_mark - console->addr)), 8) == 65);
    CHECK(note->off + (s_banner - note->addr) + 7 <= g_len);
    CHECK(memcmp(d + note->off + (s_banner - note->addr), "COPIHUE", 7) == 0);
    CHECK(text->addr + text->size <= console->addr);

    int found = 0;
    for (unsigned i = 0; i < phnum; i++) {
        size_t ph = (size_t)(phoff + (uint64_t)i * 56);
        if (rd(d, ph, 4) == 1 && rd(d, ph + 16, 8) == 0x12000 &&
            rd(d, ph + 32, 8) == console->size && rd(d, ph + 48, 8) == 1)
            found = 1;
    }
    CHECK(found);
    return 0;
}
