#pragma once
#include <cstdint>
#include <vector>

class Pipeline;
class GUI; // To be used later


// Enum for why the clock is halted - used by GUI
enum class HaltReason {
  NONE,           // Not halted
  HLT_INSTR,      // HLT instruction reached Writeback
  ERROR,          // Critical error occurred during execution (e.g. divide by zero)
  BREAKPOINT,     // PC matched a user-set breakpoint address
  CYCLE_TARGET,   // User-specified number of cycles completed
  INSTR_TARGET,   // User-specified number of instructions completed
  USER_PAUSE      // User manually paused via UI
};

class Clock {
public:

  Clock();

  // Set the pipeline and UI outside the constructor to avoid circular dependency
  void setPipeline(Pipeline* pipeline) { this->pipeline = pipeline; }
  void setGUI(GUI* ui) { this->ui = ui; }

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

  // Clear a prior halted state so execution can continue.
  void resume();

  // Breakpoints
  void addBreakpoint    (uint32_t address);
  void removeBreakpoint (uint32_t address);
  void clearBreakpoints ();
  bool isBreakpoint     (uint32_t address) const;

  /* Notifications from pipeline */

  // Called by Writeback when HLT reaches it
  virtual void onHalt();

  // Called by Execute when a critical error occurs
  virtual void onCriticalError(int conditionCode, int isFloat);

  // Called by Fetch when PC matches a breakpoint
  virtual void onBreakpoint();

  // Called by Writeback each time an instruction retires
  virtual void onInstructionRetired();

  void reset();

  /* Side door info */

  uint32_t   getCycleCount()  const { return cycleCount;  }
  uint32_t   getInstrCount()  const { return instrCount;  }
  bool       isHalted()       const { return halted;       }
  HaltReason getHaltReason()  const { return haltReason;  }

private:

  Pipeline* pipeline = nullptr; // Explicitly null until set
  GUI* ui;

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