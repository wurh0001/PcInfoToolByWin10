@echo off
setlocal
set "VS_DIR=E:\dev-tools\vs-buildtools"
if exist "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" (
    call "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" x64
) else (
    echo [Error] vcvarsall.bat not found at %VS_DIR%
    exit /b 1
)

if not exist build mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo [Error] CMake configuration failed!
    exit /b %ERRORLEVEL%
)

cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [Error] Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ==========================================================
echo [Success] All targets built successfully:
echo   - Lite (Minimal):  build\Release\PcInfoTool-Lite.exe
echo   - Pro  (Stats):    build\Release\PcInfoTool-Pro.exe
echo   - Test Suite:      test\test_performance.exe
echo ==========================================================
cd ..
endlocal
