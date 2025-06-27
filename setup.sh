#!/bin/bash

set -e  # Exit on error
cd "$HOME"

echo "🔧 Installing system dependencies..."
sudo apt update
sudo apt install -y build-essential autoconf libtool pkg-config \
    cmake git curl unzip libssl-dev

# Step 1: Install Protobuf 
echo "Installing Protobuf compiler..."
PB_REL="https://github.com/protocolbuffers/protobuf/releases"
curl -LO $PB_REL/download/v30.2/protoc-30.2-linux-x86_64.zip
unzip protoc-30.2-linux-x86_64.zip -d $HOME/.local
export PATH="/usr/local/bin:$PATH"
rm protoc-30.2-linux-x86_64.zip

echo "✅ Dependencies installed."

# Step 2: Build and install gRPC from source
if [ -d "grpc" ]; then
    echo "Removing existing grpc directory..."
    rm -rf grpc
fi

echo "Cloning gRPC repository..."
git clone --recurse-submodules -b v1.73.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
cd grpc

echo "Building and installing gRPC..."
mkdir -p cmake/build
cd cmake/build
cmake ../.. -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local
make -j$(nproc)
sudo make install

echo "✅ gRPC installed successfully!"

# Step 3: Copy CMakeLists
cd $HOME/AlgoTrader
echo "Replacing CMakeLists.txt with Ubuntu-compatible version..."
cp configure/CMakeLists.txt ./CMakeLists.txt

# Step 4: Ask for API key and write config
echo "🔐 Please enter your sFOX API key:"
read -r API_KEY

echo "API_KEY=${API_KEY}" > config.cfg
echo "✅ Wrote API key to config.cfg"

# Done
echo "🎉 Setup complete."
