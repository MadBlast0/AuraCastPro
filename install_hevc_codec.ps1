# Install HEVC/H.265 Codec for AuraCastPro
Write-Host "=== HEVC Codec Installation ===" -ForegroundColor Cyan
Write-Host ""

Write-Host "Your system needs the HEVC/H.265 codec to display video from iPad." -ForegroundColor Yellow
Write-Host ""

Write-Host "Option 1: Free HEVC Codec (Recommended)" -ForegroundColor Green
Write-Host "  This will open Microsoft Store to install the free HEVC codec."
Write-Host "  Click 'Get' or 'Install' in the Store page."
Write-Host ""

$choice = Read-Host "Install HEVC codec now? (Y/N)"

if ($choice -eq "Y" -or $choice -eq "y") {
    Write-Host ""
    Write-Host "Opening Microsoft Store..." -ForegroundColor Yellow
    
    # Open Microsoft Store to HEVC Video Extensions (free version)
    Start-Process "ms-windows-store://pdp/?ProductId=9n4wgh0z6vhq"
    
    Write-Host ""
    Write-Host "Steps:" -ForegroundColor Cyan
    Write-Host "1. Click 'Get' or 'Install' in the Store" -ForegroundColor White
    Write-Host "2. Wait for installation to complete" -ForegroundColor White
    Write-Host "3. Close the Store" -ForegroundColor White
    Write-Host "4. Restart AuraCastPro" -ForegroundColor White
    Write-Host ""
    Write-Host "After installation, run:" -ForegroundColor Yellow
    Write-Host "  .\build\Release\AuraCastPro.exe" -ForegroundColor Gray
    Write-Host ""
} else {
    Write-Host ""
    Write-Host "Manual Installation:" -ForegroundColor Yellow
    Write-Host "1. Open Microsoft Store" -ForegroundColor White
    Write-Host "2. Search for 'HEVC Video Extensions'" -ForegroundColor White
    Write-Host "3. Install the FREE version (from device manufacturer)" -ForegroundColor White
    Write-Host "4. Restart AuraCastPro" -ForegroundColor White
    Write-Host ""
}

Write-Host "Alternative: Use H.264 instead" -ForegroundColor Cyan
Write-Host "If HEVC installation fails, you can configure iPad to use H.264:" -ForegroundColor Gray
Write-Host "  (This requires code changes in AuraCastPro)" -ForegroundColor Gray
