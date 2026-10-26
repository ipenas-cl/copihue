# Low-latency primitives

Measured by `bench/lowlat/run.sh` on 2026-10-26T00:18:42Z.
The SPSC process was pinned with `taskset -c 0,1` so the producer and the consumer can run on two CPUs. The pool was pinned with `taskset -c 0`.
One warmup, then 7 runs. The table is the median and the min–max, in milliseconds.
Copihue is `-O2`. C++ is `g++ -O2`. Rust is `rustc -O`.

The SPSC ring holds 1024 `u64` slots. The write index and the read index are on separate 64-byte lines. The producer pushes 1000000 values (1 through 1000000) with an acquire load of the read index and a release store of the write index. The consumer sums them. The sum is 500000500000. A full or empty ring calls `pause` / `cpu_pause` / `spin_loop`.

The pool is a Treiber freelist of 4096 indexes. `compare_exchange` is sequential: `lock cmpxchg` in Copihue, `compare_exchange_strong` with `memory_order_seq_cst` in C++, and `compare_exchange` with `SeqCst` in Rust. One thread allocates and frees 1000000 times. `storage[0]` is 1000000.

| Benchmark | Copihue | C++ | Rust |
|---|---|---|---|
| spsc | 45.0 (18.4–76.1) | 34.1 (11.2–44.4) | 15.8 (10.5–30.6) |
| pool | 21.7 (21.3–22.0) | 21.4 (21.2–23.6) | 20.4 (20.2–21.4) |

These runs are the whole process, including thread creation for the SPSC ring and freelist setup for the pool. They are not part of `make test`.
