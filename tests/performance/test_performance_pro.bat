@echo off
chcp 65001 >nul
title PcInfoTool-Pro

"%~dp0..\..\build\Release\test_performance.exe" --pro 20
pause
