# Testing and Build Commands

This project supports comprehensive testing including loop unrolling performance tests.

## Available Commands

### Building

```bash
./make              # Build the Tubular compiler
./make clean        # Clean all generated files
```

### Testing

```bash
./make test         # Run all tests (standard + loop unrolling)
./make clean-test   # Clean all test files
```

### Loop Unrolling Specific

```bash
./make clean-unroll # Clean only loop unrolling test files
```

### Direct Test Access

```bash
# Standard tests only
cd tests && ./run_tests.sh

# Loop unrolling tests only
cd tests/loop-unrolling && ./run_unroll_tests.sh

# Browser-based performance testing
cd tests && python -m http.server
# Then open: http://localhost:8000/loop-unrolling/loop-unrolling-tester.html
```

## Test Coverage

### Standard Tests

- **24 regular tests** (test-01.tube to test-24.tube)
- **30 Project 3 tests** (P3-test-01.tube to P3-test-30.tube)
- **11 error tests** for error handling validation
- **19 Project 3 error tests** for compatibility validation

### Loop Unrolling Tests

- **4 performance test cases**:
  - `ultra-01.tube`: UltraLight - Simple arithmetic with 100M iterations
  - `ultra-02.tube`: ParallelStreams - Multiple independent calculations with 3M iterations
  - `ultra-03.tube`: RegisterPressure - High register usage with 1.5M iterations
  - `ultra-04.tube`: MemoryAccess - Complex memory patterns with 2M iterations
- **4 unroll factors tested**: 1x, 4x, 8x, 16x (16 total test combinations)
- **Performance improvements observed**: Up to 22% faster execution with optimal unrolling
- **Browser-based timing analysis** with anti-optimization techniques

## Expected Results

All tests should pass with:

- **Standard tests**: 100% compilation success rate
- **Loop unrolling tests**: 100% compilation success rate with measurable performance improvements
- **Browser tests**: All functions should execute correctly with expected outputs
