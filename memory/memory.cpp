#include "memory.h"

std::array<int, 3> Memory::addressFind(word address) {
    if (mode == MEMmode::DRAM) {
        int wordOffset = static_cast<int>(address & 0x7);
        int lineIndex = static_cast<int>((address >> 3) & 0x3F);
        return { wordOffset, lineIndex, 0 };
    } else {
        int wordOffset = static_cast<int>(address & 0x7);
        int lineIndex = static_cast<int>((address >> 3) & 0x7);
        int tag = static_cast<int>(address >> 6);
        return { wordOffset, lineIndex, tag };
    }
}

int Memory::findWay(int setIndex, word tag) const {
    for (int w = 0; w < WAYS; ++w) {
        int i = setIndex * WAYS + w;
        if (memory_data[i].valid && memory_data[i].data[w] == tag) {
            return w;
        }
    }
    return -1;
}

int Memory::evictLRU(int setIndex) const {
    for (int w = 0; w < WAYS; ++w) {
        int i = setIndex * WAYS + w;
        if (memory_data[i].valid) {
            return w;
        }
    }
    return 0;
}

Memory::Memory() : mode(MEMmode::DRAM), memDelay(0),
cByte(0), cLine(0), blocked(false), cooldown(0), dataReady(false) {}

Memory::Memory(MEMmode mode, byte_ delay) : mode(MEMmode::DRAM),
memDelay(0), cByte(0), cLine(0), blocked(false),
cooldown(0), dataReady(false) {}

void Memory::initialize() {}

word Memory::readWord(word address) {
    dataReady = false;
    auto loc = addressFind(address);

    if (mode == MEMmode::DRAM) {
        return memory_data[loc[1]].data[loc[0]];
    } else {
        int way = findWay(loc[1], static_cast<word>(loc[2]));
        if (way < 0) {
            throw std::runtime_error("Way not found");
        }
        int lineIndex = loc[1] * WAYS + way;
        return memory_data[lineIndex].data[loc[0]];
    }
};

void Memory::writeWord(word data, word address) {
    dataReady = false;
    auto loc = addressFind(address);

    if (mode == MEMmode::DRAM) {
        memory_data[loc[1]].data[loc[0]] = data;
    } else {
        int way = findWay(loc[1], static_cast<word>(loc[2]));
        if (way < 0) {
            throw std::runtime_error("Way not found");
        }
        int lineIndex = loc[1] * WAYS + way;
        memory_data[lineIndex].data[loc[0]] = data;
        memory_data[lineIndex].dirty = true;
    }
};

word Memory::writeWordED(word data) {
    memory_data[cLine].data[cByte] = data;
    cByte++;

    if (cByte >= LINE_SIZE) {
        cByte = 0;
        cLine++;
    }
    return 0;
}

// return true if the memory is blocked
// return false if the memory is avaliable to use
byte_ Memory::isBusy() {

    // if there is no cooldown
    if (memDelay == 0) return false;

    // memory is blocked, i.e. cooldown > 0
    if (blocked) return true;

    // A request for the data that is ready
    if (dataReady) return false;

    // There is a new request for new data
    blocked = true;
    cooldown = memDelay;
    return true;
}

int Memory::cycle() {
    // if memory next cycle goes to 0, dataReady
    if ((cooldown - 1) == 0) dataReady = true;

    // If memory is in cooldown, minus 1
    if (cooldown > 0) cooldown--;

    // if cooldown is no longer blocked
    if (cooldown == 0) blocked = false;

    return 0;
};

bool Memory::isValid(word address) const {
    auto loc = const_cast<Memory*>(this)->addressFind(address);
    int way = findWay(loc[1], static_cast<word>(loc[2]));
    if (way < 0) return false;
    return memory_data[loc[1] * WAYS + way].valid;
}

bool Memory::isDirty(word address) const {
    auto loc = const_cast<Memory*>(this)->addressFind(address);
    int way = findWay(loc[1], static_cast<word>(loc[2]));
    if (way < 0) return false;
    return memory_data[loc[1] * WAYS + way].dirty;
}

word Memory::getTag(word address) const {
    auto loc = const_cast<Memory*>(this)->addressFind(address);
    int way = findWay(loc[1], static_cast<word>(loc[2]));
    if (way < 0) return 0xFFFFFFFF;
    return memory_data[loc[1] * WAYS + way].tag;
}

void Memory::setValid(word address, bool v) {
    auto loc = addressFind(address);
    int way = findWay(loc[1], static_cast<word>(loc[2]));
    if (way < 0) return;
    memory_data[loc[1] * WAYS + way].valid = v;
}

void Memory::setDirty(word address, bool d) {
    auto loc = addressFind(address);
    int way = findWay(loc[1], static_cast<word>(loc[2]));
    if (way < 0) return;
    memory_data[loc[1] * WAYS + way].dirty = d;
}

void Memory::fillLine(word address, const memory_line &line,
    uint8_t owner_id, uint32_t cycle) {

    auto loc = addressFind(address);
    int setIndex = loc[1];
    word tag = static_cast<word>(loc[2]);

    int target = evictLRU(setIndex);
    int lineIndex = setIndex * WAYS + target;

    memory_data[lineIndex] = line;
    memory_data[lineIndex].valid = true;
    memory_data[lineIndex].dirty = false;
    memory_data[lineIndex].tag = tag;
    memory_data[lineIndex].last_owner = owner_id;
    memory_data[lineIndex].last_cycle = cycle;
}

memory_line Memory::evictLine(word address) {

    auto loc = addressFind(address);
    int setIndex = loc[1];
    word tag = static_cast<word>(loc[2]);

    int way = findWay(setIndex, tag);
    if (way < 0) return memory_line{};

    int lineIndex = setIndex * WAYS + way;
    memory_line evicted = memory_data[lineIndex];

    memory_data[lineIndex].valid = false;
    memory_data[lineIndex].dirty = false;

    return evicted;
}

void Memory::dumpMem() const {
    std::string filename = (mode == MEMmode::DRAM) ? "memory_dump.txt" : "cache_dump.txt";
    std::ofstream outfile(filename);
    if (!outfile) {
        std::cerr << "Error opening " << filename << " for memory dump!\n";
        return;
    }

    // Loop over all lines
    if (mode == MEMmode::DRAM) {
        for (int line = 0; line < MEMORY_LINE_SIZE; ++line) {
            outfile << "Line " << std::dec << std::setw(4) << std::setfill('0') << line << ": ";

            // Loop over words in this line
            for (int w = 0; w < LINE_SIZE; ++w) {
                outfile << "0x"
                        << std::hex << std::uppercase
                        << std::setw(8) << std::setfill('0')
                        << memory_data[line].data[w] << " ";
            }

            outfile << "\n"; // new line after each memory line
        }
    } else {
        // Cache dump
        outfile << std::left
        << std::setw(5) << "Set"
        << std::setw(5) << "Way"
        << std::setw(7) << "Valid"
        << std::setw(7) << "Dirty"
        << std::setw(12) << "Tag"
        << std::setw(6) << "Owner"
        << std::setw(8) << "Cycle"
        << "Data\n"
        << std::string(70, '-') << "\n";

        for (int s = 0; s < SETS; ++s) {
            for (int w = 0; w < WAYS; ++w) {
                int index = s * WAYS + w;
                const auto& ln = memory_data[index];
                outfile << std::dec
                << std::setw(5) << s
                << std::setw(5) << w
                << std::setw(7) << (ln.valid ? "Y" : "N")
                << std::setw(7) << (ln.dirty ? "Y" : "N")
                << "0x" << std::hex
                << std::setw(8) << std::setfill('0') << ln.tag
                << std::dec << std::setfill(' ')
                << std::setw(6) << ln.last_owner
                << std::setw(8) << ln.last_cycle
                << " [";

                for (int d = 0; d < LINE_SIZE; ++d) {
                    outfile << " 0x" << std::hex
                    << std::setw(8) << std::setfill('0')
                    << ln.data[d];
                }

                outfile << " ]\n" << std::dec << std::setfill(' ');
            }
        }
    }

    outfile.close();
    std::cout << "Memory dumped to " << filename << "\n";
}

void Memory::loadMem(const std::string& file) {
    // Build relative path
    std::string path = "../assembly/" + file;

    std::ifstream infile(path);
    if (!infile.is_open()) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return;
    }

    std::string line;
    while (std::getline(infile, line)) {
        word data = static_cast<word>(std::stoul(line, nullptr, 16));
        memory_data[cLine].data[cByte] = data;
        cByte++;

        if (cByte >= LINE_SIZE) {
            cByte = 0;
            cLine++;
        }
    }

    infile.close();
}
