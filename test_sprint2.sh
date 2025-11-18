#!/bin/bash

# SentinelFS-Neo Sprint 2 Test Script
# Tests FileAPI + SnapshotEngine

set -e

echo "========================================"
echo "SentinelFS-Neo Sprint 2 - FileAPI Test"
echo "========================================"
echo ""

cd "$(dirname "$0")"

# Build
echo "Building Sprint 2..."
cd build
cmake .. > /dev/null 2>&1
cmake --build . -j$(nproc) > /dev/null 2>&1

if [ ! -f "./bin/sentinelfs-sprint2" ]; then
    echo "❌ Build failed - binary not found"
    exit 1
fi

echo "✓ Build successful"
echo ""

# Run test
echo "Running Sprint 2 test..."
echo "========================================"
./bin/sentinelfs-sprint2

exit_code=$?

if [ $exit_code -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "✅ Sprint 2 Complete!"
    echo "========================================"
else
    echo ""
    echo "❌ Test failed with exit code $exit_code"
    exit $exit_code
fi
