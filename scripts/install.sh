#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${BUILD_DIR:-build}
CMAKE_BIN=${CMAKE_BIN:-cmake}
PREFIX=${PREFIX:-/usr/local}
DESTDIR=${DESTDIR:-}
SERVICE_NAME=sentinel_daemon.service
CONFIG_NAME=sentinel.conf
CACHE_FILE="${BUILD_DIR}/CMakeCache.txt"

if ! command -v "${CMAKE_BIN}" >/dev/null 2>&1; then
  echo "[!] ${CMAKE_BIN} not found in PATH. Please install CMake 3.17+." >&2
  exit 1
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
