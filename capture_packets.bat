@echo off
:: Capture UDP packets on port 7010 to diagnose AirPlay connection
:: Run as Administrator

echo.
echo ========================================
echo Packet Capture for AirPlay Debugging
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

echo Starting packet capture on UDP port 7010...
echo.
echo Instructions:
echo 1. Leave this window open
echo 2. Connect from your iPad
echo 3. Wait 10 seconds
echo 4. Press Ctrl+C to stop capture
echo 5. Check the output below
echo.
echo ========================================
echo.

:: Start packet monitor
pktmon filter add -p 7010
pktmon start --etw -p 0

echo.
echo Capture started! Waiting for packets...
echo Press Ctrl+C when done.
echo.

timeout /t 30 /nobreak

:: Stop capture
pktmon stop

echo.
echo ========================================
echo Capture stopped. Analyzing...
echo ========================================
echo.

:: Show results
pktmon counters

echo.
echo Cleaning up...
pktmon filter remove
pktmon reset

echo.
echo Done! Check the counters above.
echo If "Packets" is 0, the iPad is not sending UDP packets.
echo.
pause
