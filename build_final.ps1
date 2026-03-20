# Final Build with All Encoder Fixes
Write-Host "=== Building AuraCastPro with Complete Encoder Setup ===" -ForegroundColor Cyan
Write-Host ""

Write-Host "Changes applied:" -ForegroundColor Yellow
Write-Host "  1. Added Video Codec (Decoder) dropdown with hardware labels" -ForegroundColor White
Write-Host "  2. Updated Video Encoder (Recording) with GPU/CPU labels" -ForegroundColor White
Write-Host "  3. Added Direct3D 12 renderer info" -ForegroundColor White
Write-Host "  4. Removed misleading 'Hardware Encoder' dropdown" -ForegroundColor White
Write-Host "  5. Changed default codec to H.264" -ForegroundColor White
Write-Host "  6. Added H.264 support to VideoDecoder" -ForegroundColor White
Write-Host ""

Write-Host "Building..." -ForegroundColor Yellow
cmake --build build --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "✅ Build successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "What's new in Settings:" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Video Settings (Mirroring):" -ForegroundColor Yellow
    Write-Host "  • Video Codec (Decoder)" -ForegroundColor White
    Write-Host "    - H.264 (Hardware - NVIDIA/AMD/Intel) ← Default" -ForegroundColor Gray
    Write-Host "    - H.265/HEVC (Hardware - NVIDIA/AMD/Intel)" -ForegroundColor Gray
    Write-Host "  • Renderer: Direct3D 12 (Hardware - GPU)" -ForegroundColor White
    Write-Host ""
    Write-Host "Recording Settings:" -ForegroundColor Yellow
    Write-Host "  • Video Encoder (Recording)" -ForegroundColor White
    Write-Host "    - NVIDIA NVENC H.264 (Hardware - GPU)" -ForegroundColor Gray
    Write-Host "    - NVIDIA NVENC H.265 (Hardware - GPU)" -ForegroundColor Gray
    Write-Host "    - AMD AMF H.264 (Hardware - GPU)" -ForegroundColor Gray
    Write-Host "    - Intel QSV H.264 (Hardware - GPU)" -ForegroundColor Gray
    Write-Host "    - x264 (Software - CPU)" -ForegroundColor Gray
    Write-Host "    - x265 (Software - CPU)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "To test:" -ForegroundColor Cyan
    Write-Host "  1. Stop any running instances" -ForegroundColor White
    Write-Host "  2. Run: .\build\Release\AuraCastPro.exe" -ForegroundColor White
    Write-Host "  3. Check Settings → Video → Video Codec" -ForegroundColor White
    Write-Host "  4. Check Settings → Recording → Video Encoder" -ForegroundColor White
    Write-Host "  5. Connect iPad → Video should display!" -ForegroundColor White
    Write-Host ""
} else {
    Write-Host ""
    Write-Host "❌ Build failed!" -ForegroundColor Red
    Write-Host "Check errors above" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Common issues:" -ForegroundColor Yellow
    Write-Host "  • QML syntax error - check SettingsPage.qml" -ForegroundColor Gray
    Write-Host "  • Missing semicolon in C++ code" -ForegroundColor Gray
    Write-Host "  • Typo in variable name" -ForegroundColor Gray
}
