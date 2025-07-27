#!/bin/bash

# TickShaper Build Script

set -e

echo "Building TickShaper..."

# Check if we're in the cpp directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Make sure you're in the cpp directory."
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Check for dependencies
echo "Checking dependencies..."

# Check for ZeroMQ
if ! pkg-config --exists libzmq; then
    echo "Error: libzmq not found. Please install ZeroMQ development libraries."
    echo "Ubuntu/Debian: sudo apt-get install libzmq3-dev"
    echo "CentOS/RHEL: sudo yum install zeromq-devel"
    echo "Fedora: sudo dnf install zeromq-devel"
    echo "Arch: sudo pacman -S zeromq"
    echo "macOS: brew install zmq"
    echo ""
    echo "Or run the setup script: ./setup.sh"
    exit 1
fi

echo "ZeroMQ found: $(pkg-config --modversion libzmq)"

# Configure with CMake
echo "Configuring build..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo "Compiling..."
make -j$(nproc)

echo "Build completed successfully!"
echo "Executable: build/tickshaper"
echo "Sample data generator: build/create_sample"
echo "Test client: build/test_client"
echo "Configuration: config/tickshaper.conf"

# Create data directory if it doesn't exist
mkdir -p ../data

# Build test client
echo "Building test client..."
g++ -std=c++17 -O2 -o test_client ../test_client.cpp -lzmq -ljsoncpp -pthread || echo "Warning: Could not build test client (jsoncpp may not be installed)"

echo ""
echo "To run TickShaper:"
echo "  ./run.sh"
echo ""
echo "Or manually:"
echo "  cd build && ./tickshaper ../config/tickshaper.conf"
echo ""
echo "To create sample data:"
echo "  cd data && ../build/create_sample --symbols ../config/symbols.txt --count 100000"
echo ""
echo "To test ZeroMQ output:"
echo "  ./build/test_client"