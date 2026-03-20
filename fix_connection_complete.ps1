# Complete Connection Fix for AuraCastPro
Write-Host "=== AuraCastPro Connection Fix ===" -ForegroundColor Cyan
Write-Host ""

# Step 1: Kill all instances
Write-Host "[1] Stopping all AuraCastPro instances..." -ForegroundColor Yellow
Stop-Process -Name "AuraCastPro" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
Write-Host "    Done" -ForegroundColor Green

# Step 2: Clear pairing data (force re-pair)
Write-Host "[2] Clearing AirPlay pairing data..." -ForegroundColor Yellow
$pairingPath = "$env:APPDATA\AuraCastPro\AuraCastPro\security"
if (Test-Path $pairingPath) {
    Remove-Item "$pairingPath\*" -Force -ErrorAction SilentlyContinue
    Write-Host "    Pairing data cleared" -ForegroundColor Green
} else {
    Write-Host "    No pairing data found" -ForegroundColor Gray
}

# Step 3: Clear logs for fresh start
Write-Host "[3] Clearing old logs..." -ForegroundColor Yellow
$logPath = "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log"
if (Test-Path $logPath) {
    Remove-Item $logPath -Force -ErrorAction SilentlyContinue
    Write-Host "    Logs cleared" -ForegroundColor Green
}

# Step 4: Start AuraCastPro
Write-Host "[4] Starting AuraCastPro..." -ForegroundColor Yellow
Start-Process -FilePath ".\build\Release\AuraCastPro.exe" -WorkingDirectory "."
Start-Sleep -Seconds 5
Write-Host "    App started" -ForegroundColor Green

# Step 5: Verify it's running
Write-Host "[5] Verifying app is running..." -ForegroundColor Yellow
$proc = Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue
if ($proc) {
    Write-Host "    App is running (PID: $($proc.Id))" -ForegroundColor Green
} else {
    Write-Host "    ERROR: App failed to start!" -ForegroundColor Red
    exit
}

# Step 6: Check if port is listening
Write-Host "[6] Checking if AirPlay port is listening..." -ForegroundColor Yellow
Start-Sleep -Seconds 2
$listening = netstat -an | Select-String "7100.*LISTENING"
if ($listening) {
    Write-Host "    Port 7100 is listening" -ForegroundColor Green
} else {
    Write-Host "    WARNING: Port 7100 not listening yet" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Setup Complete ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. On your iPad, open Control Center" -ForegroundColor White
Write-Host "2. Tap 'Screen Mirroring'" -ForegroundColor White
Write-Host "3. Select 'AuraCastPro' from the list" -ForegroundColor White
Write-Host "4. If prompted for a PIN, check the AuraCastPro window" -ForegroundColor White
Write-Host ""
Write-Host "Monitoring for connections..." -ForegroundColor Green
Write-Host "(Press Ctrl+C to stop monitoring)" -ForegroundColor Gray
Write-Host ""

# Monitor for connections
Get-Content "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" -Wait -Tail 0 | ForEach-Object {
    if ($_ -match "accept|client|connect|session|PIN|pair") {
        Write-Host $_ -ForegroundColor Cyan
    }
}
