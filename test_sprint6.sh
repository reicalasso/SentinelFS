#!/bin/bash

# Sprint 6 Test Script - Auto-Remesh Engine
# Tests adaptive P2P topology management

set -e

echo "======================================"
echo "Sprint 6: Auto-Remesh Engine Test"
echo "======================================"
echo ""

# Navigate to SentinelFS directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "❌ Build directory not found. Please run build first."
    echo "   Try: cd build && cmake .. && make"
    exit 1
fi

# Build the project
echo "🔨 Building Sprint 6 components..."
cd build
cmake --build . --target sentinelfs-sprint6
echo "✅ Build complete"
echo ""

# Check if executable exists
if [ ! -f "bin/sentinelfs-sprint6" ]; then
    echo "❌ Sprint 6 test executable not found"
    exit 1
fi

# Run the test
echo "🚀 Running Sprint 6 Test..."
echo ""
echo "This test will demonstrate:"
echo "  • Network quality metrics collection (RTT, jitter, packet loss)"
echo "  • Peer scoring algorithm (composite quality score 0-100)"
echo "  • Auto-remesh engine with adaptive topology"
echo "  • Automatic poor performer detection and removal"
echo "  • Real-time topology optimization"
echo ""
echo "Press Enter to continue..."
read

./bin/sentinelfs-sprint6

# Check exit code
EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo "======================================"
    echo "✅ Sprint 6 Test PASSED"
    echo "======================================"
    echo ""
    echo "Sprint 6 Features Validated:"
    echo "  ✓ NetworkMetrics structure with RTT/jitter/loss tracking"
    echo "  ✓ PeerScorer with weighted composite scoring"
    echo "  ✓ Enhanced PeerRegistry with quality-based queries"
    echo "  ✓ AutoRemesh engine with evaluation loop"
    echo "  ✓ Poor performer detection with hysteresis"
    echo "  ✓ Topology change event notifications"
    echo "  ✓ Dynamic peer quality monitoring"
    echo ""
    echo "Next Steps:"
    echo "  → Sprint 7: Storage Layer (metadata persistence)"
    echo "  → Sprint 8: ML Layer (anomaly detection)"
    echo ""
else
    echo "======================================"
    echo "❌ Sprint 6 Test FAILED (exit code: $EXIT_CODE)"
    echo "======================================"
    exit $EXIT_CODE
fi
