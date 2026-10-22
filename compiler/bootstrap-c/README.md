# copihuec: the Copihue compiler

`copihuec.c` is the whole compiler, in a single C99 file: lexer, recursive-descent parser, type and symbol tables, optimizer, and x86-64 NASM code generator.

```bash
make                       # from the repository root, builds ./copihuec
./copihuec --help
```

Naming (`LANG_NAME`, `COMPILER_BIN`, `SRC_EXT`, `LANG_VERSION`) is defined once at the top of `copihuec.c`.

`archive/` holds historical compiler versions from the Chronos era and is not built.
