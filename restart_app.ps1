# Restart AuraCastPro with the new build
Write-Host "Stopping old AuraCastPro process..." -ForegroundColor Cyan

# Kill any running instances
Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

Write-Host "Starting new build..." -ForegroundColor Green
$exePath = "build\release_x64\Release\AuraCastPro.exe"

if (Test-Path $exePath) {
    Start-Process $exePath
    Start-Sleep -Seconds 3
    
    Write-Host ""
    Write-Host "Checking if port 7000 is listening..." -ForegroundColor Cyan
    $port7000 = netstat -ano | Select-String ":7000.*LISTENING"
    
    if ($port7000) {
        Write-Host "Port 7000 is LISTENING (correct!)" -ForegroundColor Green
        Write-Host ""
        Write-Host "The app is now using the standard iOS AirPlay port 7000." -ForegroundColor Green
        Write-Host "Try connecting from your iPad now:" -ForegroundColor Yellow
        Write-Host "  1. Open Control Center" -ForegroundColor White
        Write-Host "  2. Tap Screen Mirroring" -ForegroundColor White
        Write-Host "  3. Select 'AuraCastPro'" -ForegroundColor White
    } else {
        Write-Host "Port 7000 is NOT listening" -ForegroundColor Red
        Write-Host "The app may not have started correctly. Check output.log for errors." -ForegroundColor Yellow
    }
} else {
    Write-Host "Could not find $exePath" -ForegroundColor Red
    Write-Host "Please build the application first." -ForegroundColor Yellow
}
