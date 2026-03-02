#pragma once
#include <cstdint>
#include "instruction.h"

enum class WriteSource { ALU, LOAD_STORE, JUMP, PUSH_POP, PIPELINE, SIDE_DOOR };

class RegisterFile {
public:
    RegisterFile();           // Constructor

    // Read methods to return current values
    int32_t  readInt(uint8_t reg) const;
    float    readFloat(uint8_t reg) const;
    int32_t  readPC() const;
    IntConditionCode  readIntCC() const;
    FloatConditionCode readFloatCC() const;

    // Write methods to update values
    void writeInt(uint8_t reg, int32_t value, WriteSource source);
    void writeFloat(uint8_t reg, float value);
    void writePC(int32_t value);
    void writeIntCC(IntConditionCode code);
    void writeFloatCC(FloatConditionCode code);
    void incrementPC();

    void reset();

private:
    int32_t  intRegs[32];     // x0-x31
    float    floatRegs[16];   // f0-f15
    int32_t  pc;
    IntConditionCode   intCC;
    FloatConditionCode floatCC;
};