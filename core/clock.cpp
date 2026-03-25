#include "clock.h"

Clock::Clock(UI* ui) : ui(ui) {
  reset();
}

void Clock::run() {
  // run loop
}

void Clock::tick() {
  // perform one cycle step
}

void Clock::runCycles(uint32_t numCycles) {
  // run numCycles cycles
  (void)numCycles;
}

void Clock::runInstructions(uint32_t numInsts) {
  // run numInsts instructions
  (void)numInsts;
}

void Clock::pause() {
  // Placeholder: pause requested
}

void Clock::addBreakpoint(uint32_t address) {
  if (!isBreakpoint(address))
    breakpoints.push_back(address);
}

void Clock::removeBreakpoint(uint32_t address) {
  breakpoints.erase(std::remove(breakpoints.begin(), breakpoints.end(), address), breakpoints.end());
}

void Clock::clearBreakpoints() {
  breakpoints.clear();
}

bool Clock::isBreakpoint(uint32_t address) const {
  return std::find(breakpoints.begin(), breakpoints.end(), address) != breakpoints.end();
}

void Clock::onHalt() {

}

void Clock::onBreakpoint() {

}

void Clock::onInstructionRetired() {

}

void Clock::reset() {
  cycleCount = 0;
  instrCount = 0;
  halted = false;
  haltReason = HaltReason::NONE;
  cycleTarget = -1;
  instrTarget = -1;
  breakpoints.clear();
}

void Clock::executeCycle() {

}

void Clock::checkHaltConditions() {

}

void Clock::clearHalt() {
  halted = false;
  haltReason = HaltReason::NONE;
}
