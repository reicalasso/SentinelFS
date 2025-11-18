#!/bin/bash

# SentinelFS-Neo Sprint 5 Test Script
# Tests Network Layer - Peer Discovery & TCP Transfer

set -e

echo "========================================"
echo "SentinelFS-Neo Sprint 5 - Build & Test"
echo "         Network Layer"
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
echo "  Peer Registry: $(ls -lh lib/libpeer_registry.a)"
echo "  UDP Discovery Plugin: $(ls -lh lib/discovery_udp.so)"
echo "  TCP Transfer Plugin: $(ls -lh lib/transfer_tcp.so)"
echo "  Sprint 5 Test: $(ls -lh bin/sentinelfs-sprint5)"

# Run test
echo ""
echo "Running Sprint 5 test application..."
echo "========================================"
echo ""
echo "NOTE: This test demonstrates peer discovery and TCP transfer."
echo "      To test with multiple peers, run this on another machine"
echo "      or in another terminal on the same network."
echo ""
echo "Starting test..."
echo ""
./bin/sentinelfs-sprint5

echo ""
echo "========================================"
echo "   Sprint 5 Test Complete!"
echo "========================================"
echo ""
