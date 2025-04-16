#!/bin/bash

# Exit on error
set -e

echo "Removing existing ImGui files..."
rm -rf vendor/imgui

echo "Creating ImGui directory..."
mkdir -p vendor/imgui
mkdir -p vendor/imgui/imgui_impl

echo "Downloading ImGui..."
git clone https://github.com/ocornut/imgui.git vendor/imgui_temp

echo "Copying ALL ImGui files..."
# Copy ALL files from the root directory
cp vendor/imgui_temp/*.h vendor/imgui/
cp vendor/imgui_temp/*.cpp vendor/imgui/
cp vendor/imgui_temp/misc/cpp/* vendor/imgui/ 2>/dev/null || true
cp vendor/imgui_temp/misc/freetype/* vendor/imgui/ 2>/dev/null || true

# Copy ALL backend implementations
cp vendor/imgui_temp/backends/imgui_impl_* vendor/imgui/imgui_impl/

# Copy STB files that ImGui needs
cp vendor/imgui_temp/imstb_*.h vendor/imgui/

# Copy imconfig.h from the root directory
cp vendor/imgui_temp/imconfig.h vendor/imgui/

echo "Cleaning up temporary files..."
rm -rf vendor/imgui_temp

echo "ImGui reinstallation complete!"
echo "You can now rebuild your project with: rm -rf build/* && ./scripts/build_linux.sh" 