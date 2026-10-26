# Low-latency primitives

Measured by `bench/lowlat/run.sh` on 2026-10-26T01:06:45Z.
The SPSC process was pinned with `taskset -c 0,1`. The pool was pinned with `taskset -c 0`. SPMC, MPMC, and the thread pool were pinned with `taskset -c 0,1,2,3`.
One warmup, then 7 runs. The table is the median and the min–max, in milliseconds.
Copihue is `-O2`. C++ is `g++ -O2`. Rust is `rustc -O`.

The SPSC ring holds 1024 `u64` slots. The write index, the read index, and the buffer start on distinct 64-byte lines. The producer pushes 1000000 values (1 through 1000000) with an acquire load of the read index and a release store of the write index. The consumer sums them. The sum is 500000500000. A full or empty ring calls `pause` / `cpu_pause` / `spin_loop`. There is no `mfence` on that path.

SPMC is one producer and three consumers. The producer keeps a private cursor and waits until `seq == pos`, then release-stores `seq = pos + 1`. Consumers claim `deq` with `compare_exchange`. MPMC is two producers (1..500000 and 500001..1000000) and two consumers; both sides claim a ticket with `compare_exchange`. The thread pool is the same single-producer ring: main submits 1..1000000 and three workers pop. Each cell is 64 bytes, so `seq` and `data` share a line and the next cell does not. After the producers have been joined, a release store of `done` lets a consumer that sees an empty ring try once more and then leave. The sum is 500000500000.

The pool is a Treiber freelist of 4096 indexes. `compare_exchange` is sequential: `lock cmpxchg` in Copihue, `compare_exchange_strong` with `memory_order_seq_cst` in C++, and `compare_exchange` with `SeqCst` in Rust. One thread allocates and frees 1000000 times. `storage[0]` is 1000000.

| Benchmark | Copihue | C++ | Rust |
|---|---|---|---|
| spsc | 44.8 (38.9–57.3) | 45.1 (7.6–53.8) | 9.1 (7.0–13.2) |
| pool | 21.1 (20.6–24.5) | 21.0 (20.6–21.3) | 20.4 (19.9–23.8) |
| spmc | 272.3 (231.7–285.3) | 250.1 (227.2–256.8) | 266.0 (232.3–290.9) |
| mpmc | 162.9 (132.2–183.3) | 162.0 (148.3–178.0) | 281.5 (264.2–310.0) |
| tpool | 236.6 (222.2–275.1) | 246.3 (207.4–271.8) | 256.0 (236.2–262.2) |

These runs are the whole process, including thread creation and the initial `seq` fill of a ring. They are not part of `make test`.
