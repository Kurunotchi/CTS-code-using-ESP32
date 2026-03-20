@echo off
title Capacity Tester Dashboard Server
echo ========================================================
echo Starting Capacity Tester Web Dashboard...
echo ========================================================
cd /d "%~dp0"
if not exist node_modules (
    echo Installing required Node.js dependencies...
    cmd /c npm install express ws cors
)
echo.
node server.js
pause
