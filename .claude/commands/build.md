# Build Project

Build the qNewsFlash project using CMake.

## Usage
```
/build [debug|release] [clean]
```

## Parameters
- `debug`: Build with debug symbols and verbose logging (default: release)
- `clean`: Clean build directory before building

## Execution

```bash
# Parse arguments
BUILD_TYPE="Release"
CLEAN=false

for arg in "$@"; do
  case "$arg" in
    debug) BUILD_TYPE="Debug" ;;
    release) BUILD_TYPE="Release" ;;
    clean) CLEAN=true ;;
  esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
  rm -rf build
fi

# Create and enter build directory
mkdir -p build
cd build

# Configure
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DSYSCONF_DIR=/etc/qNewsFlash \
      ..

# Build
make -j $(nproc)

echo "Build complete: $BUILD_TYPE mode"
```

## Notes
- Requires Qt5/Qt6 development packages installed
- Requires libxml2 development packages
- For Qt6, OpenSSL 3 is required
