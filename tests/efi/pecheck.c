// Structural check of a PE32+ UEFI application (tests/efi/run.sh). It reads the file and checks
// what an x86-64 firmware loader relies on, using only the file: the MZ and PE signatures, Machine,
// the optional header (magic 0x20B, subsystem 10, alignments, sizes), the section table (page
// aligned RVAs in ascending order, raw data inside the file, no section both writable and
// executable), the entry point inside an executable section, and the base relocation directory
// (inside a section, blocks that add up, no entry that points outside the image).
//   pecheck FILE      exit 0 and "ok ..." on stdout, or exit 1 and the first problem on stderr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static unsigned char* buf;
static size_t len;
static const char* path;

static void fail(const char* fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
static void fail(const char* fmt, ...) {
    va_list ap;
    fprintf(stderr, "pecheck: %s: ", path);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static unsigned rd16(size_t off) {
    if (off + 2 > len) fail("offset 0x%zx is past the end of the file (%zu bytes)", off, len);
    return (unsigned)buf[off] | (unsigned)buf[off + 1] << 8;
}
static unsigned long long rd32(size_t off) {
    return rd16(off) | (unsigned long long)rd16(off + 2) << 16;
}
static unsigned long long rd64(size_t off) { return rd32(off) | rd32(off + 4) << 32; }
static int pow2(unsigned long long v) { return v && !(v & (v - 1)); }

typedef struct {
    char name[9];
    unsigned long long vsize, rva, rawsize, rawptr;
    unsigned flags;
} Sec;

int main(int argc, char** argv) {
    FILE* f;
    if (argc != 2) { fprintf(stderr, "usage: pecheck FILE\n"); return 2; }
    path = argv[1];
    f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "pecheck: cannot open %s\n", path); return 2; }
    fseek(f, 0, SEEK_END);
    len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(len ? len : 1);
    if (!buf || fread(buf, 1, len, f) != len) { fprintf(stderr, "pecheck: cannot read %s\n", path); return 2; }
    fclose(f);

    if (len < 0x40 || buf[0] != 'M' || buf[1] != 'Z') fail("no MZ signature");
    unsigned long long pe = rd32(0x3C);
    if (pe < 0x40 || pe % 4 || pe + 24 > len) fail("e_lfanew 0x%llx is not a valid offset of the PE header", pe);
    if (memcmp(buf + pe, "PE\0\0", 4)) fail("no PE signature at 0x%llx", pe);

    size_t co = (size_t)pe + 4;                       // COFF header
    unsigned machine = rd16(co), nsec = rd16(co + 2), optsize = rd16(co + 16), chars = rd16(co + 18);
    if (machine != 0x8664) fail("Machine is 0x%x, not 0x8664 (x86-64)", machine);
    if (nsec == 0 || nsec > 96) fail("NumberOfSections is %u", nsec);
    if (rd32(co + 8) != 0 || rd32(co + 12) != 0) fail("a symbol table is present; an application has none");
    if (!(chars & 0x0002)) fail("Characteristics 0x%x: not EXECUTABLE_IMAGE", chars);
    if (chars & 0x0001) fail("Characteristics 0x%x: RELOCS_STRIPPED, the firmware could not load it at any address", chars);
    if (chars & 0x2000) fail("Characteristics 0x%x: a DLL", chars);

    size_t oo = co + 20;                              // optional header
    if (optsize < 112 + 16 * 8) fail("SizeOfOptionalHeader is %u; PE32+ with 16 directories is 240", optsize);
    unsigned magic = rd16(oo);
    if (magic != 0x20B) fail("optional header magic is 0x%x, not 0x20B (PE32+)", magic);
    unsigned long long entry = rd32(oo + 16), base = rd64(oo + 24);
    unsigned long long salign = rd32(oo + 32), falign = rd32(oo + 36);
    unsigned long long soi = rd32(oo + 56), soh = rd32(oo + 60);
    unsigned subsystem = rd16(oo + 68), dllch = rd16(oo + 70);
    unsigned long long ndir = rd32(oo + 108);
    if (subsystem != 10) fail("Subsystem is %u, not 10 (EFI application)", subsystem);
    if (!pow2(salign) || !pow2(falign)) fail("alignments 0x%llx and 0x%llx are not powers of two", salign, falign);
    if (falign < 512 || falign > 65536) fail("FileAlignment 0x%llx is not between 512 and 64K", falign);
    if (salign < falign) fail("SectionAlignment 0x%llx is smaller than FileAlignment 0x%llx", salign, falign);
    if (salign < 0x1000) fail("SectionAlignment 0x%llx is under the 4096-byte page of the firmware", salign);
    if (base % 65536) fail("ImageBase 0x%llx is not a multiple of 64K", base);
    if (soh % falign) fail("SizeOfHeaders 0x%llx is not a multiple of FileAlignment 0x%llx", soh, falign);
    if (soi % salign) fail("SizeOfImage 0x%llx is not a multiple of SectionAlignment 0x%llx", soi, salign);
    if (ndir < 6 || ndir > 16) fail("NumberOfRvaAndSizes is %llu", ndir);
    if (dllch & 0x0100) { /* NX_COMPAT: checked below that no section is writable and executable */ }

    size_t so = oo + optsize;                         // section table
    if (so + 40 * (size_t)nsec > soh) fail("the section table ends at 0x%zx, past SizeOfHeaders 0x%llx", so + 40 * (size_t)nsec, soh);
    if (soh > len) fail("SizeOfHeaders 0x%llx is past the end of the file", soh);
    Sec* s = calloc(nsec, sizeof *s);
    unsigned long long prev_end = (soh + salign - 1) / salign * salign, raw_end = soh;
    int entry_ok = 0;
    for (unsigned i = 0; i < nsec; i++) {
        size_t e = so + 40 * (size_t)i;
        memcpy(s[i].name, buf + e, 8);
        s[i].vsize = rd32(e + 8); s[i].rva = rd32(e + 12);
        s[i].rawsize = rd32(e + 16); s[i].rawptr = rd32(e + 20);
        s[i].flags = (unsigned)rd32(e + 36);
        if (s[i].rva % salign) fail("section %s: RVA 0x%llx is not a multiple of SectionAlignment", s[i].name, s[i].rva);
        if (s[i].rva < prev_end) fail("section %s: RVA 0x%llx overlaps the headers or the section before it (free from 0x%llx)", s[i].name, s[i].rva, prev_end);
        unsigned long long vspan = s[i].vsize > s[i].rawsize ? s[i].vsize : s[i].rawsize;
        if (vspan == 0) fail("section %s is empty", s[i].name);
        prev_end = (s[i].rva + vspan + salign - 1) / salign * salign;
        if (s[i].rawsize) {
            if (s[i].rawsize % falign) fail("section %s: SizeOfRawData 0x%llx is not a multiple of FileAlignment", s[i].name, s[i].rawsize);
            if (s[i].rawptr % falign) fail("section %s: PointerToRawData 0x%llx is not a multiple of FileAlignment", s[i].name, s[i].rawptr);
            if (s[i].rawptr < raw_end) fail("section %s: raw data at 0x%llx overlaps the headers or the section before it", s[i].name, s[i].rawptr);
            if (s[i].rawptr + s[i].rawsize > len) fail("section %s: raw data ends at 0x%llx, past the end of the file (%zu bytes)", s[i].name, s[i].rawptr + s[i].rawsize, len);
            raw_end = s[i].rawptr + s[i].rawsize;
        } else if (s[i].rawptr) fail("section %s: PointerToRawData without SizeOfRawData", s[i].name);
        if ((s[i].flags & 0x20000000) && (s[i].flags & 0x80000000)) fail("section %s is both executable and writable", s[i].name);
        if (entry >= s[i].rva && entry < s[i].rva + s[i].vsize) {
            if (!(s[i].flags & 0x20000000) || !(s[i].flags & 0x20)) fail("the entry point 0x%llx is in section %s, which is not code", entry, s[i].name);
            entry_ok = 1;
        }
    }
    if (!entry_ok) fail("AddressOfEntryPoint 0x%llx is not inside any section", entry);
    if (soi != prev_end) fail("SizeOfImage is 0x%llx; the last section ends at 0x%llx", soi, prev_end);
    if (raw_end != len) fail("the file is %zu bytes; the last raw data ends at 0x%llx", len, raw_end);

    // base relocations: directory 5
    unsigned long long rr = rd32(oo + 112 + 5 * 8), rs = rd32(oo + 112 + 5 * 8 + 4);
    if (rs == 0) fail("no base relocation directory (.reloc)");
    unsigned long long roff = 0;
    int rin = -1;
    for (unsigned i = 0; i < nsec; i++)
        if (rr >= s[i].rva && rr + rs <= s[i].rva + s[i].rawsize) { rin = (int)i; roff = s[i].rawptr + (rr - s[i].rva); }
    if (rin < 0) fail("the relocation directory (RVA 0x%llx, 0x%llx bytes) is not inside the raw data of a section", rr, rs);
    unsigned long long used = 0, nreloc = 0;
    while (used < rs) {
        if (rs - used < 8) fail("relocation directory: %llu bytes left, a block header is 8", rs - used);
        unsigned long long page = rd32((size_t)(roff + used)), bs = rd32((size_t)(roff + used + 4));
        if (bs < 8 || bs % 4 || used + bs > rs) fail("relocation block at +0x%llx has SizeOfBlock %llu", used, bs);
        if (page % 0x1000 || page >= soi) fail("relocation block at +0x%llx: page RVA 0x%llx is not a page of the image", used, page);
        for (unsigned long long k = 8; k < bs; k += 2) {
            unsigned ent = rd16((size_t)(roff + used + k)), type = ent >> 12;
            if (type == 0) continue;
            if (type != 10) fail("relocation block at +0x%llx: type %u (only ABSOLUTE and DIR64 are valid in PE32+)", used, type);
            if (page + (ent & 0xFFF) + 8 > soi) fail("relocation at page 0x%llx offset 0x%x is outside the image", page, ent & 0xFFF);
            nreloc++;
        }
        used += bs;
    }
    printf("ok entry=0x%llx sections=%u image=0x%llx relocs=%llu\n", entry, nsec, soi, nreloc);
    return 0;
}
