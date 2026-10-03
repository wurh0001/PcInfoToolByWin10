@echo off
start "" "build\Release\PcInfoTool-Pro.exe"
powershell -Command "Start-Sleep -Milliseconds 1500; powershell -ExecutionPolicy Bypass -File 'test\test_interaction.ps1'; Stop-Process -Name 'PcInfoTool-Pro' -Force -ErrorAction SilentlyContinue"
