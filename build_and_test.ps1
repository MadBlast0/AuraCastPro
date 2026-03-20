# Build AuraCastPro with H.264 support
Write-Host "Building AuraCastPro..." -ForegroundColor Cyan

# Build
cmake --build build --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild successful!" -ForegroundColor Green
    Write-Host "`nChanges made:" -ForegroundColor Yellow
    Write-Host "1. Added H.264 support to VideoDecoder" -ForegroundColor White
    Write-Host "2. VideoDecoder now reads codec from settings" -ForegroundColor White
    Write-Host "3. Defaults to H.264 for better compatibility" -ForegroundColor White
    Write-Host ""
    Write-Host "Settings updated:" -ForegroundColor Yellow
    Write-Host "  video.preferredCodec = 'h264'" -ForegroundColor White
    Write-Host ""
    Write-Host "To test:" -ForegroundColor Yellow
    Write-Host "1. Kill any running instances" -ForegroundColor White
    Write-Host "2. Run: .\build\Release\AuraCastPro.exe" -ForegroundColor White
    Write-Host "3. Connect iPad" -ForegroundColor White
    Write-Host "4. Video should now display!" -ForegroundColor White
} else {
    Write-Host "`nBuild failed!" -ForegroundColor Red
    Write-Host "Check errors above" -ForegroundColor Yellow
}
