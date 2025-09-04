#!/bin/bash

# Loop Unrolling Performance Test Script
# Tests compilation with different unroll factors: 1, 4, 8, 16

echo "=== LOOP UNROLLING PERFORMANCE TESTS ==="
echo ""

# Array of test files
test_files=("ultra-01.tube" "ultra-02.tube" "ultra-03.tube" "ultra-04.tube" "ultra-05.tube")
unroll_factors=(1 4 8 16)

# Summary counters
total_tests=0
successful_compilations=0

# Function to measure compilation time
measure_compilation_time() {
    local file=$1
    local factor=$2
    local output_suffix=$3
    
    # Measure compilation time
    start_time=$(date +%s%N)
    ../../build/Tubular "$file" --unroll-factor=$factor > "${file%.tube}-unroll${output_suffix}.wat" 2>/dev/null
    exit_code=$?
    end_time=$(date +%s%N)
    
    if [ $exit_code -eq 0 ]; then
        # Calculate time in milliseconds
        time_diff=$(( (end_time - start_time) / 1000000 ))
        echo "Compiling $file with unroll factor $factor... SUCCESS (${time_diff}ms)"
        echo $time_diff
        return 0
    else
        echo "Compiling $file with unroll factor $factor... FAILED"
        echo "0"
        return 1
    fi
}

# Function to generate WASM files and test execution
generate_wasm() {
    local wat_file=$1
    local wasm_file="${wat_file%.wat}.wasm"
    
    if [[ -f "$wat_file" ]]; then
        wat2wasm "$wat_file" 2>/dev/null
        if [ $? -eq 0 ] && [[ -f "$wasm_file" ]]; then
            echo "  ✓ Generated $wasm_file"
        else
            echo "  ✗ Failed to generate $wasm_file"
        fi
    fi
}

echo "Testing loop unrolling with factors: ${unroll_factors[*]}"
echo ""

# Test each file with different unroll factors
for file in "${test_files[@]}"; do
    if [[ ! -f "$file" ]]; then
        echo "ERROR: Test file $file not found!"
        continue
    fi
    
    echo "--- Testing $file ---"
    
    # Store compilation times for this file
    declare -a times
    
    # Test with each unroll factor
    for factor in "${unroll_factors[@]}"; do
        ((total_tests++))
        # Capture both output lines from measure_compilation_time
        output=$(measure_compilation_time "$file" "$factor" "$factor")
        if [ $? -eq 0 ]; then
            ((successful_compilations++))
        fi
        time_result=$(echo "$output" | tail -n 1)
        times+=($time_result)
        
        # Generate WASM file for testing (default inlining)
        wat_file="${file%.tube}-unroll${factor}.wat"
        generate_wasm "$wat_file"

        # (No separate no-inline variant; inlining can be controlled via CLI if needed)
    done
    
    # Display compilation time comparison
    echo "  Compilation Time Analysis:"
    for i in "${!unroll_factors[@]}"; do
        factor=${unroll_factors[$i]}
        time=${times[$i]}
        if [ "$time" != "0" ]; then
            echo "    Unroll Factor $factor: ${time}ms"
        else
            echo "    Unroll Factor $factor: FAILED"
        fi
    done
    
    # Calculate performance differences
    if [ "${times[0]}" != "0" ] && [ "${times[0]}" != "" ]; then
        baseline=${times[0]}
        echo "  Performance vs baseline (factor=1):"
        for i in "${!unroll_factors[@]}"; do
            if [ $i -gt 0 ] && [ "${times[$i]}" != "0" ] && [ "${times[$i]}" != "" ]; then
                factor=${unroll_factors[$i]}
                time=${times[$i]}
                diff=$((time - baseline))
                if [ $diff -gt 0 ]; then
                    echo "    Factor $factor: +${diff}ms (+$(( (diff * 100) / baseline ))%)"
                elif [ $diff -lt 0 ]; then
                    diff_abs=$((baseline - time))
                    echo "    Factor $factor: -${diff_abs}ms (-$(( (diff_abs * 100) / baseline ))%)"
                else
                    echo "    Factor $factor: No change"
                fi
            fi
        done
    fi
    
    echo ""
    unset times
done

# Summary
echo "=== SUMMARY ==="
echo "Total compilation attempts: $total_tests"
echo "Successful compilations: $successful_compilations"
if [ $total_tests -gt 0 ]; then
    echo "Success rate: $(( (successful_compilations * 100) / total_tests ))%"
else
    echo "Success rate: 0%"
fi

# Clean up old WAT files that might interfere with web testing
echo ""
echo "Generated files for web testing:"
for file in "${test_files[@]}"; do
    base="${file%.tube}"
    for factor in "${unroll_factors[@]}"; do
        wat_file="${base}-unroll${factor}.wat"
        wasm_file="${base}-unroll${factor}.wasm"
        if [[ -f "$wasm_file" ]]; then
            echo "  ✓ $wasm_file (ready for performance testing)"
        fi
        # (No separate no-inline artifacts listed)
    done
done

echo ""
echo "To test in browser, open loop-unrolling-tester.html"
