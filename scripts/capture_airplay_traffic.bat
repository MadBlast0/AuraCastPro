@echo off
echo ========================================
echo AirPlay Traffic Capture
echo ========================================
echo This will capture network traffic on port 7236
echo to help diagnose connection issues.
echo.
echo Press Ctrl+C to stop capturing.
echo.
pause

echo Starting capture...
echo Packets will be saved to airplay_capture.txt
echo.

netsh trace start capture=yes tracefile=airplay_trace.etl maxsize=100 filemode=circular overwrite=yes

echo.
echo Capture started! Now try connecting from your iPhone.
echo Press any key to stop capture...
pause > nul

netsh trace stop

echo.
echo Capture saved to airplay_trace.etl
echo Convert with: netsh trace convert airplay_trace.etl
echo.
pause
