#!/bin/bash

# Quick daemon launcher with correct environment
# Usage: ./scripts/run_daemon.sh [daemon_args...]

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

export SENTINELFS_PLUGIN_DIR="$PROJECT_ROOT/build/plugins"
export LD_LIBRARY_PATH="$PROJECT_ROOT/build/core:${LD_LIBRARY_PATH:-}"

DAEMON="$PROJECT_ROOT/build/app/daemon/sentinel_daemon"

if [ ! -f "$DAEMON" ]; then
    echo "Error: Daemon not built. Run: cd build && make"
    exit 1
fi

echo "Starting SentinelFS Daemon..."
echo "  Plugin Dir: $SENTINELFS_PLUGIN_DIR"
echo "  Library Path: $LD_LIBRARY_PATH"
echo ""

exec "$DAEMON" "$@"
