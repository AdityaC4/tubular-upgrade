# Development Setup Guide

This guide provides complete setup instructions for developing and testing the Tubular WebAssembly compiler.

## Prerequisites

### Required Dependencies

#### 1. Build Tools
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake git

# macOS
xcode-select --install
brew install cmake
```

#### 2. WebAssembly Toolchain

**WABT (WebAssembly Binary Toolkit) - Provides wat2wasm:**
```bash
# Ubuntu/Debian
sudo apt install -y wabt

# macOS
brew install wabt

# Verify installation
wat2wasm --version
```

**Wasmtime Runtime:**
```bash
# Linux x86_64
cd /tmp
wget https://github.com/bytecodealliance/wasmtime/releases/download/v25.0.3/wasmtime-v25.0.3-x86_64-linux.tar.xz
tar xf wasmtime-v25.0.3-x86_64-linux.tar.xz
sudo cp wasmtime-v25.0.3-x86_64-linux/wasmtime /usr/local/bin/

# macOS
brew install wasmtime

# Verify installation
wasmtime --version
```

#### 3. Python (for autotuning framework)
```bash
# Ubuntu/Debian
sudo apt install -y python3 python3-pip

# macOS
brew install python3

# Install required packages
pip3 install numpy matplotlib pandas
```

## Project Setup

### 1. Clone and Build
```bash
git clone https://github.com/AdityaC4/tubular-upgrade.git
cd tubular-upgrade

# Build the compiler (creates build/ directory)
./build.sh release

# Verify build
./build/Tubular --help
```

### 2. Run Tests
```bash
# Run complete test suite
./make test

# Individual test suites
cd tests && ./run_tests.sh                           # Standard tests
cd tests/loop-unrolling && ./run_unroll_tests.sh    # Loop unrolling tests
cd tests/function-inlining && ./run_inlining_tests.sh # Function inlining tests
```

### 3. Verify WebAssembly Pipeline
```bash
# Test complete compilation pipeline
./build/Tubular tests/test-01.tube    # Should generate test-01.wat
wat2wasm tests/test-01.wat            # Should generate test-01.wasm
wasmtime tests/test-01.wasm           # Should execute without error
```

## Development Workflow

### Build Options
```bash
./build.sh release      # Release build with -O3 (default)
./build.sh debug        # Debug build with -g
./build.sh grumpy       # Extra warnings with -pedantic
./build.sh clean tests  # Clean and run tests
```

### CMake Direct Usage
```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Clean
cmake --build build --target clean
```

### Testing During Development
```bash
# Quick build and test
./build.sh clean tests

# Test specific optimization
./build/Tubular tests/test-01.tube --unroll-factor=4

# Performance testing
cd tests && python -m http.server
# Open: http://localhost:8000/loop-unrolling/loop-unrolling-tester.html
```

## IDE Setup

### VS Code Configuration
Create `.vscode/settings.json`:
```json
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "C_Cpp.default.cppStandard": "c++20",
    "files.associations": {
        "*.tube": "plaintext"
    }
}
```

### Compile Commands
The build system generates `compile_commands.json` for IDE integration:
```bash
./build.sh  # Generates build/compile_commands.json
```

## Troubleshooting

### Common Issues

#### 1. wat2wasm not found
```bash
# Verify WABT installation
which wat2wasm
wat2wasm --version

# If missing, install via package manager or build from source
```

#### 2. wasmtime not found
```bash
# Verify Wasmtime installation
which wasmtime
wasmtime --version

# Check PATH includes /usr/local/bin
echo $PATH
```

#### 3. Build errors
```bash
# Clean rebuild
./build.sh clean
./build.sh debug  # For better error messages

# Check C++20 support
gcc --version  # Should be >= 8.0
```

#### 4. Test failures
```bash
# Run individual test for debugging
./build/Tubular tests/test-01.tube -v  # Verbose output

# Check generated WAT file
cat tests/test-01.wat

# Manual WebAssembly execution
wat2wasm tests/test-01.wat
wasmtime tests/test-01.wasm
```

### Performance Testing Issues

#### Browser tests not loading
```bash
# Start HTTP server for browser tests
cd tests
python3 -m http.server 8000

# Access via: http://localhost:8000/loop-unrolling/loop-unrolling-tester.html
```

#### Inconsistent timing results
- Ensure no other heavy processes running
- Use multiple runs for statistical significance
- Consider CPU frequency scaling effects

## Development Guidelines

### Code Style
```bash
# Format code before committing
clang-format -i $(git ls-files '*.cpp' '*.hpp')
```

### Testing New Features
1. Add unit tests for new passes
2. Update integration tests
3. Add performance benchmarks if applicable
4. Verify all optimization combinations work

### Debugging Optimizations
```bash
# Compile with debug info
./build.sh debug

# Use verbose output
./build/Tubular test.tube --verbose

# Check generated WAT
cat test.wat

# Compare optimization results
./build/Tubular test.tube --no-inline > no-inline.wat
./build/Tubular test.tube > with-inline.wat
diff no-inline.wat with-inline.wat
```

## Docker Development Environment

For consistent development environment:

```dockerfile
FROM ubuntu:24.04

RUN apt update && apt install -y \
    build-essential cmake git \
    wabt python3 python3-pip \
    wget curl

# Install wasmtime
RUN cd /tmp && \
    wget https://github.com/bytecodealliance/wasmtime/releases/download/v25.0.3/wasmtime-v25.0.3-x86_64-linux.tar.xz && \
    tar xf wasmtime-v25.0.3-x86_64-linux.tar.xz && \
    cp wasmtime-v25.0.3-x86_64-linux/wasmtime /usr/local/bin/

WORKDIR /workspace
```

Build and run:
```bash
docker build -t tubular-dev .
docker run -v $(pwd):/workspace -it tubular-dev bash
```

## Next Steps

Once setup is complete:
1. Review [PROJECT_ISSUES.md](PROJECT_ISSUES.md) for development roadmap
2. Check [PLAN.md](PLAN.md) for research objectives
3. Explore optimization passes in `src/middle_end/`
4. Run performance benchmarks in `tests/loop-unrolling/`

For questions or issues, refer to the project documentation or create a GitHub issue.