@echo off
:: generate_fixtures.bat
:: Generates test H.264, H.265, and AV1 video fixture files for unit tests.
:: Requires ffmpeg.exe to be in PATH or the same folder as this script.
:: Run once before executing tests.

set OUTDIR=%~dp0fixtures
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

where ffmpeg >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: ffmpeg.exe not found in PATH.
    echo Download from https://ffmpeg.org/download.html
    pause & exit /b 1
)

echo Generating test fixtures in %OUTDIR%...

echo [1/6] H.264 1920x1080 30fps 5s...
ffmpeg -y -f lavfi -i "testsrc2=size=1920x1080:rate=30" -t 5 -c:v libx264 -profile:v baseline -level 4.0 -pix_fmt yuv420p "%OUTDIR%\test_1080p_h264.mp4" 2>nul

echo [2/6] H.264 1280x720 60fps 5s...
ffmpeg -y -f lavfi -i "testsrc2=size=1280x720:rate=60" -t 5 -c:v libx264 -profile:v high -pix_fmt yuv420p "%OUTDIR%\test_720p_h264_60fps.mp4" 2>nul

echo [3/6] H.265/HEVC 1920x1080 30fps 5s...
ffmpeg -y -f lavfi -i "testsrc2=size=1920x1080:rate=30" -t 5 -c:v libx265 -x265-params log-level=error -pix_fmt yuv420p "%OUTDIR%\test_1080p_h265.mp4" 2>nul

echo [4/6] H.265 portrait 1080x1920 30fps 3s...
ffmpeg -y -f lavfi -i "testsrc2=size=1080x1920:rate=30" -t 3 -c:v libx265 -x265-params log-level=error -pix_fmt yuv420p "%OUTDIR%\test_portrait_h265.mp4" 2>nul

echo [5/6] AV1 1920x1080 30fps 3s (slow — please wait)...
ffmpeg -y -f lavfi -i "testsrc2=size=1920x1080:rate=30" -t 3 -c:v libaom-av1 -cpu-used 8 -crf 30 -pix_fmt yuv420p "%OUTDIR%\test_1080p_av1.mp4" 2>nul

echo [6/6] Short corrupt/truncated file for error handling tests...
echo INVALID_DATA_FOR_TESTING > "%OUTDIR%\test_corrupt.mp4"

echo.
echo Fixtures generated:
dir "%OUTDIR%\*.mp4" /b
echo.
echo Done.
pause
