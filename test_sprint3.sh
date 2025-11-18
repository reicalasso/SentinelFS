#!/bin/bash

# SentinelFS-Neo Sprint 3 Test Script
# Tests Filesystem Watcher Plugin

set -e

echo "========================================"
echo "SentinelFS-Neo Sprint 3 - Watcher Test"
echo "========================================"
echo ""

cd "$(dirname "$0")"

# Build
echo "Building Sprint 3..."
cd build 2>/dev/null || (cd .. && mkdir -p build && cd build)
cmake .. > /dev/null 2>&1
cmake --build . -j$(nproc) 2>&1 | grep -E "(error|Building|Built)" || true

if [ ! -f "./bin/sentinelfs-sprint3-simple" ]; then
    echo "❌ Build failed - binary not found"
    exit 1
fi

if [ ! -f "./lib/watcher_linux.so" ]; then
    echo "❌ Build failed - watcher plugin not found"
    exit 1
fi

echo "✓ Build successful"
echo "  - sentinelfs-sprint3-simple executable"
echo "  - watcher_linux.so plugin"
echo ""

# Run test
echo "Running Sprint 3 test..."
echo "========================================"
./bin/sentinelfs-sprint3-simple

exit_code=$?

if [ $exit_code -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "✅ Sprint 3 Complete!"
    echo "========================================"
else
    echo ""
    echo "❌ Test exited with code $exit_code"
    exit $exit_code
fi
