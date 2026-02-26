# Build System Documentation

## Overview

Sentience MUD provides multiple build approaches for flexibility and different use cases. All build systems support conditional compilation for the integration test framework.

## Build Scripts (Recommended)

### ./build - One-Step Build
Combines configuration and compilation in a single command.

```bash
# Basic usage
./build                 # Build Debug with default compiler
./build release         # Build Release configuration
./build tests           # Build Debug with test framework
./build tests release   # Build Release with test framework

# Compiler selection
./build gcc debug       # Build Debug with GCC
./build clang release   # Build Release with Clang

# Maintenance
./build clean release   # Clean rebuild Release
./build reconfig gcc    # Force reconfigure with GCC, then build
```

### ./config + ./compile - Two-Step Build
More granular control over configuration and compilation phases.

```bash
# Configure once
./config               # Configure with default settings
./config gcc           # Configure with GCC
./config tests         # Configure with test framework

# Compile multiple times
./compile              # Build Debug
./compile release      # Build Release
./compile clean debug  # Clean rebuild Debug
```

## Direct Build Systems

### CMake (Advanced)
For direct CMake control or integration with IDEs.

```bash
# Configure
cmake -S . -B .build -G "Ninja Multi-Config"
cmake -S . -B .build -DBUILD_TESTS=ON  # With tests
cmake -S . -B .build -DCMAKE_C_COMPILER=gcc  # With GCC

# Build
cmake --build .build --config Debug
cmake --build .build --config Release --clean-first
```

### Makefile (Simple)
For quick builds without CMake infrastructure.

```bash
# Basic build
make                    # Build with GCC (outputs to ./sent)
make BUILD_TESTS=1      # Build with test framework
make clean              # Clean build
```

## Build Configurations

### Debug (Default)
- Debug symbols included
- No optimization
- Best for development and debugging
- Output: `.build/Debug/sent`

### Release
- Optimized for performance
- No debug symbols
- Best for production deployment
- Output: `.build/Release/sent`

### RelWithDebInfo
- Optimized with debug symbols
- Good for profiling and production debugging
- Output: `.build/RelWithDebInfo/sent`

## Test Framework Integration

### Enabling Tests
Add `tests` option to any build command:

```bash
./build tests           # Debug build with tests
./build tests release   # Release build with tests
./config tests          # Configure with tests
make BUILD_TESTS=1      # Makefile with tests
```

### Running Tests
After building with test support:

```bash
./sent -test            # Run all tests
./sent -test:unit       # Run unit tests
./sent -test:integration # Run integration tests
./sent -test:wnum       # Run pattern-matched tests
```

## Code Coverage

### One-Step Coverage Report

```bash
./build coverage
```

This single command will:
1. Configure with coverage instrumentation and test framework enabled
2. Build Debug configuration
3. Run the full test suite
4. Capture coverage data with lcov
5. Filter out external dependencies and test framework code
6. Generate an HTML report and print a summary

### Output Locations

| File | Description |
|------|-------------|
| `.build/coverage_html/index.html` | Interactive HTML coverage report |
| `.build/coverage_filtered.info` | Filtered lcov data (project code only) |
| `.build/coverage.info` | Raw lcov data (includes all sources) |

### Manual Coverage Workflow

For more control, configure and build separately, then run lcov manually:

```bash
# Configure and build with coverage
./config coverage
./compile

# Run tests to generate .gcda files
cd /sentience && ./sent -test && cd src

# Capture and filter
lcov --capture --directory .build -o .build/coverage.info --ignore-errors mismatch
lcov --remove .build/coverage.info '*/.deps/*' '*/tests/*' '/usr/*' -o .build/coverage_filtered.info

# Generate HTML report
genhtml .build/coverage_filtered.info -o .build/coverage_html

# View summary
lcov --summary .build/coverage_filtered.info
```

### Makefile Coverage

```bash
make BUILD_COVERAGE=1       # Build with coverage flags (implies BUILD_TESTS=1)
```

Note: The Makefile only handles compilation with coverage flags. Use the `./build` script for the full coverage pipeline (build + test + report).

### Prerequisites

Coverage analysis requires `lcov` and `genhtml`:

```bash
# Fedora/RHEL
sudo dnf install lcov

# Debian/Ubuntu
sudo apt install lcov
```

## Installation

After building, install the binary to project root (only necessary if changing between debug and release):

```bash
./install debug         # Install Debug build as /sentience/sent
./install release       # Install Release build as /sentience/sent
./install               # Install Debug build (default)
```

## Build System Comparison

| Feature | ./build | ./config + ./compile | CMake Direct | Makefile |
|---------|---------|---------------------|--------------|----------|
| Ease of Use | ★★★★★ | ★★★★☆ | ★★☆☆☆ | ★★★☆☆ |
| Flexibility | ★★★★☆ | ★★★★★ | ★★★★★ | ★★☆☆☆ |
| IDE Integration | ★★★☆☆ | ★★★☆☆ | ★★★★★ | ★☆☆☆☆ |
| Build Speed | ★★★★☆ | ★★★★☆ | ★★★★☆ | ★★★★★ |
| Multi-Config | ★★★★★ | ★★★★★ | ★★★★★ | ★☆☆☆☆ |

## Quick Reference

### Common Development Workflows

**Daily Development:**
```bash
./build                 # Fast debug build
./install               # Install for testing
```

**Testing:**
```bash
./build tests           # Build with test framework
./sent -test:unit       # Run unit tests
```

**Release Preparation:**
```bash
./build clean release   # Clean release build
./install release       # Install release binary
```

**CI/CD Pipeline:**
```bash
./build clean tests release  # Build release with tests
./sent -test               # Run full test suite
echo $?                    # Check exit code
```

### Troubleshooting

**Build fails after git pull:**
```bash
./build reconfig        # Force reconfiguration
```

**Clean everything:**
```bash
rm -rf .build obj sent  # Remove all build artifacts
./build                 # Fresh build
```

**Switch compilers:**
```bash
./build reconfig gcc    # Force reconfigure with GCC
./build reconfig clang  # Force reconfigure with Clang
```

All build approaches maintain synchronized CMakeLists.txt and Makefile for consistency across different development environments and CI systems.