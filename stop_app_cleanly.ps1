# Stop AuraCastPro cleanly (allows goodbye packets to be sent)
Write-Host "Stopping AuraCastPro cleanly..." -ForegroundColor Cyan

$process = Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue

if ($process) {
    Write-Host "Found AuraCastPro process (PID: $($process.Id))" -ForegroundColor Yellow
    Write-Host "Sending close signal (allows cleanup code to run)..." -ForegroundColor Cyan
    
    # Try graceful close first (sends WM_CLOSE message)
    $process.CloseMainWindow() | Out-Null
    
    # Wait up to 5 seconds for graceful shutdown
    $waited = 0
    while (!$process.HasExited -and $waited -lt 5000) {
        Start-Sleep -Milliseconds 500
        $waited += 500
        $process.Refresh()
    }
    
    if ($process.HasExited) {
        Write-Host "✓ App closed cleanly (goodbye packets sent)" -ForegroundColor Green
        Write-Host ""
        Write-Host "The service should disappear from your iPad within 10-30 seconds." -ForegroundColor White
    } else {
        Write-Host "⚠ App didn't close gracefully, force-killing..." -ForegroundColor Yellow
        $process.Kill()
        Write-Host "✓ App force-killed" -ForegroundColor Green
        Write-Host ""
        Write-Host "Note: Goodbye packets may not have been sent." -ForegroundColor Yellow
        Write-Host "The service may still appear on your iPad for 2-4 minutes." -ForegroundColor Yellow
        Write-Host ""
        Write-Host "To clear it immediately on iPad:" -ForegroundColor Cyan
        Write-Host "  Settings → Wi-Fi → Turn OFF → Wait 5s → Turn ON" -ForegroundColor White
    }
} else {
    Write-Host "✓ AuraCastPro is not running" -ForegroundColor Green
}
