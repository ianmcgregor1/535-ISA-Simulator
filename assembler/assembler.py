#!/usr/bin/env python3
"""
GIA Assembler — Two-pass assembler for the GIA ISA.
Outputs a .hex file with one 32-bit word per line (0x prefixed, uppercase).

Assembly syntax:
  [label]  mnemonic  [operands...]   # optional comment

  - Tokens are separated by spaces.
  - Labels are plain identifiers (no colon), on the same line as an instruction
    or alone on their own line.
  - Registers:   x0-x31 (integer/special),  f0-f15 (float)
  - Immediates:  signed decimal integers
  - Comments:    # to end of line

Pseudo-ops:
  .loc <addr>              Set the current address counter to <addr>.
  .dataInt <value>         Store a 32-bit signed integer word.
  .dataFloat <value>       Store a 32-bit IEEE 754 float word.
  <label> .var <value>     Define <label> at the current address and store <value>.

Instruction operand formats (operands left-to-right, space-separated):
  RRR    rd rs1 rs2           e.g.  ADD x4 x5 x6
  RR     rd rs1               e.g.  NEG x4 x5         (also FNEG, NOT, CPY)
  RRI    rd rs1 imm           e.g.  ADDI x4 x5 10
  JALR   rd rs1 imm           e.g.  JALR x3 x5 0
  LOAD   dest_reg addr_reg    e.g.  LW x4 x5          (x4 = *x5)
  STORE  addr_reg src_reg     e.g.  SW x5 x4          (*x5 = x4)
  JUMP   rd imm|label         e.g.  J x3 myloop       (PC-relative offset)
  BRANCH rs1 rs2 imm|label    e.g.  BEQ x4 x5 myloop (PC-relative offset)
  PUSH   rs1                  e.g.  PUSH x6
  POP    rd                   e.g.  POP x6
  NONE   (no operands)        e.g.  HLT
"""

import sys
import os
import struct


# Instruction type constants
TYPE_REG_REG    = 0b000
TYPE_IMMEDIATE  = 0b001
TYPE_LOAD_STORE = 0b010
TYPE_JUMP       = 0b011
TYPE_BRANCH     = 0b100
TYPE_PUSH_POP   = 0b101
TYPE_MISC       = 0b111


# Mnemonic table
MNEMONICS = {
    # ── REG_REG  Integer Arithmetic  (opcode 0) ──────────────────────────────
    'ADD':   (TYPE_REG_REG,    0, 0, 'RRR'),
    'SUB':   (TYPE_REG_REG,    0, 1, 'RRR'),
    'NEG':   (TYPE_REG_REG,    0, 2, 'RR'),   # rd = -rs1  (proposal says rd=-rd)
    'MUL':   (TYPE_REG_REG,    0, 3, 'RRR'),
    'DIV':   (TYPE_REG_REG,    0, 4, 'RRR'),
    'REM':   (TYPE_REG_REG,    0, 5, 'RRR'),
    # ── REG_REG  Float Arithmetic  (opcode 1) ────────────────────────────────
    'FADD':  (TYPE_REG_REG,    1, 0, 'RRR'),
    'FSUB':  (TYPE_REG_REG,    1, 1, 'RRR'),
    'FNEG':  (TYPE_REG_REG,    1, 2, 'RR'),   # rd = -rs1  (proposal says rd=-rd)
    'FMUL':  (TYPE_REG_REG,    1, 3, 'RRR'),
    'FDIV':  (TYPE_REG_REG,    1, 4, 'RRR'),
    # ── REG_REG  Bitwise  (opcode 2) ─────────────────────────────────────────
    'AND':   (TYPE_REG_REG,    2, 0, 'RRR'),
    'OR':    (TYPE_REG_REG,    2, 1, 'RRR'),
    'NOT':   (TYPE_REG_REG,    2, 2, 'RR'),   # rd = ~rs1  (proposal says rd=!rd)
    'XOR':   (TYPE_REG_REG,    2, 3, 'RRR'),
    # ── REG_REG  Compare and Store  (opcode 3) ───────────────────────────────
    'SEQ':   (TYPE_REG_REG,    3, 0, 'RRR'),
    'SNEQ':  (TYPE_REG_REG,    3, 1, 'RRR'),
    'SLT':   (TYPE_REG_REG,    3, 2, 'RRR'),
    'SGT':   (TYPE_REG_REG,    3, 3, 'RRR'),
    'SLE':   (TYPE_REG_REG,    3, 4, 'RRR'),
    'SGE':   (TYPE_REG_REG,    3, 5, 'RRR'),
    # ── REG_REG  Copy  (opcode 4) ────────────────────────────────────────────
    'CPY':   (TYPE_REG_REG,    4, 0, 'RR'),
    # ── IMMEDIATE  Integer Arithmetic  (opcode 0) ────────────────────────────
    'ADDI':  (TYPE_IMMEDIATE,  0, 0, 'RRI'),
    'SUBI':  (TYPE_IMMEDIATE,  0, 1, 'RRI'),
    'MULI':  (TYPE_IMMEDIATE,  0, 2, 'RRI'),
    'DIVI':  (TYPE_IMMEDIATE,  0, 3, 'RRI'),
    'REMI':  (TYPE_IMMEDIATE,  0, 4, 'RRI'),
    # ── IMMEDIATE  Shift  (opcode 1) ─────────────────────────────────────────
    'SLLI':  (TYPE_IMMEDIATE,  1, 0, 'RRI'),
    'SRLI':  (TYPE_IMMEDIATE,  1, 1, 'RRI'),
    'SRAI':  (TYPE_IMMEDIATE,  1, 2, 'RRI'),
    # ── IMMEDIATE  Bitwise  (opcode 2) ───────────────────────────────────────
    'ANDI':  (TYPE_IMMEDIATE,  2, 0, 'RRI'),
    'ORI':   (TYPE_IMMEDIATE,  2, 1, 'RRI'),
    'XORI':  (TYPE_IMMEDIATE,  2, 2, 'RRI'),
    # ── IMMEDIATE  Compare and Store  (opcode 3) ─────────────────────────────
    'SEQI':  (TYPE_IMMEDIATE,  3, 0, 'RRI'),
    'SNEQI': (TYPE_IMMEDIATE,  3, 1, 'RRI'),
    'SLTI':  (TYPE_IMMEDIATE,  3, 2, 'RRI'),
    'SGTI':  (TYPE_IMMEDIATE,  3, 3, 'RRI'),
    'SLEI':  (TYPE_IMMEDIATE,  3, 4, 'RRI'),
    'SGEI':  (TYPE_IMMEDIATE,  3, 5, 'RRI'),
    # ── IMMEDIATE  JALR  (opcode 4) ──────────────────────────────────────────
    'JALR':  (TYPE_IMMEDIATE,  4, 0, 'JALR'),
    # ── LOAD_STORE  Integer  (opcode 0) ──────────────────────────────────────
    'LW':    (TYPE_LOAD_STORE, 0, 0, 'LOAD'),
    'SW':    (TYPE_LOAD_STORE, 0, 1, 'STORE'),
    # ── LOAD_STORE  Float  (opcode 1) ────────────────────────────────────────
    'LWF':   (TYPE_LOAD_STORE, 1, 0, 'LOAD'),
    'SWF':   (TYPE_LOAD_STORE, 1, 1, 'STORE'),
    # ── JUMP ─────────────────────────────────────────────────────────────────
    'J':     (TYPE_JUMP,       0, 0, 'JUMP'),
    # ── BRANCH ───────────────────────────────────────────────────────────────
    'BEQ':   (TYPE_BRANCH,     0, 0, 'BRANCH'),
    'BNEQ':  (TYPE_BRANCH,     0, 1, 'BRANCH'),
    'BLT':   (TYPE_BRANCH,     0, 2, 'BRANCH'),
    'BGT':   (TYPE_BRANCH,     0, 3, 'BRANCH'),
    'BLE':   (TYPE_BRANCH,     0, 4, 'BRANCH'),
    'BGE':   (TYPE_BRANCH,     0, 5, 'BRANCH'),
    # ── PUSH / POP ───────────────────────────────────────────────────────────
    'PUSH':  (TYPE_PUSH_POP,   0, 0, 'PUSH'),
    'POP':   (TYPE_PUSH_POP,   0, 1, 'POP'),
    # ── MISC ─────────────────────────────────────────────────────────────────
    'HLT':   (TYPE_MISC,       0, 0, 'NONE'),
    'RET':   (TYPE_MISC,       1, 0, 'NONE'),
}

# Pseudo-op keywords (checked before label detection)
PSEUDO_OPS = {'.loc', '.dataInt', '.dataFloat', '.var'}


# Error class 
class AssemblyError(Exception):
    def __init__(self, line_num, message):
        self.line_num = line_num
        self.message  = message
        super().__init__(f"Line {line_num}: {message}")


# Register parsing

def parse_int_reg(token, line_num):
    """Parse an integer register (x0-x31). Returns the register number."""
    if not token.startswith('x'):
        raise AssemblyError(line_num,
            f"Expected integer register (x0-x31), got '{token}'")
    try:
        n = int(token[1:])
    except ValueError:
        raise AssemblyError(line_num, f"Invalid register '{token}'")
    if n < 0 or n > 31:
        raise AssemblyError(line_num,
            f"Register '{token}' out of range (x0-x31)")
    return n

def parse_float_reg(token, line_num):
    """Parse a float register (f0-f15). Returns the register number."""
    if not token.startswith('f'):
        raise AssemblyError(line_num,
            f"Expected float register (f0-f15), got '{token}'")
    try:
        n = int(token[1:])
    except ValueError:
        raise AssemblyError(line_num, f"Invalid register '{token}'")
    if n < 0 or n > 15:
        raise AssemblyError(line_num,
            f"Register '{token}' out of range (f0-f15)")
    return n

def parse_reg(token, line_num, is_float):
    """Dispatch to int or float register parser based on is_float."""
    return parse_float_reg(token, line_num) if is_float \
           else parse_int_reg(token, line_num)

def parse_immediate(token, line_num):
    """Parse a decimal integer. Returns int."""
    try:
        return int(token, 0)
    except ValueError:
        raise AssemblyError(line_num,
            f"Expected decimal integer immediate, got '{token}'")


# Bit-field helpers

def mask(value, bits):
    """Truncate value to 'bits' bits (two's-complement for negatives)."""
    return int(value) & ((1 << bits) - 1)

def check_range(value, bits, line_num, context):
    """Raise AssemblyError if value doesn't fit in a signed 'bits'-bit field."""
    lo = -(1 << (bits - 1))
    hi =  (1 << (bits - 1)) - 1
    if value < lo or value > hi:
        raise AssemblyError(line_num,
            f"{context}: immediate {value} out of range for {bits}-bit signed "
            f"field [{lo}, {hi}]")

def float_to_bits(f):
    """Return the 32-bit IEEE 754 bit pattern for float f."""
    return struct.unpack('I', struct.pack('f', float(f)))[0]


# Instruction encoders

def enc_reg_reg(inst_type, opcode, funct3, rs1, rs2, rd):
    """
    Bits 31-29: type  (3)
    Bits 28-25: opcode (4)
    Bits 24-22: funct3 (3)
    Bits 21-15: funct7 (7)
    Bits 14-10: rs1    (5)
    Bits  9- 5: rs2    (5)
    Bits  4- 0: rd     (5)
    """
    w  = mask(inst_type, 3) << 29
    w |= mask(opcode,    4) << 25
    w |= mask(funct3,    3) << 22
    # funct7 bits 21-15 left as 0
    w |= mask(rs1,       5) << 10
    w |= mask(rs2,       5) << 5
    w |= mask(rd,        5) << 0
    return w

def enc_immediate(inst_type, opcode, funct3, imm, rs1, rd):
    """
    Bits 31-29: type      (3)
    Bits 28-25: opcode    (4)
    Bits 24-22: funct3    (3)
    Bits 21-10: immediate (12)
    Bits  9- 5: rs1       (5)
    Bits  4- 0: rd        (5)
    """
    w  = mask(inst_type, 3) << 29
    w |= mask(opcode,    4) << 25
    w |= mask(funct3,    3) << 22
    w |= mask(imm,      12) << 10
    w |= mask(rs1,       5) << 5
    w |= mask(rd,        5) << 0
    return w

def enc_load_store(inst_type, opcode, funct3, rs1, rs2):
    """
    Bits 31-29: type   (3)
    Bits 28-25: opcode (4)
    Bits 24-22: funct3 (3)
    Bits 21-10: unused (12) — left as 0
    Bits  9- 5: rs1    (5)   address register
    Bits  4- 0: rs2    (5)   data register
    """
    w  = mask(inst_type, 3) << 29
    w |= mask(opcode,    4) << 25
    w |= mask(funct3,    3) << 22
    # bits 21-10 left as 0
    w |= mask(rs1,       5) << 5
    w |= mask(rs2,       5) << 0
    return w

def enc_jump(imm, rd):
    """
    Bits 31-29: type      (3) = 011
    Bits 28- 5: immediate (24)
    Bits  4- 0: rd        (5)
    """
    w  = mask(TYPE_JUMP, 3) << 29
    w |= mask(imm,      24) << 5
    w |= mask(rd,        5) << 0
    return w

def enc_branch(funct3, imm, rs1, rs2):
    """
    Bits 31-29: type   (3) = 100
    Bits 28-26: funct3 (3)
    Bits 25-10: imm    (16)
    Bits  9- 5: rs1    (5)
    Bits  4- 0: rs2    (5)
    """
    w  = mask(TYPE_BRANCH, 3) << 29
    w |= mask(funct3,      3) << 26
    w |= mask(imm,        16) << 10
    w |= mask(rs1,         5) << 5
    w |= mask(rs2,         5) << 0
    return w

def enc_push_pop(funct3, reg):
    """
    Bits 31-29: type   (3) = 101
    Bits 28-26: funct3 (3)
    Bits 25- 5: unused (21) — left as 0
    Bits  4- 0: reg    (5)   rs1 for PUSH, rd for POP
    """
    w  = mask(TYPE_PUSH_POP, 3) << 29
    w |= mask(funct3,        3) << 26
    # bits 25-5 left as 0
    w |= mask(reg,           5) << 0
    return w

def enc_misc(opcode):
    """
    Bits 31-29: type   (3) = 111
    Bits 28-25: opcode (4)
    Bits 24- 0: unused (25) — left as 0
    """
    w  = mask(TYPE_MISC, 3) << 29
    w |= mask(opcode,    4) << 25
    return w


# Tokenizer
def tokenize(line):
    """Strip inline comments and split into whitespace-separated tokens."""
    if '#' in line:
        line = line[:line.index('#')]
    return line.replace(',', '').split()


# First pass — build symbol table

def first_pass(lines):
    """
    Scan every line, track the address counter, and record label → address.
    Returns (symbol_table dict, list of error strings).
    """
    symbol_table = {}
    errors       = []
    loc          = 0  # current word address

    for line_num, raw in enumerate(lines, start=1):
        tokens = tokenize(raw)
        if not tokens:
            continue

        idx = 0

        # Detect a leading label 
        # A token is a label if it is not a mnemonic (case-insensitive) and not a pseudo-op keyword.
        if tokens[idx].upper() not in MNEMONICS and tokens[idx] not in PSEUDO_OPS:
            label = tokens[idx]
            if label in symbol_table:
                errors.append(f"Line {line_num}: Duplicate symbol '{label}'")
            else:
                symbol_table[label] = loc
            idx += 1
            if idx >= len(tokens):
                continue  # label-only line; location does not advance

        if idx >= len(tokens):
            continue

        directive = tokens[idx]

        # Pseudo-ops 
        if directive == '.loc':
            if idx + 1 >= len(tokens):
                errors.append(f"Line {line_num}: .loc requires an address")
                continue
            try:
                loc = int(tokens[idx + 1], 0)
            except ValueError:
                errors.append(f"Line {line_num}: .loc address must be an integer")
            # .loc itself occupies no word

        elif directive in ('.dataInt', '.dataFloat', '.var'):
            loc += 1  # each stores exactly one word

        elif directive.upper() in MNEMONICS:
            loc += 1  # every instruction is one word

        else:
            errors.append(
                f"Line {line_num}: Unknown mnemonic or directive '{directive}'")

    return symbol_table, errors


# Second pass — encode instructions and data words

def resolve(token, symbol_table, line_num, current_pc, pc_relative):
    """
    Resolve a token to an integer:
      - If it names a symbol, return its address (pc_relative=True → address - current_pc).
      - Otherwise parse it as a decimal integer literal.
    """
    if token in symbol_table:
        addr = symbol_table[token]
        return (addr - current_pc) if pc_relative else addr
    return parse_immediate(token, line_num)


def encode_instruction(mnemonic, inst_type, opcode, funct3, fmt,
                       operands, symbol_table, current_pc, line_num):
    """
    Build and return the 32-bit encoded word for one instruction.
    Raises AssemblyError on any operand problem.
    """
    # Float instructions: REG_REG opcode 1, or LOAD_STORE opcode 1
    is_float = (inst_type == TYPE_REG_REG    and opcode == 1) or \
               (inst_type == TYPE_LOAD_STORE and opcode == 1)

    def need(n):
        if len(operands) != n:
            raise AssemblyError(line_num,
                f"'{mnemonic}' expects {n} operand(s), got {len(operands)}")

    def ireg(t):  return parse_int_reg(t, line_num)
    def freg(t):  return parse_float_reg(t, line_num)
    def areg(t):  return parse_reg(t, line_num, is_float)

    if fmt == 'RRR':
        need(3)
        rd  = areg(operands[0])
        rs1 = areg(operands[1])
        rs2 = areg(operands[2])
        return enc_reg_reg(inst_type, opcode, funct3, rs1, rs2, rd)

    elif fmt == 'RR':
        # rd, rs1  — rs2 unused (encoded as 0)
        need(2)
        rd  = areg(operands[0])
        rs1 = areg(operands[1])
        return enc_reg_reg(inst_type, opcode, funct3, rs1, 0, rd)

    elif fmt in ('RRI', 'JALR'):
        # rd, rs1, imm  — JALR has the same field layout as RRI
        need(3)
        rd  = ireg(operands[0])
        rs1 = ireg(operands[1])
        imm = parse_immediate(operands[2], line_num)
        check_range(imm, 12, line_num, mnemonic)
        return enc_immediate(inst_type, opcode, funct3, imm, rs1, rd)

    elif fmt == 'LOAD':
        # dest_reg  addr_reg   →   rs2 = *rs1
        need(2)
        rs2 = areg(operands[0])   # destination
        rs1 = ireg(operands[1])   # address
        return enc_load_store(inst_type, opcode, funct3, rs1, rs2)

    elif fmt == 'STORE':
        # addr_reg  src_reg    →   *rs1 = rs2
        need(2)
        rs1 = ireg(operands[0])   # address
        rs2 = areg(operands[1])   # source value
        return enc_load_store(inst_type, opcode, funct3, rs1, rs2)

    elif fmt == 'JUMP':
        # rd  imm|label   (PC-relative: pc += imm)
        need(2)
        rd  = ireg(operands[0])
        imm = resolve(operands[1], symbol_table, line_num, current_pc,
                      pc_relative=True)
        check_range(imm, 24, line_num, mnemonic)
        return enc_jump(imm, rd)

    elif fmt == 'BRANCH':
        # rs1  rs2  imm|label   (PC-relative: if cond: pc += imm)
        need(3)
        rs1 = ireg(operands[0])
        rs2 = ireg(operands[1])
        imm = resolve(operands[2], symbol_table, line_num, current_pc,
                      pc_relative=True)
        check_range(imm, 16, line_num, mnemonic)
        return enc_branch(funct3, imm, rs1, rs2)

    elif fmt == 'PUSH':
        need(1)
        rs1 = ireg(operands[0])
        return enc_push_pop(funct3, rs1)

    elif fmt == 'POP':
        need(1)
        rd = ireg(operands[0])
        return enc_push_pop(funct3, rd)

    elif fmt == 'NONE':
        need(0)
        return enc_misc(opcode)

    else:
        raise AssemblyError(line_num, f"Internal: unknown format '{fmt}'")


def second_pass(lines, symbol_table):
    """
    Walk all lines a second time and emit (address, word) pairs.
    Returns (output list, list of error strings).
    """
    output = []   # list of (address: int, word: int)
    errors = []
    loc    = 0

    for line_num, raw in enumerate(lines, start=1):
        tokens = tokenize(raw)
        if not tokens:
            continue

        idx = 0

        # Skip leading label (already recorded in first pass)
        if tokens[idx].upper() not in MNEMONICS and tokens[idx] not in PSEUDO_OPS:
            idx += 1
            if idx >= len(tokens):
                continue

        if idx >= len(tokens):
            continue

        directive = tokens[idx]
        rest      = tokens[idx + 1:]   # tokens after the mnemonic/directive

        # Pseudo-ops 

        if directive == '.loc':
            if rest:
                try:
                    loc = int(rest[0], 0)
                except ValueError:
                    errors.append(
                        f"Line {line_num}: .loc address must be an integer")
            else:
                errors.append(f"Line {line_num}: .loc requires an address")
            continue

        if directive == '.dataInt':
            if not rest:
                errors.append(f"Line {line_num}: .dataInt requires a value")
                continue
            try:
                word = mask(int(rest[0], 0), 32)
                output.append((loc, word))
                loc += 1
            except ValueError:
                errors.append(
                    f"Line {line_num}: .dataInt value must be an integer")
            continue

        if directive == '.dataFloat':
            if not rest:
                errors.append(f"Line {line_num}: .dataFloat requires a value")
                continue
            try:
                word = float_to_bits(rest[0])
                output.append((loc, word))
                loc += 1
            except ValueError:
                errors.append(
                    f"Line {line_num}: .dataFloat value must be a number")
            continue

        if directive == '.var':
            # <label> .var <initial_value>  — label already in symbol table
            if not rest:
                errors.append(f"Line {line_num}: .var requires an initial value")
                continue
            try:
                word = mask(int(rest[0], 0), 32)
                output.append((loc, word))
                loc += 1
            except ValueError:
                errors.append(
                    f"Line {line_num}: .var initial value must be an integer")
            continue

        # ── Instructions ──────────────────────────────────────────────────────

        mnemonic = directive.upper()
        if mnemonic not in MNEMONICS:
            errors.append(f"Line {line_num}: Unknown mnemonic '{directive}'")
            loc += 1
            continue

        inst_type, opcode, funct3, fmt = MNEMONICS[mnemonic]

        try:
            word = encode_instruction(mnemonic, inst_type, opcode, funct3, fmt,
                                      rest, symbol_table, loc, line_num)
            output.append((loc, word))
        except AssemblyError as e:
            errors.append(str(e))

        loc += 1

    return output, errors


# Top-level assembler

def assemble(input_path, output_path):
    with open(input_path, 'r') as f:
        lines = f.readlines()

    print(f"Assembling '{input_path}' ...")

    # First pass 
    symbol_table, errors = first_pass(lines)
    if errors:
        print("\nErrors in first pass:", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        sys.exit(1)

    if symbol_table:
        print(f"\n  Symbol table ({len(symbol_table)} entries):")
        for sym, addr in sorted(symbol_table.items(), key=lambda kv: kv[1]):
            print(f"    {sym:<24s}  addr {addr}")
    else:
        print("  (No symbols defined)")

    # Second pass
    output, errors = second_pass(lines, symbol_table)
    if errors:
        print("\nErrors in second pass:", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        sys.exit(1)

    if not output:
        print("\n  Warning: no output words generated.")
        return

    # Write .hex
    # Sort by address. Fill any gaps between non-contiguous segments with 0.
    output.sort(key=lambda kv: kv[0])
    max_addr = output[-1][0]
    word_map = {addr: word for addr, word in output}

    with open(output_path, 'w') as f:
        for addr in range(max_addr + 1):
            word = word_map.get(addr, 0x00000000)
            f.write(f"0x{word:08X}\n")

    total = max_addr + 1
    print(f"\n  Wrote {total} word(s) to '{output_path}'")
    print("Assembly complete.")


# Entry point
def main():
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <input.asm> [output.hex]")
        sys.exit(1)

    input_path  = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) >= 3 \
                  else os.path.splitext(input_path)[0] + '.hex'

    assemble(input_path, output_path)


if __name__ == '__main__':
    main()