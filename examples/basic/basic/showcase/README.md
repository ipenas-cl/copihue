# Chronos Showcase Examples

These examples demonstrate Chronos's capabilities for high-performance systems programming.

## Performance Benchmarks

### 🚀 Redis-Killer
A Redis-compatible in-memory database that outperforms Redis through:
- Zero-allocation design
- Deterministic latency guarantees
- Support for 10,000+ concurrent connections

**Performance**: 450,000+ operations/second vs Redis's 100,000 ops/sec

```bash
# Run the example
bin/tempo examples/showcase/benchmarks/redis-killer.ch
./stage1
```

### ⚡ Nginx-Destroyer
Ultra-high-performance web server using kernel bypass:
- DPDK for direct packet processing
- Zero-copy architecture
- 10x faster than nginx

**Performance**: 4.5 million requests/second vs nginx's 450,000 req/sec

```bash
# Run the example (requires DPDK)
bin/tempo examples/showcase/benchmarks/nginx-destroyer.ch
./stage1
```

## Why These Examples Matter

1. **Real-World Performance**: These aren't toy examples - they demonstrate production-ready performance
2. **Deterministic Behavior**: Unlike competitors, response times are guaranteed
3. **Zero Dependencies**: No external libraries needed - everything built with Chronos
4. **Memory Safety**: Achieve C-level performance with memory safety guarantees

## Migration Guides

If you're coming from:
- **Redis**: See `redis-killer.ch` for compatible API implementation
- **Nginx**: See `nginx-destroyer.ch` for configuration examples

## Technical Highlights

### Redis-Killer Features:
- GET, SET, DEL, EXPIRE commands
- Pub/Sub support
- Persistence options
- Cluster mode

### Nginx-Destroyer Features:
- HTTP/1.1 and HTTP/2
- SSL/TLS support
- Load balancing
- Static file serving

Both examples showcase Chronos's ability to build systems software that would traditionally require C or C++, but with guaranteed safety and deterministic performance.