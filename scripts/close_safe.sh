#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== Stopping SentinelFS Safely ===${NC}"

# 1. Stop Daemon (Graceful Shutdown)
if pgrep -x "sentinel_daemon" > /dev/null; then
    echo -e "${GREEN}Sending shutdown signal to Daemon...${NC}"
    # SIGTERM allows the daemon to close DB connections and save state
    pkill -SIGTERM -x sentinel_daemon
    
    # Wait loop
    count=0
    while pgrep -x "sentinel_daemon" > /dev/null && [ $count -lt 10 ]; do
        echo -n "."
        sleep 0.5
        ((count++))
    done
    echo ""
    
    if pgrep -x "sentinel_daemon" > /dev/null; then
        echo -e "${RED}Daemon did not stop gracefully. Forcing kill...${NC}"
        pkill -SIGKILL -x sentinel_daemon
    else
        echo -e "${GREEN}Daemon stopped successfully.${NC}"
    fi
else
    echo "Daemon is not running."
fi

# 2. Stop UI Processes (Dev Mode)
# We search for processes related to the GUI folder to avoid killing the IDE
echo -e "${GREEN}Stopping UI processes...${NC}"

# Kill the vite dev server started by npm run dev
pkill -f "vite" 2>/dev/null

# Kill any electron processes launched from this project path
# Careful not to kill VS Code or other electron apps
# This assumes the process cmdline contains the project path
pkill -f "sentinelFS-demo/SentinelFS/gui" 2>/dev/null

echo -e "${GREEN}SentinelFS shutdown complete.${NC}"
