#pragma once
#include <cstdint>
#include <string>


//  Instruction Types (3-bit, bits 31-29)
enum class InstructionType : uint8_t {
    REG_REG     = 0b000,  // Register-Register (ALU, bitwise, compare)
    IMMEDIATE   = 0b001,  // Immediate
    LOAD_STORE  = 0b010,  // Load and store
    JUMP        = 0b011,  // Jump and link
    BRANCH      = 0b100,  // Conditional branches
    PUSH_POP    = 0b101,  // Stack operations
    MISC        = 0b111,  // HLT, RET
    INVALID     = 0xFF
};


//  Opcodes per type (4-bit)

// Type 000 - Register-Register opcodes
enum class RegRegOpcode : uint8_t {
    INT_ARITHMETIC   = 0,
    FLOAT_ARITHMETIC = 1,
    BITWISE          = 2,
    COMPARE          = 3,
    COPY             = 4
};

// Type 001 - Immediate opcodes
enum class ImmediateOpcode : uint8_t {
    INT_ARITHMETIC = 0,
    SHIFT          = 1,
    BITWISE        = 2,
    COMPARE        = 3,
    JALR           = 4
};

// Type 010 - Load/Store opcodes
enum class LoadStoreOpcode : uint8_t {
    INT_LOADSTORE   = 0,
    FLOAT_LOADSTORE = 1
};

// Type 111 - Misc opcodes
enum class MiscOpcode : uint8_t {
    HLT = 0,
    RET = 1
};

//  Funct3 values

// Integer arithmetic (RegReg opcode 0, Immediate opcode 0)
enum class IntArithFunct3 : uint8_t {
    ADD  = 0,
    SUB  = 1,
    NEG  = 2,
    MUL  = 3,
    DIV  = 4,
    REM  = 5
};

// Immediate arithmetic
enum class ImmArithFunct3 : uint8_t {
    ADDI = 0,
    SUBI = 1,
    MULI = 2,
    DIVI = 3,
    REMI = 4
};

// Float arithmetic (RegReg opcode 1)
enum class FloatArithFunct3 : uint8_t {
    FADD = 0,
    FSUB = 1,
    FNEG = 2,
    FMUL = 3,
    FDIV = 4
};

// Bitwise (RegReg opcode 2, Immediate opcode 2)
enum class BitwiseFunct3 : uint8_t {
    AND  = 0, ANDI = 0,
    OR   = 1, ORI  = 1,
    NOT  = 2,
    XOR  = 3, XORI = 2
};

// Compare (RegReg opcode 3, Immediate opcode 3)
enum class CompareFunct3 : uint8_t {
    SEQ  = 0, SEQI  = 0,
    SNEQ = 1, SNEQI = 1,
    SLT  = 2, SLTI  = 2,
    SGT  = 3, SGTI  = 3,
    SLE  = 4, SLEI  = 4,
    SGE  = 5, SGEI  = 5
};

// Shift immediate (Immediate opcode 1)
enum class ShiftFunct3 : uint8_t {
    SLLI = 0,
    SRLI = 1,
    SRAI = 2
};

// Load/Store (funct3 within each opcode)
enum class LoadStoreFunct3 : uint8_t {
    LOAD  = 0,   // LW or LWF
    STORE = 1    // SW or SWF
};

// Branch (Type 100)
enum class BranchFunct3 : uint8_t {
    BEQ  = 0,
    BNEQ = 1,
    BLT  = 2,
    BGT  = 3,
    BLE  = 4,
    BGE  = 5
};

// Push/Pop (Type 101)
enum class PushPopFunct3 : uint8_t {
    PUSH = 0,
    POP  = 1
};

//  Condition Code values

enum class IntConditionCode : uint8_t {
    NONE           = 0,
    DIVIDE_BY_ZERO = 1,
    OVERFLOW_      = 2  // Keywords have _ to prevent errors
};

enum class FloatConditionCode : uint8_t {
    NONE           = 0,
    DIVIDE_BY_ZERO = 1,
    OVERFLOW_      = 2,
    UNDERFLOW_     = 3,
    NAN_           = 4,   
    INFINITY_      = 5
};

// ─────────────────────────────────────────────
//  Decoded Instruction
// ─────────────────────────────────────────────

struct Instruction {
    uint32_t raw;           // The original 32-bit word, useful for debugging

    InstructionType type;   // 3-bit type field, defines instruction format
    uint8_t  opcode;        // 4-bit opcode, meaning depends on type
    uint8_t  funct3;        // 3-bit function code
    uint8_t  funct7;        // 7-bit function code (extra)

    uint8_t  rs1;           // First source register
    uint8_t  rs2;           // Second source register
    uint8_t  rd;            // Destination register

    int32_t  immediate;     // Sign-extended immediate value

    bool     valid;         // False if instruction should be treated as NOP
    bool     isFloat;       // True if this instruction operates on FP registers

    std::string getCommonName() const; // Returns a short name like "ADD" or "BEQ", used by toString()

    // Returns a human-readable string, useful for trace logs and the GUI
    std::string toString() const;
};