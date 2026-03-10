@echo off
REM =============================================================================
REM compile_shaders.bat — Compiles all HLSL shaders to .cso bytecode
REM Run from the assets/shaders/ directory after installing Windows SDK.
REM dxc.exe is in: C:\Program Files (x86)\Windows Kits\10\bin\<version>\x64\
REM =============================================================================

set DXC="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\dxc.exe"
if not exist %DXC% (
    echo ERROR: dxc.exe not found at %DXC%
    echo Install Windows SDK 10.0.22621 or update the path above.
    exit /b 1
)

set OUT=..\..\build\shaders
if not exist %OUT% mkdir %OUT%

echo Compiling fullscreen_vs.hlsl...
%DXC% -T vs_6_0 -E main fullscreen_vs.hlsl -Fo %OUT%\fullscreen_vs.cso
if %errorlevel% neq 0 goto :error

echo Compiling nv12_to_rgb.hlsl...
%DXC% -T ps_6_0 -E main nv12_to_rgb.hlsl -Fo %OUT%\nv12_to_rgb.cso
if %errorlevel% neq 0 goto :error

echo Compiling hdr10_tonemap.hlsl...
%DXC% -T ps_6_0 -E main hdr10_tonemap.hlsl -Fo %OUT%\hdr10_tonemap.cso
if %errorlevel% neq 0 goto :error

echo Compiling lanczos8.hlsl...
%DXC% -T ps_6_0 -E main lanczos8.hlsl -Fo %OUT%\lanczos8.cso
if %errorlevel% neq 0 goto :error

echo Compiling chroma_upsample.hlsl...
%DXC% -T ps_6_0 -E main chroma_upsample.hlsl -Fo %OUT%\chroma_upsample.cso
if %errorlevel% neq 0 goto :error

echo Compiling temporal_frame_pacing.hlsl...
%DXC% -T ps_6_0 -E main temporal_frame_pacing.hlsl -Fo %OUT%\temporal_frame_pacing.cso
if %errorlevel% neq 0 goto :error

echo.
echo All shaders compiled successfully to %OUT%\
goto :eof

:error
echo.
echo COMPILATION FAILED (errorlevel=%errorlevel%)
exit /b %errorlevel%
