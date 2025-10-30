# v0.4 Status - Two-Pass Assembler

## Implementation Completed:
- ✅ Symbol table structure (256 symbols max)
- ✅ Two-pass architecture
- ✅ Label extraction and storage
- ✅ Jump instruction encoders (jmp, je, jne, jl, jg, jge, jle, jns, jz, jnz)
- ✅ Relative offset calculation
- ✅ 60+ instructions total

## Issues Found:
1. **Pass 1 chicken-and-egg problem:** Trying to parse instructions before symbol table is complete
2. **Comment handling:** Comments being recognized as labels
3. **Size calculation:** Need simpler approach to calculate instruction sizes in Pass 1

## Solution Required:
Need a simplified Pass 1 that:
- Only collects labels and their positions
- Calculates positions by counting instruction lengths WITHOUT full parsing
- Simpler heuristic: estimate based on mnemonic

## Alternative Approach:
Use a **three-phase** system instead:
1. **Phase 1:** Collect all labels, assign temporary addresses (0-based indexing)
2. **Phase 2:** Calculate actual addresses by parsing and counting byte lengths
3. **Phase 3:** Final assembly with resolved symbols

OR simpler:
Just estimate all instructions as max size (10 bytes) in Pass 1, then do accurate Pass 2.

## Current Status:
- Code compiles successfully (1335 lines)
- Pass 1 runs but has bugs
- Pass 2 assembles 0 instructions (symbol resolution failing)
- Needs debugging and simplification

## Line Count Comparison:
- v0.1: 526 lines (9 instructions)
- v0.2: 703 lines (9 instructions + security)
- v0.3: 824 lines (40+ instructions)
- v0.4: 1335 lines (60+ instructions + symbols + jumps)

## Next Steps:
1. Fix Pass 1 to properly count instruction sizes
2. Fix comment/label detection
3. Test with simple jump program
4. Test with complex program
5. Document when working
