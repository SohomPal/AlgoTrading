#!/bin/bash

set -e  # Exit on first error

echo "Cleaning old build..."
rm -rf build

echo "Cleaning gRPC artifacts..."
rm -r grpc/*.cc
rm -r grpc/*.h
cd grpc
protoc -I=. --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` orderbook.proto
cd ..


echo "Creating build directory..."
mkdir build
cd build

echo "Running CMake..."
cmake ..

echo "Building project..."
make

echo "Build complete."
