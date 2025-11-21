#!/usr/bin/env bash
set -euo pipefail

PREFIX=${PREFIX:-/usr/local}
CONFIG_DIR=${CONFIG_DIR:-/etc/sentinelfs}
SYSTEMD_DIR=${SYSTEMD_DIR:-/lib/systemd/system}
SERVICE_NAME=sentinel_daemon.service
CONFIG_NAME=sentinel.conf
BINARIES=(sentinel_daemon sentinel_cli)

if [[ $EUID -ne 0 ]]; then
  echo "[!] This installer must be run as root (sudo)." >&2
  exit 1
fi

install -d -m 0755 "${PREFIX}/bin"
install -d -m 0755 "${CONFIG_DIR}"
install -d -m 0755 "${SYSTEMD_DIR}"

for bin in "${BINARIES[@]}"; do
  if [[ -f "build/app/daemon/${bin}" ]]; then
    install -m 0755 "build/app/daemon/${bin}" "${PREFIX}/bin/${bin}"
  elif [[ -f "build/app/cli/${bin}" ]]; then
    install -m 0755 "build/app/cli/${bin}" "${PREFIX}/bin/${bin}"
  else
    echo "[!] Missing ${bin}, please run cmake && make first." >&2
    exit 1
  fi
fi

install -m 0640 config/${CONFIG_NAME} "${CONFIG_DIR}/${CONFIG_NAME}.example"
install -m 0644 docs/${SERVICE_NAME} "${SYSTEMD_DIR}/${SERVICE_NAME}"

if command -v systemctl >/dev/null 2>&1; then
  systemctl daemon-reload
  echo "[i] Systemd unit installed. Enable with: systemctl enable --now ${SERVICE_NAME%.service}"
else
  echo "[i] systemctl not found; skipping daemon-reload."
fi

echo "[✓] SentinelFS installed under ${PREFIX}. Copy ${CONFIG_DIR}/${CONFIG_NAME}.example to ${CONFIG_DIR}/${CONFIG_NAME} and edit before starting."
