#include "memory.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <climits>
#include <iostream>

Memory::Memory(uint32_t numLines, uint32_t delay, Memory* nextLevel, uint32_t associativity) :
    numLines(numLines),
    accessDelay(delay),
    associativity(associativity),
    numSets(numLines / associativity),
    isCache(nextLevel != nullptr),
    cacheEnabled(nextLevel != nullptr),
    nextLevel(nextLevel),
    busy(false),
    currentID(AccessID::NONE),
    currentOp(MemOp::LOAD),
    currentAddress(0),
    currentData(0),
    delayCount(-1),
    pendingWriteThrough(false),
    hitCount(0),
    missCount(0)
{
  // Instantiate to 0
  for (int w = 0; w < WORDS_PER_LINE; w++)
    data[w].assign(numLines, 0);

  // Only allocate cache metadata if this is a cache level
  if (isCache)
    lines.resize(numSets, std::vector<CacheLine>(associativity));
}

/*
* Front door methods - called by pipeline stages or higher levels of memory
*/

// Read a word or line, returns WAIT if busy
MemoryResponse Memory::load(uint32_t address, AccessID id, bool isLine) {

  // If level is a cache and disabled, forward address directly to nextLevel without doing anything
  if (isCache && !cacheEnabled && nextLevel != nullptr) {
    MemoryResponse r = loadFromNext(address);
    return r;
  }

  // If not busy, accept request and set delay
  if (!busy) {
    busy                = true;
    currentID           = id;
    currentAddress      = address;
    delayCount          = accessDelay;
    pendingWriteThrough    = false;
  }

  // If request ID does not match, return WAIT
  if (currentID != id) {
    return MemoryResponse::resWait();
  }


  // If count is nonzero, decrement and return WAIT
  if (delayCount > 0) {
    delayCount--;
    return MemoryResponse::resWait();
  }


  /*
  * Count is zero - read the data and react accordingly
  */

  // If this is the DRAM level, return data
  if (!isCache) {
    finishOperation();
    if (isLine) { // Should always be true, but adding word functionality in case
      uint32_t lineIndex = currentAddress / WORDS_PER_LINE;
      uint32_t lineData[WORDS_PER_LINE];
      for (int w = 0; w < WORDS_PER_LINE; w++)
        lineData[w] = data[w][lineIndex];
      return MemoryResponse::resLine(lineData);
    }
    return MemoryResponse::resWord(readWord(currentAddress));
  }

  // Otherwise, this is the cache level. Check for hit/miss and return or forward to next level as needed.
  uint32_t tag = address / (WORDS_PER_LINE * numSets);
  uint32_t setIndex = (address / WORDS_PER_LINE) % numSets;
  uint32_t wordOffset = address % WORDS_PER_LINE;


  // Find matching way in set, if it exists
  int way = findWay(setIndex, tag);


  if (way != -1) {
    // Hit — return data
    hitCount++;
    updateLRU(setIndex, way);
    busy = false;
    if (isLine) {
      return MemoryResponse::resLine(lines[setIndex][way].data);
    }
    return MemoryResponse::resWord(lines[setIndex][way].data[wordOffset]);

  } else {
    // Miss - call next level
    if (nextLevel == nullptr) {
      std::cerr << "Memory::read: miss at bottom level\n";
      return MemoryResponse::resWait(); // This shouldn't happen
    }

    MemoryResponse r = loadFromNext(currentAddress);
    // Forward a WAIT response
    if (r.status == MemoryResponse::Status::WAIT)
      return r;

    // Update miss count here so it isn't incremented on every cycle of waiting for the next level
    missCount++;

    // Got data from lower level, update cache and return
    int evictWay = findLRUWay(setIndex);
    lines[setIndex][evictWay].valid = true;
    lines[setIndex][evictWay].tag = tag;
    for (int w = 0; w < WORDS_PER_LINE; w++)
      lines[setIndex][evictWay].data[w] = r.line[w];
    updateLRU(setIndex, evictWay);

    // Operation complete, clear busy and return data
    finishOperation();

    if (isLine) {
      return MemoryResponse::resLine(lines[setIndex][evictWay].data);
    }
    return MemoryResponse::resWord(lines[setIndex][evictWay].data[wordOffset]);
  }
}

// Write a single word, returns WAIT if busy
MemoryResponse Memory::storeWord(uint32_t address, uint32_t inData, AccessID id) {

  // If level is a cache and disabled, forward directly to nextLevel
  if (isCache && !cacheEnabled && nextLevel != nullptr) {
    return storeWordNext(address, inData);
  }

  // If not busy, accept request and set delay
  if (!busy) {
    busy                    = true;
    currentID               = id;
    currentAddress          = address;
    currentData             = inData;
    delayCount              = accessDelay;
    pendingWriteThrough     = false;
  }

  // If request ID does not match, return WAIT
  if (currentID != id) {
    return MemoryResponse::resWait();
  }

  // Write-through in progress - keep forwarding to nextLevel until OK
  if (pendingWriteThrough) {
    MemoryResponse r = storeWordNext(currentAddress, currentData);
    if (r.status == MemoryResponse::Status::WAIT)
      return r;
    finishOperation();
    return r;
  }

  // If count is nonzero, decrement and return WAIT
  if (delayCount > 0) {
    delayCount--;
    return MemoryResponse::resWait();
  }


  /*
  * Count is zero - write the data and react accordingly
  */

  // If this is the DRAM level, write directly to flat storage
  if (!isCache) {
    writeWordDirect(currentAddress, currentData);
    finishOperation();
    return MemoryResponse::resWord(0);
  }

  // Otherwise, this is the cache level
  uint32_t tag = currentAddress / (WORDS_PER_LINE * numSets);
  uint32_t setIndex = (currentAddress / WORDS_PER_LINE) % numSets;
  uint32_t wordOffset = currentAddress % WORDS_PER_LINE;

  int way = findWay(setIndex, tag);

  if (way != -1) {
    // Hit - write word to cache
    hitCount++;
    lines[setIndex][way].data[wordOffset] = currentData;
    updateLRU(setIndex, way);
  } else {
    // Miss - no-allocate, so skip cache and write-through only
    missCount++;
    if (nextLevel == nullptr) {
      std::cerr << "Memory::storeWord: miss at bottom level\n";
      return MemoryResponse::resWait();
    }
  }

  // Initiate write-through regardless of hit or miss
  pendingWriteThrough = true;
  MemoryResponse r = storeWordNext(currentAddress, currentData);
  if (r.status == MemoryResponse::Status::WAIT)
    return r;

  // This only happens if the next level responded immediately
  finishOperation();
  return r;
}

// Write a full line, returns WAIT if busy and a word response with 0 data when successful
MemoryResponse Memory::storeLine(uint32_t address, const uint32_t* inData, AccessID id) {

  // If level is a cache and disabled, forward directly to nextLevel
  if (isCache && !cacheEnabled && nextLevel != nullptr) {
    return storeLineNext(address, inData);
  }

  // If not busy, accept request and set delay
  if (!busy) {
    busy                    = true;
    currentID               = id;
    currentAddress          = address;
    delayCount              = accessDelay;
    pendingWriteThrough     = false;
    for (int w = 0; w < WORDS_PER_LINE; w++)
      currentLineData[w] = inData[w];
  }

  // If request ID does not match, return WAIT
  if (currentID != id) {
    return MemoryResponse::resWait();
  }

  // Write-through in progress - keep forwarding to nextLevel until OK
  if (pendingWriteThrough) {
    MemoryResponse r = storeLineNext(currentAddress, currentLineData);
    if (r.status == MemoryResponse::Status::WAIT)
      return r;
    finishOperation();
    return r;
  }

  // If count is nonzero, decrement and return WAIT
  if (delayCount > 0) {
    delayCount--;
    return MemoryResponse::resWait();
  }

  /*
  * Count is zero - write the data and react accordingly
  */

  // If this is the DRAM level, write line to flat storage
  if (!isCache) {
    uint32_t lineIndex = currentAddress / WORDS_PER_LINE;
    for (int w = 0; w < WORDS_PER_LINE; w++)
      data[w][lineIndex] = currentLineData[w];
    finishOperation();
    return MemoryResponse::resWord(0);
  }

  // Otherwise, this is the cache level
  uint32_t tag = currentAddress / (WORDS_PER_LINE * numSets);
  uint32_t setIndex = (currentAddress / WORDS_PER_LINE) % numSets;

  int way = findWay(setIndex, tag);

  if (way != -1) {
    // Hit - write full line to cache
    hitCount++;
    for (int w = 0; w < WORDS_PER_LINE; w++)
      lines[setIndex][way].data[w] = currentLineData[w];
    updateLRU(setIndex, way);
  } else {
    // Miss - no-allocate, skip cache and write-through only
    missCount++;
    if (nextLevel == nullptr) {
      std::cerr << "Memory::writeLine: miss at bottom level\n";
      return MemoryResponse::resWait();
    }
  }

  // Initiate write-through regardless of hit or miss
  pendingWriteThrough = true;
  MemoryResponse r = storeLineNext(currentAddress, currentLineData);
  if (r.status == MemoryResponse::Status::WAIT)
    return r;

  // nextLevel responded immediately
  finishOperation();
  return r;
}


/*
* Back door operations to forward load/store requests to the next level of memory
*/
MemoryResponse Memory::loadFromNext(uint32_t address) {
  // If no lower level, return
  if (nextLevel == nullptr) {
    return MemoryResponse::resWait();
  }

  return nextLevel->load(address, AccessID::L1, true);
}

MemoryResponse Memory::storeWordNext(uint32_t address, uint32_t value) {
  // If no lower level, return
  if (nextLevel == nullptr) {
    return MemoryResponse::resWait();
  }

  return nextLevel->storeWord(address, value, AccessID::L1);
}

MemoryResponse Memory::storeLineNext(uint32_t address, const uint32_t* data) {
  // If no lower level, return
  if (nextLevel == nullptr) {
    return MemoryResponse::resWait();
  }

  return nextLevel->storeLine(address, data, AccessID::L1);
}


/*
* Side door - immediately read data without waiting
*/

uint32_t Memory::peekWord(uint32_t address) const {
  return readWord(address);
}

const uint32_t* Memory::peekLine(uint32_t lineIndex) const {
  for (int w = 0; w < WORDS_PER_LINE; w++)
    peekLineBuffer[w] = data[w][lineIndex];
  return peekLineBuffer;
}

const CacheLine* Memory::peekCacheLine(uint32_t setIndex, uint32_t way) const {
  if (!cacheEnabled || setIndex >= numSets || way >= associativity)
    return nullptr;
  return &lines[setIndex][way];
}


/*
* Side door - immediately write data
*/

void Memory::writeWordDirect(uint32_t address, uint32_t value) {
  writeWord(address, value);
}

void Memory::loadProgram(const uint32_t* program, uint32_t numWords, uint32_t startAddress) {
  for (uint32_t i = 0; i < numWords; i++)
    writeWord(startAddress + i, program[i]);
}

void Memory::reset() {
  for (int w = 0; w < WORDS_PER_LINE; w++)
    std::fill(data[w].begin(), data[w].end(), 0u);

  if (cacheEnabled)
    for (auto& set : lines)
      for (auto& line : set)
        line = CacheLine();

  busy                = false;
  currentID           = AccessID::NONE;
  currentOp           = MemOp::LOAD;
  currentAddress      = 0;
  currentData         = 0;
  delayCount          = 0;
  pendingWriteThrough = false;
  hitCount            = 0;
  missCount           = 0;
}

/*
* Side door - set config variables
*/
void Memory::setCacheEnabled(bool enabled) {
  // Cannnot disable DRAM
  if (!isCache) {
    return;
  }
  // Disabling cache mid-run flushes any current instructions
  if (!enabled) {
    finishOperation();
  }
  cacheEnabled = enabled;
}

void Memory::setAccessDelay(int delay) {
  accessDelay = delay;
}

/* 
* Side door - save/load memory images
*/
void Memory::saveImage(const std::string& filename) const {
  std::ofstream out(filename, std::ios::binary);
  if (!out)
    throw std::runtime_error("Memory::saveImage: cannot open " + filename);

  // Write header: numLines
  out.write(reinterpret_cast<const char*>(&numLines), sizeof(numLines));

  // Write flat data word by word, in address order
  for (uint32_t line = 0; line < numLines; line++)
    for (int w = 0; w < WORDS_PER_LINE; w++)
      out.write(reinterpret_cast<const char*>(&data[w][line]), sizeof(uint32_t));
}

void Memory::loadImage(const std::string& filename) {
  std::ifstream in(filename, std::ios::binary);
  if (!in)
    throw std::runtime_error("Memory::loadImage: cannot open " + filename);

  uint32_t savedLines = 0;
  in.read(reinterpret_cast<char*>(&savedLines), sizeof(savedLines));
  if (savedLines != numLines)
    throw std::runtime_error("Memory::loadImage: size mismatch");

  for (uint32_t line = 0; line < numLines; line++)
    for (int w = 0; w < WORDS_PER_LINE; w++)
      in.read(reinterpret_cast<char*>(&data[w][line]), sizeof(uint32_t));
}

/*
* Side door - get statistics
*/

float Memory::getMissRate() const {
  uint32_t total = hitCount + missCount;
  if (total == 0) return 0.0f;
  return static_cast<float>(missCount) / static_cast<float>(total);
}

/*
* Private helper functions
*/

// Decomposes an address into tag, set index, and word offset components and stores them
void Memory::decompose(uint32_t  address, uint32_t& tag, uint32_t& setIndex, uint32_t& wordOffset) const {
  wordOffset = address % WORDS_PER_LINE;
  setIndex = (address / WORDS_PER_LINE) % numSets;
  tag = address / (WORDS_PER_LINE * numSets);
}

// Search a set for a valid line matching tag. Returns way index, or -1 if not found.
int Memory::findWay(uint32_t setIndex, uint32_t tag) const {
  for (int way = 0; way < associativity; way++) {
    const CacheLine& line = lines[setIndex][way];
    if (line.valid && line.tag == tag)
      return way;
  }
  return -1;
}

// Find the way with the lowest lruCounter (least recently used).
int Memory::findLRUWay(uint32_t setIndex) const {
  int lruWay = 0;
  uint32_t lruCounter = UINT32_MAX;

  for (int way = 0; way < associativity; way++) {
    // Prefer invalid lines over any LRU values
    if (!lines[setIndex][way].valid)
      return static_cast<int>(way);
    if (lines[setIndex][way].lruCounter < lruCounter) {
      lruCounter = lines[setIndex][way].lruCounter;
      lruWay = way;
    }
  }
  return lruWay;
}

// Update LRU counters after accessing a given way.
// Assigns the used way to the associativity number, and decrements the rest
void Memory::updateLRU(uint32_t setIndex, uint32_t usedWay) {
  // Assign the used way to associativity number
  lines[setIndex][usedWay].lruCounter = associativity;

  // Decrement all other ways in the set
  for (int way = 0; way < static_cast<int>(associativity); way++) {
    if (way != static_cast<int>(usedWay))
      lines[setIndex][way].lruCounter--;
  }
}

// Read a word from flat storage by word address.
uint32_t Memory::readWord(uint32_t address) const {
  uint32_t lineIndex  = address / WORDS_PER_LINE;
  uint32_t wordOffset = address % WORDS_PER_LINE;
  return data[wordOffset][lineIndex];
}

// Write a word to flat storage by word address.
void Memory::writeWord(uint32_t address, uint32_t value) {
  uint32_t lineIndex  = address / WORDS_PER_LINE;
  uint32_t wordOffset = address % WORDS_PER_LINE;
  data[wordOffset][lineIndex] = value;
}

// Clear metadata associated with in-progress operations
void Memory::finishOperation() {
  busy = false;
  currentID = AccessID::NONE;
  currentAddress = 0;
  currentData = 0;
  pendingWriteThrough = false;
  delayCount = 0;
}