#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "../memory/memory.h"
#include "../core/registers.h"
#include "../core/instruction.h"
#include "pipeline.h"
#include "clock.h"

// ─────────────────────────────────────────────
//  Test configuration
//  Cache: 0 delay (hit returns immediately)
//  DRAM:  delay=3 (requires 3 pings to return)
// ─────────────────────────────────────────────

static constexpr uint32_t DRAM_LINES  = 2048;   // 2048 lines * 4 words = 8K words
static constexpr uint32_t DRAM_DELAY  = 3;
static constexpr uint32_t CACHE_LINES = 64;
static constexpr uint32_t CACHE_DELAY = 0;
static constexpr uint32_t CACHE_ASSOC = 1;      // direct-mapped

// ─────────────────────────────────────────────
//  Minimal Clock stub
//  The real Clock requires a UI object.
//  This stub satisfies Pipeline's callbacks
//  without needing a full Clock implementation.
// ─────────────────────────────────────────────

class TestClock : public Clock {
public:
  TestClock() : Clock() {}

  void onHalt() override {
    halted = true;
    std::cout << "  [clock] HLT reached Writeback — pipeline halted\n";
  }

  void onBreakpoint() override {
    std::cout << "  [clock] Breakpoint hit\n";
  }

  void onInstructionRetired() override {
    retiredCount++;
  }

  bool     halted       = false;
  uint32_t retiredCount = 0;
};

// ─────────────────────────────────────────────
//  Display helpers
// ─────────────────────────────────────────────

static std::string stageLabel(const std::string& name, bool stalled) {
  return name + (stalled ? " [STALL]" : "       ");
}

static void printInstruction(const std::string& label, const Instruction& inst, bool stalled) {
  std::cout << "  " << std::left << std::setw(18) << stageLabel(label, stalled);

  if (!inst.valid) {
    std::cout << "  --- bubble ---\n";
    return;
  }

  std::cout << "  " << std::left << std::setw(12) << inst.toString();
  std::cout << "  raw=0x" << std::hex << std::setw(8) << std::setfill('0')
            << inst.raw << std::dec << std::setfill(' ');

  // Show result if executed
  if (inst.executed)
    std::cout << "  result=" << inst.result;

  // Show memory address if relevant
  if (inst.type == InstructionType::LOAD_STORE && inst.decoded)
    std::cout << "  memAddr=" << inst.memAddress;

  // Show flags
  if (inst.decoded)      std::cout << " [DEC]";
  if (inst.operandsRead) std::cout << " [OPR]";
  if (inst.executed)     std::cout << " [EXE]";
  if (inst.memoryAccessed) std::cout << " [MEM]";
  if (inst.complete)     std::cout << " [DONE]";

  std::cout << "\n";
}

static void printPipelineState(Pipeline& pipe, TestClock& clk,
                               RegisterFile& regs, uint32_t cycle) {
  std::cout << "\n--------------------------------------------------\n";
  std::cout << "  Cycle " << cycle
            << "   Retired: " << clk.retiredCount
            << "   PC: " << regs.readPC() << "\n";
  std::cout << "--------------------------------------------------\n";

  printInstruction("FETCH",     pipe.getFetchInst(),     pipe.isFetchStalled());
  printInstruction("DECODE",    pipe.getDecodeInst(),    pipe.isDecodeStalled());
  printInstruction("EXECUTE",   pipe.getExecuteInst(),   pipe.isExecuteStalled());
  printInstruction("MEMORY",    pipe.getMemoryInst(),    pipe.isMemoryStalled());
  printInstruction("WRITEBACK", pipe.getWritebackInst(), pipe.isWritebackStalled());

  std::cout << "--------------------------------------------------\n";

  // Dependency lists
  const auto& intDeps   = pipe.getIntDependencies();
  const auto& floatDeps = pipe.getFloatDependencies();

  std::cout << "  Int deps:  [ ";
  for (uint8_t r : intDeps) std::cout << "x" << (int)r << " ";
  std::cout << "]\n";

  std::cout << "  Float deps:[ ";
  for (uint8_t r : floatDeps) std::cout << "f" << (int)r << " ";
  std::cout << "]\n";

  // Integer registers — only print non-zero ones to keep output readable
  std::cout << "  Registers (non-zero):\n";
  int32_t* iregs = regs.getIntRegs();
  for (int i = 0; i < 32; i++) {
    if (iregs[i] != 0) {
      std::string name;
      if      (i == 0) name = "zero";
      else if (i == 1) name = "pc  ";
      else if (i == 2) name = "sp  ";
      else if (i == 3) name = "ra  ";
      else             name = "x" + std::to_string(i) + "  ";
      std::cout << "    " << std::left << std::setw(6) << name
                << " = " << std::right << std::setw(10) << iregs[i]
                << "  (0x" << std::hex << std::setw(8) << std::setfill('0')
                << static_cast<uint32_t>(iregs[i]) << ")"
                << std::dec << std::setfill(' ') << "\n";
    }
  }

  std::cout << "--------------------------------------------------\n";
}

// ─────────────────────────────────────────────
//  Program loader
//  Writes a list of 32-bit words into DRAM
//  starting at word address 0
// ─────────────────────────────────────────────

static void loadProgram(Memory& dram,
                        const std::vector<uint32_t>& words,
                        uint32_t startAddr = 0) {
  for (uint32_t i = 0; i < words.size(); i++)
    dram.writeWordDirect(startAddr + i, words[i]);
  std::cout << "  Loaded " << words.size()
            << " words into DRAM at address " << startAddr << "\n";
}

// ─────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────

int main() {

  // ── Build memory hierarchy ───────────────────
  Memory dram (DRAM_LINES, DRAM_DELAY, nullptr,  1);
  Memory cache(CACHE_LINES, CACHE_DELAY, &dram, CACHE_ASSOC);

  // ── Register file ────────────────────────────
  RegisterFile regs;

  // ── Clock stub ───────────────────────────────
  TestClock clk;

  // ── Pipeline ─────────────────────────────────
  Pipeline pipe(&cache, &regs);
  pipe.setClock(&clk);
  clk.setPipeline(&pipe);

  // ══════════════════════════════════════════════
  //  PROGRAM SETUP
  //
  //  Replace or extend this section with your
  //  test instructions. Each entry is a 32-bit
  //  encoded instruction word. The assembler is
  //  not yet complete, so raw binary is used.
  //
  //  Example layout:
  //    words[0]  = first instruction (PC=0)
  //    words[1]  = second instruction (PC=1)
  //    ...
  //    words[N]  = data values start here
  //
  //  To set initial register values for testing,
  //  use regs.writeInt() directly below.
  // ══════════════════════════════════════════════

  std::vector<uint32_t> program = {
    // ── Insert encoded instructions here ────────
    // Example: NOP sequence (all zeros decode as invalid/NOP)
    0x20019004,   // word 0 - put 100 in register 4
    0x20002805,   // word 1 - put 10 in register 5
    0x40400085,   // word 2 - store 10 into memory @ 100
    0x20019406,   // word 3 - put 101 into register 6
    0x20001407,   // word 4 - put 5 into register 7
    0x404000C7,   // word 5 - store 5 into memory @ 101
    0x40000085,   // word 6 - load 100 into register 5
    0x400000C6,   // word 7 - load 5 into register 6
    0x000014C7,   // word 8 - add 100 + 5
  };

  loadProgram(dram, program);

  // Optional: pre-load register values for testing
  // regs.writeInt(4, 100, WriteSource::SIDE_DOOR);  // x4 = 100
  // regs.writeInt(5, 200, WriteSource::SIDE_DOOR);  // x5 = 200

  // Optional: pre-load data values into DRAM
  // dram.writeWordDirect(256, 42);   // address 256 = 42
  // dram.writeWordDirect(257, 99);   // address 257 = 99

  // ── Run loop ─────────────────────────────────
  std::cout << "\nGIA Pipeline Tester\n";
  std::cout << "  DRAM:  " << DRAM_LINES << " lines, delay=" << DRAM_DELAY << "\n";
  std::cout << "  Cache: " << CACHE_LINES << " lines, delay=" << CACHE_DELAY
            << ", " << CACHE_ASSOC << "-way\n\n";
  std::cout << "Commands:\n";
  std::cout << "  <n>    Run n cycles (default 1 if blank)\n";
  std::cout << "  r      Print all registers\n";
  std::cout << "  m <a>  Print DRAM word at address a\n";
  std::cout << "  c <a>  Print cache line containing address a\n";
  std::cout << "  rst    Reset pipeline, memory, and registers\n";
  std::cout << "  q      Quit\n\n";

  // Print initial state
  uint32_t cycleCount = 0;
  printPipelineState(pipe, clk, regs, cycleCount);

  std::string line;
  while (true) {
    std::cout << "\n> ";
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) line = "1";

    std::istringstream ss(line);
    std::string cmd;
    ss >> cmd;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "q") {
      std::cout << "Exiting.\n";
      break;
    }

    if (cmd == "rst") {
      cache.reset();
      dram.reset();
      regs.reset();
      pipe.reset();
      clk.halted       = false;
      clk.retiredCount = 0;
      cycleCount       = 0;
      loadProgram(dram, program);
      std::cout << "  Reset complete.\n";
      printPipelineState(pipe, clk, regs, cycleCount);
      continue;
    }

    if (cmd == "r") {
      int32_t* ir = regs.getIntRegs();
      std::cout << "  Integer Registers:\n";
      for (int i = 0; i < 32; i++) {
        std::string name;
        if      (i == 0) name = "zero";
        else if (i == 1) name = "pc  ";
        else if (i == 2) name = "sp  ";
        else if (i == 3) name = "ra  ";
        else             name = "x" + std::to_string(i);
        std::cout << "    " << std::left << std::setw(6) << name
                  << " = " << std::right << std::setw(10) << ir[i]
                  << "  (0x" << std::hex << std::setw(8) << std::setfill('0')
                  << static_cast<uint32_t>(ir[i]) << ")"
                  << std::dec << std::setfill(' ') << "\n";
      }
      continue;
    }

    if (cmd == "m") {
      uint32_t addr;
      if (!(ss >> addr)) {
        std::cout << "  Usage: m <address>\n";
        continue;
      }
      std::cout << "  DRAM[" << addr << "] = "
                << dram.peekWord(addr)
                << "  (0x" << std::hex << std::setw(8) << std::setfill('0')
                << dram.peekWord(addr) << std::dec << std::setfill(' ') << ")\n";
      continue;
    }

    if (cmd == "c") {
      uint32_t addr;
      if (!(ss >> addr)) {
        std::cout << "  Usage: c <address>\n";
        continue;
      }
      uint32_t setIndex = (addr / WORDS_PER_LINE) % CACHE_LINES;
      const CacheLine* cl = cache.peekCacheLine(setIndex, 0);
      if (!cl) {
        std::cout << "  Cache line not found\n";
        continue;
      }
      std::cout << "  Cache set " << setIndex
                << " | valid=" << cl->valid
                << " | tag="   << cl->tag
                << " | lru="   << cl->lruCounter
                << " | data=[ ";
      for (int w = 0; w < WORDS_PER_LINE; w++)
        std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0')
                  << cl->data[w] << " ";
      std::cout << std::dec << std::setfill(' ') << "]\n";
      continue;
    }

    // Default: parse as number of cycles to run
    int numCycles = 1;
    try {
      numCycles = std::stoi(cmd);
      if (numCycles < 1) numCycles = 1;
    } catch (...) {
      std::cout << "  Unknown command. Use a number, r, m, c, rst, or q.\n";
      continue;
    }

    for (int i = 0; i < numCycles; i++) {
      if (clk.halted) {
        std::cout << "  Pipeline is halted. Use rst to reset.\n";
        break;
      }
      pipe.tick();
      cycleCount++;
    }

    printPipelineState(pipe, clk, regs, cycleCount);
  }

  return 0;
}