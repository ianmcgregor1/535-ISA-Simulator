#include "../memory/memory.h"
#include "../core/registers.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <filesystem>


static constexpr uint32_t DRAM_LINES      = 8192;  // 8192 lines * 4 words = 32K words
static constexpr uint32_t DRAM_DELAY      = 3;
static constexpr uint32_t CACHE_LINES_CFG = 8;     // minimum 8 lines per demo spec
static constexpr uint32_t CACHE_DELAY     = 0;
static constexpr uint32_t CACHE_ASSOC     = 4;     // N-way set associative

// Helper functions

// parse AccessID from integer
static AccessID parseID(int raw) {
    switch (raw) {
        case 0:  return AccessID::FETCH;
        case 1:  return AccessID::EXECUTE;
        case 2:  return AccessID::L1;
        case 3:  return AccessID::UI;
        default:
            std::cout << "  [warn] unknown ID " << raw << ", defaulting to EXECUTE\n";
            return AccessID::EXECUTE;
    }
}

// print a DRAM line (side door)
static void printDRAMLine(Memory& dram, uint32_t address) {
    uint32_t lineIndex = address / WORDS_PER_LINE;
    const uint32_t* line = dram.peekLine(lineIndex);
    std::cout << "  DRAM line " << lineIndex
              << " (addr " << address << ")"
              << " | data=[ ";
    for (int w = 0; w < WORDS_PER_LINE; w++)
        std::cout << line[w] << ", ";
    std::cout << std::dec << "]\n";
}

//  print a cache line with metadata (side door)
static void printCacheLine(Memory& cache, uint32_t setIndex, uint32_t numWays) {
  for (uint32_t way = 0; way < numWays; way++) {
    const CacheLine* cl = cache.peekCacheLine(setIndex, way);
    if (cl == nullptr) {
      std::cout << "  Cache set " << setIndex << " way " << way
                << " | [null - invalid set/way]\n";
      continue;
    }
    std::cout << "  Cache set " << setIndex
              << " way " << way
              << " | tag="   << cl->tag
              << " | valid=" << (cl->valid ? "1" : "0")
              << " | lru="   << cl->lruCounter
              << " | data=[ ";
    for (int w = 0; w < WORDS_PER_LINE; w++)
      std::cout << cl->data[w] << ", ";
    std::cout << std::dec << "]\n";
  }
}

// print stats for both levels
static void printStats(Memory& cache, Memory& dram, int cmdCount) {
    std::cout << "  Cache hits   : " << cache.getHitCount()  << "\n"
              << "  Cache misses : " << cache.getMissCount() << "\n"
              << "  Miss rate    : " << std::fixed << std::setprecision(2)
              << cache.getMissRate() * 100.0f << "%\n"
              << "  DRAM hits    : " << dram.getHitCount()   << "\n"
              << "  DRAM misses  : " << dram.getMissCount()  << "\n"
              << "  Total W+R cmds (clock cycle): " << cmdCount << "\n";
}

// print all integer registers (side door)
static void printIntRegs(RegisterFile& regs) {
    int32_t* r = regs.getIntRegs();
    std::cout << "  Integer Registers:\n";
    for (int i = 0; i < 32; i++) {
        std::string label;
        if      (i == 0) label = " (zero)";
        else if (i == 1) label = " (pc)  ";
        else if (i == 2) label = " (sp)  ";
        else if (i == 3) label = " (ra)  ";
        else             label = "       ";

        std::cout << "    x" << std::left << std::setw(2) << i << label
                  << " = " << std::right
                  << std::dec << std::setw(12) << r[i]
                  << "  (0x" << std::hex << std::setw(8) << std::setfill('0')
                  << static_cast<uint32_t>(r[i]) << ")"
                  << std::dec << std::setfill(' ') << "\n";
    }
}

// load hex file from programs/ into DRAM (side door)
static void loadHexFile(Memory& dram, const std::string& filename) {
    std::string path = "programs/" + filename;

    std::cout << "  Loading " << path << " into DRAM...\n";
    std::ifstream file(path);
    if (!file) {
        std::cout << "  Error: could not open " << path << "\n";
        return;
    }

    uint32_t address = 0;
    std::string token;
    while (file >> token) {
        uint32_t word = static_cast<uint32_t>(std::stoul(token, nullptr, 16));
        dram.writeWordDirect(address, word);
        address++;
    }

    std::cout << "  Loaded " << address << " word"
              << (address == 1 ? "" : "s")
              << " from " << path
              << " into DRAM starting at address 0\n";
}

int main() {
    // Build memory hierarchy
    Memory dram (DRAM_LINES,      DRAM_DELAY,  nullptr, 1);
    Memory cache(CACHE_LINES_CFG, CACHE_DELAY, &dram,   CACHE_ASSOC);

    // Register file
    RegisterFile regs;

    int cmdCount = 0;  // proxy for clock cycles for UI

    std::cout << "GIA Demo 1:\n"
              << "  DRAM : " << DRAM_LINES << " lines, delay=" << DRAM_DELAY << "\n"
              << "  Cache: " << CACHE_LINES_CFG << " lines, delay=" << CACHE_DELAY
              << ", " << CACHE_ASSOC << "-way\n"
              << "  Registers: 32 integer (x0-x31)\n\n"
              << "Commands:\n"
              << "  W    <M/C> <value> <address> <id>   Write to Memory\n"
              << "  R    <reg> <address> <id>            Read Word into xN\n"
              << "  V    <M/C> <startLine> <endLine>     View lines (M=DRAM, C=Cache)\n"
              << "  RG                                   View Registers\n"
              << "  LOAD <filename>                      Load .hex file\n"
              << "  S                                    Hit/Miss Stats\n"
              << "  RST                                  Reset memory and registers\n"
              << "  Q                                    Quit\n"
              << "  (id: 0=FETCH 1=EXECUTE 2=L1 3=UI)\n\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;

        // Case-insensitive command match
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

        // QUIT
        if (cmd == "Q") {
            std::cout << "Exiting...\n";
            break;
        }

        // STATS
        if (cmd == "S") {
            printStats(cache, dram, cmdCount);
            continue;
        }

        // RESET
        if (cmd == "RST") {
            cache.reset();
            dram.reset();
            regs.reset();
            cmdCount = 0;
            std::cout << "  Memory and registers reset.\n";
            continue;
        }

        // WRITE
        if (cmd == "W") {
            uint32_t val, addr;
            char level;
            int rawID;
            if (!(ss >> level >> val >> addr >> rawID)) {
                std::cout << "  Usage: W <M/C> <value> <address> <id>\n";
                continue;
            }
            level = toupper(level);
            if (level != 'M' && level != 'C') {
                std::cout << "  Level must be M (DRAM) or C (cache)\n";
                continue;
            }
            AccessID id = parseID(rawID);
            ++cmdCount;

            Memory& target = (level == 'M') ? dram : cache;
            MemoryResponse resp = target.storeWord(addr, val, id);
            if (resp.status == MemoryResponse::Status::WAIT) {
                int remaining = (target.getCurrentID() == AccessID::NONE)
                              ? static_cast<int>(target.getAccessDelay())
                              : target.getDelayCount() + 1;
                std::cout << "  WAIT (" << remaining << " cycles remaining)\n";
            } else {
                std::cout << "  DONE\n";
            }
            continue;
        }

        // Write Direct (no cycle count)
        if (cmd == "WD") {
          uint32_t val, addr;
          if (!(ss >> val >> addr)) { std::cout << "  Usage: WD <value> <address>\n"; continue; }
          dram.writeWordDirect(addr, val);
          std::cout << "  Direct write: " << val << " -> DRAM[" << addr << "]\n";
          continue;
        }

        // READ 
        // One command = one clock cycle. Caller must re-issue each cycle until
        // done. On a miss the memory system counts down internally across calls.
        // Switching to a different cache line mid-miss resets the countdown.
        if (cmd == "R") {
            int regN;
            uint32_t addr;
            int rawID;
            if (!(ss >> regN >> addr >> rawID) || regN < 0 || regN > 31) {
                std::cout << "  Usage: R <reg(0-31)> <address> <id>\n";
                continue;
            }
            AccessID id = parseID(rawID);
            ++cmdCount;

            MemoryResponse resp = cache.loadWord(addr, id);

            if (resp.status == MemoryResponse::Status::WAIT) {
                int remaining = (dram.getCurrentID() == AccessID::NONE)
                              ? static_cast<int>(dram.getAccessDelay())
                              : dram.getDelayCount() + 1;
                std::cout << "  WAIT (" << remaining << " cycles remaining)\n";
                continue;
            }
            std::cout << "  READ complete:\n Word: " << resp.word;

            // Done — write to integer register file
            regs.writeInt(static_cast<uint8_t>(regN),
                          static_cast<int32_t>(resp.word),
                          WriteSource::LOAD_STORE);
            std::cout << "  DONE  x" << regN << " = "
                      << resp.word << std::dec << std::setfill(' ') << "\n";
            continue;
        }

        // VIEW
        if (cmd == "V") {
            char level;
            uint32_t min, max;
            if (!(ss >> level >> min >> max)) {
                std::cout << "  Usage: V <M/C> <startLine> <endLine>\n";
                continue;
            }
            if (min > max) {
                std::cout << "  startLine must be <= endLine\n";
                continue;
            }
            level = toupper(level);
            if (level == 'M')
                for (uint32_t m = min; m <= max; ++m) printDRAMLine(dram, m * WORDS_PER_LINE);
            else if (level == 'C')
                for (uint32_t m = min; m <= max; ++m) printCacheLine(cache, m, CACHE_ASSOC);
            else
                std::cout << "  Level must be M (DRAM) or C (cache)\n";
            continue;
        }

        // VIEW INTEGER REGISTERS 
        if (cmd == "RG") {
            printIntRegs(regs);
            continue;
        }

        // LOAD HEX FILE INTO DRAM 
        if (cmd == "LOAD") {
            std::string filename;
            if (!(ss >> filename)) {
                std::cout << "  Usage: LOAD <filename>\n";
                continue;
            }
            loadHexFile(dram, filename);
            continue;
        }

        std::cout << "  Unknown command. Use W, R, V, RG, LOAD, S, RST, or Q.\n";
    }

    return 0;
}