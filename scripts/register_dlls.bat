@echo off
:: ===========================================================================
:: register_dlls.bat — Register AuraCastPro COM components and virtual camera
::
:: Called by the NSIS installer with elevated privileges (RunAs admin).
:: Registers MirrorCam.dll as a DirectShow / Media Foundation virtual camera
:: so it appears in OBS, Zoom, Teams and any other video capture application.
:: ===========================================================================

setlocal

:: Determine install directory (passed as %1, default to script directory)
if "%~1"=="" (
    set "INSTALL_DIR=%~dp0.."
) else (
    set "INSTALL_DIR=%~1"
)

echo AuraCastPro — Registering components...
echo Install dir: %INSTALL_DIR%

:: Register virtual camera DirectShow filter
echo Registering virtual camera (MirrorCam.dll)...
regsvr32 /s "%INSTALL_DIR%\MirrorCam.dll"
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: MirrorCam.dll registration failed (error %ERRORLEVEL%)
    echo The virtual camera may not appear in OBS/Zoom/Teams.
) else (
    echo   OK: MirrorCam.dll registered successfully.
)

:: Register with Windows Camera Devices
echo Registering with Windows Device Manager...
pnputil /add-driver "%INSTALL_DIR%\drivers\MirrorCam_Installer.inf" /install
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Driver registration failed. Virtual camera may require manual install.
)

:: Set registry entries for virtual camera name and resolution
reg add "HKLM\SOFTWARE\AuraCastPro\VirtualCamera" /v "FriendlyName" /t REG_SZ /d "AuraCastPro Virtual Camera" /f
reg add "HKLM\SOFTWARE\AuraCastPro\VirtualCamera" /v "MaxWidth"     /t REG_DWORD /d 3840 /f
reg add "HKLM\SOFTWARE\AuraCastPro\VirtualCamera" /v "MaxHeight"    /t REG_DWORD /d 2160 /f
reg add "HKLM\SOFTWARE\AuraCastPro\VirtualCamera" /v "MaxFPS"       /t REG_DWORD /d 120  /f

echo.
echo Registration complete.
exit /b 0
