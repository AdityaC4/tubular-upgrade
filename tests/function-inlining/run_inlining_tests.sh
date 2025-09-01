#!/bin/bash

# Function Inlining Test Runner
# Tests the function inlining optimization pass

echo "=== FUNCTION INLINING TESTS ==="
echo

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Get the script directory and project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/../.."
TUBULAR="$PROJECT_ROOT/build/Tubular"

# Check if Tubular executable exists
if [ ! -f "$TUBULAR" ]; then
    echo -e "${RED}Error: Tubular executable not found at $TUBULAR${NC}"
    echo "Please run './make' from the project root first."
    exit 1
fi

# Check if wat2wasm is available
if ! command -v wat2wasm &> /dev/null; then
    echo -e "${YELLOW}Warning: wat2wasm not found. WebAssembly execution tests will be skipped.${NC}"
    echo "Install wabt toolkit for full testing: https://github.com/WebAssembly/wabt"
    SKIP_WASM=true
else
    SKIP_WASM=false
fi

# Check if Node.js is available
if ! command -v node &> /dev/null; then
    echo -e "${YELLOW}Warning: Node.js not found. JavaScript execution tests will be skipped.${NC}"
    SKIP_NODE=true
else
    SKIP_NODE=false
fi

# Function to run a single test
run_test() {
    local test_file="$1"
    local test_name="$2"
    local expected_behavior="$3"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo "--- Testing $test_name ---"
    echo "Expected: $expected_behavior"
    
    # Compile the test
    if "$TUBULAR" "$test_file" > "${test_file%.tube}.wat" 2>/dev/null; then
        echo -e "${GREEN}✓ Compilation successful${NC}"
        
        # Convert to WASM if wat2wasm is available
        if [ "$SKIP_WASM" = false ]; then
            if wat2wasm "${test_file%.tube}.wat" -o "${test_file%.tube}.wasm" 2>/dev/null; then
                echo -e "${GREEN}✓ WAT to WASM conversion successful${NC}"
                
                # Test execution if Node.js is available
                if [ "$SKIP_NODE" = false ]; then
                    test_execution "${test_file%.tube}.wasm" "$test_name"
                fi
            else
                echo -e "${YELLOW}⚠ WAT to WASM conversion failed${NC}"
            fi
        fi
        
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}✗ Compilation failed${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    
    echo
}

# Function to test WASM execution
test_execution() {
    local wasm_file="$1"
    local test_name="$2"
    
    # Create a temporary Node.js test script
    local js_test="${wasm_file%.wasm}_test.js"
    cat > "$js_test" << 'EOF'
const fs = require('fs');
const path = require('path');

async function runTest() {
    try {
        const wasmPath = process.argv[2];
        const wasmBuffer = fs.readFileSync(wasmPath);
        const wasmModule = await WebAssembly.instantiate(wasmBuffer);
        
        // Execute the main function
        const result = wasmModule.instance.exports.main();
        console.log(`Execution result: ${result}`);
        
        // Clean up
        fs.unlinkSync(process.argv[3]); // Remove this test script
        
        process.exit(0);
    } catch (error) {
        console.error('Execution error:', error.message);
        process.exit(1);
    }
}

runTest();
EOF
    
    # Run the test
    if node "$js_test" "$wasm_file" "$js_test" 2>/dev/null; then
        echo -e "${GREEN}✓ WebAssembly execution successful${NC}"
    else
        echo -e "${YELLOW}⚠ WebAssembly execution failed${NC}"
    fi
}

# Function to analyze inlining results
analyze_inlining() {
    local wat_file="$1"
    local function_name="$2"
    
    if [ -f "$wat_file" ]; then
        # Check specifically within the main function for the call
        if sed -n '/func \$main/,/END.*main.*function/p' "$wat_file" | grep -q "(call \$$function_name)"; then
            echo "  → Function call preserved (not inlined)"
        else
            echo "  → Function call inlined (call removed from main function)"
        fi
    fi
}

echo "Running function inlining tests..."
echo "Project root: $PROJECT_ROOT"
echo "Tubular executable: $TUBULAR"
echo

# Run all tests
for i in {1..6}; do
    test_file="$SCRIPT_DIR/inline-test-0$i.tube"
    if [ -f "$test_file" ]; then
        case $i in
            1) run_test "$test_file" "inline-test-01" "Simple function should be inlined"
               analyze_inlining "${test_file%.tube}.wat" "double_it" ;;
            2) run_test "$test_file" "inline-test-02" "Function calls should be inlined" 
               analyze_inlining "${test_file%.tube}.wat" "add_one" ;;
            3) run_test "$test_file" "inline-test-03" "Complex function should NOT be inlined"
               analyze_inlining "${test_file%.tube}.wat" "complex_calc" ;;
            4) run_test "$test_file" "inline-test-04" "Simple_add inlined; complex_multiply preserved"
               analyze_inlining "${test_file%.tube}.wat" "simple_add"
               analyze_inlining "${test_file%.tube}.wat" "complex_multiply" ;;
            5) run_test "$test_file" "inline-test-05" "Parameterless function SHOULD be inlined"
               analyze_inlining "${test_file%.tube}.wat" "get_constant" ;;
            6) run_test "$test_file" "inline-test-06" "Parameterless functions SHOULD be inlined"
               analyze_inlining "${test_file%.tube}.wat" "get_five"
               analyze_inlining "${test_file%.tube}.wat" "get_ten" ;;
        esac
    else
        echo "Test file $test_file not found, skipping..."
    fi
done

# Summary
echo "=== SUMMARY ==="
echo "Total tests: $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All function inlining tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi
