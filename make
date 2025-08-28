#!/bin/bash

# Make wrapper for CMake-based Tubular project
# Provides familiar make commands for the CMake build system

case "$1" in
    "test" | "tests")
        echo "Running tests via CMake..."
        cd build && make tests
        ;;
    "clean")
        echo "Cleaning all files via CMake..."
        cd build && make clean-all
        ;;
    "clean-test")
        echo "Cleaning test files via CMake..."
        cd build && make clean-test
        ;;
    "clean-unroll")
        echo "Cleaning loop unrolling test files via CMake..."
        cd build && make clean-unroll
        ;;
    "")
        echo "Building Tubular via CMake..."
        cd build && make
        ;;
    *)
        echo "Unknown make target: $1"
        echo "Available targets:"
        echo "  make          - Build the project"
        echo "  make test     - Run all tests (standard + loop unrolling)"
        echo "  make clean    - Clean all generated files"
        echo "  make clean-test - Clean only test files"
        echo "  make clean-unroll - Clean only loop unrolling test files"
        exit 1
        ;;
esac
