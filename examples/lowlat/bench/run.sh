#!/usr/bin/env bash
# Mide pool, libro y motor con taskset -c 0, contra C y Rust.
# Mide vec (acumular y recorrer) con taskset -c 0, contra C++ y Rust.
# Mide false sharing, afinidad y watchdog sin taskset, contra C++ y Rust.
# Una calentamiento y siete corridas. La mediana es la 4.ª.
# Escribe examples/lowlat/RESULTS.md.
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
NCPU=$(nproc)

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

compile_c() { # $1 name  (C y Rust)
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

compile_cpp() { # $1 name  (C++ y Rust)
    local name=$1
    local t
    t=$(tag_of "$ROOT/examples/lowlat/bench/$name.lap")
    for ext in cpp rs; do
        local u
        u=$(tag_of "$ROOT/examples/lowlat/bench/$name.$ext")
        if [ "$t" != "$u" ]; then
            echo "bench: carga distinta en $name.$ext" >&2
            exit 1
        fi
    done
    "$CC" -O2 -o "$WORK/$name.lap" "$ROOT/examples/lowlat/bench/$name.lap"
    g++ -O2 -std=c++17 -Wall -Werror -pthread -o "$WORK/$name.cpp" "$ROOT/examples/lowlat/bench/$name.cpp"
    rustc -O -o "$WORK/$name.rs" "$ROOT/examples/lowlat/bench/$name.rs"
    "$WORK/$name.lap" > "$WORK/$name.lap.out"
    "$WORK/$name.cpp" > "$WORK/$name.cpp.out"
    "$WORK/$name.rs" > "$WORK/$name.rs.out"
    cmp -s "$WORK/$name.lap.out" "$WORK/$name.cpp.out"
    cmp -s "$WORK/$name.lap.out" "$WORK/$name.rs.out"
    echo "bench: $name salida $(cat "$WORK/$name.lap.out")"
}

measure() { # $1 bin $2 outfile $3 cpus (empty = sin taskset)
    local bin=$1 out=$2 cpus=$3 i a b
    if [ -n "$cpus" ]; then
        taskset -c "$cpus" "$bin" >/dev/null
    else
        "$bin" >/dev/null
    fi
    : > "$out"
    for i in 1 2 3 4 5 6 7; do
        a=$(ns_now)
        if [ -n "$cpus" ]; then
            taskset -c "$cpus" "$bin" >/dev/null
        else
            "$bin" >/dev/null
        fi
        b=$(ns_now)
        echo $((b - a)) >> "$out"
    done
}

cell() { # $1 ns file
    echo "$(ms "$(median_of "$1")") ($(ms "$(min_of "$1")")–$(ms "$(max_of "$1")"))"
}

START=$(date -u +%Y-%m-%dT%H:%M:%SZ)
for name in pool book match; do
    compile_c "$name"
    measure "$WORK/$name.lap" "$WORK/$name.lap.ns" 0
    measure "$WORK/$name.c" "$WORK/$name.c.ns" 0
    measure "$WORK/$name.rs" "$WORK/$name.rs.ns" 0
done
for name in vec; do
    compile_cpp "$name"
    measure "$WORK/$name.lap" "$WORK/$name.lap.ns" 0
    measure "$WORK/$name.cpp" "$WORK/$name.cpp.ns" 0
    measure "$WORK/$name.rs" "$WORK/$name.rs.ns" 0
done
for name in share split pin same free watch; do
    compile_cpp "$name"
    measure "$WORK/$name.lap" "$WORK/$name.lap.ns" ""
    measure "$WORK/$name.cpp" "$WORK/$name.cpp.ns" ""
    measure "$WORK/$name.rs" "$WORK/$name.rs.ns" ""
done
END=$(date -u +%Y-%m-%dT%H:%M:%SZ)

OUT=$ROOT/examples/lowlat/RESULTS.md
{
    echo "# Baja latencia: pool, libro, motor, vec, false sharing, afinidad y watchdog"
    echo
    echo "Generado por \`examples/lowlat/bench/run.sh\`. Muestras $START — $END."
    echo "Una pasada de calentamiento y siete corridas. La mediana es la 4.ª. stdout se descarta al medir, después de comparar las tres salidas."
    echo "Copihue \`$($CC --version)\` \`-O2\`. Pool, libro y motor: gcc \`-O2 -Wall -Werror\`, rustc \`-O\`, \`taskset -c 0\`."
    echo "\`vec\`: g++ \`-O2 -std=c++17 -Wall -Werror\`, rustc \`-O\`, \`taskset -c 0\`. Empuja \`0..999999\` sin reservar y suma el recorrido."
    echo "False sharing, afinidad y watchdog: g++ \`-O2 -std=c++17 -Wall -Werror -pthread\`, rustc \`-O\`, sin \`taskset\`. Esta máquina reporta $NCPU CPU."
    echo "El anillo no se mide aquí. La cola SPSC de un millón de mensajes está en \`bench/lowlat\` (0.32.0)."
    echo
    echo "## Pool, libro y motor"
    echo
    echo "| Programa | Salida | Copihue | C | Rust | frente a C | frente a Rust |"
    echo "|---|---:|---:|---:|---:|---|---|"
    for name in pool book match; do
        out=$(cat "$WORK/$name.lap.out")
        echo "| \`$name\` | $out | $(cell "$WORK/$name.lap.ns") | $(cell "$WORK/$name.c.ns") | $(cell "$WORK/$name.rs.ns") | pendiente | pendiente |"
    done
    echo
    echo "\`por debajo\` quiere decir que el máximo de Copihue es menor que el mínimo del otro. \`se solapan\` quiere decir que no se puede afirmar un orden. Un empate de un extremo cuenta como solape."
    echo "Rust del pool usa \`unsafe\` porque la lista libre es de punteros. El libro y el motor son índices, sin \`unsafe\`."
    echo
    echo "## vec"
    echo
    echo "Un \`vec i64\` vacío recibe \`vec_push\` de \`0..999999\`. El primero reserva 4 y después la capacidad dobla. Luego \`for x in\` suma los elementos. C++ es \`std::vector\` sin \`reserve\`. Rust es \`Vec\` sin \`with_capacity\`. La suma 499999500000 se imprime en dos campos y después va la longitud."
    echo
    echo "| Programa | Salida | Copihue | C++ | Rust | frente a C++ | frente a Rust |"
    echo "|---|---:|---:|---:|---:|---|---|"
    for name in vec; do
        out=$(cat "$WORK/$name.lap.out")
        echo "| \`$name\` | $out | $(cell "$WORK/$name.lap.ns") | $(cell "$WORK/$name.cpp.ns") | $(cell "$WORK/$name.rs.ns") | pendiente | pendiente |"
    done
    echo
    echo "Esta fila va bajo \`taskset -c 0\`. El índice de Copihue en \`v[i]\` comprueba la longitud; este banco recorre con \`for x in\`, que copia puntero y longitud al entrar."
    echo
    echo "## False sharing, afinidad y watchdog"
    echo
    echo "\`share\` incrementa dos contadores de la misma línea de 64 bytes. \`split\` los separa con \`align(64)\` (el segundo queda en el offset 64). \`pin\` fija las hebras a cpu 0 y cpu 1. \`same\` fija las dos a cpu 0. \`free\` no llama a \`sched_setaffinity\`. Esas tres usan el layout de \`split\`, así que el tiempo no mezcla el false sharing. \`watch\` escribe 4000000 latidos y después exige 1000000 lecturas iguales. Si la afinidad falla, \`pin\` y \`same\` imprimen 0."
    echo
    echo "| Programa | Salida | Copihue | C++ | Rust | frente a C++ | frente a Rust |"
    echo "|---|---:|---:|---:|---:|---|---|"
    for name in share split pin same free watch; do
        out=$(cat "$WORK/$name.lap.out")
        echo "| \`$name\` | $out | $(cell "$WORK/$name.lap.ns") | $(cell "$WORK/$name.cpp.ns") | $(cell "$WORK/$name.rs.ns") | pendiente | pendiente |"
    done
    echo
    echo "Estas seis filas no van bajo \`taskset -c 0\`: ese pin deja las dos hebras en un solo núcleo y apaga el false sharing entre núcleos. El efecto de abajo compara los rangos de Copihue entre sí, y el mismo par en C++ y en Rust. \`por encima\` quiere decir que el mínimo del primero supera el máximo del segundo."
} > "$OUT"

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

def other(name):
    if (work / f"{name}.c.ns").exists():
        return "c"
    return "cpp"

rows = []
for line in text:
    if line.startswith("| `"):
        name = line.split("`")[1]
        lang = other(name)
        rows.append((name, word(samples(name, "lap"), samples(name, lang)),
                     word(samples(name, "lap"), samples(name, "rs"))))

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

def effect(title, a, b):
    bits = [f"{title}: Copihue {word(samples(a, 'lap'), samples(b, 'lap'))}"]
    for lang, label in (("cpp", "C++"), ("rs", "Rust")):
        bits.append(f"{label} {word(samples(a, lang), samples(b, lang))}")
    return ", ".join(bits) + "."

new.append("")
new.append(effect("`share` frente a `split`", "share", "split"))
new.append(effect("`same` frente a `pin`", "same", "pin"))
new.append(effect("`pin` frente a `free`", "pin", "free"))
new.append(effect("`same` frente a `free`", "same", "free"))
out.write_text("\n".join(new) + "\n")
PY

echo "bench: escribió $OUT"
cat "$OUT"
