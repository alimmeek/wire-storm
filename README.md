# Operation WIRE STORM

## Pre-requisites
- A C compiler
- Unix system

## Compiling and Running
- The Makefile has two commands:
    - `make all` -> compiles all source files into the `build/` directory, and creates the `wire-storm` executable
    - `make clean` -> removes the build directory and its contents
- After running `make all`, 

## Known Issues
- All supplied test cases pass individually, but for some reason when running the test file, I get a ConnectionRefusedError
    - Seems like I can run at most 3 cases before encountering this
    - Current theory is that Windows (I was working on WSL) is somehow killing the connection