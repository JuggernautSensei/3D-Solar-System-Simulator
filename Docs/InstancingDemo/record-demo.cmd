@echo off
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0record-demo.ps1"
echo.
echo Exit code: %ERRORLEVEL%
pause
