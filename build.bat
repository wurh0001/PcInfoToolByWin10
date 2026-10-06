@echo off
setlocal
cd /d "%~dp0"
set "VS_DIR=E:\dev-tools\vs-buildtools"
if exist "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" (
    call "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" x64
) else (
    echo [Error] vcvarsall.bat not found at %VS_DIR%
    exit /b 1
)

cmake -S . -B build/cmake -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo [Error] CMake configuration failed!
    exit /b %ERRORLEVEL%
)

cmake --build build/cmake --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [Error] Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ==========================================================
echo [Success] All targets built successfully:
echo   - Lite (Minimal):  build\Release\PcInfoTool-Lite.exe
echo   - Pro  (Stats):    build\Release\PcInfoTool-Pro.exe
echo   - Test Suite:      build\Release\test_performance.exe
echo ==========================================================
endlocal
