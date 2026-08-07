# Using this core in a real system (e.g. a NES)

The NES CPU is a Ricoh 2A03: a 6502 with decimal mode fused off, plus an integrated
APU and DMA. The instruction core is ready — it passes the functional test, implements
all 256 opcodes, has working IRQ/NMI, and charges correct cycle counts including branch
and page-crossing penalties.

What is left is everything *around* the CPU. This is the order to build it in.

## 1. A bus that decodes addresses

This is now the main structural gap. `Bus` forwards every access to a single flat
`Memory`. A NES needs the CPU's 16-bit space split into regions with wildly different
behaviour:

| Range | Behaviour |
| --- | --- |
| `$0000–$1FFF` | 2 KB internal RAM, **mirrored** every `$0800` |
| `$2000–$3FFF` | 8 PPU registers, **mirrored** every 8 bytes; several have read side effects |
| `$4000–$4017` | APU and I/O registers; `$4014` triggers OAM DMA; `$4016`/`$4017` are the controllers |
| `$4020–$FFFF` | Cartridge space — routed through the mapper |

The seam already exists: `Bus::read` and `Bus::write` are virtual. Subclass and decode.

```cpp
class NesBus : public Bus {
    uint8 ram[0x800];
    Ppu* ppu; Apu* apu; Cartridge* cart;
public:
    uint8 read(uint16 a) override {
        if (a < 0x2000)  return ram[a & 0x07FF];        // mirroring
        if (a < 0x4000)  return ppu->readRegister(a & 7);
        if (a < 0x4018)  return apu->read(a);
        return cart->cpuRead(a);
    }
    void write(uint16 a, uint8 v) override { /* symmetrical */ }
};
```

Two things to get right while doing this:

- `Bus::connect(Memory*)` and the protected `p_mem` become dead weight. Leave them or
  narrow the base class; nothing in the core requires them.
- **Reads must be allowed to have side effects.** PPU `$2007` auto-increments, and
  `$2002` clears the vblank flag on read. That means a debugger's memory view cannot
  use the same `read()` the CPU uses — it needs a separate side-effect-free `peek()`.
  Add it now; retrofitting it after you have a memory viewer is painful. The two
  debuggers in `debugger_src/` both call `bus->read()` for their memory dumps and will
  need updating.

## 2. Wire the interrupts up

The CPU side is done. `Processor::nmi()` latches an edge-triggered request serviced
regardless of the `I` flag; `Processor::irq(bool)` holds a level-triggered line serviced
whenever `I` is clear. Both are atomic, so a device on another thread can assert them.
`serviceInterrupt()` pushes with `B` clear, sets `I`, vectors through `$FFFA`/`$FFFE`,
and charges 7 cycles. `RTI` and `BRK` already agree with that.

What you supply is the sources:

- PPU vblank → `nmi()`, gated by `$2000` bit 7.
- APU frame counter and DMC → `irq(true)`, released when the handler acknowledges.
- Mapper IRQs (MMC3's scanline counter) → `irq(true)`.

Remember that IRQ is a *level*: the device holds the line until its handler clears the
source. Asserting it once and forgetting to release re-enters the handler forever.

## 3. Drive the PPU from the cycle count

The NES runs the PPU at exactly 3 dots per CPU cycle. `Processor::step()` charges the
instruction's full cost to the clock, so:

```cpp
uint64_t before = clock.cycles();
cpu.step();
ppu.tick(3 * (clock.cycles() - before));
```

That is *instruction-stepped* timing, and it is enough for a scanline-accurate PPU —
which is how most "good enough to play games" emulators are built. Start there.

It is **not** enough for cycle-accurate timing, where a write must land on the exact
dot it happened. If you later want that, the change is to make the bus access itself
the unit of time: have `Bus::read`/`write` tick the PPU 3 dots on every access. That
works without restructuring `Processor`, because the instruction implementations funnel
every memory access through the bus — a genuinely nice property of the current design.
What you lose is the ability to count cycles from the clock delta, since the ticking
happens inside the access.

Note that read-modify-write instructions do not perform the hardware's dummy write, and
indexed reads that cross a page do not perform the dummy read. Those matter for a
handful of titles, not most. See [accuracy.md](accuracy.md#instruction-stepped-not-cycle-stepped).

## 4. Disable decimal mode

The 2A03 ignores the `D` flag in `ADC`/`SBC`, though `SED`/`CLD` still set and clear it.
This core implements full BCD — it is the one place it does *more* than the target
hardware. You need a switch.

The cleanest option given the current design is a bool on `Processor` consulted by
`adcCore`/`sbcCore` in `src/InstructionImpl.h`; the alternative is a compile-time flag
if you never need both behaviours in one binary.

## Smaller things you will hit

- **Power-on RAM state.** `Memory` zero-fills, which is defined but not what a NES does.
  Some games read RAM before writing it and a few detect a cold boot that way; fill NES
  RAM with whatever pattern you want to model.
- **OAM DMA.** `$4014` copies 256 bytes and stalls the CPU for 513 or 514 cycles. There
  is no representation for that — you will need a way for the bus to tell the processor
  "stall N cycles". A counter on `Processor` that `step()` drains before fetching is the
  smallest change that works.
- **iNES loading.** Nothing in this repo parses ROM files. Header, PRG/CHR banks, mirroring
  mode, mapper number.
- **`I6502Emulator` is thin.** It resets, starts and stops. A NES needs a master clock
  driving CPU, PPU and APU in ratio; you will likely write your own top-level object and
  use `Processor` directly rather than extending this one.
- **The assembler cannot write undocumented mnemonics.** The disassembler names all 256
  opcodes, but `Asm` only accepts the documented set. Not a problem for running ROMs —
  only if you plan to hand-write test programs that use them.

## Suggested milestones

1. Subclass `Bus` for NES decoding + mirroring; add `peek()`; load an iNES file;
   implement mapper 0.
2. PPU background rendering driven at 3 dots per CPU cycle; NMI on vblank.
3. Controllers, sprites, sprite-zero hit.
4. Decimal-mode switch; OAM DMA stall.
5. APU.

Nestest (`nestest.nes`) is the usual next gate after the functional test. Run it in its
automated mode from `$C000` and diff your CPU trace against the published log — it
checks undocumented opcodes and exact cycle counts instruction by instruction, so it
will tell you precisely where any remaining disagreement is. It is a good idea to do
this *before* starting the PPU, while the CPU is still the only thing that can be wrong.
