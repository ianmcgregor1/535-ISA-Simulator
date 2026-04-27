# 535-ISA-Simulator

This is a functional simulator for a custom RISC-based Instruction Set Architecture (ISA).

A user manual and detailed description of the project can be found in the Report.pdf file.

## Components

The project features the following components
- A five-stage F/D/E/M/W pipeline
- A clock to drive the pipeline
- A memory object to support reads/writes for cache and DRAM
- A write-through, no-allocate, n-way associative cache
- A register file with integer and floating-point registers
- Capability to upload 32-bit instructions to memory to be executed
- A Python assembler to generate .hex programs from .txt assembly code
- Pre-made benchmark assessments for the simulator
- Breakpoint support for mid-execution halting
- A QT GUI to allow visualization of all above components

## Running the Project

This is a C++ project, and as such it will require some compiler compatible IDE.

This project also requires Qt GUI (built using Qt6). Your main file should be GUI/main_gui to run the simulator in its entirety.

To run the program without Qt GUI (and without the GUI in general), the main file can be set to main.cpp. The clock, pipeline, registers, and memory can be instantiated there to run the simulator without Qt.


## Benchmarks

This project contains several pre-made benchmark programs to be run in the simulator. They are located in the programs/ folder.

## Assembler

The project also contains an assembler to create custom programs to be run. It requires a .txt file with assembly code, and outputs a .hex file to be loaded into the simulator.

## Loading Memory Images
This project features the ability to load memory into the DRAM from a file. For this to work properly, your C working directory must either directly contain the programs/ folder, or your directory needs to contain a copy of said folder.
As an example, if you are using CMake, your CMakeLists.txt file should contain:

`
file(COPY ${CMAKE_SOURCE_DIR}/programs
     DESTINATION ${CMAKE_BINARY_DIR}/Debug)
`

The project contains some example programs, located in the programs/ folder. These, if loaded in at runtime, will perform their described program in full.