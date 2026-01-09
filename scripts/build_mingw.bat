@echo off
echo ==========================================
echo freeDiameter MinGW Build Script
echo ==========================================

REM Check CMake
echo Checking CMake...
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found, please install CMake first
    echo Download: https://cmake.org/download/
    pause
    exit /b 1
)

REM Check MinGW
echo Checking MinGW...
gcc --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] MinGW not found
    echo Please install MinGW-w64 or MSYS2
    echo MSYS2 Download: https://www.msys2.org/
    echo After installing MSYS2, run:
    echo   pacman -S mingw-w64-x86_64-gcc
    echo   pacman -S mingw-w64-x86_64-cmake
    echo   pacman -S make
    pause
    exit /b 1
)

REM Display versions
for /f "tokens=3" %%i in ('cmake --version ^| findstr "cmake version"') do set CMAKE_VERSION=%%i
for /f "tokens=3" %%i in ('gcc --version ^| findstr "gcc"') do set GCC_VERSION=%%i
echo [INFO] CMake version: %CMAKE_VERSION%
echo [INFO] GCC version: %GCC_VERSION%

REM Create build directory
echo Creating build directory...
if not exist build mkdir build
cd build

REM Clean previous build
if exist CMakeCache.txt (
    echo Cleaning previous build...
    del CMakeCache.txt
)

REM Generate Makefiles
echo Generating Makefiles...
cmake .. -G "MinGW Makefiles"
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    echo Please check:
    echo 1. MinGW is properly installed
    echo 2. CMake version compatibility
    echo 3. Source code completeness
    pause
    exit /b 1
)

REM Build project
echo ==========================================
echo Starting build...
echo ==========================================
mingw32-make
if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    echo Trying with verbose output...
    mingw32-make VERBOSE=1
    if %errorlevel% neq 0 (
        echo [ERROR] Build failed with verbose output
        pause
        exit /b 1
    )
)

REM Copy configuration files
echo Copying configuration files...
if exist "..\config\client.conf" (
    copy "..\config\client.conf" "."
)
if exist "..\certs" (
    xcopy "..\certs" "certs\" /E /I /Y
)

echo ==========================================
echo Build completed!
echo ==========================================
echo Executable location: build\diameter_client.exe
echo Configuration file: build\client.conf
echo Certificates directory: build\certs\

echo.
echo Next steps:
echo 1. Ensure WSL Ubuntu server is running
echo 2. Run: diameter_client.exe --conf client.conf
echo.

pause