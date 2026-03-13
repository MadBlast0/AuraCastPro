@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1

echo === Test-compiling all shaders with dxc directly ===
set DXC="C:\Users\MadBlast\AppData\Local\Microsoft\WinGet\Packages\Microsoft.DirectX.ShaderCompiler_Microsoft.Winget.Source_8wekyb3d8bbwe\bin\x64\dxc.exe"
set SRC=assets\shaders
set OUT=build\shaders

if not exist "%OUT%" mkdir "%OUT%"

echo.
echo --- fullscreen_vs.hlsl [vs_6_0, main] ---
%DXC% -T vs_6_0 -E main "%SRC%\fullscreen_vs.hlsl" -Fo "%OUT%\fullscreen_vs.cso" 2>&1
echo Exit: %ERRORLEVEL%

echo.
echo --- nv12_to_rgb.hlsl [ps_6_0, PSMain] ---
%DXC% -T ps_6_0 -E PSMain "%SRC%\nv12_to_rgb.hlsl" -Fo "%OUT%\nv12_to_rgb.cso" 2>&1
echo Exit: %ERRORLEVEL%

echo.
echo --- hdr10_tonemap.hlsl [ps_6_0, PSMain] ---
%DXC% -T ps_6_0 -E PSMain "%SRC%\hdr10_tonemap.hlsl" -Fo "%OUT%\hdr10_tonemap.cso" 2>&1
echo Exit: %ERRORLEVEL%

echo.
echo --- lanczos8.hlsl [ps_6_0, PSMain] ---
%DXC% -T ps_6_0 -E PSMain "%SRC%\lanczos8.hlsl" -Fo "%OUT%\lanczos8.cso" 2>&1
echo Exit: %ERRORLEVEL%

echo.
echo --- chroma_upsample.hlsl [cs_6_0, CSMain] ---
%DXC% -T cs_6_0 -E CSMain "%SRC%\chroma_upsample.hlsl" -Fo "%OUT%\chroma_upsample.cso" 2>&1
echo Exit: %ERRORLEVEL%

echo.
echo --- temporal_frame_pacing.hlsl [cs_6_0, CSMain] ---
%DXC% -T cs_6_0 -E CSMain "%SRC%\temporal_frame_pacing.hlsl" -Fo "%OUT%\temporal_frame_pacing.cso" 2>&1
echo Exit: %ERRORLEVEL%

echo.
echo === Done ===
