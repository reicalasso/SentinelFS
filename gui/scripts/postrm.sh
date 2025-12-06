#!/bin/bash
# SentinelFS Post-Removal Script
# This script runs after the DEB package is removed

set -e

# Remove symlinks
rm -f /usr/local/bin/sentinel-cli 2>/dev/null || true
rm -f /usr/local/bin/sentinel-daemon 2>/dev/null || true

# Stop any running daemon processes
pkill -f sentinel_daemon 2>/dev/null || true

# Update desktop database
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database /usr/share/applications 2>/dev/null || true
fi

# Update icon cache
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache /usr/share/icons/hicolor 2>/dev/null || true
fi

# Note: We don't remove /var/log/sentinelfs, /var/lib/sentinelfs, or /etc/sentinelfs
# to preserve user data and logs. Use purge for complete removal.

echo "SentinelFS removed."

exit 0
