#!/bin/bash

# SentinelFS-Neo Sprint 1 Test Script
# Tests Core Infrastructure

set -e

echo "========================================"
echo "SentinelFS-Neo Sprint 1 - Build & Test"
echo "========================================"
echo ""

# Navigate to project root
cd "$(dirname "$0")"

# Clean previous build
if [ -d "build" ]; then
    echo "Cleaning previous build..."
    rm -rf build
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure
echo ""
echo "Configuring CMake..."
cmake .. || { echo "CMake configuration failed"; exit 1; }

# Build
echo ""
echo "Building project..."
cmake --build . -j$(nproc) || { echo "Build failed"; exit 1; }

# Check outputs
echo ""
echo "Build artifacts:"
echo "  Core library: $(ls -lh lib/libsfs-core.a)"
echo "  Test app: $(ls -lh bin/sentinelfs-test)"
echo "  Example plugin: $(ls -lh lib/hello_plugin.so 2>/dev/null || echo 'Not built')"

# Run test
echo ""
echo "Running test application..."
echo "========================================"
./bin/sentinelfs-test

echo ""
echo "========================================"
echo "✓ Sprint 1 Complete!"
echo "========================================"
