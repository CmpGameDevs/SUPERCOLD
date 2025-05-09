#!/bin/bash

# --- Configuration ---
# Define your build directory
BUILD_DIR="build"
# Define your CMake toolchain file path
VCPKG_TOOLCHAIN_FILE="${HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake"
# Define your CMake prefix path (adjust if needed)
CMAKE_PREFIX="/usr/include/"
BUILD_TYPE="Release"

# --- Functions ---

# Function to display usage information
usage() {
  echo "Usage: $0 [options]"
  echo "Options:"
  echo "  -b, --build    Build the project"
  echo "  -c, --clean    Clean the build directory"
  echo "  -h, --help     Display this help message"
  echo ""
  echo "Example: $0 --build"
  echo "Example: $0 --clean"
}

# Function to configure and build the project
build_project() {
  echo "Entering build directory: $BUILD_DIR"
  # Create build directory if it doesn't exist
  mkdir -p "$BUILD_DIR" || { echo "Error: Failed to create directory '$BUILD_DIR'"; exit 1; }

  # Enter build directory
  cd "$BUILD_DIR" || { echo "Error: Failed to enter directory '$BUILD_DIR'"; exit 1; }

  # Configure with CMake
  echo "Configuring project with CMake..."
  cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN_FILE" \
    -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX" \
    -DCMAKE_BUILD_TYPE=Release \
    || { echo "Error: CMake configuration failed"; return 1; }

  # Build the project
  echo "Building project..."
  make -j$(nproc) || { echo "Error: Build failed"; return 1; }

  echo "Build completed successfully!"
  echo "The executable is typically located in the bin directory relative to the build directory."

  # Return to the original directory
  cd - > /dev/null
  return 0 # Indicate success
}

# Function to clean the build directory
clean_project() {
  echo "Cleaning the build directory: $BUILD_DIR"
  if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR" || { echo "Error: Failed to remove directory '$BUILD_DIR'"; return 1; }
    echo "Clean complete."
  else
    echo "Build directory '$BUILD_DIR' does not exist. Nothing to clean."
  fi
  return 0 # Indicate success
}

# --- Main Script Logic ---

# Use set -e to exit immediately if a command exits with a non-zero status.
set -e

# Default action if no options are provided
if [ $# -eq 0 ]; then
  usage
  exit 1
fi

# Parse command line options
while [[ "$#" -gt 0 ]]; do
  case $1 in
    -b|--build)
      build_project
      ;;
    -c|--clean)
      clean_project
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Error: Unknown option '$1'"
      usage
      exit 1
      ;;
  esac
  shift
done

exit 0

