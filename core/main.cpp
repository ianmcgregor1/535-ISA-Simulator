// ─────────────────────────────────────────────────────────────────────────────
//  GIA Memory System – Command-Line Driver
//
//  Setup:
//    Two Memory instances are created:
//      - dram  : DRAM level (no next level, delay=3, returns lines)
//      - cache : L1 cache   (next=&dram, delay=0, returns words, associativity=1)
//    One RegisterFile holds all CPU registers.
//
//  Commands:
//    W    <value> <address> <id>      – write a word to memory (front door)
//    R    <reg> <address> <id>        – read a word from memory into register xN or fN
//                                        based on the opcode of the word at that address)
//    V    <level> <address>           – view a line (side door, no delay)
//                                       level 0 = DRAM, level 1 = cache
//    RG                               – view all integer registers
//    LOAD <filename>                  – load hex file from programs/ folder into DRAM
//    S                                – print hit/miss stats
//    RST                              – reset memory and registers
//    Q                                – quit
//
//  Access IDs for <id> argument:
//    0 = FETCH, 1 = EXECUTE, 2 = L1, 3 = UI
//
//  Memory responds to W/R with "wait" or "done" each cycle.
//  V/RG/RF/LOAD always respond immediately with no delay.
//  A running count of all W and R commands is tracked as a clock-cycle proxy.
// ─────────────────────────────────────────────────────────────────────────────

#include "../memory/memory.h"
#include "registers.h"
#include "decoder.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cstring>

// ─────────────────────────────────────────────
//  Configuration constants (matching lec10 demo requirements)
// ─────────────────────────────────────────────
static constexpr uint32_t DRAM_LINES      = 8192;  // 8192 lines * 4 words = 32K words
static constexpr uint32_t DRAM_DELAY      = 3;
static constexpr uint32_t CACHE_LINES_CFG = 8;     // minimum 8 lines per demo spec
static constexpr uint32_t CACHE_DELAY     = 0;
static constexpr uint32_t CACHE_ASSOC     = 1;     // direct-mapped

// ─────────────────────────────────────────────
//  Helper: parse AccessID from integer
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
//  Helper: print a DRAM line (side door)
// ─────────────────────────────────────────────
static void printDRAMLine(Memory& dram, uint32_t address) {
    uint32_t lineIndex = address / WORDS_PER_LINE;
    const uint32_t* line = dram.peekLine(lineIndex);
    std::cout << "  DRAM line " << lineIndex
              << " (addr " << address << ")"
              << " | data=[ ";
    for (int w = 0; w < WORDS_PER_LINE; w++)
        std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0')
                  << line[w] << " ";
    std::cout << std::dec << "]\n";
}

// ─────────────────────────────────────────────
//  Helper: print a cache line with metadata (side door)
// ─────────────────────────────────────────────
static void printCacheLine(Memory& cache, uint32_t address) {
    uint32_t setIndex = (address / WORDS_PER_LINE) % CACHE_LINES_CFG;
    uint32_t way      = 0;

    const CacheLine* cl = cache.peekCacheLine(setIndex, way);
    if (cl == nullptr) {
        std::cout << "  [cache] peekCacheLine returned null (cache disabled?)\n";
        return;
    }

    std::cout << "  Cache set " << setIndex
              << " way " << way
              << " | tag="   << cl->tag
              << " | valid=" << (cl->valid ? "1" : "0")
              << " | lru="   << cl->lruCounter
              << " | data=[ ";
    for (int w = 0; w < WORDS_PER_LINE; w++)
        std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0')
                  << cl->data[w] << " ";
    std::cout << std::dec << "]\n";
}

// ─────────────────────────────────────────────
//  Helper: print stats for both levels
// ─────────────────────────────────────────────
static void printStats(Memory& cache, Memory& dram, int cmdCount) {
    std::cout << "  Cache hits   : " << cache.getHitCount()  << "\n"
              << "  Cache misses : " << cache.getMissCount() << "\n"
              << "  Miss rate    : " << std::fixed << std::setprecision(2)
              << cache.getMissRate() * 100.0f << "%\n"
              << "  Total W+R cmds (clock-cycle proxy): " << cmdCount << "\n";
}

// ─────────────────────────────────────────────
//  Helper: print all integer registers (side door)
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
//  Helper: load hex file from programs/ into DRAM (side door)
// ─────────────────────────────────────────────
static void loadHexFile(Memory& dram, const std::string& filename) {
    std::string path = "../programs/" + filename;
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

// ─────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────
int main() {
    // Build memory hierarchy: cache -> dram
    Memory dram (DRAM_LINES,      DRAM_DELAY,  nullptr, 1);
    Memory cache(CACHE_LINES_CFG, CACHE_DELAY, &dram,   CACHE_ASSOC);

    // Register file
    RegisterFile regs;

    int cmdCount = 0;  // proxy for clock cycles (counts W and R commands)

    std::cout << "GIA Memory System Driver\n"
              << "  DRAM : " << DRAM_LINES << " lines, delay=" << DRAM_DELAY << "\n"
              << "  Cache: " << CACHE_LINES_CFG << " lines, delay=" << CACHE_DELAY
              << ", " << CACHE_ASSOC << "-way\n"
              << "  Registers: 32 integer (x0-x31)\n\n"
              << "Commands:\n"
              << "  W    <value> <address> <id>   write word to memory\n"
              << "  R    <reg> <address> <id>     read word from memory into xN or fN\n"
              << "  V    <level> <address>         view line (0=DRAM, 1=cache)\n"
              << "  RG                             view integer registers\n"
              << "  LOAD <filename>                load hex file from programs/ into DRAM\n"
              << "  S                              stats\n"
              << "  RST                            reset memory and registers\n"
              << "  Q                              quit\n"
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

        // ── QUIT ────────────────────────────────
        if (cmd == "Q") {
            std::cout << "Exiting.\n";
            break;
        }

        // ── STATS ───────────────────────────────
        if (cmd == "S") {
            printStats(cache, dram, cmdCount);
            continue;
        }

        // ── RESET ───────────────────────────────
        if (cmd == "RST") {
            cache.reset();
            dram.reset();
            regs.reset();
            cmdCount = 0;
            std::cout << "  Memory and registers reset.\n";
            continue;
        }

        // ── WRITE ───────────────────────────────
        if (cmd == "W") {
            uint32_t val, addr;
            int rawID;
            if (!(ss >> val >> addr >> rawID)) {
                std::cout << "  Usage: W <value> <address> <id>\n";
                continue;
            }
            AccessID id = parseID(rawID);
            ++cmdCount;

            MemoryResponse resp;
            int cycles = 0;
            do {
                resp = cache.storeWord(addr, val, id);
                if (resp.status == MemoryResponse::Status::WAIT) {
                    std::cout << "  wait\n";
                    ++cycles;
                }
            } while (resp.status == MemoryResponse::Status::WAIT);

            std::cout << "  done  (took " << cycles << " wait cycles)\n";
            continue;
        }

        // ── READ ────────────────────────────────
        // Fetches a word from memory, then routes the result to the correct register file:
        // (reg 0-31, normal write-source rules apply)
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

            MemoryResponse resp;
            int cycles = 0;
            do {
                resp = cache.loadWord(addr, id);
                if (resp.status == MemoryResponse::Status::WAIT) {
                    std::cout << "  wait\n";
                    ++cycles;
                }
            } while (resp.status == MemoryResponse::Status::WAIT);

            // Integer or other instruction type — write to integer register file
            regs.writeInt(static_cast<uint8_t>(regN),
                          static_cast<int32_t>(resp.word),
                          WriteSource::LOAD_STORE);
            std::cout << "  done  x" << regN << " = 0x"
                      << std::hex << std::setw(8) << std::setfill('0')
                      << resp.word << std::dec << std::setfill(' ')
                      << "  [int]  (took " << cycles << " wait cycles)\n";
            // continue;
        }

        // ── VIEW ────────────────────────────────
        if (cmd == "V") {
            int level;
            uint32_t addr;
            if (!(ss >> level >> addr)) {
                std::cout << "  Usage: V <level(0=DRAM, 1=cache)> <address>\n";
                continue;
            }
            if (level == 0)
                printDRAMLine(dram, addr);
            else if (level == 1)
                printCacheLine(cache, addr);
            else
                std::cout << "  Level must be 0 (DRAM) or 1 (cache)\n";
            continue;
        }

        // ── VIEW INTEGER REGISTERS ───────────────
        if (cmd == "RG") {
            printIntRegs(regs);
            continue;
        }

        // ── LOAD HEX FILE INTO DRAM ─────────────
        if (cmd == "LOAD") {
            std::string filename;
            if (!(ss >> filename)) {
                std::cout << "  Usage: LOAD <filename>\n";
                continue;
            }
            loadHexFile(dram, filename);
            continue;
        }

        std::cout << "  Unknown command. Use W, R, V, RG, RF, LOAD, S, RST, or Q.\n";
    }

    return 0;
}