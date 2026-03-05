#ifndef BUS_H
#define BUS_H

#include <optional>
#include "memory.h"
#include <iostream>

enum class BusClient { None, Fetch, Mem };

struct MemoryBus {
    BusClient owner = BusClient::None;
};

//
// // Set the owner to Fetch
// bus.owner = BusClient::Fetch;
//
// // Set the owner to Mem stage
// bus.owner = BusClient::Mem;
//
// // Reset owner to None
// bus.owner = BusClient::None;
// if (bus.owner == BusClient::Fetch) {
//     // bus is currently owned by the Fetch stage
//     std::cout << "Fetch stage owns the bus\n";
// }

enum class BusOwner { NONE, FETCH, MEMORY };

class Bus {
    Memory& cache;
    Memory& dram;
    BusOwner owner;

    bool filling = false;
    word fill_address = 0;
    bool fill_write = false;
    word fill_wdata = 0;

    uint32_t cycle_count = 0;

    void fill(word address);
    void evictHandler(word address);

public:
    Bus(Memory& cache, Memory& dram);

    std::optional<word> readWord(word address, BusOwner caller);
    std::optional<word> writeWord(word data, word address, BusOwner caller);

    void cycle();

    void dump() const;
};

#endif //BUS_H
