#!/bin/bash

# SentinelFS-Neo Sprint 4 Test Script
# Tests Delta Engine (Rsync-style)

set -e

echo "========================================"
echo "SentinelFS-Neo Sprint 4 - Delta Engine"
echo "========================================"
echo ""

cd "$(dirname "$0")"

# Build
echo "Building Sprint 4..."
cd build 2>/dev/null || (cd .. && mkdir -p build && cd build)
cmake .. > /dev/null 2>&1
cmake --build . -j$(nproc) 2>&1 | grep -E "(error|Building|Built)" || true

if [ ! -f "./bin/sentinelfs-sprint4" ]; then
    echo "❌ Build failed - binary not found"
    exit 1
fi

echo "✓ Build successful"
echo "  - sentinelfs-sprint4 executable"
echo ""

# Run test
echo "Running Sprint 4 test..."
echo "========================================"
./bin/sentinelfs-sprint4

exit_code=$?

if [ $exit_code -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "✅ Sprint 4 Complete!"
    echo "========================================"
else
    echo ""
    echo "❌ Test exited with code $exit_code"
    exit $exit_code
fi
