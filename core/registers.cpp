#include "registers.h"

RegisterFile::RegisterFile() {
  reset();
}

void RegisterFile::reset() {
  for (int i = 0; i < 32; i++) intRegs[i] = 0;
  for (int i = 0; i < 16; i++) floatRegs[i] = 0.0f;
  pc = 0;
  intCC = IntConditionCode::NONE;
  floatCC = FloatConditionCode::NONE;
}

/*
* Read methods - returns value
*/
int32_t RegisterFile::readInt(uint8_t reg) const {
  if (reg == 0) return 0;           // x0 always zero
  if (reg == 1) return pc;          // x1 is the PC
  return intRegs[reg];
}

float RegisterFile::readFloat(uint8_t reg) const {
  if (reg == 0) return 0;           // f0 always zero
  return floatRegs[reg];
}

int32_t RegisterFile::readPC() const {
  return pc;
}

IntConditionCode RegisterFile::readIntCC() const {
  return intCC;
}

FloatConditionCode RegisterFile::readFloatCC() const {
  return floatCC;
}
/*
* Side door - returns entire arrays
*/
int32_t* RegisterFile::getIntRegs() {
  return intRegs;
}
float* RegisterFile::getFloatRegs() {
  return floatRegs;
}

/*
* Write methods - checks access rules and updates value
*/
void RegisterFile::writeInt(uint8_t reg, int32_t value, WriteSource source) {

  if (reg == 0) return;             // x0 discards writes

  // Special case - side door has full access
  if (source == WriteSource::SIDE_DOOR) {
    if (reg == 1) {
      writePC(value);  // Keep PC and x1 in sync
      return;
    }
    intRegs[reg] = value;
    return;
  }

  if (reg == 1) return;             // PC only via writePC()
  if (reg == 2 && source != WriteSource::PUSH_POP) return;   // SP only via push/pop
  if (reg == 3 && source != WriteSource::JUMP)     return;   // RA only via jump/ret
  intRegs[reg] = value;
}

void RegisterFile::writeFloat(uint8_t reg, float value) {
  if (reg == 0) return;             // f0 discards writes
  floatRegs[reg] = value;
}

void RegisterFile::writePC(int32_t value) {
  pc = value;
  intRegs[1] = value;  // Keep x1 in sync with PC
}

void RegisterFile::writeIntCC(IntConditionCode code) {
  intCC = code;
}

void RegisterFile::writeFloatCC(FloatConditionCode code) {
  floatCC = code;
}

void RegisterFile::incrementPC() {
  pc += 1;
}

