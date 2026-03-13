@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1

cmake --build build --config Release --parallel 8 2>&1 | findstr /i "error warning C2 C3 C4 fatal LNK"

echo.
echo === Build done ===
