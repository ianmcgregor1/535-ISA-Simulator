#include "instruction.h"
#include <sstream>

// NOT IMPLEMENTED YET - convenience method for human-readable instruction, useful later
std::string Instruction::toString() const {
    if (!valid) return "NOP";

    std::ostringstream out;

    switch (type) {
        case InstructionType::REG_REG:
            switch (static_cast<RegRegOpcode>(opcode)) {
                case RegRegOpcode::INT_ARITHMETIC:
                    switch (funct3) {
                        case 0: out << "ADD x"  << (int)rd << ", x" << (int)rs1 << ", x" << (int)rs2; break;
                        case 1: out << "SUB x"  << (int)rd << ", x" << (int)rs1 << ", x" << (int)rs2; break;
                        case 2: out << "NEG x"  << (int)rd << ", x" << (int)rs1; break;
                        case 3: out << "MUL x"  << (int)rd << ", x" << (int)rs1 << ", x" << (int)rs2; break;
                        case 4: out << "DIV x"  << (int)rd << ", x" << (int)rs1 << ", x" << (int)rs2; break;
                        case 5: out << "REM x"  << (int)rd << ", x" << (int)rs1 << ", x" << (int)rs2; break;
                        default: out << "UNKNOWN"; break;
                    }
                    break;
                // ... other opcodes follow the same pattern
                default: out << "UNKNOWN"; break;
            }
            break;

        case InstructionType::BRANCH:
            switch (funct3) {
                case 0: out << "BEQ x"  << (int)rs1 << ", x" << (int)rs2 << ", " << immediate; break;
                case 1: out << "BNEQ x" << (int)rs1 << ", x" << (int)rs2 << ", " << immediate; break;
                // ...
                default: out << "UNKNOWN"; break;
            }
            break;

        case InstructionType::MISC:
            switch (opcode) {
                case 0: out << "HLT"; break;
                case 1: out << "RET"; break;
                default: out << "UNKNOWN"; break;
            }
            break;

        // ... remaining types
        default: out << "UNKNOWN"; break;
    }

    return out.str();
}