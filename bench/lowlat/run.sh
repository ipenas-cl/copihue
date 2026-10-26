#!/bin/sh
# Time the low-latency benches. Not part of `make test`.
# Writes bench/lowlat/RESULTS.md. One warmup, then 7 runs.
# The pool is pinned to cpu 0. SPSC uses two CPUs.
# SPMC, MPMC, and the thread pool use up to four.
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
# SPMC, MPMC, and the thread pool each run four threads. Threads inherit
# the process mask, so the mask has one CPU per thread when the machine
# has them. They are not all pinned to cpu 0.
POOL_PIN=""
SPSC_PIN=""
MULTI_PIN=""
if command -v taskset >/dev/null 2>&1; then
    POOL_PIN="0"
    n=$(nproc)
    if [ "$n" -ge 2 ]; then
        SPSC_PIN="0,1"
    else
        SPSC_PIN="0"
    fi
    if [ "$n" -ge 4 ]; then
        MULTI_PIN="0,1,2,3"
    elif [ "$n" -eq 3 ]; then
        MULTI_PIN="0,1,2"
    elif [ "$n" -eq 2 ]; then
        MULTI_PIN="0,1"
    else
        MULTI_PIN="0"
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
    elif [ "$1" = "pool" ]; then
        echo "$POOL_PIN"
    else
        echo "$MULTI_PIN"
    fi
}

want_for() {
    if [ "$1" = "pool" ]; then
        echo "1000000"
    else
        echo "500000500000"
    fi
}

check_out() {
    name=$1
    want=$(want_for "$name")
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

for name in spsc pool spmc mpmc tpool; do
    compile_one "$name"
    check_out "$name"
done

for name in spsc pool spmc mpmc tpool; do
    for lang in lap cpp rs; do
        measure "$name" "$lang"
    done
done

{
    echo "# Low-latency primitives"
    echo
    echo "Measured by \`bench/lowlat/run.sh\` on $(date -u +%Y-%m-%dT%H:%M:%SZ)."
    if [ -n "$SPSC_PIN" ]; then
        echo "The SPSC process was pinned with \`taskset -c $SPSC_PIN\`. The pool was pinned with \`taskset -c $POOL_PIN\`. SPMC, MPMC, and the thread pool were pinned with \`taskset -c $MULTI_PIN\`."
    else
        echo "\`taskset\` was not available, so the processes were not pinned."
    fi
    echo "One warmup, then $RUNS runs. The table is the median and the min–max, in milliseconds."
    echo "Copihue is \`-O2\`. C++ is \`g++ -O2\`. Rust is \`rustc -O\`."
    echo
    echo "The SPSC ring holds 1024 \`u64\` slots. The write index, the read index, and the buffer start on distinct 64-byte lines. The producer pushes 1000000 values (1 through 1000000) with an acquire load of the read index and a release store of the write index. The consumer sums them. The sum is 500000500000. A full or empty ring calls \`pause\` / \`cpu_pause\` / \`spin_loop\`. There is no \`mfence\` on that path."
    echo
    echo "SPMC is one producer and three consumers. The producer keeps a private cursor and waits until \`seq == pos\`, then release-stores \`seq = pos + 1\`. Consumers claim \`deq\` with \`compare_exchange\`. MPMC is two producers (1..500000 and 500001..1000000) and two consumers; both sides claim a ticket with \`compare_exchange\`. The thread pool is the same single-producer ring: main submits 1..1000000 and three workers pop. Each cell is 64 bytes, so \`seq\` and \`data\` share a line and the next cell does not. After the producers have been joined, a release store of \`done\` lets a consumer that sees an empty ring try once more and then leave. The sum is 500000500000."
    echo
    echo "The pool is a Treiber freelist of 4096 indexes. \`compare_exchange\` is sequential: \`lock cmpxchg\` in Copihue, \`compare_exchange_strong\` with \`memory_order_seq_cst\` in C++, and \`compare_exchange\` with \`SeqCst\` in Rust. One thread allocates and frees 1000000 times. \`storage[0]\` is 1000000."
    echo
    echo "| Benchmark | Copihue | C++ | Rust |"
    echo "|---|---|---|---|"
    for name in spsc pool spmc mpmc tpool; do
        line="| $name |"
        for lang in lap cpp rs; do
            med=$(median_ms "$WORK/$name.$lang.ns")
            set -- $(range_ms "$WORK/$name.$lang.ns")
            line="$line $med ($1–$2) |"
        done
        echo "$line"
    done
    echo
    echo "These runs are the whole process, including thread creation and the initial \`seq\` fill of a ring. They are not part of \`make test\`."
} > "$HERE/RESULTS.md"

cat "$HERE/RESULTS.md"
