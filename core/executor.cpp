#include "executor.h"
#include <cstring>
#include <iostream>
#include <climits>
#include <cmath>
#include <limits>
#include <cfloat>

// Bit conversion helpers
static float bitsToFloat(int32_t bits) {
  float f;
  std::memcpy(&f, &bits, sizeof(float));
  return f;
}

static int32_t floatToBits(float f) {
  int32_t bits;
  std::memcpy(&bits, &f, sizeof(int32_t));
  return bits;
}

// Condition code helpers
// Set integer or float condition codes on inst based on the result of an operation

static void setIntCC(Instruction& inst, int64_t fullResult) {
  if (fullResult > INT32_MAX || fullResult < INT32_MIN)
    inst.intCC = IntConditionCode::OVERFLOW_;
}

static void setFloatCC(Instruction& inst, float result) {
  if (std::isinf(result))
    inst.floatCC = FloatConditionCode::INFINITY_;
  else if (std::isnan(result))
    inst.floatCC = FloatConditionCode::NAN_;
  else if (result != 0.0f && std::fpclassify(result) == FP_SUBNORMAL)
    inst.floatCC = FloatConditionCode::UNDERFLOW_;
  else
    inst.floatCC = FloatConditionCode::NONE;
}

// executeInstruction - takes a decoded instruction and performs necessary computations
// Instructions not yet implemented return the instruction unchanged with a warning
Instruction executeInstruction(Instruction inst) {

  if (!inst.valid || inst.squashed) return inst;

  switch (inst.type) {

    case InstructionType::REG_REG: {

      switch (static_cast<RegRegOpcode>(inst.opcode)) {

        // Integer Arithmetic
        case RegRegOpcode::INT_ARITHMETIC: {
          int32_t a = inst.rs1_value;
          int32_t b = inst.rs2_value;
          int64_t full = 0;

          switch (static_cast<IntArithFunct3>(inst.funct3)) {

            case IntArithFunct3::ADD:
              full = static_cast<int64_t>(a) + b;
              inst.result = static_cast<int32_t>(full);
              setIntCC(inst, full);
              break;

            case IntArithFunct3::SUB:
              full = static_cast<int64_t>(a) - b;
              inst.result = static_cast<int32_t>(full);
              setIntCC(inst, full);
              break;

            case IntArithFunct3::NEG:
              inst.result = -a;
              break;

            case IntArithFunct3::MUL:
              full = static_cast<int64_t>(a) * b;
              inst.result = static_cast<int32_t>(full & 0xFFFFFFFF);
              setIntCC(inst, full);
              break;

            case IntArithFunct3::DIV:
              if (b == 0) {
                inst.intCC = IntConditionCode::DIVIDE_BY_ZERO;
                inst.result = 0;
              } else {
                inst.result = a / b;
              }
              break;

            case IntArithFunct3::REM:
              if (b == 0) {
                inst.intCC  = IntConditionCode::DIVIDE_BY_ZERO;
                inst.result = 0;
              } else {
                inst.result = a % b;
              }
              break;

            default:
              std::cerr << "executeInstruction: unknown INT_ARITHMETIC funct3 "
                        << (int)inst.funct3 << "\n";
              break;
          }
          break;
        }

        // Floating point arithmetic
        case RegRegOpcode::FLOAT_ARITHMETIC: {
          float a = inst.fs1_value;
          float b = inst.fs2_value;
          float r = 0.0f;

          switch (static_cast<FloatArithFunct3>(inst.funct3)) {
            case FloatArithFunct3::FADD: 
              inst.fresult = a + b; 
              break;
            case FloatArithFunct3::FSUB: 
              inst.fresult = a - b; 
              break;
            case FloatArithFunct3::FNEG: 
              inst.fresult = -a;    
              break;
            case FloatArithFunct3::FMUL: 
              inst.fresult = a * b; 
              break;
            case FloatArithFunct3::FDIV:
              if (b == 0.0f) {
                inst.floatCC = FloatConditionCode::DIVIDE_BY_ZERO;
                inst.fresult = a >= 0.0f ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();
                return inst;
              }
              else {
                inst.fresult = a / b;
              }
              break;
            default:
              std::cerr << "executeInstruction: unknown FLOAT_ARITHMETIC funct3 "
                        << (int)inst.funct3 << "\n";
              break;
          }
          
          setFloatCC(inst, r);
          break;
        }

        // Bitwise operations
        case RegRegOpcode::BITWISE: {
          int32_t a = inst.rs1_value;
          int32_t b = inst.rs2_value;

          switch (static_cast<BitwiseFunct3>(inst.funct3)) {
            case BitwiseFunct3::AND: inst.result = a & b; break;
            case BitwiseFunct3::OR:  inst.result = a | b; break;
            case BitwiseFunct3::NOT: inst.result = ~a;    break;
            case BitwiseFunct3::XOR: inst.result = a ^ b; break;
            default:
              std::cerr << "executeInstruction: unknown BITWISE funct3 "
                        << (int)inst.funct3 << "\n";
              break;
          }
          break;
        }

        // Compare and store
        case RegRegOpcode::COMPARE: {
          int32_t a = inst.rs1_value;
          int32_t b = inst.rs2_value;

          switch (static_cast<CompareFunct3>(inst.funct3)) {
            case CompareFunct3::SEQ:  inst.result = (a == b) ? 1 : 0; break;
            case CompareFunct3::SNEQ: inst.result = (a != b) ? 1 : 0; break;
            case CompareFunct3::SLT:  inst.result = (a <  b) ? 1 : 0; break;
            case CompareFunct3::SGT:  inst.result = (a >  b) ? 1 : 0; break;
            case CompareFunct3::SLE:  inst.result = (a <= b) ? 1 : 0; break;
            case CompareFunct3::SGE:  inst.result = (a >= b) ? 1 : 0; break;
            default:
              std::cerr << "executeInstruction: unknown COMPARE funct3 "
                        << (int)inst.funct3 << "\n";
              break;
          }
          break;
        }

        // Copy
        case RegRegOpcode::COPY:
          inst.result = inst.rs1_value;
          break;

        default:
          std::cerr << "executeInstruction: unknown REG_REG opcode "
                    << (int)inst.opcode << "\n";
          break;
      }
      break;
    }

    case InstructionType::IMMEDIATE: {

      int32_t a = inst.rs1_value;
      int32_t imm = inst.immediate;
      int64_t full = 0;

      switch (static_cast<ImmediateOpcode>(inst.opcode)) {

        // Integer arithmetic immediate
        case ImmediateOpcode::INT_ARITHMETIC: {

          switch (static_cast<ImmArithFunct3>(inst.funct3)) {

            case ImmArithFunct3::ADDI:
              full = static_cast<int64_t>(a) + imm;
              inst.result = static_cast<int32_t>(full);
              setIntCC(inst, full);
              break;

            case ImmArithFunct3::SUBI:
              full = static_cast<int64_t>(a) - imm;
              inst.result = static_cast<int32_t>(full);
              setIntCC(inst, full);
              break;

            case ImmArithFunct3::MULI:
              full = static_cast<int64_t>(a) * imm;
              inst.result = static_cast<int32_t>(full & 0xFFFFFFFF);
              setIntCC(inst, full);
              break;

            case ImmArithFunct3::DIVI:
              if (imm == 0) {
                inst.intCC  = IntConditionCode::DIVIDE_BY_ZERO;
                inst.result = 0;
              } else {
                inst.result = a / imm;
              }
              break;

            case ImmArithFunct3::REMI:
              if (imm == 0) {
                inst.intCC  = IntConditionCode::DIVIDE_BY_ZERO;
                inst.result = 0;
              } else {
                inst.result = a % imm;
                inst.intCC  = IntConditionCode::NONE;
              }
              break;
              
            default:
              std::cerr << "executeInstruction: unknown INT_ARITHMETIC imm funct3 "
                        << (int)inst.funct3 << "\n";
              break;
          }
          break;
        }

        // Shift immediate
        case ImmediateOpcode::SHIFT: {

          // Read as unsigned to perform logical shifts
          uint32_t unsigned_a = static_cast<uint32_t>(a);

          switch (static_cast<ShiftFunct3>(inst.funct3)) {
            case ShiftFunct3::SLLI:
              inst.result = static_cast<int32_t>(unsigned_a << imm);
              break;

            case ShiftFunct3::SRLI:
              inst.result = static_cast<int32_t>(unsigned_a >> imm);
              break;

            case ShiftFunct3::SRAI:
              // Arithmetic right shift preserves sign bit
              inst.result = a >> imm;
              break;

            default:
              std::cerr << "executeInstruction: unknown SHIFT funct3 "
                        << (int)inst.funct3 << "\n";
              break;
          }
          break;
        }

        // Bitwise immediate
        case ImmediateOpcode::BITWISE: {

          switch (static_cast<BitwiseFunct3>(inst.funct3)) {
            case BitwiseFunct3::AND: inst.result = a & imm; break;
            case BitwiseFunct3::OR:  inst.result = a | imm; break;
            case BitwiseFunct3::XOR: inst.result = a ^ imm; break;
            default:
              std::cerr << "executeInstruction: unknown BITWISE imm funct3 "
                        << (int)inst.funct3 << "\n";
              break;
          }
          break;
        }

        // Compare immediate
        case ImmediateOpcode::COMPARE: {

          switch (static_cast<CompareFunct3>(inst.funct3)) {
            case CompareFunct3::SEQ:  inst.result = (a == imm) ? 1 : 0; break;
            case CompareFunct3::SNEQ: inst.result = (a != imm) ? 1 : 0; break;
            case CompareFunct3::SLT:  inst.result = (a <  imm) ? 1 : 0; break;
            case CompareFunct3::SGT:  inst.result = (a >  imm) ? 1 : 0; break;
            case CompareFunct3::SLE:  inst.result = (a <= imm) ? 1 : 0; break;
            case CompareFunct3::SGE:  inst.result = (a >= imm) ? 1 : 0; break;
            default:
              std::cerr << "executeInstruction: unknown COMPARE imm funct3 "
                        << (int)inst.funct3 << "\n";
              break;
          }
          break;
        }
        
        // Jump and Link Register
        // rd = pc+1 (return address), pc = rs1 + imm
        // result stores the return address for Writeback
        // branchTarget stores the jump destination for Pipeline
        case ImmediateOpcode::JALR:
          inst.result = inst.pc + 1;  // return address (pc already incremented by Fetch)
          inst.branchTaken  = true;
          inst.branchTarget = static_cast<uint32_t>(inst.rs1_value + inst.immediate);
          break;

        default:
          std::cerr << "executeInstruction: unknown IMMEDIATE opcode "
                    << (int)inst.opcode << "\n";
          break;
      }
      break;
    }

    // Load/Store
    // Compute memory address only, actual access done later
    case InstructionType::LOAD_STORE:
      // Address is the value in rs1
      inst.memAddress = static_cast<uint32_t>(inst.rs1_value);
      break;

    // Jump
    // rd = pc+1 (return address), pc += imm
    case InstructionType::JUMP:
      inst.result       = inst.pc + 1;   // return address
      inst.branchTaken  = true;
      inst.branchTarget = static_cast<uint32_t>(static_cast<int32_t>(inst.pc) + inst.immediate);
      break;


    // TODO - Resume from here

    // Branch
    case InstructionType::BRANCH:
      switch (static_cast<BranchFunct3>(inst.funct3)) {
        case BranchFunct3::BEQ:
          inst.branchTaken = (inst.rs1_value == inst.rs2_value);
          break;

        case BranchFunct3::BNEQ:
          inst.branchTaken = (inst.rs1_value != inst.rs2_value);
          break;

        case BranchFunct3::BLT:
          inst.branchTaken = (inst.rs1_value < inst.rs2_value);
          break;

        case BranchFunct3::BGT:
          inst.branchTaken = (inst.rs1_value > inst.rs2_value);
          break;

        case BranchFunct3::BLE:
          inst.branchTaken = (inst.rs1_value <= inst.rs2_value);
          break;

        case BranchFunct3::BGE:
          inst.branchTaken = (inst.rs1_value >= inst.rs2_value);
          break;

        default:
          std::cerr << "executeInstruction: unknown BRANCH funct3 "
                    << (int)inst.funct3 << "\n";
          break;
      }

      if (inst.branchTaken) {
        inst.branchTarget =
            static_cast<uint32_t>(static_cast<int32_t>(inst.pc) + inst.immediate);
      }
      break;

    // Push/Pop
    case InstructionType::PUSH_POP:
      std::cerr << "executeInstruction: PUSH_POP not yet implemented\n";
      break;

    // Misc
    case InstructionType::MISC:
      // HLT and RET are handled by Pipeline, so nothing needed here
      break;

    default:
      std::cerr << "executeInstruction: unknown instruction type\n";
      break;
  }

  inst.executed = true;
  return inst;
}