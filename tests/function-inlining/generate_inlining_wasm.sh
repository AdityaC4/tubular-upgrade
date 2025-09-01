#!/bin/bash

# Generate WASM files for function inlining HTML test page
# Creates both inlined and non-inlined versions of each test

echo "=== FUNCTION INLINING WASM GENERATOR ==="
echo

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

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
    echo -e "${RED}Error: wat2wasm not found. Please install wabt toolkit.${NC}"
    echo "Install wabt toolkit for WASM generation: https://github.com/WebAssembly/wabt"
    exit 1
fi

echo "Generating WASM files for function inlining HTML test page..."
echo "Project root: $PROJECT_ROOT"
echo "Tubular executable: $TUBULAR"
echo

# Function to generate WASM files for a test
generate_test_wasm() {
    local test_file="$1"
    local test_name="${test_file%.tube}"
    
    echo "Processing $test_name..."
    
    # Generate non-inlined version
    echo "  → Generating non-inlined version..."
    if "$TUBULAR" "$test_file" --no-inline --unroll-factor=1 > "${test_name}-no-inline.wat" 2>/dev/null; then
        if wat2wasm "${test_name}-no-inline.wat" -o "${test_name}-no-inline.wasm" 2>/dev/null; then
            echo -e "    ${GREEN}✓ Non-inlined WASM generated successfully${NC}"
        else
            echo -e "    ${RED}✗ Non-inlined WAT to WASM conversion failed${NC}"
            return 1
        fi
    else
        echo -e "    ${RED}✗ Non-inlined compilation failed${NC}"
        return 1
    fi
    
    # Generate inlined version (default behavior)
    echo "  → Generating inlined version..."
    if "$TUBULAR" "$test_file" --unroll-factor=1 > "${test_name}-inline.wat" 2>/dev/null; then
        if wat2wasm "${test_name}-inline.wat" -o "${test_name}-inline.wasm" 2>/dev/null; then
            echo -e "    ${GREEN}✓ Inlined WASM generated successfully${NC}"
        else
            echo -e "    ${RED}✗ Inlined WAT to WASM conversion failed${NC}"
            return 1
        fi
    else
        echo -e "    ${RED}✗ Inlined compilation failed${NC}"
        return 1
    fi
    
    echo
    return 0
}

# Process all test files
success_count=0
total_count=0

# Process both correctness tests and perf tests
for test_file in "$SCRIPT_DIR"/inline-*.tube; do
    if [ -f "$test_file" ]; then
        total_count=$((total_count + 1))
        if generate_test_wasm "$test_file"; then
            success_count=$((success_count + 1))
        fi
    fi
done

# Summary
echo "=== SUMMARY ==="
echo "Total tests processed: $total_count"
echo -e "Successful: ${GREEN}$success_count${NC}"
echo -e "Failed: ${RED}$((total_count - success_count))${NC}"

if [ $success_count -eq $total_count ]; then
    echo -e "${GREEN}All WASM files generated successfully!${NC}"
    echo
    echo "You can now open tests/function-inlining/function-inlining-tester.html in a browser"
    echo "or run: cd tests && python -m http.server"
    exit 0
else
    echo -e "${RED}Some WASM files failed to generate.${NC}"
    exit 1
fi
