#!/bin/bash

# Build script for SentinelFS-Neo

echo "Building SentinelFS-Neo..."

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Run CMake
echo "Running CMake..."
cmake ..

# Build the project
echo "Building project..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "You can run the application with: ./sentinelfs-neo --help"
else
    echo "Build failed!"
    exit 1
fi