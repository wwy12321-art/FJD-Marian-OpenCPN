#!/bin/bash

# OpenCPN Android Build Script
# This script builds the Docker environment and compiles OpenCPN for Android

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

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    print_error "CMakeLists.txt not found. Please run this script from the OpenCPN root directory."
    exit 1
fi

print_status "OpenCPN Android Build Script"
print_status "=============================="

# Check Docker availability
if ! command -v docker &> /dev/null; then
    print_error "Docker is not installed or not in PATH"
    exit 1
fi

print_status "Docker found: $(docker --version)"

# Build Docker image
print_status "Building Android Docker environment..."
docker build -f docker/Dockerfile.android -t opencpn-android-builder:latest .

if [ $? -ne 0 ]; then
    print_error "Docker build failed!"
    exit 1
fi

print_status "Docker image built successfully!"

# Create build script inside container
print_status "Starting Android build..."
docker run --rm -v "$(pwd):/src:Z" opencpn-android-builder:latest /build-android.sh

if [ $? -eq 0 ]; then
    print_status "Build completed successfully!"
    print_status "APK file should be available in: build_android/outputs/apk/"
else
    print_error "Build failed!"
    exit 1
fi