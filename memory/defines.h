//
// Created by mrgri on 3/2/2026.
//

#ifndef DEFINES_H
#define DEFINES_H

#include <cstdint>

typedef uint32_t word;
typedef uint64_t doubleword;
typedef uint16_t halfword;
typedef uint8_t byte_;

#define ZR 0 // zero
#define PC 1 // Program Counter
#define SP 2 // Stack pointer
#define RA 3 // Return address

#define IC 49 // Integer condition code
#define FC 50 // FP condition code

#endif // DEFINES_H