#!/bin/bash

# TickShaper Linux Setup Script
# This script installs dependencies and builds TickShaper on Linux

set -e

echo "========================================="
echo "TickShaper Linux Setup Script"
echo "========================================="

# Detect Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
    VER=$VERSION_ID
else
    echo "Cannot detect Linux distribution"
    exit 1
fi

echo "Detected OS: $OS $VER"

# Function to install dependencies on Ubuntu/Debian
install_ubuntu_deps() {
    echo "Installing dependencies for Ubuntu/Debian..."
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        pkg-config \
        libzmq3-dev \
        libzmq5 \
        git \
        wget \
        curl \
        htop \
        valgrind \
        gdb
    
    # Install Google Test (optional)
    sudo apt-get install -y libgtest-dev
    
    echo "Ubuntu/Debian dependencies installed successfully!"
}

# Function to install dependencies on CentOS/RHEL/Fedora
install_redhat_deps() {
    echo "Installing dependencies for RedHat-based systems..."
    
    # Check if dnf or yum is available
    if command -v dnf &> /dev/null; then
        PKG_MANAGER="dnf"
    else
        PKG_MANAGER="yum"
    fi
    
    sudo $PKG_MANAGER groupinstall -y "Development Tools"
    sudo $PKG_MANAGER install -y \
        cmake \
        pkgconfig \
        zeromq-devel \
        zeromq \
        git \
        wget \
        curl \
        htop \
        valgrind \
        gdb
    
    # Install Google Test (optional)
    sudo $PKG_MANAGER install -y gtest-devel || echo "Google Test not available via package manager"
    
    echo "RedHat-based dependencies installed successfully!"
}

# Function to install dependencies on Arch Linux
install_arch_deps() {
    echo "Installing dependencies for Arch Linux..."
    sudo pacman -Syu --noconfirm
    sudo pacman -S --noconfirm \
        base-devel \
        cmake \
        pkgconf \
        zeromq \
        git \
        wget \
        curl \
        htop \
        valgrind \
        gdb \
        gtest
    
    echo "Arch Linux dependencies installed successfully!"
}

# Install dependencies based on distribution
case "$OS" in
    *"Ubuntu"*|*"Debian"*)
        install_ubuntu_deps
        ;;
    *"CentOS"*|*"Red Hat"*|*"Fedora"*)
        install_redhat_deps
        ;;
    *"Arch"*)
        install_arch_deps
        ;;
    *)
        echo "Unsupported Linux distribution: $OS"
        echo "Please install the following packages manually:"
        echo "- build-essential or equivalent"
        echo "- cmake"
        echo "- libzmq3-dev or zeromq-devel"
        echo "- pkg-config"
        exit 1
        ;;
esac

# Verify ZeroMQ installation
echo "Verifying ZeroMQ installation..."
if pkg-config --exists libzmq; then
    ZMQ_VERSION=$(pkg-config --modversion libzmq)
    echo "ZeroMQ version: $ZMQ_VERSION"
else
    echo "ERROR: ZeroMQ not found!"
    echo "Please install ZeroMQ development libraries manually."
    exit 1
fi

# Create necessary directories
echo "Creating project directories..."
mkdir -p build
mkdir -p data
mkdir -p logs

# Set permissions
chmod +x build.sh

echo "========================================="
echo "Setup completed successfully!"
echo "========================================="
echo ""
echo "Next steps:"
echo "1. Run: ./build.sh"
echo "2. Run: cd build && ./tickshaper ../config/tickshaper.conf"
echo ""
echo "For sample data generation:"
echo "1. cd data"
echo "2. ../build/create_sample"
echo ""