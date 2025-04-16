#!/bin/bash

# Exit on error
set -e

# Create build directory if it doesn't exist
mkdir -p build

# Enter build directory
cd build

# Configure with CMake
echo "Configuring project with CMake..."
cmake ..

# Build the project
echo "Building project..."
make -j$(nproc)

echo "Build completed successfully!"
echo "The executable is located in the bin directory." 