# Using this core in a real system (e.g. a NES)

The NES CPU is a Ricoh 2A03: a 6502 with decimal mode fused off, plus an integrated
APU and DMA. The instruction core here is a good starting point — it passes the
functional test once `(zp,X)` is fixed — but four structural things are missing before
any commercial ROM will run. They are listed in the order you should tackle them,
because each one blocks the next.

## 1. A bus that decodes addresses

Today `Bus` forwards every access to a single flat `Memory`. A NES needs the CPU's
16-bit space split into regions with wildly different behaviour:

| Range | Behaviour |
| --- | --- |
| `$0000–$1FFF` | 2 KB internal RAM, **mirrored** every `$0800` |
| `$2000–$3FFF` | 8 PPU registers, **mirrored** every 8 bytes; several have read side effects |
| `$4000–$4017` | APU and I/O registers; `$4014` triggers OAM DMA; `$4016`/`$4017` are the controllers |
| `$4020–$FFFF` | Cartridge space — routed through the mapper |

The good news is that the seam already exists: `Bus::read` and `Bus::write` are
virtual. Subclass it and decode there.

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

Two things to be aware of while doing this:

- `Bus::connect(Memory*)` and the protected `p_mem` become dead weight. Leave them or
  narrow the base class; nothing in the core requires them.
- **Reads must be allowed to have side effects.** PPU `$2007` auto-increments, and
  `$2002` clears the vblank flag on read. That means a debugger's memory view cannot
  use the same `read()` the CPU uses — it needs a separate side-effect-free `peek()`.
  Add it now; retrofitting it after you have a memory viewer is painful.

## 2. Interrupts

This is the single largest gap. There is no IRQ or NMI path at all. Without NMI a NES
game has no vblank signal, so it will never advance past its title screen wait loop.

You need, on `Processor`:

- Lines to assert: `void nmi()` (edge-triggered, latches) and `void irq(bool level)`
  (level-triggered, held while the source is active).
- A check at the top of `step()`, *before* the opcode fetch: if NMI is pending, or IRQ
  is asserted and the `I` flag is clear, service it instead of executing.
- The sequence itself: push `PC` (high then low), push `SR` with `B` **clear** and bit 5
  set, set `I`, load `PC` from `$FFFA` (NMI) or `$FFFE` (IRQ), charge 7 cycles.

`RTI` is already implemented and correct, so it will work the moment something can
generate a frame for it to return from. `BRK` already pushes with `B` set, which is the
distinction software uses to tell `BRK` from a hardware IRQ.

Sources you will need to wire: PPU vblank → NMI (gated by `$2000` bit 7), APU frame
counter and DMC → IRQ, and mapper IRQs (MMC3's scanline counter) → IRQ.

## 3. Cycle accounting good enough to drive the PPU

The NES runs the PPU at exactly 3 dots per CPU cycle. Everything visible — sprite zero
hits, mid-frame scroll splits, raster effects — depends on the CPU's cycle count being
right. Right now:

- Branches return 0 or 2 cycles instead of 2/3/4 ([accuracy.md §3](accuracy.md#3-branch-instructions-report-the-wrong-cycle-count)).
- Page-crossing penalties on `abs,X` / `abs,Y` / `(zp),Y` are missing ([§4](accuracy.md#4-no-page-crossing-cycle-penalty-on-indexed-addressing)).

Both must be fixed. The page-cross one needs a small design change: cycle count is
currently a compile-time template argument, so an addressing mode has no way to say "I
crossed a page". Give the params structs a `bool crossed` that `init()`/address
resolution sets, and have `execute()` return `cycles + params.crossed`.

### Instruction-stepped vs cycle-stepped

`Processor::step()` runs a whole instruction and then charges all its cycles at once.
That is *instruction-stepped*. It is enough for a scanline-accurate PPU — run the CPU,
take the returned cycle count, advance the PPU by 3× that — and that is how most
"good enough to play games" emulators are built. Start there.

It is **not** enough for cycle-accurate timing, where a write must land on the exact
dot it happened. If you later want that, the change is to make the bus access itself
the unit of time: have `Bus::read`/`write` tick the PPU 3 dots on every access. That
works without restructuring `Processor`, because the instruction implementations
already funnel every memory access through the bus — a genuinely nice property of the
current design. What you lose is the ability to count cycles from the return value,
since the ticking happens inside the access.

Note also that the returned cycle count is currently a fixed number per opcode, so
read-modify-write instructions (`INC abs,X` and friends) do not model their dummy
read/write. That matters for a handful of NES titles, not most.

## 4. Undocumented opcodes

All 105 unassigned opcodes currently decode as a shared 2-cycle `NOP` with the wrong
length, so a 3-byte undocumented opcode leaves `PC` inside its own operands and
execution derails immediately. Commercial NES software does use these — `LAX`, `SAX`,
`DCP`, `ISC`, `SLO`, `RLA`, `SRE`, `RRA`, and the multi-byte `NOP`s cover nearly all
real-world use.

The existing template design makes this cheap: most illegal opcodes are just an
existing operation composed with an existing addressing mode, or two operations
combined. Add them as new `*_Impl` templates and fill in the table entries.

## Smaller things you will hit

- **Decimal mode.** The 2A03 has BCD disabled — `ADC`/`SBC` ignore the `D` flag,
  though `SED`/`CLD` still set and clear it. This core implements full BCD, so you need
  a compile-time or constructor switch to turn it off. (Everything else is a *missing*
  feature; this is the one place the core does *more* than the target hardware.)
- **Power-on RAM state.** `Memory` leaves its buffer uninitialised. Some games read RAM
  before writing it; give NES RAM a defined fill.
- **OAM DMA.** `$4014` copies 256 bytes and stalls the CPU for 513 or 514 cycles. It has
  no representation here — you will need a way for the bus to tell the processor "stall
  N cycles".
- **Thread safety.** The static instruction table holds mutable operand state in its
  `params` members ([architecture.md](architecture.md#consequences-worth-knowing)), and
  the run/stop flags are `volatile` rather than `std::atomic`. If you ever run the CPU
  on its own thread — which the debuggers already do — fix both. Moving `params` to a
  local inside `execute()` is a one-line change per instruction and removes the shared
  state entirely.
- **`I6502Emulator` is thin.** It resets, starts and stops. A NES needs a master clock
  driving CPU, PPU and APU in ratio; you will likely write your own top-level object
  and use `Processor` directly rather than extending this one.

## Suggested milestones

1. Fix `(zp,X)`; wire `6502_functional_test.bin` into the test suite as a regression gate.
2. Fix branch cycles and page-cross penalties; add `peek()` alongside `read()`.
3. Add IRQ/NMI to `Processor`; verify with a test ROM that uses `BRK`/`IRQ`.
4. Subclass `Bus` for NES decoding + mirroring; load an iNES file; implement mapper 0.
5. PPU background rendering driven at 3 dots per CPU cycle; NMI on vblank.
6. Controllers, sprites, sprite-zero hit.
7. Undocumented opcodes; APU.

Nestest (`nestest.nes`) is the usual next gate after the functional test — run it in
its automated mode from `$C000` and diff your CPU trace against the published log. It
exercises undocumented opcodes and exact cycle counts, so it will fail until steps 2
and 7 are done, but the diff tells you precisely where.
