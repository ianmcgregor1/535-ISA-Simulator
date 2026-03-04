#ifndef MEMORY_H
#define MEMORY_H

#include "defines.h"
#include <array>
#include <optional>
#include <iomanip>
#include <iostream>
#include <string>
#include <stdexcept>

#define LINE_SIZE 8
#define REG_NUM 32 // num of registers, not really bits
#define MAXMEMORY 2048 // 2KB, 2^11

constexpr word LINE_SIZE_BYTE = LINE_SIZE*4;
constexpr word MEMORY_LINE_SIZE = MAXMEMORY/LINE_SIZE_BYTE;

#define SETS 8
#define WAYS 2

enum class MEMmode { DRAM, CACHE };

typedef struct memory_line {
    word data[LINE_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0}; // force 0 at init

    // Cache metadata - only accessible when in CACHE mode
    bool valid = false;
    bool dirty = false;
    word tag = 0;
    uint8_t last_owner = 0xFF;
    uint32_t last_cycle = 0;

} memory_line;

class Memory {

    word cByte;
    word cLine;

    byte_ blocked;
    byte_ cooldown;
    bool dataReady;

    memory_line memory_data[MEMORY_LINE_SIZE]; // 2000 * 8 * 4 = 65kb

    MEMmode mode;
    byte_ memDelay;

    std::array<int, 3> addressFind(word address);

    int findWay(int setIndex, word tag) const;

    int evictLRU(int setIndex) const;

public:
    Memory();

    Memory(MEMmode mode, byte_ delay);

    void initialize();

    word readWord(word address);

    void writeWord(word data, word address);

    word writeWordED(word data);

    byte_ isBusy();

    int cycle();

    bool isValid(word address) const;
    bool isDirty(word address) const;
    word getTag(word address) const;

    void setValid(word address, bool v);
    void setDirty(word address, bool d);

    void fillLine(word address, const memory_line &line,
        uint8_t owner_id = 0xFF, uint32_t cycle = 0);

    memory_line evictLine(word address);

    void dumpMem() const;

    void loadMem(const std::string& file);

};

#endif //MEMORY_H
