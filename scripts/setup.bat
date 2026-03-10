@echo off
REM =============================================================================
REM setup.bat — Developer environment setup script
REM Run this once after cloning to verify all prerequisites are installed.
REM =============================================================================

echo.
echo =========================================================
echo  AuraCastPro — Developer Setup Verification
echo =========================================================
echo.

set ERRORS=0

REM ── Check CMake ──────────────────────────────────────────────────────────────
cmake --version >nul 2>&1
if errorlevel 1 (
    echo [MISSING] CMake not found. Download from cmake.org/download
    set /a ERRORS+=1
) else (
    for /f "tokens=3" %%v in ('cmake --version 2^>^&1 ^| findstr /i "cmake version"') do (
        echo [  OK  ] CMake %%v
    )
)

REM ── Check Git ────────────────────────────────────────────────────────────────
git --version >nul 2>&1
if errorlevel 1 (
    echo [MISSING] Git not found. Download from git-scm.com
    set /a ERRORS+=1
) else (
    for /f "tokens=3" %%v in ('git --version') do echo [  OK  ] Git %%v
)

REM ── Check Python ─────────────────────────────────────────────────────────────
python --version >nul 2>&1
if errorlevel 1 (
    echo [MISSING] Python not found. Download from python.org
    set /a ERRORS+=1
) else (
    for /f "tokens=2" %%v in ('python --version') do echo [  OK  ] Python %%v
)

REM ── Check vcpkg ──────────────────────────────────────────────────────────────
if not defined VCPKG_ROOT (
    echo [MISSING] VCPKG_ROOT environment variable not set.
    echo          Run: git clone https://github.com/microsoft/vcpkg C:\vcpkg
    echo          Then: C:\vcpkg\bootstrap-vcpkg.bat
    echo          Then set VCPKG_ROOT=C:\vcpkg
    set /a ERRORS+=1
) else (
    echo [  OK  ] vcpkg at %VCPKG_ROOT%
)

REM ── Check Qt ─────────────────────────────────────────────────────────────────
if not defined QTDIR (
    echo [MISSING] QTDIR environment variable not set.
    echo          Install Qt 6.6+ from qt.io then set QTDIR=C:\Qt\6.6.0\msvc2022_64
    set /a ERRORS+=1
) else (
    echo [  OK  ] Qt at %QTDIR%
)

REM ── Check dxc.exe ────────────────────────────────────────────────────────────
dxc --version >nul 2>&1
if errorlevel 1 (
    echo [MISSING] dxc.exe not found in PATH.
    echo          Download DirectX Shader Compiler from github.com/microsoft/DirectXShaderCompiler
    set /a ERRORS+=1
) else (
    echo [  OK  ] DirectX Shader Compiler (dxc) found
)

REM ── Check adb.exe ────────────────────────────────────────────────────────────
adb version >nul 2>&1
if errorlevel 1 (
    echo [ WARN ] adb.exe not found. Android USB mirroring will not work.
    echo          Download from developer.android.com/tools/releases/platform-tools
) else (
    echo [  OK  ] Android Debug Bridge (adb) found
)

REM ── Check Windows SDK (D3D12 / WASAPI headers) ──────────────────────────────
if exist "C:\Program Files (x86)\Windows Kits\10\Include" (
    echo [  OK  ] Windows SDK found
) else (
    echo [MISSING] Windows 10 SDK not found.
    echo          Install via Visual Studio Installer: "Windows 10 SDK (10.0.22621.0)"
    set /a ERRORS+=1
)

REM ── Check OpenSSL ─────────────────────────────────────────────────────────────
if defined OPENSSL_ROOT_DIR (
    echo [  OK  ] OPENSSL_ROOT_DIR set to %OPENSSL_ROOT_DIR%
) else if exist "C:\OpenSSL-Win64\include\openssl\ssl.h" (
    echo [  OK  ] OpenSSL found at C:\OpenSSL-Win64
) else (
    echo [ WARN ] OpenSSL not found at C:\OpenSSL-Win64.
    echo          Download Win64 installer from slproweb.com/products/Win32OpenSSL.html
    echo          Or set OPENSSL_ROOT_DIR before running cmake.
)

REM ── Check NSIS ───────────────────────────────────────────────────────────────
makensis /VERSION >nul 2>&1
if errorlevel 1 (
    echo [ WARN ] NSIS not found. Cannot build installer.
    echo          Download from nsis.sourceforge.io
) else (
    echo [  OK  ] NSIS found
)

echo.
echo =========================================================
if %ERRORS% == 0 (
    echo  All required tools found. Ready to build!
    echo.
    echo  Run: cmake --preset windows-debug
    echo       cmake --build --preset build-debug
) else (
    echo  %ERRORS% required tool(s) missing. Install them and re-run this script.
)
echo =========================================================
echo.
