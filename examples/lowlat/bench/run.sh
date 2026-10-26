#!/usr/bin/env bash
# Mide pool, libro y motor. Una calentamiento y siete corridas, taskset -c 0.
# La mediana es la 4.ª de las siete. Escribe examples/lowlat/RESULTS.md.
set -eu
cd "$(dirname "$0")/../../.."
ROOT=$PWD
if ! date +%s%N >/dev/null 2>&1; then
    echo "bench: hace falta GNU date con +%s%N" >&2
    exit 1
fi
make -s copihue
CC=$ROOT/copihue
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

tag_of() {
    sed -n 's/.*BENCH: //p' "$1" | head -n 1 | sed 's/[[:space:]]*\*\/.*//; s/[[:space:]]*$//'
}

ns_now() { date +%s%N; }

median_of() { # file of one ns per line
    sort -n "$1" | awk 'NR==4 { print; exit }'
}
min_of() { sort -n "$1" | head -n 1; }
max_of() { sort -n "$1" | tail -n 1; }
ms() { awk -v ns="$1" 'BEGIN { printf "%.1f", ns / 1000000 }'; }

compile_one() { # $1 name
    local name=$1
    local t
    t=$(tag_of "$ROOT/examples/lowlat/bench/$name.lap")
    for ext in c rs; do
        local u
        u=$(tag_of "$ROOT/examples/lowlat/bench/$name.$ext")
        if [ "$t" != "$u" ]; then
            echo "bench: carga distinta en $name.$ext" >&2
            exit 1
        fi
    done
    "$CC" -O2 -o "$WORK/$name.lap" "$ROOT/examples/lowlat/bench/$name.lap"
    gcc -O2 -Wall -Werror -o "$WORK/$name.c" "$ROOT/examples/lowlat/bench/$name.c"
    rustc -O -o "$WORK/$name.rs" "$ROOT/examples/lowlat/bench/$name.rs"
    "$WORK/$name.lap" > "$WORK/$name.lap.out"
    "$WORK/$name.c" > "$WORK/$name.c.out"
    "$WORK/$name.rs" > "$WORK/$name.rs.out"
    cmp -s "$WORK/$name.lap.out" "$WORK/$name.c.out"
    cmp -s "$WORK/$name.lap.out" "$WORK/$name.rs.out"
    echo "bench: $name salida $(cat "$WORK/$name.lap.out")"
}

measure() { # $1 bin $2 outfile
    local bin=$1 out=$2 i a b
    taskset -c 0 "$bin" >/dev/null
    : > "$out"
    for i in 1 2 3 4 5 6 7; do
        a=$(ns_now)
        taskset -c 0 "$bin" >/dev/null
        b=$(ns_now)
        echo $((b - a)) >> "$out"
    done
}

relate() { # $1 copihue ns file $2 other
    local a b amin amax bmin bmax
    amin=$(min_of "$1"); amax=$(max_of "$1")
    bmin=$(min_of "$2"); bmax=$(max_of "$2")
    if [ "$amax" -lt "$bmin" ]; then echo "por debajo"
    elif [ "$amin" -gt "$bmax" ]; then echo "por encima"
    else echo "se solapan"
    fi
}

START=$(date -u +%Y-%m-%dT%H:%M:%SZ)
for name in pool book match; do
    compile_one "$name"
    measure "$WORK/$name.lap" "$WORK/$name.lap.ns"
    measure "$WORK/$name.c" "$WORK/$name.c.ns"
    measure "$WORK/$name.rs" "$WORK/$name.rs.ns"
done
END=$(date -u +%Y-%m-%dT%H:%M:%SZ)

OUT=$ROOT/examples/lowlat/RESULTS.md
{
    echo "# Baja latencia: pool, libro y motor"
    echo
    echo "Generado por \`examples/lowlat/bench/run.sh\`. Muestras $START — $END."
    echo "Una pasada de calentamiento y siete corridas, \`taskset -c 0\`. La mediana es la 4.ª. stdout se descarta al medir, después de comparar las tres salidas."
    echo "Copihue \`$($CC --version)\` \`-O2\`. gcc \`-O2 -Wall -Werror\`. rustc \`-O\`."
    echo "El anillo no se mide: es de una hebra y no hay atómicos."
    echo
    echo "| Programa | Salida | Copihue | C | Rust | frente a C | frente a Rust |"
    echo "|---|---:|---:|---:|---:|---|---|"
    for name in pool book match; do
        out=$(cat "$WORK/$name.lap.out")
        lm=$(median_of "$WORK/$name.lap.ns")
        cm=$(median_of "$WORK/$name.c.ns")
        rm=$(median_of "$WORK/$name.rs.ns")
        echo "| \`$name\` | $out | $(ms "$lm") ($(ms "$(min_of "$WORK/$name.lap.ns")")–$(ms "$(max_of "$WORK/$name.lap.ns")")) | $(ms "$cm") ($(ms "$(min_of "$WORK/$name.c.ns")")–$(ms "$(max_of "$WORK/$name.c.ns")")) | $(ms "$rm") ($(ms "$(min_of "$WORK/$name.rs.ns")")–$(ms "$(max_of "$WORK/$name.rs.ns")")) | $(relate "$WORK/$name.lap.ns" "$WORK/$name.c.ns") | $(relate "$WORK/$name.rs.ns" "$WORK/$name.lap.ns" | sed 's/por debajo/por encima/; s/por encima/por debajo/') |"
    done
    echo
    echo "\`por debajo\` quiere decir que el máximo de Copihue es menor que el mínimo del otro. \`se solapan\` quiere decir que no se puede afirmar un orden. Un empate de un extremo cuenta como solape."
    echo "Rust del pool usa \`unsafe\` porque la lista libre es de punteros. El libro y el motor son índices, sin \`unsafe\`."
} > "$OUT"

# The relate against Rust above swapped the files and then swapped the words,
# which double-swaps. Rewrite that column directly.
python3 - "$WORK" "$OUT" << 'PY'
import sys
from pathlib import Path
work = Path(sys.argv[1])
out = Path(sys.argv[2])
text = out.read_text().splitlines()

def samples(name, lang):
    xs = sorted(int(x) for x in (work / f"{name}.{lang}.ns").read_text().split())
    return xs[0], xs[3], xs[-1]

def word(a, b):
    if a[2] < b[0]:
        return "por debajo"
    if a[0] > b[2]:
        return "por encima"
    return "se solapan"

rows = []
for line in text:
    if line.startswith("| `"):
        name = line.split("`")[1]
        rows.append((name, word(samples(name, "lap"), samples(name, "c")),
                     word(samples(name, "lap"), samples(name, "rs"))))
# rebuild the two verdict columns from scratch by regenerating those cells
new = []
it = iter(rows)
for line in text:
    if line.startswith("| `"):
        name, vc, vr = next(it)
        parts = [p.strip() for p in line.strip("|").split("|")]
        parts[-2] = vc
        parts[-1] = vr
        new.append("| " + " | ".join(parts) + " |")
    else:
        new.append(line)
out.write_text("\n".join(new) + "\n")
PY

echo "bench: escribió $OUT"
cat "$OUT"
