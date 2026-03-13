@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1

cmake -B build -S . ^
    -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DCMAKE_PREFIX_PATH=C:/Qt/6.7.3/msvc2022_64 ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    2>&1

echo.
echo === CMake configure exit code: %ERRORLEVEL% ===
