# Watch for iPad connection attempts
Write-Host "Watching for iPad connections..." -ForegroundColor Green
Write-Host "Try connecting your iPad now!" -ForegroundColor Yellow
Write-Host ""

# Monitor the log file for new entries
$logPath = "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log"
Get-Content $logPath -Wait -Tail 0 | ForEach-Object {
    if ($_ -match "accept|client|connect|session|TCP") {
        Write-Host $_ -ForegroundColor Cyan
    }
}
