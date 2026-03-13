# Real-time connection debugging
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "AuraCastPro Connection Debugger" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

Write-Host "This will monitor connections in real-time." -ForegroundColor Yellow
Write-Host "Try connecting from your iPhone now...`n" -ForegroundColor Yellow

# Check if AuraCastPro is running
$process = Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue
if ($process) {
    Write-Host "✓ AuraCastPro is running (PID: $($process.Id))" -ForegroundColor Green
} else {
    Write-Host "✗ AuraCastPro is NOT running!" -ForegroundColor Red
    Write-Host "  Start AuraCastPro first, then run this script again.`n" -ForegroundColor Yellow
    pause
    exit
}

# Monitor TCP connections on port 7236
Write-Host "`nMonitoring TCP port 7236 (AirPlay)..." -ForegroundColor Cyan
Write-Host "Press Ctrl+C to stop`n" -ForegroundColor Gray

$lastConnections = @()

while ($true) {
    $connections = netstat -ano | Select-String ":7236.*ESTABLISHED"
    
    if ($connections) {
        foreach ($conn in $connections) {
            $connStr = $conn.ToString().Trim()
            if ($lastConnections -notcontains $connStr) {
                # New connection!
                Write-Host "[$(Get-Date -Format 'HH:mm:ss')] NEW CONNECTION:" -ForegroundColor Green
                Write-Host "  $connStr" -ForegroundColor White
                
                # Extract remote IP
                if ($connStr -match '(\d+\.\d+\.\d+\.\d+):7236\s+(\d+\.\d+\.\d+\.\d+):(\d+)') {
                    $remoteIP = $matches[2]
                    $remotePort = $matches[3]
                    Write-Host "  Remote device: $remoteIP:$remotePort" -ForegroundColor Cyan
                }
                
                $lastConnections += $connStr
            }
        }
    }
    
    # Check for disconnections
    $currentConnStrs = $connections | ForEach-Object { $_.ToString().Trim() }
    foreach ($oldConn in $lastConnections) {
        if ($currentConnStrs -notcontains $oldConn) {
            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] DISCONNECTED:" -ForegroundColor Red
            Write-Host "  $oldConn" -ForegroundColor Gray
        }
    }
    $lastConnections = $currentConnStrs
    
    Start-Sleep -Milliseconds 500
}
