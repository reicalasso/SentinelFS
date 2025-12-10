#!/bin/bash
#
# SentinelFS Dependency Installer
# Automatically installs all required dependencies for building and running SentinelFS
#
# Usage: ./install_deps.sh [--dev] [--gui] [--all]
#   --dev   Install development dependencies only (build tools)
#   --gui   Install GUI dependencies only (Node.js, npm)
#   --all   Install everything (default)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Detect package manager
detect_package_manager() {
    if command -v apt-get &> /dev/null; then
        PKG_MANAGER="apt"
        PKG_UPDATE="sudo apt-get update"
        PKG_INSTALL="sudo apt-get install -y"
    elif command -v dnf &> /dev/null; then
        PKG_MANAGER="dnf"
        PKG_UPDATE="sudo dnf check-update || true"
        PKG_INSTALL="sudo dnf install -y"
    elif command -v yum &> /dev/null; then
        PKG_MANAGER="yum"
        PKG_UPDATE="sudo yum check-update || true"
        PKG_INSTALL="sudo yum install -y"
    elif command -v pacman &> /dev/null; then
        PKG_MANAGER="pacman"
        PKG_UPDATE="sudo pacman -Sy"
        PKG_INSTALL="sudo pacman -S --noconfirm"
    elif command -v zypper &> /dev/null; then
        PKG_MANAGER="zypper"
        PKG_UPDATE="sudo zypper refresh"
        PKG_INSTALL="sudo zypper install -y"
    else
        log_error "No supported package manager found (apt, dnf, yum, pacman, zypper)"
        exit 1
    fi
    log_info "Detected package manager: $PKG_MANAGER"
}

# Build dependencies (C++ backend)
install_build_deps() {
    log_info "Installing build dependencies..."
    
    case $PKG_MANAGER in
        apt)
            $PKG_INSTALL \
                build-essential \
                cmake \
                pkg-config \
                libssl-dev \
                libsqlite3-dev \
                libfuse3-dev \
                libuv1-dev \
                libcurl4-openssl-dev \
                libsodium-dev \
                nlohmann-json3-dev \
                git
            ;;
        dnf|yum)
            $PKG_INSTALL \
                gcc-c++ \
                cmake \
                pkgconfig \
                openssl-devel \
                sqlite-devel \
                fuse3-devel \
                libuv-devel \
                libcurl-devel \
                libsodium-devel \
                json-devel \
                git
            ;;
        pacman)
            $PKG_INSTALL \
                base-devel \
                cmake \
                pkgconf \
                openssl \
                sqlite \
                fuse3 \
                libuv \
                curl \
                libsodium \
                nlohmann-json \
                git
            ;;
        zypper)
            $PKG_INSTALL \
                gcc-c++ \
                cmake \
                pkg-config \
                libopenssl-devel \
                sqlite3-devel \
                fuse3-devel \
                libuv-devel \
                libcurl-devel \
                libsodium-devel \
                nlohmann_json-devel \
                git
            ;;
    esac
    
    log_success "Build dependencies installed"
}

# Node.js installation
install_nodejs() {
    log_info "Checking Node.js installation..."
    
    if command -v node &> /dev/null; then
        local node_version=$(node --version | sed 's/v//' | cut -d. -f1)
        if [ "$node_version" -ge 18 ]; then
            log_success "Node.js $(node --version) already installed"
            return 0
        else
            log_warn "Node.js version too old ($(node --version)), need v18+"
        fi
    fi
    
    log_info "Installing Node.js v20 LTS..."
    
    case $PKG_MANAGER in
        apt)
            # Use NodeSource repository for latest LTS
            if [ ! -f /etc/apt/sources.list.d/nodesource.list ]; then
                curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
            fi
            $PKG_INSTALL nodejs
            ;;
        dnf|yum)
            curl -fsSL https://rpm.nodesource.com/setup_20.x | sudo bash -
            $PKG_INSTALL nodejs
            ;;
        pacman)
            $PKG_INSTALL nodejs npm
            ;;
        zypper)
            $PKG_INSTALL nodejs20 npm20
            ;;
    esac
    
    log_success "Node.js $(node --version) installed"
}

# GUI dependencies (Electron build requirements)
install_gui_deps() {
    log_info "Installing GUI dependencies..."
    
    case $PKG_MANAGER in
        apt)
            $PKG_INSTALL \
                libgtk-3-0 \
                libnotify4 \
                libnss3 \
                libxss1 \
                libxtst6 \
                xdg-utils \
                libatspi2.0-0 \
                libuuid1 \
                libsecret-1-0 \
                libfuse2 \
                libgbm1 \
                libasound2
            ;;
        dnf|yum)
            $PKG_INSTALL \
                gtk3 \
                libnotify \
                nss \
                libXScrnSaver \
                libXtst \
                xdg-utils \
                at-spi2-core \
                libuuid \
                libsecret \
                fuse-libs \
                mesa-libgbm \
                alsa-lib
            ;;
        pacman)
            $PKG_INSTALL \
                gtk3 \
                libnotify \
                nss \
                libxss \
                libxtst \
                xdg-utils \
                at-spi2-core \
                libsecret \
                fuse2 \
                alsa-lib
            ;;
        zypper)
            $PKG_INSTALL \
                gtk3 \
                libnotify4 \
                mozilla-nss \
                libXScrnSaver1 \
                libXtst6 \
                xdg-utils \
                at-spi2-core \
                libsecret-1-0 \
                fuse \
                libgbm1 \
                alsa
            ;;
    esac
    
    log_success "GUI dependencies installed"
}

# Install npm packages for GUI
install_npm_packages() {
    log_info "Installing npm packages for GUI..."
    
    if [ ! -d "$PROJECT_ROOT/gui" ]; then
        log_warn "GUI directory not found, skipping npm install"
        return 0
    fi
    
    cd "$PROJECT_ROOT/gui"
    
    if [ -f "package-lock.json" ]; then
        npm ci
    else
        npm install
    fi
    
    log_success "npm packages installed"
}

# Optional: Install electron-builder globally
install_electron_builder() {
    log_info "Checking electron-builder..."
    
    if ! command -v electron-builder &> /dev/null; then
        log_info "Installing electron-builder globally..."
        sudo npm install -g electron-builder
    fi
    
    log_success "electron-builder ready"
}

# Verify installation
verify_installation() {
    log_info "Verifying installation..."
    
    local missing=()
    
    command -v cmake &> /dev/null || missing+=("cmake")
    command -v make &> /dev/null || missing+=("make")
    command -v g++ &> /dev/null || missing+=("g++")
    command -v pkg-config &> /dev/null || missing+=("pkg-config")
    command -v node &> /dev/null || missing+=("node")
    command -v npm &> /dev/null || missing+=("npm")
    command -v git &> /dev/null || missing+=("git")
    
    if [ ${#missing[@]} -ne 0 ]; then
        log_error "Missing tools after installation: ${missing[*]}"
        return 1
    fi
    
    echo ""
    log_success "All dependencies verified!"
    echo ""
    echo -e "${BLUE}Installed versions:${NC}"
    echo "  cmake:  $(cmake --version | head -1)"
    echo "  g++:    $(g++ --version | head -1)"
    echo "  node:   $(node --version)"
    echo "  npm:    $(npm --version)"
    echo ""
}

# Print usage
print_usage() {
    echo "SentinelFS Dependency Installer"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --dev       Install development/build dependencies only"
    echo "  --gui       Install GUI dependencies only (Node.js + Electron deps)"
    echo "  --npm       Install npm packages for GUI"
    echo "  --all       Install everything (default)"
    echo "  --check     Only check if dependencies are installed"
    echo "  --help      Show this help message"
    echo ""
}

# Check only mode
check_dependencies() {
    log_info "Checking installed dependencies..."
    
    local all_ok=true
    
    echo ""
    echo "Build tools:"
    for cmd in cmake make g++ pkg-config git; do
        if command -v $cmd &> /dev/null; then
            echo -e "  ${GREEN}✓${NC} $cmd"
        else
            echo -e "  ${RED}✗${NC} $cmd"
            all_ok=false
        fi
    done
    
    echo ""
    echo "Node.js:"
    for cmd in node npm; do
        if command -v $cmd &> /dev/null; then
            echo -e "  ${GREEN}✓${NC} $cmd ($(command $cmd --version 2>/dev/null || echo 'unknown'))"
        else
            echo -e "  ${RED}✗${NC} $cmd"
            all_ok=false
        fi
    done
    
    echo ""
    if [ "$all_ok" = true ]; then
        log_success "All dependencies are installed"
        return 0
    else
        log_warn "Some dependencies are missing. Run: $0 --all"
        return 1
    fi
}

# Main
main() {
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║   SentinelFS Dependency Installer      ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
    echo ""
    
    case "${1:-}" in
        --help|-h)
            print_usage
            exit 0
            ;;
        --check)
            check_dependencies
            exit $?
            ;;
        --dev)
            detect_package_manager
            $PKG_UPDATE
            install_build_deps
            verify_installation
            ;;
        --gui)
            detect_package_manager
            $PKG_UPDATE
            install_nodejs
            install_gui_deps
            install_npm_packages
            verify_installation
            ;;
        --npm)
            install_npm_packages
            ;;
        --all|"")
            detect_package_manager
            log_info "Updating package lists..."
            $PKG_UPDATE
            install_build_deps
            install_nodejs
            install_gui_deps
            install_npm_packages
            install_electron_builder
            verify_installation
            ;;
        *)
            log_error "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
    
    echo ""
    log_success "Dependency installation complete!"
    echo ""
    echo "Next steps:"
    echo "  1. Build the project:  ./scripts/build-linux.sh"
    echo "  2. Or run directly:    ./scripts/start_safe.sh"
    echo ""
}

main "$@"
