# Accuracy

## Conformance against the functional test

[Klaus Dormann's 6502 functional test](https://github.com/Klaus2m5/6502_65C02_functional_tests)
is built with `disable_decimal = 0` (decimal mode **is** tested) and `report = 0`
(failures spin on a self-branch). The image loads flat at `$0000` and is entered at
`$0400` — the reset vector deliberately points at a trap handler, so PC is forced to
the code segment as Klaus's readme describes. Success is the `JMP $3469` self-jump,
immediately followed by `JMP $0400`.

**It passes**, reaching `$3469` after exactly 30,646,177 instructions and 96,241,374
cycles. It is wired into the suite as `functional_test_suite` (see
`test_src/testFunctional.cpp`), so any regression in a documented opcode, addressing
mode, flag or BCD operation fails the build.

The image is GPL-3.0-or-later and is **not** vendored in this repository — CMake fetches
it at configure time from a pinned revision. See `CMakeLists.txt`, or the build section
of the [README](../README.md) for how to opt out or point at a local copy.

Both totals above are asserted, not just the trap address. The cycle count is the more
sensitive of the two: it exercises branch and page-crossing penalties across 30 million
instructions, which no targeted test covers as thoroughly. Because it is pinned to that
exact image, bumping the revision means re-deriving both constants from the failure
message.

```bash
cmake --build build --config Release && ./build/Release/emu6502_test
```

Alongside it, `test_src/testCpu.cpp` pins down the cases the functional test does not
reach: interrupts, page-crossing cycle counts, zero-page pointer wrapping, the
`JMP ($xxFF)` quirk, memory bounds, clock pacing and every undocumented opcode.

## What is modelled

| Area | State |
| --- | --- |
| Documented opcodes | all 151, verified by the functional test |
| Undocumented opcodes | all 256 slots filled; see the caveats below |
| Flags, ALU, stack, BCD | correct |
| Addressing modes | correct, including zero-page pointer wrapping |
| `JMP ($xxFF)` | reproduces the hardware page-boundary bug |
| Cycle counts | base, branch taken/page-cross, and indexed page-cross penalties |
| IRQ / NMI | implemented, with correct `B`-flag and `I`-gating semantics |
| Reset | `SP=$FD`, `I` set, vector from `$FFFC`, 7 cycles |

## Remaining deviations

These are known and deliberate. None of them affect the functional test, and none block
a scanline-accurate NES; they are listed so you know where the edges are.

### Instruction-stepped, not cycle-stepped

`Processor::step()` executes a whole instruction and then charges its total cycle cost
to the clock. Bus accesses within an instruction therefore all appear to happen at
once. Consequences:

- Read-modify-write instructions (`INC`, `ASL abs,X`, and the undocumented RMW family)
  do not perform the hardware's extra dummy write of the unmodified value. A few NES
  titles use that write to acknowledge an interrupt register.
- Indexed reads that cross a page do not perform the dummy read of the un-carried
  address.
- The cycle count is correct in total, but not in distribution.

Fixing this does not require restructuring `Processor` — because every instruction
funnels its memory access through `Bus`, making `Bus::read`/`write` tick a co-processor
gives cycle-level timing directly. See [nes-roadmap.md](nes-roadmap.md).

### Interrupts are polled at instruction boundaries

Real hardware samples the interrupt lines partway through an instruction, which
produces two observable effects this core does not reproduce:

- **Delayed flag effect.** `CLI`, `SEI` and `PLP` change `I` one instruction later than
  you would expect, so an IRQ can slip in immediately after `SEI`.
- **BRK hijacking.** An NMI asserted during a `BRK` sequence takes over the vector
  while keeping `BRK`'s pushed state.

Both are edge cases that essentially no software depends on.

### Unstable undocumented opcodes are approximated

Most undocumented opcodes are stable across NMOS parts and are implemented exactly:
`LAX`, `SAX`, `DCP`, `ISC`, `SLO`, `RLA`, `SRE`, `RRA`, `ANC`, `ALR`, `SBX`, `LAS`, the
`SBC` alias at `$EB`, and every multi-byte `NOP`. `KIL`/`JAM` locks the processor by
rewinding `PC` onto itself, which is what the hardware does.

These depend on analog behaviour and use the conventional approximations instead:

| Opcode | Instruction | Approximation |
| --- | --- | --- |
| `$8B` | XAA / ANE | `A = (A \| $EE) & X & imm` — the magic constant varies by chip |
| `$AB` | LXA | treated as `LAX #imm` |
| `$93`, `$9F` | SHA | `store (A & X) & (addr_high + 1)` |
| `$9B` | TAS | as SHA, and also loads `SP` |
| `$9C` | SHY | `store Y & (addr_high + 1)` |
| `$9E` | SHX | `store X & (addr_high + 1)` |

On hardware the `& (addr_high + 1)` is dropped when the index carries into a new page,
and the result is not reliably reproducible. Do not write software that depends on
these six.

`ARR` (`$6B`) implements the non-decimal flag rules. Its decimal-mode behaviour is
genuinely strange and is not modelled — irrelevant on a 2A03, which has decimal
disabled.

### Decimal mode is always available

`ADC`/`SBC` honour the `D` flag. The NES's 2A03 has decimal mode fused off, so a NES
port needs a switch to disable it. This is the one place the core does *more* than that
target hardware rather than less.

### Memory mirrors rather than faulting

`Memory` wraps addresses beyond its allocated size (`address % size`) instead of running
off the end. That keeps a short `Memory` memory-safe under any program and mirrors what
undecoded address lines do on real hardware, but it will not tell you that a program
strayed out of range. Allocate the full 256 pages if you want a flat 64 KB with no
mirroring.

## Build and portability

Both Windows build defects are fixed in `CMakeLists.txt`:

- `emu6502_lib` sets `WINDOWS_EXPORT_ALL_SYMBOLS`, so MSVC produces the import library
  that `emu6502_test` and `emu6502_debugger` need.
- The debugger links with `/MANIFEST:NO`, because `debugger_src/Resource.rc` already
  embeds `Application.manifest` as an `RT_MANIFEST` resource and letting the linker
  generate a second one gave `CVT1100` / `LNK1123`.

`enable_testing()` and `add_test()` are in place, so `ctest` runs the suite.
`testFunctional.cpp` finds its data file through the `EMU6502_TEST_DATA_DIR` compile
definition rather than a relative path, so the suite passes from any working directory.

The duplicated `emu_Bus.h` / `emu_bus.h` include in `src/parameters.h` is gone; all
includes now match the on-disk filenames, so the tree builds on a case-sensitive
filesystem.

Both debuggers preload the functional test image on startup, using the same absolute
path CMake supplies to the test suite. If no image was configured they start with zeroed
memory instead.
