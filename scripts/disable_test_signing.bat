@echo off
:: disable_test_signing.bat — Disables Windows Test Signing Mode.
:: Run this when you are done testing unsigned drivers.
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Must run as Administrator.
    pause & exit /b 1
)
echo Disabling test signing mode...
bcdedit /set testsigning off
if %errorlevel% equ 0 (
    echo SUCCESS. Reboot required to remove Test Mode watermark.
) else (
    echo FAILED ^(code %errorlevel%^).
)
pause
