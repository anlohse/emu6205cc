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

Each addressing mode is a plain struct with a fixed four-method interface
(`src/parameters.h`) — `init`, `get8bit`/`get16bit`, `set8bit`/`set16bit`. There is no
common base class and no virtual call: the mode is a template argument, so the compiler
inlines the whole address calculation into each instruction body.

`Processor::_instruction_table` (in `src/Processor.cpp`) is then a literal 16×16
transcription of the official opcode matrix:

```cpp
new ADC_Impl<ImmediateParams,2>(),  // 0x69
new ADC_Impl<ZeroPageParams, 3>(),  // 0x65
```

Unassigned opcodes point at a single shared `nop_instruction`.

The payoff is that adding an operation means writing one template, and adding an
addressing mode means writing one struct — the 151-entry matrix stays declarative. The
cost is 151 distinct template instantiations and one `virtual` call per instruction.

### Consequences worth knowing

- **The table is `static`.** All `Processor` instances share the same 256 instruction
  objects.
- **Those objects hold mutable state.** `IParams params` is a *member*, and `init()`
  writes to it. Execution is therefore not reentrant: two `Processor`s stepping on
  different threads, or a nested `step()` from a callback, will corrupt each other's
  operand. Single-threaded stepping is fine because `init()` and `get8bit()` always
  happen inside one `execute()` call.
- The 256 objects are `new`ed during static initialisation and never freed.

Making `params` a local inside `execute()` instead of a member would remove the shared
state at no cost, and would let the table become `constexpr`-friendly.

## The fetch-execute loop

```cpp
void Processor::step() {
    int code = p_bus->read(p_regs->pc++);      // fetch
    BaseInstruction* p_instr = _instruction_table[code];
    int cycles = p_instr->execute(p_regs, p_bus);   // operands + effects
    p_clock->waitCycles(cycles);                    // charge the clock
    if (p_instr_callback) p_instr_callback();       // per-instruction hook
}
```

Everything after the fetch is the instruction's business — including advancing `pc`
past its operand bytes, which `params.init()` does. `run()` is just `step()` in a loop
until `m_stopping` is set.

`step()` is the integration point for anything that must advance in lockstep with the
CPU. It returns after a whole instruction, so this is an **instruction-stepped**
core, not a cycle-stepped one; see [nes-roadmap.md](nes-roadmap.md) for why that
matters.

## Memory and the bus

`Memory` is a flat `uint8` array of `pages * 256` bytes with no address decoding.
`Bus` sits between the processor and memory and forwards `read`/`write`.

`Bus::read` and `Bus::write` are **virtual**, and that is the extension seam. A system
with memory-mapped I/O subclasses `Bus` and decodes the address before deciding where
the access lands. The base class only knows how to talk to one `Memory`.

Two rough edges in `Memory`:

- `checkAddress()` is an empty function, and `read`/`write` accept the full 16-bit
  range regardless of how many pages were allocated. `Memory(2)` allocates 512 bytes
  but will happily be asked for `$FFFF`.
- The buffer is left uninitialised by the constructor, and the destructor uses
  `delete` on an array allocated with `new[]`.

There is also an overload hazard: `Memory::write(0, x)` binds `0` to the
`write(uint8* src, int length, uint16 offsetDest)` overload as a null pointer constant,
not to `write(uint16 address, uint8 value)`. Cast the address to `uint16` when the
literal is zero.

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
| `chrono_clock(mhz)` | Paces with `std::chrono` + `sleep_for`. Unit-correct. |
| `precision_clock(mhz)` | Intended to be tighter; see the caveat below. |

`cycles()` returns the running total and is what a debugger displays.

**`precision_clock` is currently miscalibrated on Windows.** `nanoTime()` returns a raw
`QueryPerformanceCounter` value, which is measured in units of
`QueryPerformanceFrequency` — 10 MHz (100 ns per tick) on typical Windows 11 hardware,
not 1 ns. `waitCycles` subtracts that tick count from a nanosecond budget, so elapsed
time is under-counted by ~100× and the clock over-sleeps. The Linux path uses
`clock_gettime(CLOCK_MONOTONIC)` and is genuinely nanoseconds. The tighter busy-wait
loop is also behind `#ifdef USE_PRECISION_CLOCK`, which the build never defines.

Independently of that bug, per-instruction sleeping cannot pace a 1 MHz CPU on a
desktop OS: a 2-cycle instruction is 2 µs, while `sleep_for` granularity is ~1–15 ms.
Real emulators run a batch of cycles (a frame, a scanline) and sleep once at the
boundary. `default_clock` plus your own frame pacing is the practical choice.

## Threading

`Processor::run()` blocks the calling thread. Both debuggers therefore run it on a
`std::thread` and stop it by setting a flag from the UI thread.

That flag is `volatile bool m_running` / `m_stopping`. `volatile` is not a
synchronisation primitive in C++ — it provides no atomicity and no ordering, so this is
a data race. `std::atomic<bool>` is the correct type and costs nothing here.
`I6502Emulator::stop()` also spins on `isRunning()` with `std::this_thread::yield()`
rather than waiting on a condition variable.

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
