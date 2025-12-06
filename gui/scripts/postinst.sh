#!/bin/bash
# SentinelFS Post-Installation Script
# This script runs after the DEB package is installed

set -e

# Create necessary directories
mkdir -p /var/log/sentinelfs
mkdir -p /var/lib/sentinelfs
mkdir -p /etc/sentinelfs

# Set permissions
chmod 755 /var/log/sentinelfs
chmod 755 /var/lib/sentinelfs
chmod 755 /etc/sentinelfs

# Copy default config if not exists
if [ ! -f /etc/sentinelfs/sentinel.conf ]; then
    if [ -f /opt/SentinelFS/resources/config/sentinel.conf ]; then
        cp /opt/SentinelFS/resources/config/sentinel.conf /etc/sentinelfs/sentinel.conf
    fi
fi

# Create symlinks for CLI tools
if [ -f /opt/SentinelFS/resources/bin/sentinel_cli ]; then
    ln -sf /opt/SentinelFS/resources/bin/sentinel_cli /usr/local/bin/sentinel-cli 2>/dev/null || true
fi

if [ -f /opt/SentinelFS/resources/bin/sentinel_daemon ]; then
    ln -sf /opt/SentinelFS/resources/bin/sentinel_daemon /usr/local/bin/sentinel-daemon 2>/dev/null || true
fi

# Update desktop database
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database /usr/share/applications 2>/dev/null || true
fi

# Update icon cache
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache /usr/share/icons/hicolor 2>/dev/null || true
fi

echo "SentinelFS installed successfully!"
echo "Run 'sentinelfs' to start the application."

exit 0
