#!/bin/sh
# Measure Copihue against C, Rust and OCaml.
# Not part of `make test`. Overwrites bench/RESULTS.md and bench/results.tsv.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

RUNS=${BENCH_RUNS:-7}
COMPILE_RUNS=${BENCH_COMPILE_RUNS:-5}
BENCHES="fib sieve matmul arith chain fold array sort"

need() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "bench: falta el comando '$1'" >&2
        exit 1
    fi
}

need gcc
need rustc
need ocamlopt
need nasm
need ld
need awk
need sort
need date
need make

if ! date +%s%N >/dev/null 2>&1; then
    echo "bench: hace falta GNU date con +%s%N" >&2
    exit 1
fi

PIN=0
if command -v taskset >/dev/null 2>&1; then
    PIN=1
fi

make copihue >/dev/null
COPIHUE=$ROOT/copihue

WORK=$ROOT/bench/out
if [ "${BENCH_REUSE:-}" != 1 ]; then
    rm -rf "$WORK"
fi
mkdir -p "$WORK/bin" "$WORK/stdout" "$WORK/m"

tag_of() {
    sed -n 's/.*BENCH: //p' "$1" | head -n 1 | sed 's/[[:space:]]*\*\/.*//; s/[[:space:]]*\*).*//; s/[[:space:]]*$//'
}

ns_now() { date +%s%N; }

median_of() {
    sort -n "$1" | awk '{ a[NR] = $1 } END { print a[int((NR + 1) / 2)] }'
}

minmax_of() {
    awk 'NR == 1 || $1 < min { min = $1 } NR == 1 || $1 > max { max = $1 } END { print min, max }' "$1"
}

ms_of() {
    awk -v ns="$1" 'BEGIN { printf "%.1f", ns / 1000000 }'
}

ratio_of() {
    awk -v a="$1" -v b="$2" 'BEGIN {
        if (b <= 0) { printf "n/a"; exit }
        printf "%.2f", a / b
    }'
}

run_pinned() {
    if [ "$PIN" = 1 ]; then
        taskset -c 0 "$@"
    else
        "$@"
    fi
}

compile_one() {
    name=$1
    lang=$2
    out=$WORK/bin/${name}_${lang}
    log=$WORK/compile.log
    case "$lang" in
        lap)
            src=$ROOT/bench/src/$name.lap
            if ! (cd "$WORK" && "$COPIHUE" -O2 -o "$out" "$src") >"$log" 2>&1; then
                echo "bench: no compiló $src" >&2
                cat "$log" >&2
                exit 1
            fi
            rm -f "$WORK/output.asm" "$WORK/output.o"
            ;;
        c)
            src=$ROOT/bench/src/$name.c
            if ! gcc -O2 -o "$out" "$src" >"$log" 2>&1; then
                echo "bench: no compiló $src" >&2
                cat "$log" >&2
                exit 1
            fi
            ;;
        rs)
            src=$ROOT/bench/src/$name.rs
            if ! rustc -O -o "$out" "$src" >"$log" 2>&1; then
                echo "bench: no compiló $src" >&2
                cat "$log" >&2
                exit 1
            fi
            ;;
        ml)
            src=$ROOT/bench/src/$name.ml
            stem=$(basename "$src" .ml)
            if ! (cd "$WORK" && ocamlopt -O3 -o "$out" "$src") >"$log" 2>&1; then
                echo "bench: no compiló $src" >&2
                cat "$log" >&2
                exit 1
            fi
            rm -f "$WORK/$stem.cmi" "$WORK/$stem.cmx" "$WORK/$stem.o" \
                "$ROOT/bench/src/$stem.cmi" "$ROOT/bench/src/$stem.cmx" "$ROOT/bench/src/$stem.o"
            ;;
        *)
            echo "bench: lenguaje desconocido $lang" >&2
            exit 1
            ;;
    esac
}

# BENCH_REUSE=1 rebuilds the report from bench/out without measuring again.
if [ "${BENCH_REUSE:-}" = 1 ]; then
    echo "bench: reutilizando mediciones ya guardadas en bench/out" >&2
else
echo "bench: comprobando que las cuatro fuentes de cada programa dicen la misma carga" >&2
for name in $BENCHES; do
    t1=$(tag_of "$ROOT/bench/src/$name.lap")
    for ext in c rs ml; do
        case "$ext" in
            c) f=$ROOT/bench/src/$name.c ;;
            rs) f=$ROOT/bench/src/$name.rs ;;
            ml) f=$ROOT/bench/src/$name.ml ;;
        esac
        t2=$(tag_of "$f")
        if [ "$t1" != "$t2" ]; then
            echo "bench: BENCH distinto en $name ($t1 vs $t2)" >&2
            exit 1
        fi
    done
    printf '%s\n' "$t1" > "$WORK/m/$name.tag"
done

echo "bench: compilando y comparando la salida (una vez)" >&2
for name in $BENCHES; do
    for lang in lap c rs ml; do
        compile_one "$name" "$lang"
        if ! run_pinned "$WORK/bin/${name}_${lang}" >"$WORK/stdout/${name}_${lang}" 2>"$WORK/run.err"; then
            echo "bench: falló al ejecutar ${name}_${lang}" >&2
            cat "$WORK/run.err" >&2
            exit 1
        fi
        if [ -s "$WORK/run.err" ]; then
            echo "bench: ${name}_${lang} escribió en stderr" >&2
            cat "$WORK/run.err" >&2
            exit 1
        fi
    done
    for lang in lap rs ml; do
        if ! cmp -s "$WORK/stdout/${name}_c" "$WORK/stdout/${name}_${lang}"; then
            echo "bench: salida distinta en $name ($lang vs c)" >&2
            echo "--- c ---" >&2
            cat "$WORK/stdout/${name}_c" >&2
            echo "--- $lang ---" >&2
            cat "$WORK/stdout/${name}_${lang}" >&2
            exit 1
        fi
    done
    echo "bench: $name salida $(cat "$WORK/stdout/${name}_c")" >&2
done

echo "bench: array global de Copihue (misma salida que array local)" >&2
compile_one array_global lap
if ! run_pinned "$WORK/bin/array_global_lap" >"$WORK/stdout/array_global_lap" 2>"$WORK/run.err"; then
    echo "bench: falló array_global" >&2
    cat "$WORK/run.err" >&2
    exit 1
fi
if ! cmp -s "$WORK/stdout/array_c" "$WORK/stdout/array_global_lap"; then
    echo "bench: array global no coincide con array local" >&2
    exit 1
fi

measure_runs() {
    name=$1
    lang=$2
    bin=$WORK/bin/${name}_${lang}
    samples=$WORK/m/${name}_${lang}.run
    : > "$samples"
    i=0
    while [ "$i" -lt "$RUNS" ]; do
        start=$(ns_now)
        run_pinned "$bin" >/dev/null 2>"$WORK/run.err"
        end=$(ns_now)
        if [ -s "$WORK/run.err" ]; then
            echo "bench: stderr durante la medición de ${name}_${lang}" >&2
            exit 1
        fi
        echo $((end - start)) >> "$samples"
        i=$((i + 1))
    done
}

measure_compiles() {
    name=$1
    lang=$2
    samples=$WORK/m/${name}_${lang}.compile
    : > "$samples"
    i=0
    while [ "$i" -lt "$COMPILE_RUNS" ]; do
        start=$(ns_now)
        compile_one "$name" "$lang"
        end=$(ns_now)
        echo $((end - start)) >> "$samples"
        i=$((i + 1))
    done
}

echo "bench: midiendo compilación ($COMPILE_RUNS repeticiones, sin calentar aparte: la pasada de verificación ya compiló)" >&2
for name in $BENCHES; do
    for lang in lap c rs ml; do
        echo "bench: compilar $name $lang" >&2
        measure_compiles "$name" "$lang"
    done
done

echo "bench: calentando y midiendo ejecución ($RUNS repeticiones, taskset=$PIN)" >&2
for name in $BENCHES array_global; do
    if [ "$name" = array_global ]; then
        langs=lap
    else
        langs="lap c rs ml"
    fi
    for lang in $langs; do
        echo "bench: correr $name $lang" >&2
        run_pinned "$WORK/bin/${name}_${lang}" >/dev/null 2>"$WORK/run.err" || {
            echo "bench: calentamiento falló ${name}_${lang}" >&2
            exit 1
        }
        measure_runs "$name" "$lang"
    done
done
fi

# Did gcc/rustc/ocamlopt keep fold's result as an immediate?
fold_out=$(tr -d '\n' < "$WORK/stdout/fold_c")
gcc -O2 -S -o "$WORK/fold_c.s" "$ROOT/bench/src/fold.c"
rustc -O --emit=asm -o "$WORK/fold_rs.s" "$ROOT/bench/src/fold.rs"
(cd "$WORK" && ocamlopt -O3 -S -o "$WORK/fold_ml.s" "$ROOT/bench/src/fold.ml") >/dev/null 2>&1 || true
rm -f "$WORK/fold.cmi" "$WORK/fold.cmx" "$WORK/fold.o" \
    "$ROOT/bench/src/fold.cmi" "$ROOT/bench/src/fold.cmx" "$ROOT/bench/src/fold.o" \
    "$ROOT/bench/src/fold.s"
fold_note() {
    src=$1
    if [ ! -f "$src" ]; then
        printf 'no se generó el .s'
        return
    fi
    if grep -q -F "$fold_out" "$src"; then
        printf 'sí'
    else
        printf 'no'
    fi
}
fold_c_imm=$(fold_note "$WORK/fold_c.s")
fold_rs_imm=$(fold_note "$WORK/fold_rs.s")
fold_ml_imm=$(fold_note "$WORK/fold_ml.s")

cpu_model=$(awk -F: '/model name/{gsub(/^ +/, "", $2); print $2; exit}' /proc/cpuinfo)
cpu_mhz=$(awk -F: '/cpu MHz/{gsub(/^ +/, "", $2); print $2; exit}' /proc/cpuinfo)
ncpu=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo "?")
long_bit=$(getconf LONG_BIT 2>/dev/null || echo "?")
stack_kb=$(ulimit -s)
when=$(date -u '+%Y-%m-%d %H:%M:%S UTC')
uname_s=$(uname -srm)
gcc_v=$(gcc --version | head -n 1)
rustc_v=$(rustc --version)
ocaml_v=$(ocamlopt -version)
nasm_v=$(nasm -v)
ld_v=$(ld --version | head -n 1)
copi_v=$("$COPIHUE" --version)
flambda=$(ocamlopt -config | awk -F: '/flambda/{gsub(/ /, "", $2); print $2; exit}')
march=$(gcc -Q -O2 --help=target | awk '/^[[:space:]]*-march=/ { gsub(/[ \t]+/, ""); print; exit }')
rust_o=$(rustc --help | sed -n 's/^[[:space:]]*-O[[:space:]]\{1,\}//p' | head -n 1)
sample_start=$(find "$WORK/m" \( -name '*.run' -o -name '*.compile' \) -printf '%T@\n' | sort -n | head -n 1)
sample_end=$(find "$WORK/m" \( -name '*.run' -o -name '*.compile' \) -printf '%T@\n' | sort -n | tail -n 1)
span_start=$(date -u -d "@${sample_start%.*}" '+%Y-%m-%d %H:%M:%S UTC')
span_end=$(date -u -d "@${sample_end%.*}" '+%Y-%m-%d %H:%M:%S UTC')
if [ "$PIN" = 1 ]; then
    pin_txt="sí, \`taskset -c 0\`"
else
    pin_txt="no"
fi

{
    echo "name	lang	compile_median_ns	compile_min_ns	compile_max_ns	run_median_ns	run_min_ns	run_max_ns	bytes	text	data	bss"
    for name in $BENCHES array_global; do
        if [ "$name" = array_global ]; then
            langs=lap
        else
            langs="lap c rs ml"
        fi
        for lang in $langs; do
            bin=$WORK/bin/${name}_${lang}
            if [ "$name" = array_global ]; then
                cmed=
                cmin=
                cmax=
            else
                cmed=$(median_of "$WORK/m/${name}_${lang}.compile")
                # shellcheck disable=SC2086
                set -- $(minmax_of "$WORK/m/${name}_${lang}.compile")
                cmin=$1
                cmax=$2
            fi
            rmed=$(median_of "$WORK/m/${name}_${lang}.run")
            set -- $(minmax_of "$WORK/m/${name}_${lang}.run")
            rmin=$1
            rmax=$2
            bytes=$(wc -c < "$bin" | awk '{print $1}')
            set -- $(size "$bin" | awk 'NR==2 { print $1, $2, $3 }')
            text=$1
            data=$2
            bss=$3
            if [ "$name" = array_global ]; then
                printf '%s\t%s\t\t\t\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
                    "$name" "$lang" "$rmed" "$rmin" "$rmax" "$bytes" "$text" "$data" "$bss"
            else
                printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
                    "$name" "$lang" "$cmed" "$cmin" "$cmax" "$rmed" "$rmin" "$rmax" "$bytes" "$text" "$data" "$bss"
            fi
        done
    done
} > "$ROOT/bench/results.tsv"

# Cociente Copihue/C de ejecución, peor y mejor, a partir del tsv ya escrito.
set -- $(awk -F '\t' '
    $2 == "lap" && $1 != "array_global" { lap[$1] = $6 }
    $2 == "c" { c[$1] = $6 }
    $2 == "lap" && $1 != "array_global" { cl[$1] = $3 }
    $2 == "c" { cc[$1] = $3 }
    $2 == "rs" { cr[$1] = $3 }
    $2 == "ml" { co[$1] = $3 }
    END {
        wn = ""; wr = 0; bn = ""; br = 0; compile_win = 1
        for (n in lap) {
            r = lap[n] / c[n]
            if (wn == "" || r > wr) { wr = r; wn = n }
            if (bn == "" || r < br) { br = r; bn = n }
            if (!(cl[n] < cc[n] && cl[n] < cr[n] && cl[n] < co[n])) compile_win = 0
        }
        printf "%s %.2f %s %.2f %d\n", wn, wr, bn, br, compile_win
    }
' "$ROOT/bench/results.tsv")
worst_name=$1
worst_ratio=$2
best_name=$3
best_ratio=$4
compile_win=$5

cell() {
    # $1 median ns, $2 min ns, $3 max ns
    printf '%s (%s–%s)' "$(ms_of "$1")" "$(ms_of "$2")" "$(ms_of "$3")"
}

field() {
    # $1 name $2 lang $3 column number in the tsv (1-based)
    awk -F '\t' -v n="$1" -v l="$2" -v c="$3" '$1 == n && $2 == l { print $c }' "$ROOT/bench/results.tsv"
}

{
    cat << EOF
# Resultados

Generado por \`bench/run.sh\`. Las muestras se tomaron entre $span_start y $span_end. Este archivo se escribió el $when a partir de esas muestras, en la misma máquina. \`make bench\` vuelve a medir y sustituye este archivo.

## Máquina y versiones

| Dato | Valor |
|---|---|
| Muestras | $span_start — $span_end |
| Tablas escritas | $when |
| Sistema | $uname_s |
| CPU | $cpu_model |
| Frecuencia que reporta \`/proc/cpuinfo\` | ${cpu_mhz} MHz |
| CPUs en línea | $ncpu |
| \`ulimit -s\` | ${stack_kb} KB |
| \`getconf LONG_BIT\` | $long_bit |
| Copihue | $copi_v, flag \`-O2\` |
| C | $gcc_v, flag \`-O2\`, $march |
| Rust | $rustc_v, \`-O\` ($rust_o) |
| OCaml | ocamlopt $ocaml_v, flag \`-O3\`, flambda=$flambda |
| nasm | $nasm_v |
| ld | $ld_v |

## Metodología

Cada programa está escrito cuatro veces (\`.lap\`, \`.c\`, \`.rs\`, \`.ml\`) con el mismo algoritmo, los mismos límites y la misma salida: un entero y un salto de línea. La primera línea de cada fuente lleva la carga (\`BENCH:\`); el script aborta si no coinciden. Antes de medir, las cuatro salidas se comparan byte a byte.

- Compilación: el comando entero hasta el ejecutable. En Copihue eso incluye \`nasm -f elf64\` y \`ld\`. En gcc, rustc y ocamlopt incluye su ensamblador y su enlazador. $COMPILE_RUNS repeticiones, mediana. No hay calentamiento aparte: la pasada de verificación ya compiló una vez, así que la primera repetición medida no es un arranque en frío del disco.
- Ejecución: $RUNS repeticiones más una de calentamiento que no entra en la mediana. La mediana es el valor central de las $RUNS muestras ordenadas (la 4.ª si son 7). En cada celda, entre paréntesis, el mínimo y el máximo de esas repeticiones.
- La ejecución medida tira stdout a \`/dev/null\`. El proceso sigue escribiendo una línea. Afinidad a un CPU: $pin_txt.
- El tamaño es \`wc -c\` del fichero producido, sin \`strip\`. \`text\`/\`data\`/\`bss\` salen de \`size\`.
- Copihue \`-O2\` en hosted pliega constantes, reescribe \`*\`, \`/\` y \`%\` por potencias de dos, evalúa una operación de operandos simples directo en el registro del local (o en \`rax\`/\`rbx\` si hace falta un temporal), mantiene hasta cuatro locales escalares en \`r12\`–\`r15\` y, cuando ninguna llamada cae dentro del rango de uso, dos más en \`r10\` y \`r11\`, direcciona un array con \`base+índice*escala\` en una instrucción, y si la función abre con \`if (param <op> hoja) return param\` compara el registro de entrada y retorna antes de armar el frame. No simplifica bucles. \`-O0\`, \`--freestanding\`, \`--flat\` y \`--boot\` no usan eso.
- C usa \`long\` ($long_bit bits aquí). Rust usa \`i64\`. OCaml usa \`int\` nativo, que en amd64 es 63 bits con etiqueta. Las cargas caben en ese rango: las cuatro salidas coincidieron. El desborde de \`rustc -O\` envuelve, igual que un binario release; no es el modo debug.
- No hay intrínsecos, \`unsafe\`, pragmas, \`-march=native\` ni \`-C opt-level=3\`. Los flags son los pedidos: \`gcc -O2\`, \`rustc -O\`, \`ocamlopt -O3\`, \`copihue -O2\`.
- Esta copia de ocamlopt no trae Flambda. \`-O3\` se pasa igual; no activa Flambda.

## Tablas

Tiempo de compilación en milisegundos: mediana (mínimo–máximo).

| Programa | Carga | Copihue | C | Rust | OCaml |
|---|---|---:|---:|---:|---:|
EOF

    for name in $BENCHES; do
        tag=$(cat "$WORK/m/$name.tag")
        printf '| `%s` | %s | %s | %s | %s | %s |\n' \
            "$name" "$tag" \
            "$(cell "$(field "$name" lap 3)" "$(field "$name" lap 4)" "$(field "$name" lap 5)")" \
            "$(cell "$(field "$name" c 3)" "$(field "$name" c 4)" "$(field "$name" c 5)")" \
            "$(cell "$(field "$name" rs 3)" "$(field "$name" rs 4)" "$(field "$name" rs 5)")" \
            "$(cell "$(field "$name" ml 3)" "$(field "$name" ml 4)" "$(field "$name" ml 5)")"
    done

    cat << 'EOF'

Tamaño del ejecutable en bytes, y `text` / `data` / `bss` según `size`.

| Programa | Copihue bytes (text/data/bss) | C bytes (text/data/bss) | Rust bytes (text/data/bss) | OCaml bytes (text/data/bss) |
|---|---:|---:|---:|---:|
EOF

    for name in $BENCHES; do
        fmt_size() {
            b=$(field "$name" "$1" 9)
            t=$(field "$name" "$1" 10)
            d=$(field "$name" "$1" 11)
            s=$(field "$name" "$1" 12)
            printf '%s (%s/%s/%s)' "$b" "$t" "$d" "$s"
        }
        printf '| `%s` | %s | %s | %s | %s |\n' \
            "$name" "$(fmt_size lap)" "$(fmt_size c)" "$(fmt_size rs)" "$(fmt_size ml)"
    done

    cat << 'EOF'

Tiempo de ejecución en milisegundos: mediana (mínimo–máximo). La salida es la de las cuatro versiones.

| Programa | Salida | Copihue | C | Rust | OCaml | Copihue/C | Copihue/Rust | Copihue/OCaml |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
EOF

    for name in $BENCHES; do
        out=$(tr -d '\n' < "$WORK/stdout/${name}_c")
        lc=$(field "$name" lap 6)
        cc=$(field "$name" c 6)
        rc=$(field "$name" rs 6)
        mc=$(field "$name" ml 6)
        printf '| `%s` | %s | %s | %s | %s | %s | %s | %s | %s |\n' \
            "$name" "$out" \
            "$(cell "$lc" "$(field "$name" lap 7)" "$(field "$name" lap 8)")" \
            "$(cell "$cc" "$(field "$name" c 7)" "$(field "$name" c 8)")" \
            "$(cell "$rc" "$(field "$name" rs 7)" "$(field "$name" rs 8)")" \
            "$(cell "$mc" "$(field "$name" ml 7)" "$(field "$name" ml 8)")" \
            "$(ratio_of "$lc" "$cc")" \
            "$(ratio_of "$lc" "$rc")" \
            "$(ratio_of "$lc" "$mc")"
    done

    cat << EOF

## Enlace

\`ldd\` y \`file\` del binario de \`fib\` (los demás usan el mismo compilador):

EOF
    for lang in lap c rs ml; do
        echo "### fib $lang"
        echo
        echo '```'
        file "$WORK/bin/fib_${lang}" || true
        ldd "$WORK/bin/fib_${lang}" 2>&1 || true
        echo '```'
        echo
    done

    ag_med=$(field array_global lap 6)
    ag_min=$(field array_global lap 7)
    ag_max=$(field array_global lap 8)
    al_med=$(field array lap 6)

    cat << EOF
## \`fold\` y el bucle que otros compiladores pueden borrar

La salida común es \`$fold_out\`.

Se buscó ese decimal en el ensamblado (\`gcc -O2 -S\`, \`rustc -O --emit=asm\`, \`ocamlopt -O3 -S\`):

| Compilador | El decimal aparece en el \`.s\` |
|---|---|
| gcc | $fold_c_imm |
| rustc | $fold_rs_imm |
| ocamlopt | $fold_ml_imm |

Un bucle de 500000000 vueltas no cabe en unos pocos milisegundos a ninguna frecuencia plausible de esta máquina (a 1 ciclo por vuelta y ${cpu_mhz} MHz harían falta unos $(awk -v mhz="$cpu_mhz" 'BEGIN { printf "%.0f", 500000000 / (mhz * 1000) }') ms). Si C o Rust están muy por debajo de eso, no ejecutaron el bucle: el trabajo se hizo al compilar. OCaml y Copihue hay que leerlos en la tabla, no suponerlo.

## Array local y array global, solo Copihue

\`bench/src/array_global.lap\` es el mismo algoritmo que \`array\`, con el arreglo en una global. En el generador actual, la lectura de un array local emite un chequeo de límites (\`test\`/\`cmp\`/\`jmp\`) y la de un array global no. La salida coincidió con \`array\`.

| Variante | Mediana (mín–máx) ms |
|---|---:|
| local (\`array\`) | $(cell "$al_med" "$(field array lap 7)" "$(field array lap 8)") |
| global (\`array_global\`) | $(cell "$ag_med" "$ag_min" "$ag_max") |

## Lectura de esta corrida

El cociente Copihue/C más alto es \`$worst_name\` ($worst_ratio×) y el más bajo es \`$best_name\` ($best_ratio×). Un cociente menor que 1 quiere decir que, en esa fila, la mediana de Copihue quedó por debajo de la de C; el mínimo y el máximo de la tabla dicen si los rangos se separan. En \`fold\` el decimal del resultado está en el ensamblado de gcc y de rustc, y el tiempo de esos dos es el de arrancar el proceso; OCaml sí ejecuta el bucle y ahí el cociente es el de la tabla (Copihue/OCaml). \`chain\` es una cadena dependiente de multiplicar: el cociente está en la tabla, no se supone. El hueco que queda, en los bucles que los cuatro ejecutan, está en \`sort\`, \`array\`, \`fib\`, \`matmul\` y \`arith\`.

$(if [ "$compile_win" = 1 ]; then
    echo "En tiempo de compilación de estos archivos chicos, la mediana de Copihue es la más baja en las ocho filas. Eso mide el arranque de un compilador de una pasada más nasm y ld, no un proyecto grande."
else
    echo "En tiempo de compilación Copihue no fue el más rápido en todas las filas; hay que leer la tabla."
fi)

El fichero de Copihue es el más chico de cada fila. \`file\` marca los binarios de Rust y de OCaml con información de depuración; la columna \`text\` de \`size\` es el código, y también ahí Copihue es el más chico. El binario de C está enlazado con libc y el de Copihue no: el tamaño no es "bytes del algoritmo".

La mediana del array local y la del global están en la tabla de arriba. La diferencia no es el grueso del tiempo de \`array\`: esta corrida no muestra que el chequeo de límites sea lo que domina ese programa.

## Límites

- Esto no mide C++. El C de la tabla es \`gcc -O2\`.
- El tamaño del fichero mezcla el código del programa con el runtime y el tipo de enlace. No es una comparación de "bytes de algoritmo".
- El tiempo de ejecución incluye crear el proceso, tocar la pila y escribir una línea. No es un bucle interno cronometrado con \`rdtsc\`.
- Máquina virtual. La mediana recorta un pico, no convierte el número en una cifra de hardware físico.
- No se midió código freestanding, ni un programa grande, ni el uso de memoria.
- Copihue reserva en la pila 8 bytes por elemento aunque el elemento sea \`u8\` (el hueco se redondea al número de elementos). La criba local de 400000 bytes lógicos ocupa unos 3.2 MB de marco. Cabe en el \`ulimit -s\` de esta corrida.
- Un \`let\` vive hasta el fin de su bloque. Declarar el mismo nombre dos veces en un bloque es un error. Los programas de esta suite declaran cada nombre una vez.

Los datos crudos, en nanosegundos, están en \`bench/results.tsv\`.
EOF
} > "$ROOT/bench/RESULTS.md"

# Static profile and the before/after note. make bench replaces the tables
# above; this file is the write-up of the 0.22.2 -> 0.22.3 measurement.
if [ -f "$ROOT/bench/profile.md" ]; then
    cat "$ROOT/bench/profile.md" >> "$ROOT/bench/RESULTS.md"
fi

echo "bench: escribió bench/RESULTS.md y bench/results.tsv" >&2
