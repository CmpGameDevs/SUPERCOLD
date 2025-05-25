#!/bin/bash

# --- Configuration ---
# Define your build directory
BUILD_DIR="build"
# Define your CMake toolchain file path
VCPKG_TOOLCHAIN_FILE="${HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake"
BUILD_TYPE="Release"

# --- Functions ---

# Function to display usage information
usage() {
  echo "Usage: $0 [options]"
  echo "Options:"
  echo "  -b, --build    Configure and build the project"
  echo "  -r, --rebuild  Clean then configure and build the project"
  echo "  -c, --clean    Clean the build directory"
  echo "  -m, --make     Only run make (use after successful configure/build)"
  echo "  -h, --help     Display this help message"
  echo ""
  echo "Example: $0 --build"
  echo "Example: $0 --clean"
  echo "Example: $0 --rebuild"
}

# Function to configure the project
configure_project() {
  echo "Entering build directory: $BUILD_DIR"
  mkdir -p "$BUILD_DIR" || { echo "Error: Failed to create directory '$BUILD_DIR'"; exit 1; }

  cd "$BUILD_DIR" || { echo "Error: Failed to enter directory '$BUILD_DIR'"; exit 1; }

  echo "Configuring project with CMake..."
  cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    || { echo "Error: CMake configuration failed"; cd - > /dev/null; return 1; }

  cd - > /dev/null
  return 0
}

# Function to build the project (run make)
make_project() {
  if [ ! -d "$BUILD_DIR" ] || [ ! -f "$BUILD_DIR/Makefile" ]; then
    echo "Error: Project not configured. Run with -b or --rebuild first."
    return 1
  fi
  echo "Entering build directory: $BUILD_DIR"
  cd "$BUILD_DIR" || { echo "Error: Failed to enter directory '$BUILD_DIR'"; exit 1; }

  echo "Building project (running make)..."
  make -j$(nproc) || { echo "Error: Build failed"; cd - > /dev/null; return 1; }

  echo "Build completed successfully!"
  cd - > /dev/null
  return 0
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
  return 0
}

# --- Main Script Logic ---

# Use set -e to exit immediately if a command exits with a non-zero status.
set -e

# Default action if no options are provided
if [ $# -eq 0 ]; then
  usage
  exit 1
fi

ACTION_CONFIGURE=0
ACTION_MAKE=0
ACTION_CLEAN=0

# Parse command line options
while [[ "$#" -gt 0 ]]; do
  case $1 in
    -b|--build) # Configure and build
      ACTION_CONFIGURE=1
      ACTION_MAKE=1
      ;;
    -r|--rebuild) # Clean, then configure and build
      ACTION_CLEAN=1
      ACTION_CONFIGURE=1
      ACTION_MAKE=1
      ;;
    -c|--clean)
      ACTION_CLEAN=1
      ;;
    -m|--make) # Only make
      ACTION_MAKE=1
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

if [ "$ACTION_CLEAN" -eq 1 ]; then
  clean_project || exit 1
fi

if [ "$ACTION_CONFIGURE" -eq 1 ]; then
  configure_project || exit 1
fi

if [ "$ACTION_MAKE" -eq 1 ]; then
  make_project || exit 1
fi

if [ "$ACTION_CLEAN" -eq 0 ] && [ "$ACTION_CONFIGURE" -eq 0 ] && [ "$ACTION_MAKE" -eq 0 ]; then
    echo "No action specified (build, clean, make)."
    usage
fi

exit 0
