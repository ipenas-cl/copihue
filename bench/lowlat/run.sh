#!/bin/sh
# Time the SPSC ring and the memory pool. Not part of `make test`.
# Writes bench/lowlat/RESULTS.md. One warmup, then 7 runs.
# The pool is pinned to cpu 0. The SPSC ring is pinned to two CPUs.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
HERE=$ROOT/bench/lowlat
WORK=$HERE/out
RUNS=7

need() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "bench/lowlat: missing '$1'" >&2
        exit 1
    fi
}

need gcc
need g++
need rustc
need nasm
need ld
need make
need awk
need date

if ! date +%s%N >/dev/null 2>&1; then
    echo "bench/lowlat: GNU date +%s%N is required" >&2
    exit 1
fi

make copihue >/dev/null
COPIHUE=$ROOT/copihue

rm -rf "$WORK"
mkdir -p "$WORK"

# The pool is one thread: pin it to cpu 0. The SPSC ring has two threads.
# Pinning both to the same CPU makes them wait out each other's pause loop
# (about 8 seconds here). Two CPUs let the producer and the consumer run
# at once, which is the case the ring is for.
POOL_PIN=""
SPSC_PIN=""
if command -v taskset >/dev/null 2>&1; then
    POOL_PIN="0"
    if [ "$(nproc)" -ge 2 ]; then
        SPSC_PIN="0,1"
    else
        SPSC_PIN="0"
    fi
fi

run_on() {
    cpus=$1
    shift
    if [ -n "$cpus" ]; then
        taskset -c "$cpus" "$@"
    else
        "$@"
    fi
}

ns_now() { date +%s%N; }

compile_one() {
    name=$1
    echo "compile $name"
    "$COPIHUE" -O2 -o "$WORK/$name.lap.bin" "$HERE/$name.lap"
    g++ -O2 -std=c++17 -pthread -o "$WORK/$name.cpp.bin" "$HERE/$name.cpp"
    rustc -O -o "$WORK/$name.rs.bin" "$HERE/$name.rs"
}

cpus_for() {
    if [ "$1" = "spsc" ]; then
        echo "$SPSC_PIN"
    else
        echo "$POOL_PIN"
    fi
}

check_out() {
    name=$1
    want=$2
    for lang in lap cpp rs; do
        got=$(run_on "$(cpus_for "$name")" "$WORK/$name.$lang.bin")
        if [ "$got" != "$want" ]; then
            echo "$name $lang printed '$got', want '$want'" >&2
            exit 1
        fi
    done
}

measure() {
    name=$1
    bin=$2
    file=$WORK/$name.$bin.ns
    : > "$file"
    i=0
    while [ "$i" -le "$RUNS" ]; do
        t0=$(ns_now)
        run_on "$(cpus_for "$name")" "$WORK/$name.$bin.bin" >/dev/null
        t1=$(ns_now)
        if [ "$i" -gt 0 ]; then
            echo $((t1 - t0)) >> "$file"
        fi
        i=$((i + 1))
    done
}

median_ms() {
    sort -n "$1" | awk '{ a[NR] = $1 } END { printf "%.1f", a[int((NR + 1) / 2)] / 1000000 }'
}

range_ms() {
    awk 'NR == 1 || $1 < min { min = $1 } NR == 1 || $1 > max { max = $1 }
         END { printf "%.1f %.1f", min / 1000000, max / 1000000 }' "$1"
}

compile_one spsc
compile_one pool
check_out spsc "500000500000"
check_out pool "1000000"

for name in spsc pool; do
    for lang in lap cpp rs; do
        measure "$name" "$lang"
    done
done

{
    echo "# Low-latency primitives"
    echo
    echo "Measured by \`bench/lowlat/run.sh\` on $(date -u +%Y-%m-%dT%H:%M:%SZ)."
    if [ -n "$SPSC_PIN" ]; then
        echo "The SPSC process was pinned with \`taskset -c $SPSC_PIN\` so the producer and the consumer can run on two CPUs. The pool was pinned with \`taskset -c $POOL_PIN\`."
    else
        echo "\`taskset\` was not available, so the processes were not pinned."
    fi
    echo "One warmup, then $RUNS runs. The table is the median and the min–max, in milliseconds."
    echo "Copihue is \`-O2\`. C++ is \`g++ -O2\`. Rust is \`rustc -O\`."
    echo
    echo "The SPSC ring holds 1024 \`u64\` slots. The write index and the read index are on separate 64-byte lines. The producer pushes 1000000 values (1 through 1000000) with an acquire load of the read index and a release store of the write index. The consumer sums them. The sum is 500000500000. A full or empty ring calls \`pause\` / \`cpu_pause\` / \`spin_loop\`."
    echo
    echo "The pool is a Treiber freelist of 4096 indexes. \`compare_exchange\` is sequential: \`lock cmpxchg\` in Copihue, \`compare_exchange_strong\` with \`memory_order_seq_cst\` in C++, and \`compare_exchange\` with \`SeqCst\` in Rust. One thread allocates and frees 1000000 times. \`storage[0]\` is 1000000."
    echo
    echo "| Benchmark | Copihue | C++ | Rust |"
    echo "|---|---|---|---|"
    for name in spsc pool; do
        line="| $name |"
        for lang in lap cpp rs; do
            med=$(median_ms "$WORK/$name.$lang.ns")
            set -- $(range_ms "$WORK/$name.$lang.ns")
            line="$line $med ($1–$2) |"
        done
        echo "$line"
    done
    echo
    echo "These runs are the whole process, including thread creation for the SPSC ring and freelist setup for the pool. They are not part of \`make test\`."
} > "$HERE/RESULTS.md"

cat "$HERE/RESULTS.md"
