@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1

echo === Reconfiguring CMake ===
cmake -B build -S . ^
    -G "Visual Studio 18 2026" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DCMAKE_PREFIX_PATH=C:/Qt/6.7.3/msvc2022_64 ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    2>&1
if %ERRORLEVEL% NEQ 0 (
    echo === Configure FAILED with code %ERRORLEVEL% ===
    exit /b %ERRORLEVEL%
)

echo.
echo === Building Release ===
cmake --build build --config Release --parallel 8 2>&1

echo.
echo === Build exit code: %ERRORLEVEL% ===
