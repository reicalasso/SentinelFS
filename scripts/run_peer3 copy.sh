#!/bin/bash
# Run a second SentinelFS daemon instance for testing peer discovery
# This instance uses different ports to avoid conflicts

DAEMON_PATH="${DAEMON_PATH:-/home/rei/sentinelFS-demo/build/app/daemon/sentinel_daemon}"
PLUGIN_DIR="${PLUGIN_DIR:-/home/rei/sentinelFS-demo/SentinelFS/build/plugins}"
RUNTIME_DIR="/tmp/sentinelfs_peer2"

# Create runtime directory
mkdir -p "$RUNTIME_DIR"

# Configuration for second peer
TCP_PORT=8081
DISCOVERY_PORT=9999  # Same discovery port to find each other
METRICS_PORT=9101
SOCKET_PATH="$RUNTIME_DIR/sentinel_daemon.sock"
DB_PATH="$RUNTIME_DIR/sentinel.db"
WATCH_DIR="$RUNTIME_DIR/sync"

# Create watch directory
mkdir -p "$WATCH_DIR"

echo "Starting SentinelFS Peer 2..."
echo "  TCP Port: $TCP_PORT"
echo "  Discovery Port: $DISCOVERY_PORT"
echo "  Metrics Port: $METRICS_PORT"
echo "  Socket: $SOCKET_PATH"
echo "  Database: $DB_PATH"
echo "  Watch Dir: $WATCH_DIR"
echo ""

# Run the daemon
SENTINELFS_PLUGIN_DIR="$PLUGIN_DIR" "$DAEMON_PATH" \
    --port "$TCP_PORT" \
    --discovery "$DISCOVERY_PORT" \
    --metrics-port "$METRICS_PORT" \
    --socket "$SOCKET_PATH" \
    --db "$DB_PATH" \
    --dir "$WATCH_DIR"
