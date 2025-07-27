#!/bin/bash

# TickShaper Run Script
# Convenient script to build and run TickShaper

set -e

echo "========================================="
echo "TickShaper Runner"
echo "========================================="

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Build directory not found. Running build first..."
    ./build.sh
fi

# Check if executable exists
if [ ! -f "build/tickshaper" ]; then
    echo "TickShaper executable not found. Building..."
    ./build.sh
fi

# Create sample data if it doesn't exist
if [ ! -f "data/sample.itch" ]; then
    echo "Creating sample ITCH data..."
    cd data
    if [ -f "../build/create_sample" ]; then
        ../build/create_sample --symbols ../config/symbols.txt --count 100000
    else
        echo "Sample data generator not found. Building..."
        cd ..
        ./build.sh
        cd data
        ../build/create_sample --symbols ../config/symbols.txt --count 100000
    fi
    cd ..
fi

# Check system resources
echo "System Information:"
echo "CPU cores: $(nproc)"
echo "Memory: $(free -h | grep '^Mem:' | awk '{print $2}')"
echo "Disk space: $(df -h . | tail -1 | awk '{print $4}') available"
echo ""

# Set optimal environment variables
export OMP_NUM_THREADS=$(nproc)
export ZMQ_IO_THREADS=4

# Run TickShaper
echo "Starting TickShaper..."
echo "Configuration: config/tickshaper.conf"
echo "Sample data: data/sample.itch"
echo ""
echo "Commands available during runtime:"
echo "  speed <multiplier>  - Set replay speed (0.1-10.0)"
echo "  throttle <rate>     - Set throttle rate (1-1000000)"
echo "  reset              - Reset counters"
echo "  metrics            - Show current metrics"
echo "  quit               - Exit application"
echo ""
echo "Press Ctrl+C to stop"
echo "========================================="

cd build
./tickshaper ../config/tickshaper.conf