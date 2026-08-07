# Architecture

Five objects make up a running system. None of them owns any other — you construct
them all and keep them alive for the duration.

```
  Registers ──┐
              ├──> Processor ──> Bus ──> Memory
  emu_clock ──┘        │
                       └─ static BaseInstruction* _instruction_table[256]
```

`I6502Emulator` is a thin façade that holds pointers to the four and implements
reset/start/stop. It deliberately does not manage lifetimes.

## Instruction dispatch

The central design idea is that **an instruction is the cross product of an operation
and an addressing mode**, resolved at compile time rather than at run time.

Each operation is a class template parameterised on the addressing mode and its cycle
count (`src/InstructionImpl.h`):

```cpp
template<class IParams, int cycles>
class ADC_Impl : public BaseInstruction {
    IParams params;
    int execute(Registers* regs, Bus* bus) override {
        params.init(regs, bus);                    // consume operand bytes, advance PC
        uint8 val = params.get8bit(regs, bus);     // resolve to a value
        ...
        return cycles;
    }
};
```

Each addressing mode is a plain struct with a fixed interface (`src/parameters.h`) —
`init`, `get8bit`/`get16bit`, `set8bit`/`set16bit`, plus `extraCycles()` for the
page-crossing penalty. They share a `ParamsBase` that carries the crossing flag, but
dispatch is not virtual: the mode is a template argument, so the compiler inlines the
whole address calculation into each instruction body.

`Processor::_instruction_table` (in `src/Processor.cpp`) is then a literal 16×16
transcription of the official opcode matrix:

```cpp
new ADC_Impl<ImmediateParams,2>(),  // 0x69
new ADC_Impl<ZeroPageParams, 3>(),  // 0x65
```

Unassigned opcodes point at a single shared `nop_instruction`.

The payoff is that adding an operation means writing one template, and adding an
addressing mode means writing one struct — the 256-entry matrix stays declarative. That
is what made filling in the undocumented opcodes cheap: most of them are an existing
operation composed with an existing mode, or two operations chained. The cost is one
`virtual` call per instruction.

### Consequences worth knowing

- **The table is `static`.** All `Processor` instances share the same 256 instruction
  objects.
- **The objects are stateless.** `IParams params` is a local inside `execute()`, not a
  member, so nothing is carried between calls and the shared table is safe to use from
  more than one `Processor` or thread.
- The 256 objects are `new`ed during static initialisation and never freed. That is
  deliberate — they live for the whole process.

## The fetch-execute loop

```cpp
void Processor::step() {
    if (m_nmi_pending.exchange(false)) {                    // NMI ignores the I flag
        p_clock->waitCycles(serviceInterrupt(0xFFFA));
    } else if (m_irq_line.load() && !p_regs->getStatus(FLAG_I)) {
        p_clock->waitCycles(serviceInterrupt(0xFFFE));
    } else {
        int code = p_bus->read(p_regs->pc++);               // fetch
        BaseInstruction* p_instr = _instruction_table[code];
        int cycles = p_instr->execute(p_regs, p_bus);       // operands + effects
        p_clock->waitCycles(cycles);                        // charge the clock
    }
    if (p_instr_callback) p_instr_callback();               // per-step hook
}
```

Everything after the fetch is the instruction's business — including advancing `pc`
past its operand bytes, which `params.init()` does. `run()` is just `step()` in a loop
until `m_stopping` is set.

`step()` is the integration point for anything that must advance in lockstep with the
CPU. It returns after a whole instruction, so this is an **instruction-stepped**
core, not a cycle-stepped one; see [nes-roadmap.md](nes-roadmap.md) for why that
matters.

### Cycle accounting

The template's `cycles` argument is the base cost. Two adjustments happen at run time:

- **Branches** return `cycles + 1` when taken and `cycles + 2` when the target is also
  in a different page, via the shared `branch()` helper.
- **Indexed reads** add `params.extraCycles()`, which is 1 when adding the index
  carried into a new page. Only read instructions do this — stores and read-modify-write
  instructions always pay for the extra bus cycle, and that is already in their base
  count, so `STA $30FF,X` is 5 cycles whether it crosses or not.

## Interrupts

`Processor` exposes two lines. `nmi()` latches an edge-triggered request that is
serviced regardless of the `I` flag; `irq(bool)` holds a level-triggered line that is
serviced whenever `I` is clear. Both are `std::atomic<bool>`, so a device on another
thread can assert them safely.

`serviceInterrupt()` pushes `PC` and then `SR` with `B` **clear** — that is how a
handler tells a hardware interrupt from `BRK`, which pushes the same byte with `B` set —
sets `I`, loads `PC` from the vector, and charges 7 cycles.

Interrupts are polled at instruction boundaries rather than mid-instruction, which
skips two hardware edge cases documented in [accuracy.md](accuracy.md#interrupts-are-polled-at-instruction-boundaries).

## Memory and the bus

`Memory` is a flat `uint8` array of `pages * 256` bytes with no address decoding.
`Bus` sits between the processor and memory and forwards `read`/`write`.

`Bus::read` and `Bus::write` are **virtual**, and that is the extension seam. A system
with memory-mapped I/O subclasses `Bus` and decodes the address before deciding where
the access lands. The base class only knows how to talk to one `Memory`.

`Memory` zero-fills on construction, and addresses beyond the allocated size wrap
(`address % size`) rather than running off the end — which keeps a short `Memory`
memory-safe and mirrors what undecoded address lines do on hardware. In the common case
of a full 64 KB block every `uint16` is in range, so the bounds check is a branch that
is never taken. The bulk `read`/`write` overloads clamp their length.

One overload hazard remains, and it is inherent to the signatures:
`Memory::write(0, x)` binds `0` to the `write(uint8* src, int length, uint16
offsetDest)` overload as a null pointer constant, not to `write(uint16 address, uint8
value)`. Cast the address to `uint16` when the literal is zero.

## Stack helpers

`push8`/`push16`/`pop8`/`pop16` live in `src/Memory.cpp` and are declared in
`InstructionImpl.h`. They implement the 6502's descending stack in page 1 and wrap
naturally because `sp` is `uint8`. `push16` writes the high byte first; `pop16` reads
low then high.

## Clocks

`emu_clock` is an abstract pacing interface. `beginCycle()` marks a timing origin and
`waitCycles(n)` charges `n` cycles and optionally sleeps to match wall-clock time.

| Class | Behaviour |
| --- | --- |
| `default_clock` | Counts cycles, never sleeps. Use for tests and headless runs. |
| `chrono_clock(mhz)` | Paces with `std::chrono` + `sleep_for`. |
| `precision_clock(mhz)` | Sleeps the bulk of a wait, spins the last millisecond. |

`cycles()` returns the running total and is what a debugger displays.

Both pacing clocks track an **absolute deadline** that advances by the cycle budget on
every call, rather than measuring each interval from a fresh origin. That distinction
matters more than it looks:

- Resetting the origin on each call charges the time spent asleep to the *next* call's
  budget, so the CPU runs at roughly twice the requested speed.
- An accumulated deadline is self-correcting. `sleep_for` granularity on Windows is
  ~1–15 ms, far coarser than the 2 µs of a 2-cycle instruction, so any single sleep
  massively overshoots — but the overshoot is absorbed by not sleeping again until the
  deadline catches up.

Measured over 100,000 cycles at 1 MHz (a 100 ms target): `precision_clock` lands on
100 ms and `chrono_clock` on ~108 ms. If the host falls more than 50 ms behind — a
breakpoint, a descheduled thread — the deadline snaps to the present instead of running
flat out to make up time that no longer matters.

`precision_clock` converts `QueryPerformanceCounter` from its own frequency units
(10 MHz, i.e. 100 ns per tick, on typical Windows hardware) into nanoseconds, rebasing
the counter on first use so the multiply cannot overflow. The Linux path uses
`clock_gettime(CLOCK_MONOTONIC)`.

For a full system you still probably want `default_clock` plus your own per-frame
pacing — one sleep per frame beats tens of thousands of tiny ones.

## Threading

`Processor::run()` blocks the calling thread. Both debuggers therefore run it on a
`std::thread` and stop it by setting a flag from the UI thread.

The run/stop flags and both interrupt lines are `std::atomic<bool>`, so asserting an
interrupt or requesting a pause from another thread is well defined. (They used to be
`volatile`, which in C++ provides neither atomicity nor ordering and was a data race.)

`I6502Emulator::stop()` still spins on `isRunning()` with `std::this_thread::yield()`
rather than waiting on a condition variable — correct, but not the tidiest way to wait.

## Debugging support

`DebugProcessor` extends `Processor` with an `unordered_set<uint16>` of breakpoints.
Its `run()` steps once unconditionally and then loops while `pc` is not in the set —
the unconditional first step is what lets you resume *from* a breakpoint without
immediately re-triggering it.

Two callbacks are available: `setInstructionCallback` fires after every instruction
(the debuggers use it for PC history and trap detection) and `setBreakpointCallback`
fires when the loop stops on a breakpoint.

`UnAsm` disassembles by decoding the opcode's `aaabbbcc` bit fields rather than by
table lookup, which is why it is only ~160 lines. It has two entry points: the
`Registers*` form advances `pc` past the instruction (use it to walk forward), and the
`uint16` form does not.
