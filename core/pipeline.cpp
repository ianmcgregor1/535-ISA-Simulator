#include "pipeline.h"
#include "./decoder.h"
#include "./clock.h"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <iostream>

// TODO - complete MEM, EX, DEC. Maybe add flags to instruction to indicate when each stage is complete.
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
    fetchComplete(false),
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

  fetchComplete = false;

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

  Instruction incoming = fetch(false);

  if (!decodeStalled)
    decInst = incoming;

  return decodeStalled ? makeBubble() : decInst;
}

// Fetch - called by Decode
// Gets an instruction from PC in memory and returns it
Instruction Pipeline::fetch(bool prevStalled) {

  // If pipeline is disabled, do nothing until current instruction retires
  if (!pipelineEnabled && inFlightCount > 0) {
    fetchStalled = true;
    return makeBubble();
  }

  // If fetch is stalled, then we need to continue getting the current instruction at PC
  if (fetchStalled) {

    uint32_t pc = static_cast<uint32_t>(regs->readPC());
    MemoryResponse r = cache->loadWord(pc, AccessID::FETCH);

    if (r.status == MemoryResponse::Status::OK) {
      // Cache responded - decode and advance PC
      fetchStalled = false;
      Instruction inst = Instruction(r.word);
      regs->incrementPC();
      fetInst = inst;
    }

    if (r.status == MemoryResponse::Status::WAIT) {
      // Cache not ready - stall and re-issue same address next cycle
      fetchStalled = true;
      return makeBubble();
    }
  }
  // If we make it here, either fetch is not stalled or we just got the instruction
  
  // If decode is not stalled, return value will be accepted. Must update accordingly
  if (!prevStalled) {
    fetchStalled = true;
  }

  return fetInst;

  // TODO: Breakpoints
}

/*
* Dependency helpers
*/

// Add destination register to dependency array
void Pipeline::addDest(uint8_t reg, bool isFloat) {
  if (isFloat) floatDependencies.push_back(reg);
  else intDependencies.push_back(reg);
}

// Remove first instance of destination register from dependency array
void Pipeline::removeDest(uint8_t reg, bool isFloat) {
  auto& list = isFloat ? floatDependencies : intDependencies;
  auto it = std::find(list.begin(), list.end(), reg);
  if (it != list.end())
    list.erase(it);
}

// Check if the given destination register is in the dependency array
bool Pipeline::isPending(uint8_t reg, bool isFloat) const {
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