# Tubular Refactoring Plan

## Current Architecture Issues

The current Tubular compiler is implemented as a single monolithic file (`Tubular.cpp`) with all functionality embedded within one class. This approach has several limitations:

1. **Maintainability**: All code is in one file, making it difficult to navigate and modify
2. **Extensibility**: Adding new features or passes requires significant changes to existing code
3. **Modularity**: No clear separation between frontend (parsing) and backend (code generation)
4. **Testability**: Hard to unit test individual components
5. **Pass Infrastructure**: No framework for implementing optimization passes

## Proposed Refactored Architecture

### 1. Core Modules

```
src/
├── frontend/
│   ├── lexer.hpp
│   ├── parser.hpp
│   ├── token_queue.hpp
│   └── ast/
│       ├── ast_node.hpp
│       ├── ast_visitor.hpp
│       └── nodes/
│           ├── expressions.hpp
│           ├── statements.hpp
│           └── declarations.hpp
├── middle_end/
│   ├── symbol_table.hpp
│   ├── type.hpp
│   └── passes/
│       ├── pass_manager.hpp
│       ├── function_inlining.hpp
│       ├── loop_unrolling.hpp
│       └── tail_recursion.hpp
├── backend/
│   ├── codegen.hpp
│   ├── wat_generator.hpp
│   └── wasm_runtime.hpp
├── core/
│   ├── file_pos.hpp
│   ├── error.hpp
│   └── tools.hpp
└── tubular.cpp (main entry point)
```

### 2. Pass Infrastructure

#### Pass Manager
```cpp
class PassManager {
private:
    std::vector<std::unique_ptr<Pass>> passes;
    
public:
    void addPass(std::unique_ptr<Pass> pass);
    void runPasses(ASTNode& root);
    void runPass(Pass& pass, ASTNode& root);
};
```

#### Base Pass Class
```cpp
class Pass {
public:
    virtual ~Pass() = default;
    virtual std::string getName() const = 0;
    virtual void run(ASTNode& node) = 0;
};
```

#### Example Pass Implementation
```cpp
class FunctionInliningPass : public Pass {
private:
    bool inlineEnabled;
    
public:
    FunctionInliningPass(bool enabled) : inlineEnabled(enabled) {}
    
    std::string getName() const override { return "FunctionInlining"; }
    void run(ASTNode& node) override;
};
```

### 3. AST Restructuring

#### Visitor Pattern
Implement a visitor pattern for AST traversal:
```cpp
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(ASTNode_Function& node);
    virtual void visit(ASTNode_While& node);
    virtual void visit(ASTNode_If& node);
    // ... other node types
};
```

#### AST Node Hierarchy
Restructure AST nodes into separate files:
- `expressions.hpp`: Math operations, literals, variables
- `statements.hpp`: If, while, return, break, continue
- `declarations.hpp`: Function and variable declarations

### 4. Configuration System

#### CLI Options
```cpp
struct CompilerOptions {
    bool enableInlining = false;
    int unrollFactor = 1;
    bool loopifyTailRecursion = false;
    
    // Parsing and validation methods
    static CompilerOptions parse(int argc, char* argv[]);
};
```

### 5. Build System Improvements

#### CMakeLists.txt
Replace Makefile with CMake for better cross-platform support:
```cmake
cmake_minimum_required(VERSION 3.14)
project(Tubular VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.hpp")

add_executable(tubular ${SOURCES} ${HEADERS})

# Compiler flags
target_compile_options(tubular PRIVATE -Wall -Wextra)
```

## Implementation Roadmap

### Phase 1: Basic Refactoring (Days 1-2)
1. Split `Tubular.cpp` into logical modules
2. Create separate header and implementation files
3. Establish basic directory structure
4. Ensure all existing tests still pass

### Phase 2: Pass Infrastructure (Days 3-4)
1. Implement PassManager and base Pass class
2. Create visitor pattern for AST traversal
3. Add configuration system for compiler options
4. Refactor existing functionality into passes

### Phase 3: Optimization Passes (Days 5-7)
1. Implement Function Inlining pass
2. Implement Loop Unrolling pass
3. Implement Tail Recursion pass
4. Add CLI flags to control passes

### Phase 4: Testing and Validation (Days 8-9)
1. Add unit tests for individual passes
2. Validate correctness of optimizations
3. Performance benchmarking
4. Documentation updates

### Phase 5: Integration with Autotuning (Day 10)
1. Integrate passes with autotuning framework
2. Create JSON configuration for pass combinations
3. Validate all pass combinations produce correct output

## Benefits of Refactored Architecture

1. **Modularity**: Clear separation of concerns between frontend, middle-end, and backend
2. **Extensibility**: Easy to add new optimization passes
3. **Maintainability**: Smaller, focused files are easier to understand and modify
4. **Testability**: Individual passes can be unit tested
5. **Performance**: Pass infrastructure enables systematic optimization exploration
6. **Research-Readiness**: Structure aligns with professional compiler frameworks like LLVM

## Pass Implementation Details

### Function Inlining Pass
- Identify direct function calls
- Replace call site with function body when enabled
- Handle parameter substitution
- Maintain correctness with variable scoping

### Loop Unrolling Pass
- Identify while loops with constant iteration counts
- Duplicate loop body N times where N is the unroll factor
- Add remainder handling for non-divisible counts
- Update control flow accordingly

### Tail Recursion Pass
- Identify tail-recursive function calls
- Transform recursive calls into while loops
- Maintain function semantics while improving performance
- Handle accumulator-style tail recursion

This refactored architecture will provide a solid foundation for implementing the autotuning features outlined in the PLAN.md document while maintaining the educational value of the compiler.