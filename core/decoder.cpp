#include "decoder.h"

// Helper to sign-extend an immediate value
// given the value and the number of bits it occupies
static int32_t signExtend(uint32_t value, int bits) {
  uint32_t signBit = 1u << (bits - 1);
  if (value & signBit)
    return static_cast<int32_t>(value | (~0u << bits));
  return static_cast<int32_t>(value);
}

Instruction decode(uint32_t raw) {
  Instruction inst;
  inst.raw   = raw;
  inst.valid = true;

  // Every instruction has a 3-bit type in bits 31-29
  inst.type = static_cast<InstructionType>((raw >> 29) & 0x7);

  switch (inst.type) {

    case InstructionType::REG_REG:
      // Type | Opcode | funct3 | funct7 | rs1 | rs2 | rd
      // 31-29  28-25    24-22    21-15   14-10  9-5   4-0
      inst.opcode  = (raw >> 25) & 0xF;
      inst.funct3  = (raw >> 22) & 0x7;
      inst.funct7  = (raw >> 15) & 0x7F;
      inst.rs1     = (raw >> 10) & 0x1F;
      inst.rs2     = (raw >>  5) & 0x1F;
      inst.rd      = (raw >>  0) & 0x1F;
      inst.isFloat = (inst.opcode == 1); // Float arithmetic opcode

      // Validity check for opcode, funct3, and float registers
      switch (inst.opcode) {
        case 0: // Integer arithmetic
          if (inst.funct3 > 5) inst.valid = false;
          break;
        case 1: // Float arithmetic
          if (inst.funct3 > 4) inst.valid = false;
          if (inst.rs1 > 15 || inst.rs2 > 15 || inst.rd > 15) inst.valid = false;
          break;
        case 2: // Bitwise
          if (inst.funct3 > 3) inst.valid = false;
          break;
        case 3: // Compare
          if (inst.funct3 > 5) inst.valid = false;
          break;
        case 4: // Copy
          if (inst.funct3 > 0) inst.valid = false;
          break;
        default:
          inst.valid = false; // Invalid opcode for REG_REG type
      }
      break;

    case InstructionType::IMMEDIATE:
      // Type | Opcode | funct3 | Immediate | rs1 | rd
      // 31-29  28-25    24-22     21-10      9-5   4-0
      inst.opcode    = (raw >> 25) & 0xF;
      inst.funct3    = (raw >> 22) & 0x7;
      inst.immediate = signExtend((raw >> 10) & 0xFFF, 12);
      inst.rs1       = (raw >>  5) & 0x1F;
      inst.rd        = (raw >>  0) & 0x1F;

      // Validity check for opcode and funct3
      switch (inst.opcode) {
        case 0: // Integer arithmetic
          if (inst.funct3 > 4) inst.valid = false;
          break;
        case 1: // Shift
          if (inst.funct3 > 2) inst.valid = false;
          break;
        case 2: // Bitwise
          if (inst.funct3 > 2) inst.valid = false;
          break;
        case 3: // Compare
          if (inst.funct3 > 5) inst.valid = false;
          break;
        case 4: // JALR
          if (inst.funct3 > 0) inst.valid = false;
          break;
        default:
          inst.valid = false; // Invalid opcode for IMMEDIATE type
      }
      break;

    case InstructionType::LOAD_STORE:
      // Type | Opcode | funct3 | Unused | rs1 | rs2
      // 31-29  28-25    24-22    21-10    9-5   4-0
      inst.opcode  = (raw >> 25) & 0xF;
      inst.funct3  = (raw >> 22) & 0x7;
      inst.rs1     = (raw >>  5) & 0x1F;
      inst.rs2     = (raw >>  0) & 0x1F;
      inst.isFloat = (inst.opcode == 1);

      // Validity check for opcode and funct3
      if (inst.opcode > 1 || inst.funct3 > 1) {
        inst.valid = false;
      }
      // Validity check for float registers
      if (inst.isFloat && (inst.rs1 > 15 || inst.rs2 > 15)) {
        inst.valid = false;
      }
      break;

    case InstructionType::JUMP:
      // Type | Immediate | rd
      // 31-29   28-5      4-0
      inst.immediate = signExtend((raw >> 5) & 0xFFFFFF, 24);
      inst.rd        = (raw >> 0) & 0x1F;
      break;

    case InstructionType::BRANCH:
      // Type | funct3 | Immediate | rs1 | rs2
      // 31-29   28-26    25-10      9-5   4-0
      inst.funct3    = (raw >> 26) & 0x7;
      inst.immediate = signExtend((raw >> 10) & 0xFFFF, 16);
      inst.rs1       = (raw >>  5) & 0x1F;
      inst.rs2       = (raw >>  0) & 0x1F;

      // Validity check for funct3
      if (inst.funct3 > 5) {
        inst.valid = false;
      }
      break;

    case InstructionType::PUSH_POP:
      // Type | funct3 | Unused | rs1
      // 31-29   28-26   25-5    4-0
      inst.funct3 = (raw >> 26) & 0x7;
      inst.rs1    = (raw >>  0) & 0x1F;

      // Validity check for funct3
      if (inst.funct3 > 1) {
        inst.valid = false;
      }
      break;

    case InstructionType::MISC:
      // Type | Opcode | Unused
      // 31-29   28-25
      inst.opcode = (raw >> 25) & 0xF;

      // Validity check for opcode
      if (inst.opcode > 1) {
        inst.valid = false;
      }
      break;

    default:
      inst.valid = false;
      break;
  }

  return inst;
}