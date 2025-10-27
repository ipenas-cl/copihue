# Chronos Compiler

The Chronos compiler implementation with zero C dependencies.

## Structure

- `bootstrap/` - Assembly bootstrap code for self-hosting
- `stages/` - Multi-stage compilation (stage0 → stage1 → stage2)  
- `platforms/` - Platform-specific implementations (macOS, Linux, Windows)
- `passes/` - Compiler optimization passes
- `tools/` - Compiler utilities and tools

## Philosophy

- **Zero C Dependencies**: Built entirely from assembly up
- **Self-Hosting**: The compiler compiles itself
- **Deterministic**: WCET guarantees at compile time
- **Cross-Platform**: Supports macOS, Linux, Windows natively

## Build

```bash
cd platforms/macos && ./build.sh
```
