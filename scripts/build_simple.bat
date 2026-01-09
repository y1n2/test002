@echo off
echo freeDiameter Windows Build Script
echo ==================================

cmake --version
if %errorlevel% neq 0 (
    echo ERROR: CMake not found
    pause
    exit /b 1
)

if not exist build mkdir build
cd build

cmake .. -G "Visual Studio 16 2019" -A x64
if %errorlevel% neq 0 (
    cmake .. -G "Visual Studio 17 2022" -A x64
    if %errorlevel% neq 0 (
        echo ERROR: CMake configuration failed
        pause
        exit /b 1
    )
)

cmake --build . --config Release
if %errorlevel% neq 0 (
    cmake --build . --config Debug
    if %errorlevel% neq 0 (
        echo ERROR: Build failed
        pause
        exit /b 1
    )
    set BUILD_CONFIG=Debug
) else (
    set BUILD_CONFIG=Release
)

if exist "..\config\client.conf" copy "..\config\client.conf" "%BUILD_CONFIG%\"
if exist "..\certs" xcopy "..\certs" "%BUILD_CONFIG%\certs\" /E /I /Y

echo Build completed successfully!
echo Executable: build\%BUILD_CONFIG%\diameter_client.exe
pause