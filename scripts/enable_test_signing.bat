@echo off
:: enable_test_signing.bat — FOR DEVELOPMENT USE ONLY
:: Enables Windows Test Signing Mode to load unsigned .sys drivers during dev/test.
:: Requires reboot. Never ship to end users.
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Must run as Administrator.
    pause & exit /b 1
)
echo Enabling test signing mode...
bcdedit /set testsigning on
if %errorlevel% equ 0 (
    echo SUCCESS. Reboot required.
) else (
    echo FAILED ^(code %errorlevel%^). Ensure Secure Boot is disabled.
)
pause
