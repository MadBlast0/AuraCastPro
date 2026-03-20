# Kill app if running and rebuild

Write-Host "Stopping AuraCastPro if running..." -ForegroundColor Yellow
$process = Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue
if ($process) {
    Stop-Process -Name "AuraCastPro" -Force
    Start-Sleep -Seconds 2
    Write-Host "✓ App stopped" -ForegroundColor Green
} else {
    Write-Host "✓ App not running" -ForegroundColor Green
}

Write-Host ""
Write-Host "Building..." -ForegroundColor Yellow
cmake --build build --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "✓ Build successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "You can now run: build\Release\AuraCastPro.exe" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "✗ Build failed" -ForegroundColor Red
}
