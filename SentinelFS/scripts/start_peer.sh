#!/bin/bash
# Start a secondary SentinelFS peer for testing
# Usage: ./start_peer.sh [peer_number] [session_code]
# Example: ./start_peer.sh 2 ABC123

set -e

# Show help
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    echo "SentinelFS Peer Launcher"
    echo ""
    echo "Usage: ./start_peer.sh [PEER_NUMBER] [SESSION_CODE]"
    echo ""
    echo "Arguments:"
    echo "  PEER_NUMBER   Peer instance number (default: 2)"
    echo "                Each peer uses different ports:"
    echo "                  - TCP Port: 9000 + PEER_NUMBER"
    echo "                  - Discovery: 9999 + PEER_NUMBER"
    echo "                  - Metrics: 9100 + PEER_NUMBER"
    echo ""
    echo "  SESSION_CODE  6-character code for peer authentication"
    echo "                If not provided, a random code is generated"
    echo ""
    echo "Examples:"
    echo "  ./start_peer.sh 2           # Start peer #2 with random code"
    echo "  ./start_peer.sh 2 ABC123    # Start peer #2 with code ABC123"
    echo "  ./start_peer.sh 3 ABC123    # Start peer #3 with same code"
    echo ""
    echo "To connect peers, use the SAME session code on all instances."
    exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Default values - validate peer number is numeric
PEER_NUM="${1:-2}"
if ! [[ "$PEER_NUM" =~ ^[0-9]+$ ]]; then
    echo "Error: Peer number must be a number, got: $PEER_NUM"
    echo "Use --help for usage information"
    exit 1
fi

SESSION_CODE="${2:-}"
BASE_PORT=9000
PEER_PORT=$((BASE_PORT + PEER_NUM))

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}╔════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║     SentinelFS Peer #${PEER_NUM} Launcher          ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════╝${NC}"
echo

# Check if daemon exists
if [ ! -f "$BUILD_DIR/app/daemon/sentinel_daemon" ]; then
    echo -e "${RED}✗ Daemon not found. Building...${NC}"
    cd "$BUILD_DIR" && make -j4
fi

# Create peer-specific directories
PEER_DIR="/tmp/sentinelfs_peer${PEER_NUM}"
PEER_SYNC_DIR="$PEER_DIR/sync"
PEER_CONFIG_DIR="$PEER_DIR/config"
PEER_DATA_DIR="$PEER_DIR/data"
PEER_SOCKET="/tmp/sentinel_peer${PEER_NUM}.sock"

echo -e "${BLUE}[1/4]${NC} Creating peer directories..."
mkdir -p "$PEER_SYNC_DIR" "$PEER_CONFIG_DIR" "$PEER_DATA_DIR"

# Create test files in sync directory
echo "Test file from Peer #${PEER_NUM}" > "$PEER_SYNC_DIR/test_peer${PEER_NUM}.txt"
echo -e "${GREEN}✓${NC} Created test file: $PEER_SYNC_DIR/test_peer${PEER_NUM}.txt"

# Create peer-specific config
echo -e "${BLUE}[2/4]${NC} Creating peer configuration..."
cat > "$PEER_CONFIG_DIR/sentinel.conf" << EOF
# SentinelFS Peer #${PEER_NUM} Configuration
sync_directory = ${PEER_SYNC_DIR}
database_path = ${PEER_DATA_DIR}/sentinel.db
log_level = debug
port = ${PEER_PORT}
ipc_socket = ${PEER_SOCKET}
session_code = ${SESSION_CODE}
encryption_enabled = true
EOF

echo -e "${GREEN}✓${NC} Config created at: $PEER_CONFIG_DIR/sentinel.conf"

# Show peer info
echo
echo -e "${BLUE}[3/4]${NC} Peer Configuration:"
echo -e "  ${YELLOW}Peer Number:${NC}    #${PEER_NUM}"
echo -e "  ${YELLOW}Port:${NC}           ${PEER_PORT}"
echo -e "  ${YELLOW}Sync Directory:${NC} ${PEER_SYNC_DIR}"
echo -e "  ${YELLOW}IPC Socket:${NC}     ${PEER_SOCKET}"
echo -e "  ${YELLOW}Session Code:${NC}   ${SESSION_CODE:-'(not set - will use discovery)'}"
echo

# Generate session code if not provided
if [ -z "$SESSION_CODE" ]; then
    SESSION_CODE=$(cat /dev/urandom | tr -dc 'A-Z0-9' | fold -w 6 | head -n 1)
    echo -e "${YELLOW}Generated session code: ${GREEN}${SESSION_CODE}${NC}"
    echo -e "${YELLOW}Share this code with other peers to connect!${NC}"
    echo
fi

# Start the daemon
echo -e "${BLUE}[4/4]${NC} Starting Peer #${PEER_NUM} daemon..."
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo

# Use same discovery port (9999) so peers can find each other
DISCOVERY_PORT=9999
METRICS_PORT=$((9100 + PEER_NUM))

# Must run from build directory so plugins can be found
cd "$BUILD_DIR"

# Set library path for plugins
export LD_LIBRARY_PATH="$BUILD_DIR/plugins/falconstore:$BUILD_DIR/plugins/netfalcon:$BUILD_DIR/plugins/ironroot:$BUILD_DIR/plugins/zer0:$BUILD_DIR/core:$LD_LIBRARY_PATH"

exec "./app/daemon/sentinel_daemon" \
    --dir "$PEER_SYNC_DIR" \
    --port "$PEER_PORT" \
    --discovery "$DISCOVERY_PORT" \
    --socket "$PEER_SOCKET" \
    --db "$PEER_DATA_DIR/sentinel.db" \
    --metrics-port "$METRICS_PORT" \
    --session-code "$SESSION_CODE" \
    --encrypt
