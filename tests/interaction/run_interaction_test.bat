@echo off
pushd "%~dp0..\.."
start "" "build\Release\PcInfoTool-Pro.exe"
powershell -Command "Start-Sleep -Milliseconds 1500; powershell -ExecutionPolicy Bypass -File 'tests\interaction\test_interaction.ps1'; Stop-Process -Name 'PcInfoTool-Pro' -Force -ErrorAction SilentlyContinue"
popd
