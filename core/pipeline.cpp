#include "pipeline.h"
#include "decoder.h"
#include "clock.h"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <iostream>
#include "executor.h"

/*
* Helper functions
*/

// Returns a default invalid (bubble) instruction
static Instruction makeBubble() {
  Instruction inst = Instruction();
  inst.valid    = false;
  return inst;
}

static Instruction makeSquashedBubble() {
  Instruction inst = makeBubble();
  inst.squashed = true;
  return inst;
}

// Returns the destination register for an instruction.
// Only meaningful if hasDestination() is true.
static uint8_t getDestReg(const Instruction& inst) {
  switch (inst.type) {
    case InstructionType::LOAD_STORE:
      if (inst.funct3 == static_cast<uint8_t>(LoadStoreFunct3::STORE))
        return 0;   // Store has no destination — write to x0 is discarded
      return inst.rs2;   // Load: rs2 = *rs1

    default:
      return inst.rd;
  }
}

// True if the instruction is a jump-and-link (saves return address to rd)
static bool isJumpAndLink(const Instruction& inst) {
  return inst.type == InstructionType::JUMP ||
         (inst.type == InstructionType::IMMEDIATE &&
          inst.opcode == static_cast<uint8_t>(ImmediateOpcode::JALR));
}

// Reinterpret int32_t bits as float
static float bitsToFloat(int32_t bits) {
  float f;
  std::memcpy(&f, &bits, sizeof(float));
  return f;
}

// Reinterpret float to int32_t bits
static int32_t floatToBits(float f) {
  int32_t bits;
  std::memcpy(&bits, &f, sizeof(int32_t));
  return bits;
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
    inFlight(false)
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

  inFlight = false;
}

void Pipeline::setPipelineEnabled(bool enabled) {
  pipelineEnabled = enabled;
}

Instruction Pipeline::tick() {
  assert(clock != nullptr && "Pipeline::tick called before setClock");

  // Call writeback
  Instruction completedInst = writeback();
  return completedInst;
}

// Writeback - called by tick()
// Writes to destination register and calls Memory()
Instruction Pipeline::writeback() {

  // Writeback is never stalled, but set here for consistency
  writebackStalled = false;

  // Get destination register
  uint8_t dest = getDestReg(wbInst);

  // Process current instruction
  if (wbInst.valid && !wbInst.squashed) {

    // Notify clock on halt
    if (wbInst.type == InstructionType::MISC && wbInst.opcode == static_cast<uint8_t>(MiscOpcode::HLT)) {
      clock->onHalt();
    }
    // Update stack pointer on Push/Pop instructions
    else if (wbInst.type == InstructionType::PUSH_POP) {
      uint32_t spValue = static_cast<uint32_t>(regs->readInt(2)); // Current stack pointer value from register file

      if (wbInst.funct3 == static_cast<uint8_t>(PushPopFunct3::PUSH)) {
        // Decrement sp by 1 (grows downwards)
        int32_t newSP = static_cast<int32_t>(spValue - 1);
        regs->writeInt(2, newSP, wbInst.writeSource);
      }
      else {
        // Increment sp by 1 (shrinks upwards)
        int32_t newSP = static_cast<int32_t>(spValue + 1);
        regs->writeInt(2, newSP, wbInst.writeSource);
      }
    }

    /* Writeback */

    // Write to destination register (RegisterFile handles permissions)
    if (wbInst.isFloat) {
      regs->writeFloat(dest, wbInst.fresult);
    }
    else {
      regs->writeInt(dest, wbInst.result, wbInst.writeSource);
    }

    // Set inFlight if necessary
    if (!pipelineEnabled) {
      inFlight = false;
    }
  }

  // Instruction out of pipeline, update accordingly
  if (wbInst.dependencyTracked) {
    removeDest(dest, wbInst.isFloat);

    // Push/Pop instructions need to remove stack pointer
    if (wbInst.type == InstructionType::PUSH_POP) {
      addDest(2, false); // Also track x2 (stack pointer) as destination
    }
    
    wbInst.dependencyTracked = false;
  }
  wbInst.complete = true;
  if (wbInst.valid && !wbInst.squashed) {
    clock->onInstructionRetired();
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

  memoryStalled = false;

  // If current instruction is valid and incomplete, ping memory
  if (memInst.valid && !memInst.squashed && !memInst.memoryAccessed) {

    // Only do anything if instruction actually needs to access memory
    switch (memInst.type) {

      case InstructionType::LOAD_STORE: {

        if (memInst.funct3 == static_cast<uint8_t>(LoadStoreFunct3::LOAD)) {
          // Load - request word from cache
          MemoryResponse r = cache->loadWord(memInst.memAddress, AccessID::MEMORY);

          if (r.status == MemoryResponse::Status::WAIT) {
            // If response is wait, stall
            memoryStalled = true;
          } else {
            // If response is ok, store in result or fresult
            if (memInst.isFloat)
              memInst.fresult = bitsToFloat(r.word);
            else
              memInst.result = static_cast<int32_t>(r.word);
            memInst.memoryAccessed = true;
            memoryStalled = false;
          }
        }
        else {
          // Store - write word to cache
          // Second register holds the value to store, memAddress holds the target
          uint32_t storeData = memInst.isFloat ? static_cast<uint32_t>(floatToBits(memInst.fs2_value)) : static_cast<uint32_t>(memInst.rs2_value);

          MemoryResponse r = cache->storeWord(memInst.memAddress, storeData, AccessID::MEMORY);

          if (r.status == MemoryResponse::Status::WAIT) {
            memoryStalled = true;
          } else {
            memInst.memoryAccessed = true;
            memoryStalled = false;
          }
        }
        break;
      }

      case InstructionType::PUSH_POP: {

        if (memInst.funct3 == static_cast<uint8_t>(PushPopFunct3::PUSH)) {
          // Store rs1_value at sp, decrement sp by 1
          uint32_t pushData = static_cast<uint32_t>(memInst.rs1_value);
          MemoryResponse r  = cache->storeWord(memInst.memAddress, pushData, AccessID::MEMORY);

          if (r.status == MemoryResponse::Status::WAIT) {
            memoryStalled = true;
          } else {
            memInst.memoryAccessed = true;
            memoryStalled = false;
          }
        }
        else {
          // POP - load from the current sp address into result
          MemoryResponse r = cache->loadWord(memInst.memAddress, AccessID::MEMORY);

          if (r.status == MemoryResponse::Status::WAIT) {
            memoryStalled = true;
          } else {
            memInst.result = static_cast<int32_t>(r.word);
            memInst.memoryAccessed = true;
            memoryStalled = false;
          }
        }
        break;
      }

      default: // All other instruction types have no memory work, mark as done
        memInst.memoryAccessed = true;
        break;
    }
  }

  // Always call execute, pass stall state
  Instruction incoming = execute(memoryStalled || prevStalled);

  // If stalled, hold memInst and return bubble
  if (memoryStalled || prevStalled) {
    return makeBubble();
  }

  // No stall — advance
  Instruction oldInst = memInst;
  memInst = incoming;
  return oldInst;
}

// Execute - called by Memory
// Performs necessary operation on instruction and returns populated instruction
// Calls Decode for next instruction
Instruction Pipeline::execute(bool prevStalled) {
  
  // Reset stall flag
  executeStalled = false;

  // If current instruction is valid and not executed, execute it or decrement count
  if (exInst.valid && !exInst.squashed && !exInst.executed) {

    if (exInst.executionCycles > 0) {
      // This is a multi-cycle instruction still in progress - decrement counter and stall
      exInst.executionCycles--;
      executeStalled = true;
    }
    else {
      // This is a new instruction ready to execute - call executor
      exInst = executeInstruction(exInst);
      exInst.executed = true;

      // Redirect PC if necessary
      if (exInst.branchTaken) {
        // If RET, get return address from register
        if (exInst.type == InstructionType::MISC && exInst.opcode == static_cast<uint8_t>(MiscOpcode::RET)) {
          exInst.branchTarget = static_cast<uint32_t>(regs->readInt(3));
        }
        squashAndRedirect(exInst.branchTarget);
      }

      // Set condition code(s) if necessary
      if (exInst.type == InstructionType::REG_REG || exInst.type == InstructionType::IMMEDIATE) {
        regs->writeIntCC(exInst.intCC);
        regs->writeFloatCC(exInst.floatCC);
      }

      // Notify clock if critical
      if (exInst.intCC == IntConditionCode::DIVIDE_BY_ZERO || exInst.floatCC == FloatConditionCode::DIVIDE_BY_ZERO) {
        clock->onCriticalError(1, exInst.isFloat); // 1 is the condition code for divide by zero
      }
    }
  }

  // Always call decode, passing whether execute is stalled
  Instruction incoming = decode(executeStalled || prevStalled);

  // If execute is stalled, ignore return value, return bubble, and keep exInst the same
  if (executeStalled || prevStalled) {
    return makeBubble();
  }

  // If no stall exists, update dependencies, return decoded instruction, and update exInst
  Instruction oldInst = exInst;
  exInst = incoming;
  return oldInst;
}

// Decode - called by Execute
// Decodes instruction to populate values based on Type
// Calls Fetch for next instruction
Instruction Pipeline::decode(bool prevStalled) {

  decodeStalled = false;

  // If current instruction is valid and not decoded, decode it
  if (decInst.valid && !decInst.squashed && !decInst.decoded) {
    decInst = decodeInstruction(decInst);
    decInst.decoded = true;
  }

  // If current instruction is valid and operands are not yet read, check for stall conditions
if (decInst.valid && !decInst.squashed && !decInst.operandsRead) {

  // Check for hazards and stall if necessary
  if (decInst.isFloat && decInst.type == InstructionType::LOAD_STORE) {
    // rs1 is always an integer address register, so check integer dependencies
    if (isPending(decInst.rs1, false)) {
      decodeStalled = true;
    }
    // For stores only: rs2 is a float data register, check float dependencies
    else if (decInst.funct3 == static_cast<uint8_t>(LoadStoreFunct3::STORE) && isPending(decInst.rs2, true)) {
      decodeStalled = true;
    }
    // For loads: rs2 is the destination, don't need a check
  } else {
    // All other instructions: both source registers match isFloat
    if (isPending(decInst.rs1, decInst.isFloat) || isPending(decInst.rs2, decInst.isFloat)) {
      decodeStalled = true;
    }
  }

  // If no stall, read from register file
  if (!decodeStalled) {
    if (decInst.isFloat) {
      if (decInst.type == InstructionType::LOAD_STORE) {
        // Memory address is always an integer
        decInst.rs1_value = regs->readInt(decInst.rs1);
        if (decInst.funct3 == static_cast<uint8_t>(LoadStoreFunct3::STORE)) {
          decInst.fs2_value = regs->readFloat(decInst.rs2); // Read data to store
        }
      } else {
        decInst.fs1_value = regs->readFloat(decInst.rs1);
        decInst.fs2_value = regs->readFloat(decInst.rs2);
      }
    }
    else {
      decInst.rs1_value = regs->readInt(decInst.rs1);
      decInst.rs2_value = regs->readInt(decInst.rs2);
    }
    decInst.operandsRead = true;
  }
}

  // Call fetch for next instruction, passing stall status
  Instruction incoming = fetch(decodeStalled || prevStalled);

  // If decode is stalled, ignore return value, return bubble, and keep decInst the same
  if (decodeStalled || prevStalled) {
    return makeBubble();
  }

  // If no stall exists, update dependencies, return decoded instruction, and update decInst
  if (decInst.valid && !decInst.squashed) {
    addDest(getDestReg(decInst), decInst.isFloat);
    decInst.dependencyTracked = (getDestReg(decInst) != 0);

    // PUSH/POP instructions also need to track the stack pointer (x2)
    if (decInst.type == InstructionType::PUSH_POP) {
      addDest(2, false); // Also track x2 (stack pointer) as destination
    }
  }
  Instruction oldInst = decInst;
  decInst = incoming;
  return oldInst;

}

// Fetch - called by Decode
// Gets an instruction from PC in memory and returns it
Instruction Pipeline::fetch(bool prevStalled) {

  // If pipeline is disabled, do nothing until current instruction retires
  if (!pipelineEnabled && inFlight) {
    fetchStalled = true;
    return makeBubble();
  }

  // If current instruction is not fetched, ping memory for it regardless of stalled state.
  if (!fetInst.fetched) {

    uint32_t pc = static_cast<uint32_t>(regs->readPC());
    MemoryResponse r = cache->loadWord(pc, AccessID::FETCH);

    if (r.status == MemoryResponse::Status::OK) {
      // Cache responded - decode and advance PC (store old PC in instruction for branching reference)
      fetchStalled = false;
      Instruction inst = Instruction(r.word);
      inst.pc = regs->readPC();
      regs->incrementPC();
      fetInst = inst;
      fetInst.fetched = true;
    }

    if (r.status == MemoryResponse::Status::WAIT) {
      // Cache not ready - stall and re-issue same address next cycle
      fetchStalled = true;
    }
  }

  // If address of fetched instruction is a breakpoint, notify clock
  if (fetInst.fetched && clock && clock->isBreakpoint(fetInst.pc)) {
    clock -> onBreakpoint();
  }
  
  // If stalled, return bubble, otherwise continue
  if (fetchStalled || prevStalled) {
    return makeBubble();
  }

  // Set inFlight if necessary
  if (!pipelineEnabled && fetInst.valid && !fetInst.squashed) {
    inFlight = true;
  }

  // Return fetched instruction and set fetInst to a new, unfetched instruction to prepare for next fetch
  Instruction oldInst = fetInst;
  fetInst = Instruction();
  return oldInst;  
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
  if (fetInst.dependencyTracked) {
    removeDest(getDestReg(fetInst), fetInst.isFloat);
  }
  fetInst.dependencyTracked = false;
  fetInst.squashed = true; // Mark as squashed for UI
  fetInst.valid = false;  // Mark as invalid to prevent any side effects
  
  cache->cancelFetch(); // Cancel any in-flight fetch request, since it will be squashed

  if (decInst.dependencyTracked) {
    removeDest(getDestReg(decInst), decInst.isFloat);
  }
  decInst.dependencyTracked = false;
  decInst.squashed = true; // Mark as squashed for UI 
  decInst.valid = false;  // Mark as invalid to prevent any side effects

  regs->writePC(static_cast<int32_t>(targetAddress));
}