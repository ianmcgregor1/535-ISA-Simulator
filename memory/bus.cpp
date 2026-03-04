#include <optional>
#include "bus.h"

Bus::Bus(Memory& cache, Memory& dram)
: cache(cache), dram(dram), owner(BusOwner::NONE) {}

std::optional<word> Bus::readWord(word address, BusOwner caller) {

    if (owner == BusOwner::NONE)
        owner = caller;
    else if(owner != caller)
        return std::nullopt;

    if (filling) {
        if (dram.isBusy()) { // check if the memory is busy
            return std::nullopt;
        }

        fill(fill_address);

        if (fill_write) cache.writeWord(fill_wdata, fill_address);

        filling = false;
        owner = BusOwner::NONE;
        owner = caller;
    }

    if (cache.isValid(address)) {
        word data = cache.readWord(address);
        owner = BusOwner::NONE;
        return data;
    }

    if (dram.isBusy()) return std::nullopt;

    evictHandler(address);

    filling = true;
    fill_address = address;
    fill_write = false;
    return std::nullopt;
};


std::optional<word> Bus::writeWord(word data, word address, BusOwner caller) {
    if (owner == BusOwner::NONE)
        owner = caller;
    else if(owner != caller)
        return std::nullopt;

    if (filling) {
        if (dram.isBusy()) { // check if the memory is busy
            return std::nullopt;
        }

        fill(fill_address);

        if (fill_write) cache.writeWord(fill_wdata, fill_address);

        filling = false;
        owner = BusOwner::NONE;
        owner = caller;
    }

    if (cache.isValid(address)) {
        cache.writeWord(data, address);
        owner = BusOwner::NONE;
        return 0;
    }

    if (dram.isBusy()) return std::nullopt;

    evictHandler(address);

    filling = true;
    fill_address = address;
    fill_write = true;
    fill_wdata = data;
    return std::nullopt;
}

void Bus::dump() const {
    dram.dumpMem();
    cache.dumpMem();
}

void Bus::fill(word address) {
    word base = address & ~(word)(LINE_SIZE - 1);

    memory_line newLine;
    for (int i = 0; i < LINE_SIZE; ++i) {
        newLine.data[i] = dram.readWord(base + i);
    }

    uint8_t oid = (owner == BusOwner::FETCH) ? 1 :
    (owner == BusOwner::MEMORY) ? 2 : 0;

    cache.fillLine(address, newLine, oid, cycle_count);
}

void Bus::evictHandler(word address) {
    memory_line evicted = cache.evictLine(address);

    if (!evicted.valid || !evicted.dirty) return;

    word setIndex = (address >> 3) & 0x7;
    word dramBase = (evicted.tag << 6) | (setIndex << 3);

    for (int i = 0; i < LINE_SIZE; ++i) {
        dram.writeWord(evicted.data[i], dramBase + i);
    }
}