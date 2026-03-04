#include "instruction.h"
#include <sstream>

std::string Instruction::getCommonName() const {
  if (!valid) return "NOP";

  // No need for validity checks since validity is set at decode time
  switch (type) {
    case InstructionType::REG_REG:
      switch (static_cast<RegRegOpcode>(opcode)) {
        case RegRegOpcode::INT_ARITHMETIC:
          switch (funct3) {
            case 0: return "ADD";
            case 1: return "SUB";
            case 2: return "NEG";
            case 3: return "MUL";
            case 4: return "DIV";
            case 5: return "REM";
          }
        case RegRegOpcode::FLOAT_ARITHMETIC:
          switch (funct3) {
            case 0: return "FADD";
            case 1: return "FSUB";
            case 2: return "FNEG";
            case 3: return "FMUL";
            case 4: return "FDIV";
          }
        case RegRegOpcode::BITWISE:
          switch (funct3) {
            case 0: return "AND";
            case 1: return "OR";
            case 2: return "NOT";
            case 3: return "XOR";
          }
        case RegRegOpcode::COMPARE:
          switch (funct3) {
            case 0: return "SEQ";
            case 1: return "SNEQ";
            case 2: return "SLT";
            case 3: return "SGT";
            case 4: return "SLE";
            case 5: return "SGE";
          }
        case RegRegOpcode::COPY: return "CPY";
      }

    case InstructionType::IMMEDIATE:
      switch (static_cast<ImmediateOpcode>(opcode)) {
        case ImmediateOpcode::INT_ARITHMETIC:
          switch (funct3) {
            case 0: return "ADDI";
            case 1: return "SUBI";
            case 2: return "MULI";
            case 3: return "DIVI";
            case 4: return "REMI";
          }
        case ImmediateOpcode::SHIFT:
          switch (funct3) {
            case 0: return "SLLI";
            case 1: return "SRLI";
            case 2: return "SRAI";
          }
        case ImmediateOpcode::BITWISE:
          switch (funct3) {
            case 0: return "ANDI";
            case 1: return "ORI";
            case 2: return "XORI";
          }
        case ImmediateOpcode::COMPARE:
          switch (funct3) {
            case 0: return "SEQI";
            case 1: return "SNEQI";
            case 2: return "SLTI";
            case 3: return "SGTI";
            case 4: return "SLEI";
            case 5: return "SGEI";
          }
        case ImmediateOpcode::JALR: return "JALR";
      }

    case InstructionType::LOAD_STORE:
      switch (static_cast<LoadStoreOpcode>(opcode)) {
        case LoadStoreOpcode::INT_LOADSTORE:
          return (funct3 == 0) ? "LW" : "SW";
        case LoadStoreOpcode::FLOAT_LOADSTORE:
          return (funct3 == 0) ? "LWF" : "SWF";
      }

    case InstructionType::JUMP:   return "J";

    case InstructionType::BRANCH:
      switch (funct3) {
        case 0: return "BEQ";
        case 1: return "BNEQ";
        case 2: return "BLT";
        case 3: return "BGT";
        case 4: return "BLE";
        case 5: return "BGE";
      }

    case InstructionType::PUSH_POP:
      return (funct3 == 0) ? "PUSH" : "POP";

    case InstructionType::MISC:
      switch (static_cast<MiscOpcode>(opcode)) {
        case MiscOpcode::HLT: return "HLT";
        case MiscOpcode::RET: return "RET";
      }
    default: return "NOP";
  }
}

std::string Instruction::toString() const {
  if (!valid) return "NOP";

  std::string name = getCommonName();
  std::string x = isFloat ? "f" : "x";

  switch (type) {
    case InstructionType::REG_REG:
      if (funct3 == 2 && (opcode == 0 || opcode == 1 || opcode == 2))
        return name + " " + x + std::to_string(rd)
                   + ", " + x + std::to_string(rs1);
      return name + " " + x + std::to_string(rd)
                 + ", " + x + std::to_string(rs1)
                 + ", " + x + std::to_string(rs2);

    case InstructionType::IMMEDIATE:
      return name + " " + x + std::to_string(rd)
                 + ", " + x + std::to_string(rs1)
                 + ", "  + std::to_string(immediate);

    case InstructionType::LOAD_STORE:
      return name + " " + x + std::to_string(rs1)
                 + ", "  + x + std::to_string(rs2);

    case InstructionType::JUMP:
      return name + " " + x + std::to_string(rd)
                 + ", "  + std::to_string(immediate);

    case InstructionType::BRANCH:
      return name + " " + x + std::to_string(rs1)
                 + ", " + x + std::to_string(rs2)
                 + ", "  + std::to_string(immediate);

    case InstructionType::PUSH_POP:
      return name + " " + x + std::to_string(rs1);

    case InstructionType::MISC:
      return name;

    default: return "NOP";
  }
}