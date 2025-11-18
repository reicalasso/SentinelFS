# SentinelFS-Neo Build Instructions

## Quick Start

```bash
# Create build directory
mkdir -p build
cd build

# Configure
cmake ..

# Build
cmake --build .

# Run test
./bin/sentinelfs-test
```

## Build Options

- `BUILD_TESTS` - Build test suite (default: ON)
- `BUILD_EXAMPLES` - Build example plugins (default: ON)

```bash
cmake -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=ON ..
```

## Build Types

```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## Platform-Specific Notes

### Linux
```bash
sudo apt-get install build-essential cmake
```

### macOS
```bash
brew install cmake
```

### Windows
Use Visual Studio 2019+ or MinGW with CMake support.

## Output Structure

```
build/
├── bin/
│   └── sentinelfs-test       # Test application
└── lib/
    ├── libsfs-core.a         # Core static library
    └── hello_plugin.so       # Example plugin
```

## Running

```bash
cd build
./bin/sentinelfs-test
```

The test application will:
1. Initialize Core components (Logger, Config, EventBus, PluginLoader)
2. Load the hello_plugin
3. Display plugin information
4. Unload the plugin
5. Exit

## Next Steps

After verifying the build:
- Sprint 2: FileAPI + Snapshot Engine
- Sprint 3: Filesystem Plugins (watcher)
- Sprint 4: Delta Engine
