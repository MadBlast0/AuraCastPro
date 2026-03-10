@echo off
REM =============================================================================
REM generate_fixtures.bat — Generate test video fixture files using FFmpeg
REM Required by unit/integration tests (Tasks 202-213)
REM Run once from any developer machine with FFmpeg in PATH.
REM =============================================================================

where ffmpeg >nul 2>&1
if errorlevel 1 (
    echo ERROR: ffmpeg.exe not found in PATH.
    echo Download from https://ffmpeg.org/download.html and add to PATH.
    exit /b 1
)

set OUT=%~dp0
echo Generating test fixture videos in: %OUT%
echo This may take 1-2 minutes...
echo.

REM H.265 1080p60 (primary test fixture — used in most unit tests)
echo [1/5] Generating test_h265_1080p.h265 ...
ffmpeg -y -f lavfi -i testsrc=size=1920x1080:rate=60 -t 5 ^
    -c:v libx265 -preset fast -x265-params "keyint=60:bframes=0" ^
    "%OUT%test_h265_1080p.h265" 2>nul
if errorlevel 1 ( echo FAILED. & exit /b 1 )

REM H.265 4K30 (4K resolution test)
echo [2/5] Generating test_h265_4k.h265 ...
ffmpeg -y -f lavfi -i testsrc=size=3840x2160:rate=30 -t 5 ^
    -c:v libx265 -preset ultrafast ^
    "%OUT%test_h265_4k.h265" 2>nul
if errorlevel 1 ( echo FAILED. & exit /b 1 )

REM H.264 1080p60 (H.264 decoder test)
echo [3/5] Generating test_h264_1080p.h264 ...
ffmpeg -y -f lavfi -i testsrc=size=1920x1080:rate=60 -t 5 ^
    -c:v libx264 -preset fast -keyint_min 60 -g 60 -bf 0 ^
    "%OUT%test_h264_1080p.h264" 2>nul
if errorlevel 1 ( echo FAILED. & exit /b 1 )

REM AV1 1080p (AV1 decoder test — slow to encode)
echo [4/5] Generating test_av1_1080p.av1 ...
ffmpeg -y -f lavfi -i testsrc=size=1920x1080:rate=30 -t 3 ^
    -c:v libaom-av1 -cpu-used 8 -crf 30 ^
    "%OUT%test_av1_1080p.av1" 2>nul
if errorlevel 1 ( echo FAILED (AV1 encoder may not be available - skipping). )

REM Fragmented MP4 (for recording crash-recovery tests)
echo [5/5] Generating test_fragmented.mp4 ...
ffmpeg -y -f lavfi -i testsrc=size=1920x1080:rate=60 ^
    -f lavfi -i sine=frequency=440:sample_rate=48000 ^
    -t 10 -c:v libx264 -preset fast -c:a aac -b:a 192k ^
    -movflags "frag_keyframe+empty_moov+default_base_moof" ^
    "%OUT%test_fragmented.mp4" 2>nul
if errorlevel 1 ( echo FAILED. & exit /b 1 )

echo.
echo =============================================================
echo  All test fixtures generated successfully!
echo  Files written to: %OUT%
echo =============================================================
