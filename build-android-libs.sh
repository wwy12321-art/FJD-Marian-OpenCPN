#!/bin/bash

# OpenCPN Android Libraries Build Script
# This script only compiles the .so files for Android, without APK packaging

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

print_status "OpenCPN Android Libraries Build Script"
print_status "======================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    print_error "CMakeLists.txt not found. Please run this script from the OpenCPN root directory."
    exit 1
fi

# Check Docker availability
if ! command -v docker &> /dev/null; then
    print_error "Docker is not installed or not in PATH"
    exit 1
fi

# Check wxlib directory
if [ ! -d "wxlib" ]; then
    print_error "wxlib directory not found. Please ensure OpenCPN source includes precompiled wxQt libraries."
    exit 1
fi

print_status "Building OpenCPN Android libraries using Docker..."

# Step 1: Build Docker environment (if not already built)
print_status "Step 1: Building Android Docker environment..."
docker build --platform=linux/amd64 -f Dockerfile.android -t opencpn-android-builder:latest .

if [ $? -ne 0 ]; then
    print_error "Docker build failed!"
    exit 1
fi

print_status "Docker environment built successfully!"

# Step 2: Compile C++ libraries using Docker
print_status "Step 2: Compiling Android .so libraries..."
docker run --rm -v "$(pwd):/src:Z" opencpn-android-builder:latest bash -c "
    cd /src
    rm -rf build_android
    mkdir -p build_android
    cd build_android
    cmake -DOCPN_TARGET_TUPLE:STRING='Android-armhf;16;armhf' \
          -DCMAKE_TOOLCHAIN_FILE=../buildandroid/build_android.cmake \
          -Dtool_base=/opt/android-sdk/ndk/25.1.8937393/toolchains/llvm/prebuilt/linux-x86_64 \
          -DCMAKE_PREFIX_PATH=/src/wxlib \
          ..
    make -j\$(nproc)
"

if [ $? -ne 0 ]; then
    print_error "C++ compilation failed!"
    exit 1
fi

print_status "Android libraries compiled successfully!"

# Step 3: Organize and verify .so files
print_status "Step 3: Organizing .so files..."
docker run --rm -v "$(pwd):/src:Z" opencpn-android-builder:latest bash -c "
    cd /src/build_android

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
        if [ -d \"\$plugin_dir\" ]; then
            plugin_name=\$(basename \"\$plugin_dir\")
            for so_file in \"\$plugin_dir\"/*.so; do
                if [ -f \"\$so_file\" ]; then
                    cp \"\$so_file\" android_libs/armeabi-v7a/
                    echo \"✅ \$(basename \"\$so_file\") copied\"
                    ((PLUGIN_COUNT++))
                fi
            done
        fi
    done

    echo \"✅ Total plugin libraries copied: \$PLUGIN_COUNT\"

    # Create library manifest
    cat > android_libs/armeabi-v7a/MANIFEST.txt << 'EOF'
OpenCPN Android Libraries Manifest
===================================

Build Information:
- Target Architecture: ARMv7 (armeabi-v7a)
- OpenCPN Version: 5.13.0
- Build Date: $(date)
- wxWidgets: 3.1 (precompiled)
- Qt5: 5.15 (Android)
- NDK: 25.1.8937393

Libraries:
EOF

    # Add library information to manifest
    echo '' >> android_libs/armeabi-v7a/MANIFEST.txt
    echo 'Main Library:' >> android_libs/armeabi-v7a/MANIFEST.txt
    for so_file in android_libs/armeabi-v7a/libgorp.so; do
        if [ -f \"\$so_file\" ]; then
            SIZE=\$(stat -c%s \"\$so_file\")
            echo \"  \$(basename \"\$so_file\"): \$SIZE bytes\" >> android_libs/armeabi-v7a/MANIFEST.txt
        fi
    done

    echo '' >> android_libs/armeabi-v7a/MANIFEST.txt
    echo 'Plugin Libraries:' >> android_libs/armeabi-v7a/MANIFEST.txt
    for so_file in android_libs/armeabi-v7a/lib*.so; do
        if [ -f \"\$so_file\" ] && [[ \"\$so_file\" != *\"libgorp.so\" ]]; then
            SIZE=\$(stat -c%s \"\$so_file\")
            echo \"  \$(basename \"\$so_file\"): \$SIZE bytes\" >> android_libs/armeabi-v7a/MANIFEST.txt
        fi
    done

    echo ''
    echo '✅ Library manifest created'
"

if [ $? -ne 0 ]; then
    print_error "Library organization failed!"
    exit 1
fi

# Step 4: Verify build results
print_status "Step 4: Verifying build results..."

# Check if libraries were created
if [ -d "build_android/android_libs" ]; then
    # Copy organized libraries to current directory
    cp -r build_android/android_libs .

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