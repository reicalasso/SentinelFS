#!/bin/bash
# Build SentinelFS Linux packages locally
# Usage: ./build-linux.sh [appimage|deb|all]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
GUI_DIR="$PROJECT_ROOT/gui"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

BUILD_TYPE="${1:-all}"

echo ""
echo "=========================================="
echo "  SentinelFS Linux Package Builder"
echo "=========================================="
echo ""

# Check dependencies
log_info "Checking dependencies..."

if ! command -v cmake &> /dev/null; then
    log_error "cmake is required but not installed."
    exit 1
fi

if ! command -v node &> /dev/null; then
    log_error "Node.js is required but not installed."
    exit 1
fi

if ! command -v npm &> /dev/null; then
    log_error "npm is required but not installed."
    exit 1
fi

log_success "All dependencies found"

# Step 1: Build C++ backend
log_info "Building C++ backend..."

cd "$PROJECT_ROOT"

if [ ! -d "build_release" ]; then
    mkdir build_release
fi

cmake -S . -B build_release \
    -DCMAKE_BUILD_TYPE=Release \
    -DSKIP_SYSTEM_INSTALL=OFF

cmake --build build_release --parallel $(nproc)

log_success "C++ backend built"

# Step 2: Verify backend binaries exist
log_info "Verifying backend binaries..."

REQUIRED_FILES=(
    "build_release/app/daemon/sentinel_daemon"
    "build_release/app/cli/sentinel_cli"
)

for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$PROJECT_ROOT/$file" ]; then
        log_error "Required file not found: $file"
        exit 1
    fi
done

log_success "Backend binaries verified"

# Step 3: Install GUI dependencies
log_info "Installing GUI dependencies..."
cd "$GUI_DIR"
npm ci

log_success "GUI dependencies installed"

# Step 4: Generate icons if needed
if [ ! -f "$GUI_DIR/assets/icons/256x256.png" ]; then
    log_info "Generating icons..."
    if [ -f "$GUI_DIR/scripts/generate-icons.sh" ]; then
        bash "$GUI_DIR/scripts/generate-icons.sh" || log_warn "Icon generation failed, continuing..."
    fi
fi

# Step 5: Build packages
log_info "Building packages: $BUILD_TYPE"

case "$BUILD_TYPE" in
    appimage)
        npm run build:appimage
        ;;
    deb)
        npm run build:deb
        ;;
    all)
        npm run build:all
        ;;
    *)
        log_error "Unknown build type: $BUILD_TYPE"
        echo "Usage: $0 [appimage|deb|all]"
        exit 1
        ;;
esac

log_success "Build complete!"

# Step 6: Show results
echo ""
echo "=========================================="
echo "  Build Results"
echo "=========================================="

if [ -d "$GUI_DIR/dist" ]; then
    echo ""
    log_info "Generated packages:"
    ls -lh "$GUI_DIR/dist/"*.AppImage "$GUI_DIR/dist/"*.deb 2>/dev/null || true
    echo ""
    
    # Generate checksums
    cd "$GUI_DIR/dist"
    for file in *.AppImage *.deb; do
        if [ -f "$file" ]; then
            sha256sum "$file" > "$file.sha256"
            log_info "Checksum: $file.sha256"
        fi
    done
fi

echo ""
log_success "All done! Packages are in: $GUI_DIR/dist/"
echo ""
