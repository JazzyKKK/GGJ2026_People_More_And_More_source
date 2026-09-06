@echo off
setlocal EnableExtensions
chcp 65001 >nul
title GGJ Unreal Project Cleaner

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0CleanProject.ps1" -ProjectRoot "%~dp0."
set "GGJ_CLEAN_RESULT=%ERRORLEVEL%"

echo.
if not "%GGJ_CLEAN_RESULT%"=="0" echo Cleanup did not finish. Resolve the error above and try again.
pause
exit /b %GGJ_CLEAN_RESULT%
