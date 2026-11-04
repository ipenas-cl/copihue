// Prints what the tests read from a nasm -f elf64 object: sections, symbols,
// relocations. A debugging aid for tests/x64 (no objdump/readelf needed).
//   dump_obj FILE.o
#include "nobj.h"

int main(int argc, char** argv) {
    if (argc != 2) { fprintf(stderr, "usage: dump_obj FILE.o\n"); return 2; }
    NObj o;
    if (!nobj_load(argv[1], &o)) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    static const char* const N[4] = {".text", ".data", ".bss", ".rodata"};
    for (int i = 0; i < 4; i++) {
        const NSec* s = &o.s[i];
        if (!s->present) continue;
        printf("%s: size %zu align %llu flags 0x%llx\n", N[i], s->n, (unsigned long long)s->align, (unsigned long long)s->flags);
        if (s->b) {
            printf("  bytes:");
            for (size_t k = 0; k < s->n && k < 96; k++) printf(" %02x", s->b[k]);
            printf(s->n > 96 ? " ...\n" : "\n");
        }
        for (size_t k = 0; k < s->nrel; k++)
            printf("  rela +%d type %d %s%s %+lld\n", s->rel[k].off, s->rel[k].type, s->rel[k].secsym ? "section " : "symbol ", s->rel[k].sym, (long long)s->rel[k].addend);
    }
    for (size_t i = 0; i < o.nsym; i++)
        printf("sym %-12s bind %d type %d shndx %d value %llu\n", o.sym[i].name, o.sym[i].bind, o.sym[i].type, o.sym[i].shndx, (unsigned long long)o.sym[i].value);
    return 0;
}
