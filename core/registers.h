#pragma once
#include <cstdint>
#include "instruction.h"

class RegisterFile {
public:
  RegisterFile();           // Constructor

  // Read methods to return current values
  int32_t  readInt(uint8_t reg) const;
  float    readFloat(uint8_t reg) const;
  uint32_t  readPC() const;
  IntConditionCode  readIntCC() const;
  FloatConditionCode readFloatCC() const;

  // Write methods to update values
  void writeInt(uint8_t reg, int32_t value, WriteSource source);
  void writeFloat(uint8_t reg, float value);
  void writePC(uint32_t value);
  void writeIntCC(IntConditionCode code);
  void writeFloatCC(FloatConditionCode code);
  void incrementPC();

  // Side door methods
  int32_t* getIntRegs();
  float* getFloatRegs();

  void reset();

private:
  int32_t  intRegs[32];     // x0-x31
  float    floatRegs[16];   // f0-f15
  uint32_t  pc;
  IntConditionCode   intCC;
  FloatConditionCode floatCC;
};