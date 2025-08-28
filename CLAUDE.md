# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Tubular is a compiler that translates `.tube` source files to WebAssembly Text Format (WAT) and WebAssembly bytecode (WASM). The language supports basic programming constructs with focus on string manipulation, mathematical operations, and includes advanced optimization passes like loop unrolling.

## Build Commands

### Basic Build
```bash
./make              # Build the Tubular compiler  
./build.sh          # Alternative build script with options
```

### Build Options (via build.sh)
```bash
./build.sh release       # Release build with -O3 (default)
./build.sh debug         # Debug build with -g
./build.sh grumpy        # Extra warnings with -pedantic
./build.sh clean tests   # Clean and run tests after building
```

### Testing
```bash
./make test         # Run all tests (standard + loop unrolling)
./make clean        # Clean all generated files
./make clean-test   # Clean only test files
./make clean-unroll # Clean only loop unrolling test files

# Direct test access
cd tests && ./run_tests.sh                           # Standard tests only
cd tests/loop-unrolling && ./run_unroll_tests.sh    # Loop unrolling tests only
```

### Browser-based Performance Testing
```bash
cd tests && python -m http.server
# Open: http://localhost:8000/loop-unrolling/loop-unrolling-tester.html
```

## Architecture

### Core Components

**Frontend** (`src/frontend/`):
- `lexer.hpp` - DFA-based tokenizer for `.tube` source files
- `ASTNode.hpp` - Complete AST node hierarchy with visitor pattern support
- `ASTVisitor.hpp` - Base visitor interface for tree traversal
- `TokenQueue.hpp` - Token stream management

**Middle-end** (`src/middle_end/`):
- `PassManager.hpp` - Infrastructure for running optimization passes
- `LoopUnrollingPass.hpp` - Advanced loop unrolling optimization (NEW feature)
- `FunctionInliningPass.hpp` - Function inlining optimization  
- `TailRecursionPass.hpp` - Tail recursion to loop conversion
- `SymbolTable.hpp` - Variable and function scope management
- `Type.hpp` - Type system implementation

**Backend** (`src/backend/`):
- `WATGenerator.hpp` - Visitor that generates WebAssembly Text Format

**Core Utilities** (`src/core/`):
- `tools.hpp` - Common utilities and helper functions
- `Control.hpp` - Compilation control and state management

### Main Compiler (`Tubular.cpp`)
The main compiler class integrates all components using recursive descent parsing and visitor pattern for code generation. It includes a sophisticated operator precedence parser and comprehensive error handling.

### Pass System Architecture
The compiler uses a modern pass-based architecture:
- `Pass` base class defines the interface for all optimization passes
- `PassManager` orchestrates execution of multiple passes
- Each pass operates on the AST and can transform it for optimizations
- Loop unrolling pass includes advanced pattern recognition and performance analysis

## Language Features

The Tubular language supports:
- **Data Types**: int, double, char, string with type casting (`:int`, `:double`, `:string`)
- **Functions**: First-class functions with recursion support
- **Control Flow**: if/else, while loops with break/continue
- **String Operations**: Concatenation, repetition, indexing, size()
- **Built-in Functions**: Math operations (sqrt), type conversions (ToInt, ToDouble)

## Testing Infrastructure

### Test Suites
- **24 standard tests** (`test-01.tube` to `test-24.tube`)
- **30 Project 3 tests** (`P3-test-01.tube` to `P3-test-30.tube`) 
- **11 error tests** for error handling validation
- **19 Project 3 error tests** for compatibility

### Loop Unrolling Performance Tests
- **4 specialized performance tests** in `tests/loop-unrolling/`
- **Multiple unroll factors**: 1x, 4x, 8x, 16x optimization levels
- **Performance improvements**: Up to 22% faster execution observed
- **Browser-based timing analysis** with anti-optimization techniques

## Development Notes

### File Extensions
- `.tube` - Source files in Tubular language
- `.wat` - WebAssembly Text Format output
- `.wasm` - WebAssembly bytecode output

### Key Build Dependencies
- CMake 3.16+ required
- C++20 standard
- CMake generates compile_commands.json for IDE support

### Memory Model
- Linear memory with fixed initial size
- Strings stored as null-terminated sequences  
- Global `$free_mem` pointer for memory management
- Type mapping: int→i32, double→f64, char→i32, string→i32 (address)

### Optimization Framework
The pass system supports sophisticated optimizations:
- **Loop Analysis**: Pattern recognition for unrollable loops
- **Performance Measurement**: Integrated timing and benchmark capabilities  
- **Configurable Parameters**: Unroll factors, inlining decisions, tail recursion conversion

The project includes comprehensive documentation in SPECS.md, TESTING.md, PLAN.md, and REFACTOR.md for detailed language specifications and development roadmap.