#!/bin/bash

set -e  # Exit on first error

echo "Cleaning old build..."
rm -rf build

echo "Regnerating Protobuf files..."
cd grpc
rm *.cc
rm *.h
protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` orderbook.proto
cd ..

echo "Creating build directory..."
mkdir build
cd build

echo "Running CMake..."
cmake ..

echo "Building project..."
make

echo "Build complete."
