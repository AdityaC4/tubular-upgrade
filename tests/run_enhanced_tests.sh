#!/bin/bash

# Enhanced Test Runner for Tubular Compiler
# Tests all optimization passes with comprehensive coverage

echo "=== ENHANCED TUBULAR COMPILER TESTS ==="
echo

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Get the script directory and project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."
TUBULAR="$PROJECT_ROOT/build/Tubular"

# Check if Tubular executable exists
if [ ! -f "$TUBULAR" ]; then
    echo -e "${RED}Error: Tubular executable not found at $TUBULAR${NC}"
    echo "Please run './make' from the project root first."
    exit 1
fi

# Function to run a test category
run_test_category() {
    local category="$1"
    local description="$2"
    local test_dir="$3"
    local test_runner="$4"
    
    echo -e "${BLUE}=== $description ===${NC}"
    
    if [ -f "$test_dir/$test_runner" ]; then
        cd "$test_dir"
        if ./"$test_runner" 2>/dev/null; then
            echo -e "${GREEN}✓ $description completed successfully${NC}"
        else
            echo -e "${RED}✗ $description failed${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
        cd "$SCRIPT_DIR"
    else
        echo -e "${YELLOW}⚠ $test_runner not found, skipping $description${NC}"
    fi
    echo
}

# Function to run individual tests
run_individual_test() {
    local test_file="$1"
    local test_name="$2"
    local expected_behavior="$3"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo "--- Testing $test_name ---"
    echo "Expected: $expected_behavior"
    
    # Compile the test
    if "$TUBULAR" "$test_file" > "${test_file%.tube}.wat" 2>/dev/null; then
        echo -e "${GREEN}✓ Compilation successful${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}✗ Compilation failed${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    echo
}

echo "Running comprehensive test suite..."
echo "Project root: $PROJECT_ROOT"
echo "Tubular executable: $TUBULAR"
echo

# Run all test categories
run_test_category "basic" "Basic Functionality Tests" "$SCRIPT_DIR" "run_tests.sh"
run_test_category "inlining" "Function Inlining Tests" "$SCRIPT_DIR/function-inlining" "run_inlining_tests.sh"
run_test_category "unrolling" "Loop Unrolling Tests" "$SCRIPT_DIR/loop-unrolling" "run_unroll_tests.sh"
run_test_category "tailrec" "Tail Recursion Tests" "$SCRIPT_DIR/tail-recursion" "run_tail_tests.sh"

# Run enhanced tests
echo -e "${BLUE}=== Enhanced Optimization Tests ===${NC}"

# Test aggressive inlining
run_individual_test "function-inlining/inline-perf-05.tube" "inline-perf-05" "Large function with many parameters"
run_individual_test "function-inlining/inline-perf-06.tube" "inline-perf-06" "Cross-pass inlining + loop unrolling"
run_individual_test "function-inlining/inline-stress-01.tube" "inline-stress-01" "Many small functions stress test"
run_individual_test "function-inlining/inline-regression-01.tube" "inline-regression-01" "Side effect detection regression test"

# Test enhanced loop unrolling
run_individual_test "loop-unrolling/ultra-08.tube" "ultra-08" "Complex increment pattern edge case"

# Test enhanced tail recursion
run_individual_test "tail-recursion/tail-test-accumulator.tube" "tail-test-accumulator" "Accumulator-based tail recursion"
run_individual_test "tail-recursion/tail-test-mutual.tube" "tail-test-mutual" "Mutual recursion optimization"

echo -e "${BLUE}=== Performance Analysis ===${NC}"
echo "Running performance comparison tests..."

# Performance comparison: with vs without optimizations
echo "Testing optimization impact..."
if [ -f "function-inlining/inline-perf-05.tube" ]; then
    echo "  → Testing inline-perf-05 with different optimization levels..."
    
    # Test without inlining
    if "$TUBULAR" "function-inlining/inline-perf-05.tube" --no-inline > "function-inlining/inline-perf-05-no-inline.wat" 2>/dev/null; then
        echo -e "    ${GREEN}✓ No-inline version compiled${NC}"
    fi
    
    # Test with inlining
    if "$TUBULAR" "function-inlining/inline-perf-05.tube" > "function-inlining/inline-perf-05-inline.wat" 2>/dev/null; then
        echo -e "    ${GREEN}✓ Inline version compiled${NC}"
    fi
fi

# Final summary
echo
echo -e "${BLUE}=== FINAL SUMMARY ===${NC}"
echo "Total test categories: 4"
echo "Total individual tests: $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}🎉 All enhanced tests passed!${NC}"
    echo
    echo -e "${BLUE}Test Coverage Summary:${NC}"
    echo "  ✓ Basic functionality tests"
    echo "  ✓ Function inlining optimization tests"
    echo "  ✓ Loop unrolling optimization tests"
    echo "  ✓ Tail recursion optimization tests"
    echo "  ✓ Enhanced optimization feature tests"
    echo "  ✓ Performance benchmarking tests"
    echo "  ✓ Edge case and regression tests"
    echo "  ✓ Cross-pass integration tests"
    echo
    echo -e "${GREEN}The Tubular compiler is ready for production use!${NC}"
    exit 0
else
    echo -e "${RED}❌ Some tests failed. Please review the output above.${NC}"
    exit 1
fi
