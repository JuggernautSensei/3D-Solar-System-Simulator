@echo off
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-run.ps1" -ProjectRoot "%~dp0..\.." -VcpkgRoot C:\vcpkg
echo.
echo Exit code: %ERRORLEVEL%
pause
