# 535-ISA-Simulator

This is a functional simulator for a custom RISC-based Instruction Set Architecture (ISA).
The specifics of this ISA, and the proposal for the simulator as a whole, are contained in the Proposal.pdf file

## Components

The project features the following components
- An instruction decoder to parse raw binary instructions
- A memory object to support reads/writes
- A write-through, no-allocate, n-way associative cache
- A register file
- A benchmark application for testing reads/writes via the command line

### Loading memory images
This project features the ability to load memory into the DRAM from a file. For this to work properly, your C working directory must either directly contain the programs/ folder, or your directory needs to contain a copy of said folder.
As an example, if you are using CMake, your CMakeLists.txt file should contain:
`
file(COPY ${CMAKE_SOURCE_DIR}/programs
     DESTINATION ${CMAKE_BINARY_DIR}/Debug)
`