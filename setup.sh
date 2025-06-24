#!/bin/bash

set -e  # Exit on error

echo "🔧 Installing system dependencies..."
sudo apt update
sudo apt install -y build-essential autoconf libtool pkg-config \
    cmake git curl unzip libssl-dev

echo "✅ Dependencies installed."

# Step 2: Copy CMakeLists
echo "📄 Replacing CMakeLists.txt with Ubuntu-compatible version..."
cp configure/CMakeLists.txt ./CMakeLists.txt

# Step 3: Ask for API key and write config
echo "🔐 Please enter your sFOX API key:"
read -r API_KEY

echo "API_KEY=${API_KEY}" > config.cfg
echo "✅ Wrote API key to config.cfg"

# Step 4: Build and install gRPC from source in $HOME
cd "$HOME"

if [ -d "grpc" ]; then
    echo "📦 Removing existing grpc directory..."
    rm -rf grpc
fi

echo "🌐 Cloning gRPC repository..."
git clone --recurse-submodules -b v1.63.0 https://github.com/grpc/grpc
cd grpc

echo "🏗️ Building and installing gRPC and Protobuf..."
mkdir -p cmake/build
cd cmake/build
cmake ../.. -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install

echo "✅ gRPC installed successfully!"

# Done
echo "🎉 Setup complete. You can now run: mkdir build && cd build && cmake .. && make -j$(nproc)"
