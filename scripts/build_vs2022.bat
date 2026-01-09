@echo off
echo ==========================================
echo freeDiameter Client Build Script for Visual Studio 2022
echo ==========================================

REM Check if CMake is installed
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: CMake is not installed or not in PATH
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

REM Check if Visual Studio 2022 is available
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo Setting up Visual Studio 2022 environment...
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
    if %errorlevel% neq 0 (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" 2>nul
        if %errorlevel% neq 0 (
            call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" 2>nul
            if %errorlevel% neq 0 (
                echo ERROR: Visual Studio 2022 not found
                echo Please install Visual Studio 2022 with C++ development tools
                pause
                exit /b 1
            )
        )
    )
)

echo CMake version:
cmake --version

echo Visual Studio 2022 detected
cl 2>&1 | findstr "Version"

echo ==========================================
echo Starting build process...
echo ==========================================

REM Create build directory
if not exist "build" mkdir build
cd build

REM Clean previous build
if exist "CMakeCache.txt" del CMakeCache.txt
if exist "CMakeFiles" rmdir /s /q CMakeFiles

echo Configuring project with CMake for Visual Studio 2022...
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    pause
    exit /b 1
)

echo Building project...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo ==========================================
echo Build completed successfully!
echo ==========================================

REM Copy configuration files
echo Copying configuration files...
if not exist "Release" mkdir Release
copy "..\config\*.conf" "Release\" >nul 2>&1
copy "..\certs\*.*" "Release\" >nul 2>&1

echo ==========================================
echo Build Summary:
echo ==========================================
echo Executable: build\Release\diameter_client.exe
echo Config files copied to: build\Release\
echo Certificates copied to: build\Release\

echo ==========================================
echo To run the client:
echo ==========================================
echo cd build\Release
echo diameter_client.exe -c client.conf

echo ==========================================
echo Build process completed!
echo ==========================================
pause