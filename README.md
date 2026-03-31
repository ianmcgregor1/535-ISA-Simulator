# 535-ISA-Simulator

This is a functional simulator for a custom RISC-based Instruction Set Architecture (ISA).
The specifics of this ISA, and the proposal for the simulator as a whole, are contained in the Proposal.pdf file

## Components

The project features the following components
- A five-stage F/D/E/M/W pipeline
- A clock to drive the pipeline
- A memory object to support reads/writes
- A write-through, no-allocate, n-way associative cache
- A register file with integer and floating-point registers
- Capability to upload 32-bit instructions to memory to be executed
- A QT GUI to allow visualization of all above components
- A set of benchmark applications to test different parts of the project

## Benchmarks

This project contains different benchmark assessments of the system. They are located in the benchmarks/ folder.
To run a benchmark, copy it into main.cpp and run the project.

**Benchmark1**:

This benchmark is a simple command-line interface to allow testing of the memory and caching system

**cmdline-pipeline**

This benchmark is a more complex command-line interface to show instruction propagation through the pipeline

## Loading Memory Images
This project features the ability to load memory into the DRAM from a file. For this to work properly, your C working directory must either directly contain the programs/ folder, or your directory needs to contain a copy of said folder.
As an example, if you are using CMake, your CMakeLists.txt file should contain:

`
file(COPY ${CMAKE_SOURCE_DIR}/programs
     DESTINATION ${CMAKE_BINARY_DIR}/Debug)
`

The project contains some example programs, located in the programs/ folder. These, if loaded in at runtime, will perform their described program in full.