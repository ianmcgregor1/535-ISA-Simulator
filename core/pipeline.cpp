#include "pipeline.h"
#include "decoder.h"
#include "clock.h"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <iostream>

// TODO - complete MEM, EX, DEC. Use flags in instruction to indicate when each stage is complete. 
// Need breakpoint support

/*
* Helper functions
*/

// Returns a default invalid (bubble) instruction
static Instruction makeBubble() {
  Instruction inst = Instruction();
  inst.valid    = false;
  return inst;
}

// Returns the destination register for an instruction.
// Only meaningful if hasDestination() is true.
static uint8_t getDestReg(const Instruction& inst) {
  switch (inst.type) {
    case InstructionType::LOAD_STORE:
      return inst.rs2;    // LW/LWF: rs2 = *rs1
    case InstructionType::PUSH_POP:
      return inst.rs1;    // POP: rs1 = *sp
    default:
      return inst.rd;     // All other instructions write to rd
  }
}

// True if the instruction is a jump-and-link (saves return address to rd)
static bool isJumpAndLink(const Instruction& inst) {
  return inst.type == InstructionType::JUMP ||
         (inst.type == InstructionType::IMMEDIATE &&
          inst.opcode == static_cast<uint8_t>(ImmediateOpcode::JALR));
}

// Reinterpret int32_t bits as float — avoids undefined behavior from cast
static float bitsToFloat(int32_t bits) {
  float f;
  std::memcpy(&f, &bits, sizeof(float));
  return f;
}

// Constructor

Pipeline::Pipeline(Memory* cache, RegisterFile* regs)
  : cache(cache),
    regs(regs),
    clock(nullptr),
    fetchStalled(false),
    decodeStalled(false),
    executeStalled(false),
    memoryStalled(false),
    writebackStalled(false),
    pipelineEnabled(true),
    inFlightCount(0)
{
  reset();
}

void Pipeline::reset() {
  Instruction bubble = makeBubble();

  fetInst = bubble;
  decInst = bubble;
  exInst  = bubble;
  memInst = bubble;
  wbInst  = bubble;

  fetchStalled     = false;
  decodeStalled    = false;
  executeStalled   = false;
  memoryStalled    = false;
  writebackStalled = false;

  intDependencies.clear();
  floatDependencies.clear();

  inFlightCount = 0;
}

void Pipeline::setPipelineEnabled(bool enabled) {
  pipelineEnabled = enabled;
}

void Pipeline::tick() {
  assert(clock != nullptr && "Pipeline::tick called before setClock");

  // Call writeback
  Instruction completedInst = writeback();
}

// Writeback - called by tick()
// Writes to destination register and calls Memory()
Instruction Pipeline::writeback() {

  // Writeback is never stalled, but set here for consistency
  writebackStalled = false;

  // Process current instruction
  if (wbInst.valid && !wbInst.squashed) {

    // Notify clock on halt
    if (wbInst.type == InstructionType::MISC && wbInst.opcode == static_cast<uint8_t>(MiscOpcode::HLT)) {
      clock->onHalt();
    }

    /* Writeback */

    // Get distination register
    uint8_t dest = getDestReg(wbInst);

    // Write to destination register (RegisterFile handles permissions)
    if (wbInst.isFloat) {
      regs->writeFloat(dest, bitsToFloat(wbInst.result));
    }
    else {
      regs->writeInt(dest, wbInst.result, wbInst.writeSource);
    }

    // Instruction out of pipeline, update accordingly
    removeDest(dest, wbInst.isFloat);
    clock->onInstructionRetired();
    inFlightCount -= 1;
  }

  // Call Memory to get new instruction
  Instruction oldInst = wbInst;
  Instruction incoming = memory(writebackStalled);
  wbInst = incoming;

  return oldInst;
}


// Memory - called by Writeback
// Performs load/store instructions and returns populated Instruction
// Calls Execute for next instruction
Instruction Pipeline::memory(bool prevStalled) {
  
  Instruction incoming = execute(false);

  if (!memoryStalled)
    memInst = incoming;

  return memoryStalled ? makeBubble() : memInst;
}

// Execute - called by Memory
// Performs necessary operation on instruction and returns populated instruction
// Calls Decode for next instruction
Instruction Pipeline::execute(bool prevStalled) {
  
  Instruction incoming = decode(false);

  if (!executeStalled)
    exInst = incoming;

  return executeStalled ? makeBubble() : exInst;
}

// Decode - called by Execute
// Decodes instruction to populate values based on Type
// Calls Fetch for next instruction
Instruction Pipeline::decode(bool prevStalled) {

  decodeStalled = false;

  // If current instruction is valid and not decoded, decode it
  if (decInst.valid && !decInst.decoded) {
    decInst = decodeInstruction(decInst);
    decInst.decoded = true;
  }

  // If current instruction is valid and operands are not yet read, check for stall conditions
  if (decInst.valid && !decInst.operandsRead) {
    // If read-registers are pending, stall
    if (isPending(decInst.rs1, decInst.isFloat) || isPending(decInst.rs2, decInst.isFloat)) {
      decodeStalled = true;
    } else {
      // No hazard - read values from register file
      if (decInst.isFloat) {
        decInst.fs1_value = regs->readFloat(decInst.rs1);
        decInst.fs2_value = regs->readFloat(decInst.rs2);
      }
      else {
        decInst.rs1_value = regs->readInt(decInst.rs1);
        decInst.rs2_value = regs->readInt(decInst.rs2);
      }
      decInst.operandsRead = true; // Mark that operands are read
    }
  }

  // Call fetch for next instruction, passing stall status
  Instruction incoming = fetch(decodeStalled || prevStalled);

  // If decode is stalled, ignore return value, return bubble, and keep decInst the same
  if (decodeStalled || prevStalled) {
    return makeBubble();
  }

  // If no stall exists, update dependencies, return decoded instruction, and update decInst
  addDest(getDestReg(decInst), decInst.isFloat);
  Instruction oldInst = decInst;
  decInst = incoming;
  return oldInst;

}

// Fetch - called by Decode
// Gets an instruction from PC in memory and returns it
Instruction Pipeline::fetch(bool prevStalled) {

  // If pipeline is disabled, do nothing until current instruction retires
  if (!pipelineEnabled && inFlightCount > 0) {
    fetchStalled = true;
    return makeBubble();
  }

  // If current instruction is not fetched, ping memory for it regardless of stalled state.
  if (!fetInst.fetched) {

    uint32_t pc = static_cast<uint32_t>(regs->readPC());
    MemoryResponse r = cache->loadWord(pc, AccessID::FETCH);

    if (r.status == MemoryResponse::Status::OK) {
      // Cache responded - decode and advance PC
      fetchStalled = false;
      Instruction inst = Instruction(r.word);
      regs->incrementPC();
      fetInst = inst;
      fetInst.fetched = true;
    }

    if (r.status == MemoryResponse::Status::WAIT) {
      // Cache not ready - stall and re-issue same address next cycle
      fetchStalled = true;
      return makeBubble();
    }
  }
  // If we make it here, we have a fetched instruction
  
  // If decode is not stalled, return value will be accepted
  // Return fetched instruction and set fetInst to a new, unfetched instruction to prepare for next fetch
  if (!prevStalled) {
    Instruction oldInst = fetInst;
    fetInst = Instruction();
    return oldInst;
  }

  // If decode is stalled, send it a bubble (will be ignored) and keep fetInst
  return makeBubble();

  // TODO: Breakpoints
}

/*
* Dependency helpers
*/

// Add destination register to dependency array
void Pipeline::addDest(uint8_t reg, bool isFloat) {
  if (reg == 0) return; // Register x0 is hardwired to zero, ignore as destination
  if (isFloat) floatDependencies.push_back(reg);
  else intDependencies.push_back(reg);
}

// Remove first instance of destination register from dependency array
void Pipeline::removeDest(uint8_t reg, bool isFloat) {
  if (reg == 0) return; // Register x0 is hardwired to zero, ignore as destination
  auto& list = isFloat ? floatDependencies : intDependencies;
  auto it = std::find(list.begin(), list.end(), reg);
  if (it != list.end())
    list.erase(it);
}

// Check if the given destination register is in the dependency array
bool Pipeline::isPending(uint8_t reg, bool isFloat) const {
  if (reg == 0) return false; // Register x0 is hardwired to zero, ignore as destination
  const auto& list = isFloat ? floatDependencies : intDependencies;
  return std::find(list.begin(), list.end(), reg) != list.end();
}


// Called by Execute when a branch is determined to be taken.
// Squashes the instructions currently in Fetch and Decode, then updates PC
void Pipeline::squashAndRedirect(uint32_t targetAddress) {

  fetInst.valid = false;
  decInst.valid = false;

  regs->writePC(static_cast<int32_t>(targetAddress));
}