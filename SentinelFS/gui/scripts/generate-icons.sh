#!/bin/bash
# Generate PNG icons from SVG for electron-builder
# Requires: inkscape or rsvg-convert

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ICONS_DIR="$SCRIPT_DIR/../assets/icons"
SVG_FILE="$ICONS_DIR/icon.svg"

if [ ! -f "$SVG_FILE" ]; then
    echo "Error: icon.svg not found at $SVG_FILE"
    exit 1
fi

# Icon sizes needed for Linux
SIZES=(16 24 32 48 64 128 256 512 1024)

echo "Generating PNG icons from SVG..."

# Try rsvg-convert first (faster), fallback to inkscape
if command -v rsvg-convert &> /dev/null; then
    for size in "${SIZES[@]}"; do
        echo "  Creating ${size}x${size}.png"
        rsvg-convert -w "$size" -h "$size" "$SVG_FILE" -o "$ICONS_DIR/${size}x${size}.png"
    done
elif command -v inkscape &> /dev/null; then
    for size in "${SIZES[@]}"; do
        echo "  Creating ${size}x${size}.png"
        inkscape "$SVG_FILE" --export-type=png --export-filename="$ICONS_DIR/${size}x${size}.png" -w "$size" -h "$size" 2>/dev/null
    done
elif command -v convert &> /dev/null; then
    # ImageMagick fallback
    for size in "${SIZES[@]}"; do
        echo "  Creating ${size}x${size}.png"
        convert -background none -resize "${size}x${size}" "$SVG_FILE" "$ICONS_DIR/${size}x${size}.png"
    done
else
    echo "Warning: No SVG to PNG converter found (rsvg-convert, inkscape, or imagemagick)"
    echo "Icon PNGs will not be generated. Install one of these tools."
    exit 0
fi

# Create icon.png (256x256 for electron-builder)
if [ -f "$ICONS_DIR/256x256.png" ]; then
    cp "$ICONS_DIR/256x256.png" "$ICONS_DIR/icon.png"
fi

echo "Done! Icons generated in $ICONS_DIR"
ls -la "$ICONS_DIR"
