# copihue: the Copihue compiler

`copihue.c` is the whole compiler, in a single C99 file: lexer, recursive-descent parser, type and symbol tables, optimizer, and x86-64 NASM code generator.

```bash
make                       # from the repository root, builds ./copihue
./copihue --help
```

Naming (`LANG_NAME`, `COMPILER_BIN`, `SRC_EXT`, `LANG_VERSION`) is defined once at the top of `copihue.c`.

`archive/` holds historical compiler versions from the Chronos era and is not built.
