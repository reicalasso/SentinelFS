#!/bin/bash

# Ensure we are in the project root
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== SentinelFS Simplified Launch ===${NC}"

# 1. Clean previous state if requested (optional, but ensures 'simple' start)

# 2. Build Daemon
echo -e "${GREEN}Building Daemon...${NC}"
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi

make -j$(nproc)
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Daemon built successfully.${NC}"
cd ..

# 3. Setup & Run UI
echo -e "${GREEN}Starting UI...${NC}"
cd gui

# Check if node_modules exists, if not install
if [ ! -d "node_modules" ]; then
    echo -e "${BLUE}Installing UI dependencies...${NC}"
    npm install
fi

echo -e "${GREEN}Launching Application...${NC}"
# This will launch Electron, which spawns the daemon from ../build/app/daemon/sentinel_daemon
# Suppress GTK/GLib warnings that are harmless but noisy
export GDK_BACKEND=x11
npm run dev 2>&1 | grep -v "GLib-GObject\|GtkFileChooserNative\|gsignal.c"

