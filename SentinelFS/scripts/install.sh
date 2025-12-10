#!/usr/bin/env bash
set -euo pipefail

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

# Auto-install dependencies if --install-deps flag is passed
if [[ "${1:-}" == "--install-deps" ]]; then
    log_info "Installing dependencies first..."
    "$SCRIPT_DIR/install_deps.sh" --all
    shift
fi

BUILD_DIR=${BUILD_DIR:-build}
CMAKE_BIN=${CMAKE_BIN:-cmake}
PREFIX=${PREFIX:-/usr/local}
DESTDIR=${DESTDIR:-}
SERVICE_NAME=sentinel_daemon.service
CONFIG_NAME=sentinel.conf
CACHE_FILE="${BUILD_DIR}/CMakeCache.txt"

if ! command -v "${CMAKE_BIN}" >/dev/null 2>&1; then
  log_warn "${CMAKE_BIN} not found in PATH."
  echo ""
  read -p "Would you like to install dependencies automatically? [y/N] " -n 1 -r
  echo ""
  if [[ $REPLY =~ ^[Yy]$ ]]; then
    "$SCRIPT_DIR/install_deps.sh" --dev
  else
    log_error "Please install CMake 3.17+ or run: $0 --install-deps"
    exit 1
  fi
fi

if [[ ! -f "${CACHE_FILE}" ]]; then
  cat <<EOF >&2
[!] ${CACHE_FILE} not found.
    Please configure the project first, e.g.:
    cmake -S . -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Release
EOF
  exit 1
fi

get_cache_path() {
  local key="$1"
  local fallback="$2"
  local line
  line=$(grep -E "^${key}:PATH=" "${CACHE_FILE}" || true)
  if [[ -n "${line}" ]]; then
    echo "${line#*=}"
  else
    echo "${fallback}"
  fi
}

CONFIG_DIR=${CONFIG_DIR:-$(get_cache_path SENTINEL_CONFIG_INSTALL_DIR /etc/sentinelfs)}
SYSTEMD_DIR=${SYSTEMD_DIR:-$(get_cache_path SENTINEL_SYSTEMD_DIR /lib/systemd/system)}

if [[ -z "${DESTDIR}" && $EUID -ne 0 ]]; then
  echo "[!] Please run as root (or set DESTDIR for staged installs)." >&2
  exit 1
fi

echo "[i] Building SentinelFS targets (directory: ${BUILD_DIR})"
"${CMAKE_BIN}" --build "${BUILD_DIR}"

echo "[i] Installing to prefix ${PREFIX}"${DESTDIR:+" with DESTDIR=${DESTDIR}"}
if [[ -n "${DESTDIR}" ]]; then
  DESTDIR="${DESTDIR}" "${CMAKE_BIN}" --install "${BUILD_DIR}" --prefix "${PREFIX}"
else
  "${CMAKE_BIN}" --install "${BUILD_DIR}" --prefix "${PREFIX}"
fi

if [[ -z "${DESTDIR}" && -x "$(command -v systemctl 2>/dev/null || true)" ]]; then
  systemctl daemon-reload
  echo "[i] Systemd unit installed under ${SYSTEMD_DIR}. Enable with: systemctl enable --now ${SERVICE_NAME%.service}"
elif [[ -z "${DESTDIR}" ]]; then
  echo "[i] systemctl not found; skipping daemon-reload."
else
  echo "[i] Skipping systemctl daemon-reload (staged DESTDIR install)."
fi

echo "[✓] SentinelFS installed. Edit ${CONFIG_DIR}/${CONFIG_NAME} as needed before starting the daemon."
