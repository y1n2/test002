@echo off
echo ==========================================
echo freeDiameter Windows Build Script
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

REM Display CMake version
for /f "tokens=3" %%i in ('cmake --version ^| findstr "cmake version"') do set CMAKE_VERSION=%%i
echo [INFO] CMake version: %CMAKE_VERSION%

REM Check Visual Studio
echo Checking Visual Studio...
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARNING] Visual Studio compiler not found
    echo Trying to setup Visual Studio environment...
    
    REM Try to find Visual Studio
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
        echo [ERROR] Visual Studio not found, please install Visual Studio 2019 or newer
        pause
        exit /b 1
    )
)

REM Create build directory
echo Creating build directory...
if not exist build mkdir build
cd build

REM Clean previous build
if exist CMakeCache.txt (
    echo Cleaning previous build...
    del CMakeCache.txt
)

REM Generate project files
echo Generating project files...
cmake .. -G "Visual Studio 16 2019" -A x64
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed, trying Visual Studio 2022...
    cmake .. -G "Visual Studio 17 2022" -A x64
    if %errorlevel% neq 0 (
        echo [ERROR] CMake configuration failed
        echo Please check:
        echo 1. Visual Studio is properly installed
        echo 2. CMake version compatibility
        echo 3. Source code completeness
        pause
        exit /b 1
    )
)

REM Build project
echo ==========================================
echo Starting build...
echo ==========================================
cmake --build . --config Release --verbose
if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    echo Trying debug mode build...
    cmake --build . --config Debug
    if %errorlevel% neq 0 (
        echo [ERROR] Debug mode build also failed
        pause
        exit /b 1
    )
    echo [INFO] Debug mode build successful
    set BUILD_CONFIG=Debug
) else (
    echo [INFO] Release mode build successful
    set BUILD_CONFIG=Release
)

REM Copy configuration files
echo Copying configuration files...
if exist "..\config\client.conf" (
    copy "..\config\client.conf" "%BUILD_CONFIG%\"
)
if exist "..\certs" (
    xcopy "..\certs" "%BUILD_CONFIG%\certs\" /E /I /Y
)

echo ==========================================
echo Build completed!
echo ==========================================
echo Executable location: build\%BUILD_CONFIG%\diameter_client.exe
echo Configuration file: build\%BUILD_CONFIG%\client.conf
echo Certificates directory: build\%BUILD_CONFIG%\certs\

echo.
echo Next steps:
echo 1. Ensure WSL Ubuntu server is running
echo 2. Run: diameter_client.exe --conf client.conf
echo.

pause