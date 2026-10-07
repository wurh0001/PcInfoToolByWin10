@echo off
chcp 65001 >nul
title PcInfoTool

"%~dp0..\..\build\Release\test_performance.exe" 20
pause
