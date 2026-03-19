#pragma once
#include <cstdint>
#include <vector>
#include "../core/instruction.h"
#include "../core/registers.h"
#include "../memory/memory.h"

class Clock;

/*
* Five stage pipeline, execution is started via clock calling tick()
* Pipeline informs clock of halts via callback, and has side-door access for UI display
*/
class Pipeline {
public:

  Pipeline(Memory* cache, RegisterFile* regs);

  // Set clock outside constructor to avoid circular dependency
  void setClock(Clock* clock) { this->clock = clock; }

  // Execute one full pipeline cycle. Called by Clock
  void tick();

  // Reset all inter-stage registers to bubbles
  void reset();

  // Enable or disable pipelined execution. When disabled, Fetch stalls after each instruction until it exits Writeback,
  void setPipelineEnabled(bool enabled);
  bool isPipelineEnabled() const { return pipelineEnabled; }

  // Side-door methods for UI

  Instruction getFetchInst()     const { return fetInst; }
  Instruction getDecodeInst()    const { return decInst; }
  Instruction getExecuteInst()   const { return exInst;  }
  Instruction getMemoryInst()    const { return memInst; }
  Instruction getWritebackInst() const { return wbInst;  }

  // Returns the current cycle's stall state
  bool isFetchStalled()     const { return fetchStalled;     }
  bool isDecodeStalled()    const { return decodeStalled;    }
  bool isExecuteStalled()   const { return executeStalled;   }
  bool isMemoryStalled()    const { return memoryStalled;    }
  bool isWritebackStalled() const { return writebackStalled; }

  // Returns the current pending destination list
  const std::vector<uint8_t>& getIntDependencies()   const { return intDependencies;   }
  const std::vector<uint8_t>& getFloatDependencies() const { return floatDependencies; }

private:

  Memory* cache;
  RegisterFile* regs;
  Clock* clock = nullptr; // Explicitly null until set


  // Instructions currently being processed in each stage
  Instruction fetInst;
  Instruction decInst;
  Instruction exInst;
  Instruction memInst;
  Instruction wbInst;

  // Stall flags
  bool fetchStalled;
  bool decodeStalled;
  bool executeStalled;
  bool memoryStalled;
  bool writebackStalled;

  bool fetchComplete; // Whether or not the instruction at PC has been fetched

  // Register dependency arrays - updated by Decode and cleared by Writeback. 
  std::vector<uint8_t> intDependencies;
  std::vector<uint8_t> floatDependencies;

  bool pipelineEnabled;

  // Check for disable mode - increments at fetch and decrements at writeback
  uint32_t inFlightCount;

  // Methods for each stage
  // Each takes the stalled state from the previous stage as input and returns the instruction to be processed by the next
  Instruction writeback ();
  Instruction memory (bool prevStalled);
  Instruction execute ( bool prevStalled);
  Instruction decode (bool prevStalled);
  Instruction fetch (bool prevStalled);

  // Helpers for dependencies
  void addDest (uint8_t reg, bool isFloat);
  void removeDest (uint8_t reg, bool isFloat);
  bool isPending (uint8_t reg, bool isFloat) const;

  // Called by Execute when a branch is taken. Squashes Fetch and Decode registers and updates PC
  void squashAndRedirect(uint32_t targetAddress);
};