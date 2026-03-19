#pragma once
#include <cstdint>
#include <vector>

class Pipeline;
class UI; // To be used later


// Enum for why the clock is halted - used by GUI
enum class HaltReason {
  NONE,           // Not halted
  HLT_INSTR,      // HLT instruction reached Writeback
  BREAKPOINT,     // PC matched a user-set breakpoint address
  CYCLE_TARGET,   // User-specified number of cycles completed
  INSTR_TARGET,   // User-specified number of instructions completed
  USER_PAUSE      // User manually paused via UI
};

class Clock {
public:

  Clock(UI* ui);

  // Set the pipeline outside the constructor to avoid circular dependency
  void setPipeline(Pipeline* pipeline) { this->pipeline = pipeline; }

  // Run the loop until a halt condition is triggered
  void run();

  // One clock cycle
  void tick();

  // Step a specified number of cycles.
  void runCycles(uint32_t numCycles);

  // Continue for a specified number of instructions.
  void runInstructions(uint32_t numInsts);

  // Pause immediately at the end of the current or next cycle.
  void pause();

  // Breakpoints
  void addBreakpoint    (uint32_t address);
  void removeBreakpoint (uint32_t address);
  void clearBreakpoints ();
  bool isBreakpoint     (uint32_t address) const;

  /* Notifications from pipeline */

  // Called by Writeback when HLT reaches it
  void onHalt();

  // Called by Fetch when PC matches a breakpoint
  void onBreakpoint();

  // Called by Writeback each time a valid instruction retires
  void onInstructionRetired();

  void reset();

  /* Side door info */

  uint32_t   getCycleCount()  const { return cycleCount;  }
  uint32_t   getInstrCount()  const { return instrCount;  }
  bool       isHalted()       const { return halted;       }
  HaltReason getHaltReason()  const { return haltReason;  }

private:

  Pipeline* pipeline = nullptr; // Explicitly null until set
  UI* ui;

  // Counters
  int32_t cycleCount;   // Total cycles executed since last reset
  int32_t instrCount;   // Total instructions retired since last reset

  bool       halted;
  HaltReason haltReason;

  // Set by resume methods, checked each cycle.
  // -1 means no active target of that type.
  int32_t cycleTarget;
  int32_t instrTarget;

  std::vector<uint32_t> breakpoints;

  /* Internal helpers */

  // Run one cycle: call tick(), increment counters, check halt conditions.
  void executeCycle();

  // Evaluate all halt conditions after a cycle, set halted and haltReason if triggered.
  void checkHaltConditions();

  // Clear halt state
  void clearHalt();
};