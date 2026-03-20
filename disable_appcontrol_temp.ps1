# Temporarily disable Windows Defender Real-time Protection to test the new build
# Run as Administrator

Write-Host "Disabling Windows Defender Real-time Protection..." -ForegroundColor Yellow
Set-MpPreference -DisableRealtimeMonitoring $true

Write-Host "✓ Real-time Protection disabled" -ForegroundColor Green
Write-Host ""
Write-Host "You can now run the app:" -ForegroundColor Cyan
Write-Host "  .\build\Release\AuraCastPro.exe" -ForegroundColor White
Write-Host ""
Write-Host "When done testing, re-enable protection:" -ForegroundColor Yellow
Write-Host "  Set-MpPreference -DisableRealtimeMonitoring `$false" -ForegroundColor White
