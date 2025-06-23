# Use official Ubuntu LTS as base image
FROM ubuntu:22.04

# Set noninteractive install mode
ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    unzip \
    libssl-dev \
    pkg-config \
    protobuf-compiler \
    libprotobuf-dev \
    libprotoc-dev \
    libgrpc++-dev \
    libgrpc-dev \
    libabsl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy project source into container
COPY . .

# Replace default CMakeLists.txt with Ubuntu-compatible one
RUN cp configure/CMakeLists.txt CMakeLists.txt

# Create build directory and compile
RUN mkdir -p build && cd build && cmake .. && make -j$(nproc)

# Default command to run the app
CMD ["./run.sh"]
