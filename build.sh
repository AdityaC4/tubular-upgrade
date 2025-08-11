#!/bin/bash

# Build script for Tubular project
# Usage: ./build.sh [release|debug|grumpy] [clean] [tests]

set -e  # Exit on any error

PROJECT_NAME="Tubular"
BUILD_DIR="build"

# Default build type
BUILD_TYPE="Release"

# Parse command line arguments
CLEAN=false
RUN_TESTS=false

for arg in "$@"; do
    case $arg in
        release)
            BUILD_TYPE="Release"
            ;;
        debug)
            BUILD_TYPE="Debug"
            ;;
        grumpy)
            BUILD_TYPE="Grumpy"
            ;;
        clean)
            CLEAN=true
            ;;
        tests)
            RUN_TESTS=true
            ;;
        help|--help|-h)
            echo "Usage: $0 [release|debug|grumpy] [clean] [tests]"
            echo ""
            echo "Build types:"
            echo "  release  - Optimized build with -O3 -DNDEBUG (default)"
            echo "  debug    - Debug build with -g"
            echo "  grumpy   - Extra warnings with -pedantic -Wconversion -Weffc++"
            echo ""
            echo "Options:"
            echo "  clean    - Clean before building"
            echo "  tests    - Run tests after building"
            echo ""
            echo "Examples:"
            echo "  $0                    # Release build"
            echo "  $0 debug              # Debug build"
            echo "  $0 grumpy clean tests # Grumpy build with clean and tests"
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Use '$0 help' for usage information"
            exit 1
            ;;
    esac
done

echo "================================================"
echo "Building $PROJECT_NAME"
echo "Build type: $BUILD_TYPE"
echo "Clean: $CLEAN"
echo "Run tests: $RUN_TESTS"
echo "================================================"

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p $BUILD_DIR
else
    echo "Using existing build directory..."
fi

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning..."
    if [ -d "$BUILD_DIR" ]; then
        cd $BUILD_DIR
        if [ -f "Makefile" ]; then
            make clean-all 2>/dev/null || true
        fi
        cd ..
    fi
    # Also clean any build artifacts in root
    rm -f $PROJECT_NAME *.o
    rm -rf $PROJECT_NAME.dSYM
    echo "Clean completed."
fi

# Configure and build
echo "Configuring..."
cd $BUILD_DIR
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

echo "Building..."
make -j$(nproc 2>/dev/null || echo 4)  # Use all CPU cores, fallback to 4

echo "Build completed successfully!"

# Run tests if requested
if [ "$RUN_TESTS" = true ]; then
    echo ""
    echo "Running tests..."
    make tests
fi

echo ""
echo "Executable: $BUILD_DIR/$PROJECT_NAME"
echo "To run: ./$BUILD_DIR/$PROJECT_NAME"
