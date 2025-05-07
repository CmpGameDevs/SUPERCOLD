# SUPERCOLD - Installation Guide

This document provides detailed instructions for setting up and running the SUPERCOLD game on your system. Follow these steps carefully to ensure proper installation of all dependencies and configuration of the development environment.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Setting Up Dependencies](#setting-up-dependencies)
  - [Required Tools](#required-tools)
  - [Installing vcpkg](#installing-vcpkg)
  - [Installing OpenAL and Other Dependencies](#installing-openal-and-other-dependencies)
- [Environment Variables](#environment-variables)
- [Building the Project](#building-the-project)
  - [Configuring with CMake](#configuring-with-cmake)
  - [Building with Visual Studio](#building-with-visual-studio)
  - [Building from Command Line](#building-from-command-line)
- [Running the Game](#running-the-game)
- [Troubleshooting](#troubleshooting)

## Prerequisites

Ensure your system meets these minimum requirements:
- Windows 10 or later (64-bit)
- 8GB RAM (16GB recommended)
- Graphics card with OpenGL 4.3+ support
- 2GB of free disk space
- Visual Studio 2019 or 2022 with "Desktop development with C++" workload

## Setting Up Dependencies

### Required Tools

1. **Visual Studio**
   - Download and install [Visual Studio](https://visualstudio.microsoft.com/downloads/)
   - During installation, select the "Desktop development with C++" workload
   - Also select "C++ CMake tools for Windows" component

2. **Git**
   - Download and install [Git](https://git-scm.com/downloads)
   - Ensure Git is added to your PATH during installation

3. **CMake**
   - Download and install [CMake](https://cmake.org/download/) (version 3.20 or higher)
   - Ensure to select "Add CMake to the system PATH" during installation

### Installing vcpkg

[vcpkg](https://github.com/microsoft/vcpkg) is a C++ package manager that we'll use to install OpenAL and other dependencies.

1. **Clone vcpkg repository**
   ```powershell
   cd C:\
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   ```

2. **Run the bootstrap script**
   ```powershell
   .\bootstrap-vcpkg.bat
   ```

3. **Integrate vcpkg with Visual Studio**
   ```powershell
   .\vcpkg integrate install
   ```

Installing OpenAL and Other Dependencies

1. **Install OpenAL and other dependencies**
   ```powershell
   .\vcpkg install openal-soft:x64-windows
   ```

### Environment Variables
Setting up environment variables ensures that the build system can find all dependencies.

1. **Set VCPKG_ROOT environment variable**
   - Open the Start menu and search for "Environment Variables"
   - Click "Edit the system environment variables"
   - In the System Properties window, click "Environment Variables..."
   - Under "System variables", click "New..."
     - Set Variable name: `VCPKG_ROOT`
     - Set Variable value: `C:\vcpkg` (or your vcpkg installation path)
   - Click "OK" to save

2. **Add vcpkg to PATH**
   - In the Environment Variables window, find the "Path" variable under System variables
   - Click "Edit..."
   - Click "New" and add `C:\vcpkg` (or your vcpkg installation path)
   - Click "OK" to save all changes

3. **Set SUPERCOLD_ASSETS environment variable (optional)**
   - This variable is used to specify the location of game assets
   - If your assets are in a specific location, set `SUPERCOLD_ASSETS` to point to that directory
   - This is useful for development when assets may be in a different location than the executable

### Building the Project
Configuring with CMake

1. **Clone the SUPERCOLD repository**
   ```powershell
   git clone https://github.com/yourusername/SUPERCOLD.git
   cd SUPERCOLD
   ```

2. **Create a build directory**
   ```powershell
   mkdir build
   cd build
   ```

3. **Run CMake to configure the project**
   ```powershell
   cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```