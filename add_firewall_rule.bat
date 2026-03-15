@echo off
:: Add Windows Firewall rule for AuraCastPro UDP ports
:: Right-click this file and select "Run as administrator"

echo.
echo ========================================
echo AuraCastPro Firewall Rule Setup
echo ========================================
echo.

:: Check for admin rights
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator!
    echo.
    echo Right-click this file and select "Run as administrator"
    echo.
    pause
    exit /b 1
)

echo Running as Administrator...
echo.

set EXE_PATH=C:\Users\MadBlast\Documents\GitHub\AuraCastPro\build\Debug\AuraCastPro.exe

echo Executable: %EXE_PATH%
echo.

:: Remove existing rule if it exists
echo Removing any existing rule...
netsh advfirewall firewall delete rule name="AuraCastPro UDP Ports" >nul 2>&1

:: Add new rule for UDP ports 7000, 7010, 7011
echo Adding new firewall rule...
netsh advfirewall firewall add rule name="AuraCastPro UDP Ports" dir=in action=allow protocol=UDP localport=7000,7010,7011 program="%EXE_PATH%" enable=yes

if %errorLevel% equ 0 (
    echo.
    echo ========================================
    echo SUCCESS: Firewall rule added!
    echo ========================================
    echo.
    echo Allowed UDP ports:
    echo   - 7000 ^(timing/control^)
    echo   - 7010 ^(video stream^)
    echo   - 7011 ^(audio stream^)
    echo.
    echo You can now try connecting from your iPad!
) else (
    echo.
    echo ERROR: Failed to add firewall rule
    echo Error code: %errorLevel%
)

echo.
pause
