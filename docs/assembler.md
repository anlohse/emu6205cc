# Assembler and disassembler

`Asm` (`include/6502cc/asm.h`) is a one-pass, regex-driven assembler meant for building
small test programs in-line. It is not a general-purpose 6502 assembler, and its operand
syntax differs from the conventional one in ways that will silently produce the wrong
opcode if you assume otherwise. For real programs use AS65 (bundled in `test/`) or
ca65.

Every behaviour below was verified against the current build.

## Output shape

`compile()` always returns a **65536-byte** vector, regardless of how much you wrote:

```cpp
Asm as(0x0400);                       // base address
std::vector<uint8> image = as.compile("lda #$01\n");
```

- The image is pre-filled with `$FF`.
- Code is emitted starting at the base address passed to the constructor.
- The reset vector at `$FFFC`/`$FFFD` is set to the base address for you.

So the result can be handed straight to `Memory::write(image.data(), 0x10000, 0)` and
reset into. There is no way to get just the emitted bytes, and no `.org` directive —
one `Asm` instance emits one contiguous run from one base address.

## Grammar

One instruction per line. Mnemonics are case-insensitive. `;` starts a comment; blank
and comment-only lines are skipped.

Anything that does not match is an `asm_syntax_exception` carrying the 1-based line
number (`"Syntax error at line: 2"`).

### Operand forms

| Mode | Syntax | Example | Emits |
| --- | --- | --- | --- |
| Implied | *(none)* | `nop` | `EA` |
| Accumulator | `a` | `asl a` | `0A` |
| Immediate | `#$XX` | `lda #$01` | `A9 01` |
| Zero page | `$XX` | `lda $12` | `A5 12` |
| Zero page,X | `$XX,x` | `lda $12,x` | `B5 12` |
| Zero page,Y | `$XX,y` | `ldx $12,y` | `B6 12` |
| Absolute | `$XXXX` | `lda $1234` | `AD 34 12` |
| Absolute,X | `$XXXX,x` | `lda $1234,x` | `BD 34 12` |
| Absolute,Y | `$XXXX,y` | `lda $1234,y` | `B9 34 12` |
| Indirect | `($XXXX)` | `jmp ($1234)` | `6C 34 12` |
| **Indexed indirect** | `($XX),x` | `lda ($12),x` | `A1 12` |
| Indirect indexed | `($XX),y` | `lda ($12),y` | `B1 12` |
| Relative | *signed decimal* | `bne +4` | `D0 00` — see below |

## Things that will catch you out

### Indexed indirect is written `($nn),x`, not `($nn,X)`

This is the big one. Conventional 6502 assembly writes indexed-indirect as `LDA
($12,X)` — the index inside the parentheses, because `X` is added to the *pointer*.
This assembler's regex closes the parenthesis before the comma, so it requires
`lda ($12),x`, and the conventional form is a syntax error.

```
lda ($12),x     ->  A1 12     (opcode for LDA (zp,X))
lda ($12,x)     ->  Syntax error
```

The disassembler prints the same non-standard form, so the two are self-consistent —
but source written for this assembler will not assemble elsewhere, and vice versa.
Note also that the runtime currently *implements* `$A1` as if it really were
`(zp),X`; see [accuracy.md](accuracy.md#1-zpx-indexed-indirect-resolves-the-wrong-pointer).

### Hex literals must be exactly 2 or 4 digits

The digit count is what selects zero-page vs absolute, so it is significant and
strict. Anything else is a syntax error:

```
lda $12         ->  A5 12       zero page
lda $1234       ->  AD 34 12    absolute
lda $1          ->  Syntax error
lda $123        ->  Syntax error
lda #$1         ->  Syntax error
```

There is no way to force `lda $0012` to assemble as absolute, and no decimal, binary or
character literals — `lda #1` and `lda #%00000001` are both errors.

### Branch offsets are parsed and then discarded

Relative operands are written as signed decimal (`bne +4`, `bne -4`, `bne 4`; hex
`bne $04` is an error). The operand byte is **always emitted as `00`**:

```
bne +4          ->  D0 00
bne -4          ->  D0 00
```

The cause is in `Asm::compile`: the value is read from the hex capture group, which is
empty for a relative operand, so it short-circuits to `0` before ever consulting the
decimal group. Every branch assembles as "branch to the next instruction". Hand-patch
the operand byte, or emit branches as raw bytes, until this is fixed.

### No labels, no directives, no expressions

`loop: lda #$01` is a syntax error. There are no `.byte`/`.word`/`.org` directives, no
symbolic constants, and no arithmetic. Combined with the branch-offset bug, this means
control flow has to be assembled by hand.

### Accumulator mode needs the explicit operand

`asl a` assembles; bare `asl` is a syntax error. Real assemblers accept both.

## Disassembler

`UnAsm` (`include/6502cc/unasm.h`) decodes opcodes from their `aaabbbcc` bit fields
rather than from a table, which keeps it compact but means it reconstructs addressing
modes structurally.

```cpp
UnAsm un;
std::string s = un.unasm_line(&bus, 0x0400);   // does not move anything
```

Two overloads:

- `unasm_line(Bus*, uint16 at)` — disassembles at `at`, leaves nothing modified. Use it
  for random access. It cannot tell you the instruction length, so you cannot walk
  forward with it.
- `unasm_line(Bus*, Registers*)` — **advances `regs->pc`** past the decoded
  instruction. This is the one to use for a scrolling listing; pass a scratch
  `Registers` copy so you do not disturb the CPU.

Unassigned opcodes render as a bare `$XX` byte. Output uses the same non-standard
`($nn), X` form as the assembler.
