@echo off
echo ========================================
echo AuraCastPro Firewall Fix
echo ========================================
echo This script will add firewall rules for AuraCastPro.
echo You need to run this as Administrator.
echo.
pause

echo Adding firewall rules...

netsh advfirewall firewall add rule name="AuraCastPro - mDNS (UDP 5353)" dir=in action=allow protocol=UDP localport=5353
netsh advfirewall firewall add rule name="AuraCastPro - AirPlay Control (TCP 7236)" dir=in action=allow protocol=TCP localport=7236
netsh advfirewall firewall add rule name="AuraCastPro - AirPlay Data (UDP 7236)" dir=in action=allow protocol=UDP localport=7236
netsh advfirewall firewall add rule name="AuraCastPro - Google Cast (TCP 8009)" dir=in action=allow protocol=TCP localport=8009

echo.
echo Firewall rules added successfully!
echo.
echo ========================================
echo Done! Try running AuraCastPro again.
echo ========================================
pause
