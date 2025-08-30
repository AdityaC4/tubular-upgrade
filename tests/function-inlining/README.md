# Function Inlining Tests

This directory contains tests for the function inlining optimization pass in the Tubular compiler.

## Test Cases

### Test 1-2: Simple Functions with Parameters (NOW INLINED! 🎉)
- **inline-test-01.tube**: Basic function with one parameter - **INLINED**
- **inline-test-02.tube**: Function called multiple times with parameters - **INLINED**

**Actual Behavior**: These simple functions with parameters are now successfully inlined with proper parameter substitution!

### Test 3: Complex Function (Should NOT be inlined)
- **inline-test-03.tube**: Complex function with multiple statements (exceeds limit) - **PRESERVED**

**Expected Behavior**: Complex functions exceed the 3-statement inlining limit and should preserve their `(call $function_name)` instructions.

### Test 4: Mixed Scenario
- **inline-test-04.tube**: `simple_add` (2 params, 1 statement) - **INLINED**, `complex_multiply` (2 params, 4 statements) - **PRESERVED**

### Test 5-6: Parameterless Functions (SHOULD be inlined)
- **inline-test-05.tube**: Simple parameterless function returning constant
- **inline-test-06.tube**: Multiple parameterless functions

**Expected Behavior**: These functions should be inlined - no `(call $function_name)` instructions should appear in the main function. The function calls are replaced with direct constant values.

**Actual Results**: 
- ✅ `get_constant()` inlined to `(i32.const 42)`
- ✅ `get_five()` inlined to `(i32.const 5)`  
- ✅ `get_ten()` inlined to `(i32.const 10)`

## Running Tests

```bash
# Run all function inlining tests
./run_inlining_tests.sh

# Run individual test
../../build/Tubular inline-test-05.tube
```

## Current Implementation Status

✅ **Fully Implemented**: 
- ✨ **Parameter substitution for functions with arguments** (NEW!)
- Parameterless function inlining
- Size-based heuristics (≤3 statements)
- Complete AST cloning with variable substitution
- WebAssembly execution verification

🚧 **Future Enhancements**:
- Advanced inlining heuristics (call frequency analysis)
- More complex expression handling
- Cross-function optimization analysis

## Architecture

The function inlining pass uses a two-phase approach:
1. **Collection Phase**: Map all function definitions
2. **Transformation Phase**: Replace suitable function calls with inlined bodies

The implementation is conservative and safe, preserving function calls when parameter substitution or complexity analysis indicates potential issues.