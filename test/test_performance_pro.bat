@echo off
chcp 65001 >nul
title PcInfoTool-Pro 专业版性能监控与开销评测

"%~dp0test_performance.exe" --pro 20
pause
