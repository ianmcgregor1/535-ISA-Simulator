#include "clock.h"
#include "pipeline.h"
#include <cassert>
#include <algorithm>
#include <iostream>

Clock::Clock()
  : ui(nullptr),
    pipeline(nullptr),
    cycleCount(0),
    instrCount(0),
    halted(false),
    haltReason(HaltReason::NONE),
    cycleTarget(-1),
    instrTarget(-1)
{}

void Clock::reset() {
  cycleCount  = 0;
  instrCount  = 0;
  halted      = false;
  haltReason  = HaltReason::NONE;
  cycleTarget = -1;
  instrTarget = -1;
  breakpoints.clear();
}

void Clock::pause() {
  halted = true;
  haltReason = HaltReason::USER_PAUSE;
}

void Clock::resume() {
  clearHalt();
}

// Runs one pipeline cycle and updates counters and halt conditions.
void Clock::tick() {
  
  assert(pipeline != nullptr && "Clock::tick called before setPipeline");

  if (halted) {
    std::cerr << "Clock::tick: pipeline is halted — call a resume method first\n";
    return;
  }

  // Execute one full pipeline cycle
  Instruction lastInst = pipeline->tick();

  // Increment cycle counter after the cycle completes
  cycleCount++;

  // Check cycle-based halt conditions
  checkHaltConditions();
}

//  Executes cycles continuously until a halt condition is reached, then notifies UI
void Clock::run() {
  assert(pipeline != nullptr && "Clock::run called before setPipeline");

  while (true) {
    if (halted) {
      // Notify UI
      if (ui != nullptr) {
        // ui->update() will be called here once UI is implemented
        // For now, return
      }
      return;
    }
    tick();
  }
}

// Runs for numCycles cycles, or if halted
void Clock::runCycles(uint32_t numCycles) {
  assert(pipeline != nullptr && "Clock::runCycles called before setPipeline");

  cycleTarget = static_cast<int32_t>(cycleCount + numCycles);

  while (!halted && static_cast<int32_t>(cycleCount) < cycleTarget) {
    tick();
  }

  // Clear cycle target
  cycleTarget = -1;
}

// Runs until numInsts additional instructions have retired, or if halted
void Clock::runInstructions(uint32_t numInsts) {
  assert(pipeline != nullptr && "Clock::runInstructions called before setPipeline");

  instrTarget = static_cast<int32_t>(instrCount + numInsts);

  while (!halted && static_cast<int32_t>(instrCount) < instrTarget) {
    tick();
  }

  // Clear instruction target
  instrTarget = -1;
}

void Clock::checkHaltConditions() {
  // Cycle target reached
  if (cycleTarget != -1 && static_cast<int32_t>(cycleCount) >= cycleTarget) {
    halted = true;
    haltReason = HaltReason::CYCLE_TARGET;
    cycleTarget = -1;
    return;
  }

  // Instruction target reached
  if (instrTarget != -1 && static_cast<int32_t>(instrCount) >= instrTarget) {
    halted = true;
    haltReason = HaltReason::INSTR_TARGET;
    instrTarget = -1;
  }
}

void Clock::clearHalt() {
  halted = false;
  haltReason = HaltReason::NONE;
}


// Breakpoint management

void Clock::addBreakpoint(uint32_t address) {
  if (!isBreakpoint(address))
    breakpoints.push_back(address);
}

void Clock::removeBreakpoint(uint32_t address) {
  auto it = std::find(breakpoints.begin(), breakpoints.end(), address);
  if (it != breakpoints.end())
    breakpoints.erase(it);
}

void Clock::clearBreakpoints() {
  breakpoints.clear();
}

bool Clock::isBreakpoint(uint32_t address) const {
  return std::find(breakpoints.begin(), breakpoints.end(), address)
         != breakpoints.end();
}

/*
* Pipeline notification callbacks
* These are called by Pipeline stages during tick()
*/

void Clock::onHalt() {
  // HLT instruction has reached Writeback - halt after this cycle
  halted = true;
  haltReason = HaltReason::HLT_INSTR;
}

void Clock::onBreakpoint() {
  // Fetch encountered a breakpoint address - halt after this cycle
  halted = true;
  haltReason = HaltReason::BREAKPOINT;
}

void Clock::onInstructionRetired() {
  // Called by Writeback for every valid instruction
  instrCount++;
}

