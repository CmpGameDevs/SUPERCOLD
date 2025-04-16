@echo off

REM Create build directory if it doesn't exist
if not exist build mkdir build

REM Enter build directory
cd build

REM Configure with CMake
echo Configuring project with CMake...
cmake ..

REM Build the project
echo Building project...
cmake --build . --config Release

echo Build completed successfully!
echo The executable is located in the bin directory. 