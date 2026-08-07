# emu6502

A MOS 6502 emulator core in C++17, written as a learning project. It ships with an
instruction-level interpreter, a small assembler and disassembler, a pluggable clock,
and two debugger front-ends (Win32 GUI and ncurses TUI).

## Status

The core executes all 151 documented 6502 opcodes. It **passes Klaus Dormann's
`6502_functional_test`** — including decimal-mode `ADC`/`SBC` — once a single
addressing-mode bug is corrected. See [docs/accuracy.md](docs/accuracy.md) for the
conformance report, the exact bug, and the full list of known deviations.

| Area | State |
| --- | --- |
| Documented opcodes | complete |
| Flags, ALU, stack, BCD | correct (functional test) |
| Addressing modes | correct except `(zp,X)` — see accuracy doc |
| Cycle counts | base counts only; no page-cross or branch penalties |
| Interrupts (IRQ/NMI) | **not implemented** (`BRK`/`RTI` exist) |
| Undocumented opcodes | decode as 2-cycle `NOP` |
| Bus / memory mapping | single flat 64 KB `Memory`; no address decoding |

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

### Windows caveats

Two defects currently stop a clean MSVC build. Both are verified, and both have
one-line fixes.

**1. No exported symbols.** `emu6502_lib` is declared `SHARED` but nothing is annotated
with `__declspec(dllexport)`, so MSVC produces no import library and both
`emu6502_test` and `emu6502_debugger` fail with `LNK1104: cannot open
'emu6502_lib.lib'`. Work around it at configure time:

```bash
cmake -S . -B build -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON
```

The durable fix is `set_target_properties(emu6502_lib PROPERTIES
WINDOWS_EXPORT_ALL_SYMBOLS ON)` in `CMakeLists.txt`, an export macro, or making the
library `STATIC`.

**2. Duplicate manifest.** `debugger_src/Resource.rc:33` embeds `Application.manifest`
as an `RT_MANIFEST` resource, and MSVC's linker generates one as well, so
`emu6502_debugger` fails with `CVT1100: duplicate resource ... MANIFEST` followed by
`LNK1123`. Suppress the generated one:

```cmake
if(MSVC)
    target_link_options(emu6502_debugger PRIVATE /MANIFEST:NO)
endif()
```

With both applied, all three targets build and the test suite passes.

### Running

```bash
./build/emu6502_test
```

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
`Processor::step()` fetches one opcode, executes it, and charges its cycles to the
clock — that is the hook you want for cycle-driven co-processors.

Assembling and disassembling:

```cpp
Asm as(0x0400);
std::vector<uint8> image = as.compile("lda #$01\nadc #$02\n");  // always 64 KB
UnAsm un;
std::string line = un.unasm_line(&bus, 0x0400);                 // "LDA $01"
```

The assembler's operand syntax is **not** standard 6502 assembly — indexed-indirect is
written `($nn),x`, hex operands must have exactly 2 or 4 digits, and labels are not
supported. See [docs/assembler.md](docs/assembler.md) before writing any source for it.

## Documentation

- [docs/architecture.md](docs/architecture.md) — how dispatch, addressing, the bus and the clock fit together
- [docs/accuracy.md](docs/accuracy.md) — functional-test results and every known deviation from real hardware
- [docs/assembler.md](docs/assembler.md) — the built-in assembler/disassembler grammar and its limits
- [docs/nes-roadmap.md](docs/nes-roadmap.md) — what this core needs before it can host a NES

## License

Copyright Alan N. Lohse, 2021.
