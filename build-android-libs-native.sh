#!/bin/bash

# OpenCPN Android Libraries Build Script (Native Linux)
# This script compiles the .so files for Android directly on Linux without Docker

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_status "OpenCPN Android Libraries Build Script (Native Linux)"
print_status "====================================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    print_error "CMakeLists.txt not found. Please run this script from the OpenCPN root directory."
    exit 1
fi

# Check Android NDK availability
if [ -z "$ANDROID_NDK_ROOT" ] && [ -z "$ANDROID_NDK" ]; then
    print_error "Android NDK not found. Please set ANDROID_NDK_ROOT or ANDROID_NDK environment variable"
    print_error "Example: export ANDROID_NDK_ROOT=/path/to/android-ndk"
    print_error "Or: export ANDROID_NDK=/path/to/android-ndk"
    exit 1
fi

# Set NDK path
if [ -n "$ANDROID_NDK_ROOT" ]; then
    NDK_PATH="$ANDROID_NDK_ROOT"
elif [ -n "$ANDROID_NDK" ]; then
    NDK_PATH="$ANDROID_NDK"
fi

# Verify NDK path exists
if [ ! -d "$NDK_PATH" ]; then
    print_error "Android NDK path does not exist: $NDK_PATH"
    exit 1
fi

print_status "Using Android NDK: $NDK_PATH"

# Check for required NDK components
if [ ! -d "$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64" ]; then
    print_error "NDK toolchain not found at: $NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64"
    print_error "Please ensure you have a complete Android NDK installation"
    exit 1
fi

# Check wxlib directory
if [ ! -d "wxlib" ]; then
    print_error "wxlib directory not found. Please ensure OpenCPN source includes precompiled wxQt libraries."
    exit 1
fi

# Check required build tools
if ! command -v cmake &> /dev/null; then
    print_error "CMake is not installed or not in PATH"
    exit 1
fi

if ! command -v make &> /dev/null; then
    print_error "Make is not installed or not in PATH"
    exit 1
fi

print_status "Building OpenCPN Android libraries natively..."

# Step 1: Prepare build environment
print_status "Step 1: Preparing build environment..."

# Clean previous build
if [ -d "build_android" ]; then
    print_status "Cleaning previous build..."
    rm -rf build_android
fi

mkdir -p build_android
cd build_android

print_status "Build environment prepared successfully!"

# Step 2: Compile C++ libraries natively
print_status "Step 2: Compiling Android .so libraries..."

# Configure CMake for Android build
cmake -DOCPN_TARGET_TUPLE:STRING='Android-armhf;16;armhf' \
      -DCMAKE_TOOLCHAIN_FILE=../buildandroid/build_android.cmake \
      -Dtool_base="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64" \
      -DCMAKE_PREFIX_PATH=../wxlib \
      ..

if [ $? -ne 0 ]; then
    print_error "CMake configuration failed!"
    exit 1
fi

# Build the project
print_status "Building with $(nproc) parallel jobs..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    print_error "C++ compilation failed!"
    exit 1
fi

print_status "Android libraries compiled successfully!"

# Step 3: Organize and verify .so files
print_status "Step 3: Organizing .so files..."

# Create organized output directory
mkdir -p android_libs/armeabi-v7a

echo 'Copying main library...'
if [ -f 'libgorp.so' ]; then
    cp libgorp.so android_libs/armeabi-v7a/
    echo '✅ libgorp.so copied'
else
    echo '❌ libgorp.so not found'
    exit 1
fi

echo 'Copying plugin libraries...'
PLUGIN_COUNT=0
for plugin_dir in plugins/*/; do
    if [ -d "$plugin_dir" ]; then
        plugin_name=$(basename "$plugin_dir")
        echo "Processing plugin: $plugin_name"
        # Use nullglob to handle empty directories properly
        shopt -s nullglob
        for so_file in "$plugin_dir"/*.so; do
            if [ -f "$so_file" ]; then
                cp "$so_file" android_libs/armeabi-v7a/
                echo "✅ $(basename "$so_file") copied"
                ((PLUGIN_COUNT++))
            fi
        done
        shopt -u nullglob
    fi
done
echo "✅ Total plugin libraries copied: $PLUGIN_COUNT"

# Create library manifest
cat > android_libs/armeabi-v7a/MANIFEST.txt << EOF
OpenCPN Android Libraries Manifest
===================================

Build Information:
- Target Architecture: ARMv7 (armeabi-v7a)
- OpenCPN Version: 5.13.0
- Build Date: $(date)
- wxWidgets: 3.1 (precompiled)
- Qt5: 5.15 (Android)
- NDK: $(basename "$NDK_PATH")
- Build Environment: Native Linux

Libraries:
EOF

# Add library information to manifest
echo '' >> android_libs/armeabi-v7a/MANIFEST.txt
echo 'Main Library:' >> android_libs/armeabi-v7a/MANIFEST.txt
for so_file in android_libs/armeabi-v7a/libgorp.so; do
    if [ -f "$so_file" ]; then
        SIZE=$(stat -c%s "$so_file")
        echo "  $(basename "$so_file"): $SIZE bytes" >> android_libs/armeabi-v7a/MANIFEST.txt
    fi
done

echo '' >> android_libs/armeabi-v7a/MANIFEST.txt
echo 'Plugin Libraries:' >> android_libs/armeabi-v7a/MANIFEST.txt
for so_file in android_libs/armeabi-v7a/lib*.so; do
    if [ -f "$so_file" ] && [[ "$so_file" != *"libgorp.so" ]]; then
        SIZE=$(stat -c%s "$so_file")
        echo "  $(basename "$so_file"): $SIZE bytes" >> android_libs/armeabi-v7a/MANIFEST.txt
    fi
done

echo ''
echo '✅ Library manifest created'

# Step 4: Verify build results
print_status "Step 4: Verifying build results..."

# Check if libraries were created
if [ -d "android_libs" ]; then
    # Copy organized libraries to parent directory
    cp -r android_libs ..

    print_status "Android libraries build completed successfully!"
    print_status "Library location: ./android_libs/"

    # Display library information
    echo ""
    print_status "Generated Libraries:"
    echo "Architecture: ARMv7 (armeabi-v7a)"
    echo "Directory: ./android_libs/armeabi-v7a/"
    echo ""

    print_status "Library Files:"
    ls -lh android_libs/armeabi-v7a/*.so | awk '{print "  " $9 " (" $5 ")"}'

    echo ""
    print_status "Total Size:"
    du -sh android_libs/

    echo ""
    print_status "Manifest:"
    cat android_libs/armeabi-v7a/MANIFEST.txt | grep -A 20 "Libraries:"

else
    print_error "Library organization failed - no output directory found!"
    exit 1
fi

print_status ""
print_status "Build completed successfully!"
print_status ""
print_status "Next steps:"
print_status "1. Copy the ./android_libs/ directory to your Android project"
print_status "2. Place .so files in your project's jniLibs/armeabi-v7a/ directory"
print_status "3. Use your preferred build system to create the final APK"
print_status ""
print_status "Usage example:"
print_status "  cp -r android_libs/* /path/to/your/app/src/main/jniLibs/"
print_status ""
print_status "Libraries are ready for integration into your Android project!"
