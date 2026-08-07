# Accuracy

## Conformance against the functional test

`test/6502_functional_test.bin` is Klaus Dormann's 6502 functional test, built here
with `disable_decimal = 0` (decimal mode **is** tested) and `report = 0` (failures spin
on a self-branch). It loads flat at `$0000`, starts at `$0400`, and reaches
`JMP $3469` — a self-jump immediately followed by `JMP $0400` — on success.

Results, running the core headlessly and stopping when `PC` stops advancing:

| Build | Result |
| --- | --- |
| As committed | Traps at `$17AD`, after 44,327 instructions |
| With `(zp,X)` corrected | **Passes** — reaches `$3469` after 30,646,177 instructions / 84,663,981 cycles |

`$17AD` is the trap after this sequence:

```
17A5  81 30     STA ($30,X)
17A7  08        PHP
17A8  49 C3     EOR #$C3
17AA  D9 17 02  CMP $0217,Y
17AD  D0 FE     BNE *          ; trap
```

That is the indexed-indirect test. Correcting `IndirectXParams` alone is sufficient to
pass the entire suite — the ALU, every flag, the stack, BCD `ADC`/`SBC`, and all other
addressing modes are already right. **This core is one bug away from being functionally
complete for documented opcodes.**

## Defects

### 1. `(zp,X)` indexed-indirect resolves the wrong pointer

`src/parameters.h`, `IndirectXParams`. The hardware computes the pointer address as
`(zp + X) & 0xFF` and reads the 16-bit target from there. The implementation instead
reads the pointer from `zp` and adds `X` to the *resulting target address*:

```cpp
uint16 address = bus->read(data) | (bus->read(data+1) << 8);
return bus->read(address + regs->x);          // wrong: X applied to the target
```

That is `(zp),X` semantics, which is not a 6502 addressing mode. It affects all eight
`$x1` opcodes with `X` indexing: `ORA` `AND` `EOR` `ADC` `STA` `LDA` `CMP` `SBC`.

Verified: `LDA ($20,X)` with `X=4`, pointer `$3000` at `$24`, decoy pointer `$4000` at
`$20`, `$3000`=`$AA`, `$4004`=`$BB` — the core loads `$BB`.

The correct form also wraps the high-byte fetch inside the zero page:

```cpp
uint8 lo = data + regs->x;          // uint8 arithmetic wraps for free
uint8 hi = data + regs->x + 1;
uint16 address = bus->read(lo) | (bus->read(hi) << 8);
```

### 2. `(zp),Y` does not wrap its pointer inside the zero page

Same file, `IndirectYParams`. `data` is `uint8`, but `data+1` promotes to `int`, so
`LDA ($FF),Y` reads its high byte from `$0100` instead of `$0000`. Verified: the core
returns the wrong value. The same promotion bug is in `ZeroPageParams::get16bit` via
the `READ16` macro.

The functional test does not exercise this case, so it passes despite the bug.

### 3. Branch instructions report the wrong cycle count

`src/InstructionImpl.h`, the shared `branch()` helper. It returns only its local page-
cross adjustment `xc` and **discards the `cycles` template argument entirely**. It also
adds 2 for a page cross where the hardware adds 1.

| Case | Hardware | This core |
| --- | --- | --- |
| Not taken | 2 | 0 |
| Taken, same page | 3 | 0 |
| Taken, crossing a page | 4 | 2 |

All measured. Fixing it means returning `cycles + (taken ? 1 : 0) + (crossed ? 1 : 0)`
and changing the page-cross adjustment from `+= 2` to `+= 1`. (The opcode table also
lists `BEQ` as 3 cycles where every other branch is 2; once `branch()` uses the
argument, that entry should become 2.)

### 4. No page-crossing cycle penalty on indexed addressing

`abs,X`, `abs,Y` and `(zp),Y` cost one extra cycle on real hardware when the index
carries into a new page. The addressing structs compute `data + regs->x` without
reporting the carry, and the cycle count is a fixed template argument, so the penalty
cannot be expressed. Measured: `LDA $30FF,X` with `X=1` costs 4 cycles; hardware
charges 5.

To fix, an addressing mode needs to return a "crossed" flag that `execute()` can add to
its return value.

### 5. `JMP ($xxFF)` does not reproduce the hardware wrap bug

On a real 6502 the indirect vector's high byte is fetched from `$xx00`, not `$xx+1,00`.
`IndirectParams::get16bit` uses the plain `READ16` macro, so `JMP ($30FF)` reads
`$30FF`/`$3100`. Measured: the core jumps to `$5634` where hardware jumps to `$1234`.

Some real programs depend on this quirk, so "fixing" it means *adding* the bug.

### 6. No interrupts

There is no IRQ or NMI path at all — no pending-interrupt state, no line to assert, and
nothing that reads the `$FFFA`/`$FFFE` vectors except `BRK`. `RTI` is implemented and
correct, but nothing can generate the frame it returns from. The `I` flag is
maintained by `SEI`/`CLI` but never consulted.

This is the single largest gap for any real system. See
[nes-roadmap.md](nes-roadmap.md).

### 7. Undocumented opcodes decode as `NOP`

All 105 unassigned opcodes map to one shared 2-cycle `NOP`, with neither the correct
cycle count nor the correct instruction length — so an undocumented 3-byte opcode
leaves `PC` pointing into the middle of its own operands. Commercial NES software does
use some of these (`LAX`, `SAX`, `DCP`, `ISC`, `SLO`, `RLA`, `SRE`, `RRA`, and the
multi-byte `NOP`s).

### 8. Reset and flag-restore details

- `I6502Emulator::start()` zeroes the registers, sets `SP=$FD` and `SR=FLAG__`, but
  does not set the `I` flag; hardware leaves interrupts disabled after reset.
- `PLP` and `RTI` write the pulled byte to `SR` and force bit 5, but do not mask off
  bit 4 (`B`). `B` is not a real register bit — it only exists in pushed copies.
- Reset is charged 8 cycles; hardware takes 7.

## Memory safety

These are not accuracy issues but they will bite in a larger project.

- `Memory::~Memory` calls `delete _bytes` on a buffer from `new uint8[]`. Undefined
  behaviour; should be `delete[]`.
- `Memory`'s constructor leaves the buffer uninitialised, so reads before any write
  return garbage rather than a defined power-on pattern.
- `checkAddress()` is empty and `read`/`write` never bound-check, so any `Memory`
  smaller than 256 pages can be driven out of bounds by ordinary program execution.
- The bulk `write(uint8*, int, uint16)` / `read(uint8*, int, uint16)` overloads
  `memcpy` without checking `offset + length` against the buffer.
- `UnAsm::unasm_line(Bus*, uint16)` constructs a `Registers` and sets only `pc`; the
  other fields are read uninitialised (harmlessly today, but sanitisers will flag it).

## Build and portability

- **The Windows build is broken for everything but the library itself.**
  `emu6502_lib` is `SHARED` with no `__declspec(dllexport)` anywhere, so MSVC emits no
  import library and both `emu6502_test` and `emu6502_debugger` fail with `LNK1104:
  cannot open 'emu6502_lib.lib'`. Fix by setting `WINDOWS_EXPORT_ALL_SYMBOLS ON` on the
  target, adding an export macro, or making the library `STATIC`.
- **The Win32 debugger has a duplicate manifest.** `debugger_src/Resource.rc:33`
  embeds `Application.manifest` as an `RT_MANIFEST` resource while the linker generates
  one too, giving `CVT1100: duplicate resource ... MANIFEST` and then `LNK1123`.
  Adding `/MANIFEST:NO` to the target's link options resolves it; the `.manifest` entry
  in `DEBUGGER_MANIFEST` can stay. Verified: with this plus the export fix, all three
  targets build.
- `src/parameters.h` includes both `emu_Bus.h` and `emu_bus.h`. Only the lowercase file
  exists. This survives on Windows and on WSL's case-insensitive `drvfs` mount, but
  will fail to compile on a genuinely case-sensitive filesystem — including most CI
  containers.
- `CMakeLists.txt` never calls `enable_testing()`/`add_test()`, so `ctest` finds
  nothing; run `emu6502_test` directly.
- Both debuggers open `test/6502_functional_test.bin` by relative path and silently
  continue with an all-zero memory if it is missing.

## Suggested order of work

1. `(zp,X)` — one addressing mode, unlocks the full functional test as a regression gate.
2. Wire the functional test into `emu6502_test` so it stays passing.
3. `delete[]`, zero-init, and bounds checks in `Memory`.
4. Interrupts (IRQ/NMI + `I`-flag gating).
5. Branch cycles, then page-cross penalties.
6. `std::atomic` for the run/stop flags; move `params` out of the instruction objects.
7. Undocumented opcodes, `JMP ($xxFF)`, and the remaining flag details.
