# Assembler and disassembler

`Asm` (`include/6502cc/asm.h`) is a one-pass, regex-driven assembler meant for building
small test programs in-line. It takes conventional 6502 operand syntax, but it is not a
general-purpose assembler: there are no labels, directives or expressions, and hex
literals have a strict width. For real programs use [ca65](https://cc65.github.io/) or
AS65 (which ships inside Klaus Dormann's
[test repository](https://github.com/Klaus2m5/6502_65C02_functional_tests) as
`as65_142.zip`).

Every behaviour below is covered by tests in `test_src/testAsm.cpp`.

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
| Indexed indirect | `($XX,x)` | `lda ($51,x)` | `A1 51` |
| Indirect indexed | `($XX),y` | `lda ($52),y` | `B1 52` |
| Relative | *signed decimal* | `bne +4` | `D0 04` |

## Constraints

### Hex literals must be exactly 2 or 4 digits

The digit count is what selects zero page vs absolute, so it is significant and
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

### Branch targets are signed decimal displacements, not labels

```
bne +4          ->  D0 04
beq -4          ->  F0 FC
bcc 0           ->  D0 00
bmi $04         ->  Syntax error   (branches do not take hex)
```

The displacement is relative to the instruction *after* the branch, matching the
hardware. Since there are no labels, you count the bytes yourself.

### No labels, directives or expressions

`loop: lda #$01` is a syntax error. There are no `.byte`/`.word`/`.org` directives, no
symbolic constants, and no arithmetic.

### Accumulator mode needs the explicit operand

`asl a` assembles; bare `asl` is a syntax error. Many real assemblers accept both.

### Only real addressing modes are accepted

`($nn),X` and `($nn,Y)` are not 6502 addressing modes and are rejected, as are
unbalanced parentheses:

```
lda ($51),x     ->  Syntax error
lda ($51,y)     ->  Syntax error
lda ($51,x      ->  Syntax error
```

## Disassembler

`UnAsm` (`include/6502cc/unasm.h`) decodes opcodes from their `aaabbbcc` bit fields
rather than from a table, which keeps it compact but means it reconstructs addressing
modes structurally. Its output uses the same conventional syntax the assembler accepts.

```cpp
UnAsm un;
std::string s = un.unasm_line(&bus, 0x0400);   // does not move anything
```

Two overloads:

- `unasm_line(Bus*, uint16 at)` — disassembles at `at`, leaves nothing modified. Use it
  for random access. It cannot tell you the instruction's length, so you cannot walk
  forward with it.
- `unasm_line(Bus*, Registers*)` — **advances `regs->pc`** past the decoded
  instruction. This is the one to use for a scrolling listing; pass a scratch
  `Registers` copy so you do not disturb the CPU.

All 256 opcodes are named, undocumented ones included (`SLO`, `LAX`, `KIL`, `SHY`, and
the rest). That matters for more than readability: a listing walks forward by asking the
disassembler how long each instruction is, so an unnamed opcode would desync the whole
view. A test (`disassembler_lengths_match_execution`) checks every opcode's decoded
length against what the processor actually consumes, excluding only the instructions
that deliberately move `PC` elsewhere.

Note that the assembler does **not** accept the undocumented mnemonics — the
disassembler can read them, but you cannot write them back.
