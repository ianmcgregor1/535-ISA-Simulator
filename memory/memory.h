#pragma once
#include <cstdint>
#include <string>
#include <vector>


// Constants
constexpr int WORDS_PER_LINE = 4;

// Enum for ID of calling programs
enum class AccessID {
  NONE,       // No current request
  FETCH,      // Fetch pipeline stage
  EXECUTE,    // Execute pipeline stage
  MEMORY,     // Memory pipeline stage
  L1,         // L1 cache requesting from DRAM
  UI          // Side-door GUI access - no restrictions
};

// Operation type
enum class MemOp { LOAD, STORE };


/*
* Return type for access calls - fields invalid if status is WAIT
*/
struct MemoryResponse {
  enum class Status { OK, WAIT };

  Status   status;
  bool     isLine;                  // Whether or not the returned data is a line or a word
  uint32_t word;                    // Valid if OK and isLine == false
  uint32_t line[WORDS_PER_LINE];    // Valid if OK and isLine == true

  // Creates a WAIT response with zeroed data
  static MemoryResponse resWait() {
    MemoryResponse r;
    r.status = Status::WAIT;
    r.isLine = false;
    r.word   = 0;
    for (int i = 0; i < WORDS_PER_LINE; i++) r.line[i] = 0;
    return r;
  }

  // Creates an OK response with a single word
  static MemoryResponse resWord(uint32_t data) {
    MemoryResponse r;
    r.status = Status::OK;
    r.isLine = false;
    r.word   = data;
    for (int i = 0; i < WORDS_PER_LINE; i++) r.line[i] = 0;
    return r;
  }

  // Creates an OK response with a full line
  static MemoryResponse resLine(const uint32_t* data) {
    MemoryResponse r;
    r.status = Status::OK;
    r.isLine = true;
    r.word   = 0;
    for (int i = 0; i < WORDS_PER_LINE; i++) r.line[i] = data[i];
    return r;
  }

};


/*
* A single cache line, with words, tag, valid bit, and LRU counter
*/
struct CacheLine {
  bool     valid;
  bool     dirty;  // Unused since this is write-through
  uint32_t tag;
  uint32_t data[WORDS_PER_LINE];
  uint32_t lruCounter;    // Resets to 0 when used, so higher = less recently used

  // Construction initializes to invalid line with zeros
  CacheLine() : valid(false), tag(0), lruCounter(0) {
    for (int i = 0; i < WORDS_PER_LINE; i++) data[i] = 0;
  }
};


/*
* Main memory class for managing the memory hierarchy, can be DRAM or cache depending on parameters
*/
class Memory {
public:

  Memory(uint32_t numLines,     
         uint32_t delay,
         Memory*  nextLevel,    // Pointer to lower level of memory
         uint32_t associativity = 1);


  /*
  * Front door functions - called by pipeline stages or higher levels of memory
  */

  // Load a word or line from the given address
  MemoryResponse loadWord(uint32_t address, AccessID id);
  MemoryResponse loadLine(uint32_t address, AccessID id);

  // Store a word or line to the given address
  MemoryResponse storeWord(uint32_t address, uint32_t data, AccessID id);
  MemoryResponse storeLine(uint32_t address, const uint32_t* data, AccessID id);

  /*
  * Back door functions - called when data is not resident
  */
  MemoryResponse loadWordNext(uint32_t address);
  MemoryResponse loadLineNext(uint32_t address);
  MemoryResponse storeWordNext(uint32_t address, uint32_t data);
  MemoryResponse storeLineNext(uint32_t address, const uint32_t* data);


  /*
  * Side door access - immediate data fetching and updating for GUI or debugging
  */

  // Fetching data

  // Read a single word directly by address
  uint32_t peekWord(uint32_t address) const;

  // Get a pointer to a full line of data
  const uint32_t* peekLine(uint32_t lineIndex) const;

  // Return a pointer to a CacheLine (includes metadata)
  const CacheLine* peekCacheLine(uint32_t setIndex, uint32_t way) const;


  // Updating data

  // Write a single word instantly by address
  void writeWordDirect(uint32_t address, uint32_t value);

  // Load a binary program into memory starting at the given address
  void loadProgram(const uint32_t* program,
                   uint32_t        numWords,
                   uint32_t        startAddress = 0);

  // Reset all data and metadata to zero/invalid
  void reset();

  // Cancel in-flight fetch request
  void cancelFetch();

  // Modifying configuration

  // Enable or disable cache at runtime.
  void setCacheEnabled(bool enabled);

  // Change the delay for this level of memory
  void setAccessDelay(int delay);


  // Images

  // Save a raw binary image of memory to file
  void saveImage(const std::string& filename) const;

  // Load a raw binary image of memory from file
  void loadImage(const std::string& filename);


  // Getter functions for metadata

  bool     isCacheEnabled() const { return cacheEnabled; }
  uint32_t getNumLines()    const { return numLines; }
  uint32_t getMissCount()   const { return missCount; }
  uint32_t getHitCount()    const { return hitCount; }
  float    getMissRate()    const;
  AccessID getCurrentID()   const { return currentID; }
  MemOp    getCurrentOp()   const { return currentOp; }
  uint32_t getCurrentAddress() const { return currentAddress; }
  uint32_t getCurrentData() const { return currentData; }
  int      getDelayCount() const { return delayCount; }
  uint32_t getAccessDelay() const { return accessDelay; }
  const uint32_t* getCurrentLineData() const { return currentLineData; }


private:

  // Metadata

  uint32_t  numLines;
  int       associativity;    
  uint32_t  numSets;          // numLines / associativity
  bool      cacheEnabled;     // Whether or not the cache is enabled (only changeable if level is a cache)
  bool      isCache;          // Set true if this level is a cache at instantiation (nextLevel != null)
  int       accessDelay;      
  Memory*   nextLevel;        

  std::vector<std::vector<uint32_t>> data;

  std::vector<std::vector<CacheLine>> lines;

  // Private variables for front door
  bool      busy;
  AccessID  currentID;
  MemOp     currentOp;
  uint32_t  currentAddress;
  uint32_t  currentData;
  int       delayCount;
  bool      pendingWriteThrough;
  uint32_t  currentLineData[WORDS_PER_LINE];  // Stores incoming line data


  // Statistics
  uint32_t  hitCount;
  uint32_t  missCount;


  // Helper functions

  // Search a set for a matching valid tag, returns index or -1 on miss
  int  findWay(uint32_t setIndex, uint32_t tag) const;

  // Find the least-recently-used way in a set
  int  findLRUWay(uint32_t setIndex) const;

  // Update LRU counters
  void updateLRU(uint32_t setIndex, uint32_t usedWay);

  // Read/write by word address
  uint32_t readWord(uint32_t address) const;
  void     writeWord(uint32_t address, uint32_t value);

  // Helper to check if memory can accept a request, and set busy parameters if so
  bool checkAndSetBusy(AccessID id, MemOp op, uint32_t address);

  // Reset private variables when an operation is completed
  void finishOperation();
};