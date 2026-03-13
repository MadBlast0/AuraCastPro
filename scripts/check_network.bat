@echo off
echo ========================================
echo AuraCastPro Network Diagnostic Tool
echo ========================================
echo.

echo Checking network interfaces...
ipconfig | findstr /C:"IPv4"
echo.

echo Checking if port 5353 (mDNS) is available...
netstat -ano | findstr ":5353"
if %ERRORLEVEL% EQU 0 (
    echo WARNING: Port 5353 is already in use!
    echo This may be Bonjour, iTunes, or another service.
    echo Try closing other apps or restarting your computer.
) else (
    echo OK: Port 5353 is available.
)
echo.

echo Checking if port 7236 (AirPlay) is available...
netstat -ano | findstr ":7236"
if %ERRORLEVEL% EQU 0 (
    echo WARNING: Port 7236 is already in use!
) else (
    echo OK: Port 7236 is available.
)
echo.

echo Checking Windows Firewall status...
netsh advfirewall show allprofiles state
echo.

echo ========================================
echo Diagnostic complete!
echo ========================================
pause
