#!/bin/bash
set -e

echo "=========================================="
echo "Building SentinelFS AppImage"
echo "=========================================="

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build_release"
GUI_DIR="$PROJECT_ROOT/gui"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# Check if running on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    log_error "AppImage can only be built on Linux"
fi

# Step 1: Build C++ daemon and libraries
log_info "Building C++ daemon and libraries..."
cd "$PROJECT_ROOT"

if [ -d "$BUILD_DIR" ]; then
    log_info "Removing existing build directory..."
    rm -rf "$BUILD_DIR"
fi

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel $(nproc)
log_success "C++ components built successfully"

# Step 2: Install Node.js dependencies for GUI
log_info "Installing GUI dependencies..."
cd "$GUI_DIR"

if [ ! -d "node_modules" ]; then
    npm install
fi
log_success "GUI dependencies installed"

# Step 3: Build Electron GUI with AppImage
log_info "Building Electron GUI with AppImage packaging..."

# Make sure electron-builder is available
if ! command -v electron-builder &> /dev/null; then
    log_info "Installing electron-builder globally..."
    npm install -g electron-builder
fi

# Build the Electron app (compiles TypeScript and creates AppImage)
npm run build

log_success "Electron GUI built successfully"

# Step 4: Find and report the AppImage location
APPIMAGE_PATH=$(find "$GUI_DIR/dist" -name "*.AppImage" -type f | head -n 1)

if [ -n "$APPIMAGE_PATH" ]; then
    log_success "AppImage created successfully!"
    echo ""
    echo "=========================================="
    echo "AppImage Location:"
    echo "$APPIMAGE_PATH"
    echo "=========================================="
    echo ""
    
    # Make it executable
    chmod +x "$APPIMAGE_PATH"
    
    # Optionally copy to project root for easy access
    APPIMAGE_NAME=$(basename "$APPIMAGE_PATH")
    cp "$APPIMAGE_PATH" "$PROJECT_ROOT/$APPIMAGE_NAME"
    log_success "AppImage copied to: $PROJECT_ROOT/$APPIMAGE_NAME"
    
    echo ""
    echo "To run the AppImage:"
    echo "  ./$APPIMAGE_NAME"
    echo ""
else
    log_error "AppImage not found in $GUI_DIR/dist"
fi

log_success "Build complete!"
