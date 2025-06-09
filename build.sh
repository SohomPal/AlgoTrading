#!/bin/bash

set -e  # Exit on first error

echo "Cleaning old build..."
rm -rf build

echo "Creating build directory..."
mkdir build
cd build

echo "Running CMake..."
cmake ..

echo "Building project..."
make

echo "Build complete."
