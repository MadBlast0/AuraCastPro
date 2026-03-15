@echo off
echo Stopping AuraCastPro if running...
taskkill /F /IM AuraCastPro.exe 2>nul
timeout /t 2 /nobreak >nul

echo Building...
cmake --build build --config Release
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Starting AuraCastPro...
start "" "build\Release\AuraCastPro.exe"
