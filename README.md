# emu6502

A MOS 6502 emulator core in C++17, written as a learning project. It ships with an
instruction-level interpreter, a small assembler and disassembler, a pluggable clock,
and two debugger front-ends (Win32 GUI and ncurses TUI).

## Status

**Passes Klaus Dormann's `6502_functional_test`**, including decimal-mode `ADC`/`SBC`,
after 30.6M instructions. The test is wired into the suite, so it gates every build.

| Area | State |
| --- | --- |
| Documented opcodes | all 151, verified by the functional test |
| Undocumented opcodes | all 256 slots filled; six unstable ones approximated |
| Flags, ALU, stack, BCD | correct |
| Addressing modes | correct, including zero-page wrap and the `JMP ($xxFF)` bug |
| Cycle counts | base, branch taken/page-cross, indexed page-cross |
| Interrupts (IRQ/NMI) | implemented, with correct `B` and `I` semantics |
| Timing granularity | instruction-stepped, not cycle-stepped |
| Bus / memory mapping | single flat 64 KB `Memory`; `Bus` is virtual for decoding |

See [docs/accuracy.md](docs/accuracy.md) for the conformance report and the remaining
known deviations.

## Layout

```
include/6502cc/     public API headers
src/                core implementation (Opcode.h, parameters.h, InstructionImpl.h are private)
debugger_src/       Win32 GUI debugger + ncurses TUI debugger
test_src/           doctest unit tests
test/               Klaus Dormann functional test binary + AS65 assembler
```

## Building

```bash
cmake -S . -B build && cmake --build build --config Release
```

Targets: `emu6502_lib` (shared library), `emu6502_debugger`, `emu6502_test`.

### Running the tests

```bash
ctest --test-dir build -C Release --output-on-failure
```

Or run the binary directly — `./build/Release/emu6502_test` on MSVC,
`./build/emu6502_test` on single-config generators. It finds its data files through a
compile definition, so any working directory works.

The functional test dominates the runtime (about 30 seconds in a Release build, much
longer in Debug). To skip it while iterating:

```bash
./build/Release/emu6502_test -tce=functional_test_suite
```

### Running the debugger

The debugger loads `test/6502_functional_test.bin` from the **current working
directory**, so launch it from the repository root.

## Quick start

Wire up the four pieces yourself — `I6502Emulator` is a convenience façade over them,
not an owner.

```cpp
#include <6502cc/I6502Emulator.h>

Registers regs;
Memory    mem(256);          // 256 pages = 64 KB
Bus       bus;               // Bus::read/write are virtual — override to add mapping
bus.connect(&mem);

default_clock clock;         // free-running: counts cycles, never sleeps
Processor cpu(&bus, &regs, &clock);
I6502Emulator emu(&regs, &mem, &bus, &cpu);

emu.start(true);             // reset, then run() on THIS thread until stopped
```

`start(false)` resets and returns immediately, leaving you to drive `cpu.step()`.
`Processor::step()` executes one instruction (or services a pending interrupt) and
charges its cycles to the clock — that is the hook you want for driving co-processors:

```cpp
uint64_t before = clock.cycles();
cpu.step();
ppu.tick(3 * (clock.cycles() - before));   // the NES runs 3 PPU dots per CPU cycle
```

Interrupts are asserted from outside, and both calls are thread-safe:

```cpp
cpu.nmi();          // edge-triggered, latches, ignores the I flag
cpu.irq(true);      // level-triggered, held until the device releases it
```

Assembling and disassembling:

```cpp
Asm as(0x0400);
std::vector<uint8> image = as.compile("lda #$01\nadc #$02\n");  // always 64 KB
UnAsm un;
std::string line = un.unasm_line(&bus, 0x0400);                 // "LDA $01"
```

The assembler takes conventional operand syntax — `($nn,X)`, `($nn),Y`, `#$nn` — but
hex operands must have exactly 2 or 4 digits, branch targets are signed decimal
offsets, and labels are not supported. See [docs/assembler.md](docs/assembler.md)
before writing source for it.

## Documentation

- [docs/architecture.md](docs/architecture.md) — how dispatch, addressing, the bus and the clock fit together
- [docs/accuracy.md](docs/accuracy.md) — functional-test results and every known deviation from real hardware
- [docs/assembler.md](docs/assembler.md) — the built-in assembler/disassembler grammar and its limits
- [docs/nes-roadmap.md](docs/nes-roadmap.md) — what this core needs before it can host a NES

## License

Copyright Alan N. Lohse, 2021.
