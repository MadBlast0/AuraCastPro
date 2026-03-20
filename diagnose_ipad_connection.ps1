# Diagnose iPad Connection Issues
Write-Host "=== AuraCastPro Connection Diagnostic ===" -ForegroundColor Cyan
Write-Host ""

# 1. Check if app is running
Write-Host "[1] Checking if AuraCastPro is running..." -ForegroundColor Yellow
$process = Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue
if ($process) {
    Write-Host "    ✓ AuraCastPro is running (PID: $($process.Id))" -ForegroundColor Green
} else {
    Write-Host "    ✗ AuraCastPro is NOT running!" -ForegroundColor Red
    Write-Host "    Run: .\build\Release\AuraCastPro.exe" -ForegroundColor Yellow
    exit
}

# 2. Check if port 7236 is listening
Write-Host "[2] Checking if AirPlay port 7236 is listening..." -ForegroundColor Yellow
$port = Get-NetTCPConnection -LocalPort 7236 -State Listen -ErrorAction SilentlyContinue
if ($port) {
    Write-Host "    ✓ Port 7236 is listening" -ForegroundColor Green
} else {
    Write-Host "    ✗ Port 7236 is NOT listening!" -ForegroundColor Red
}

# 3. Check firewall rules
Write-Host "[3] Checking firewall rules..." -ForegroundColor Yellow
$rules = Get-NetFirewallRule -DisplayName "*AuraCast*" -Enabled True -ErrorAction SilentlyContinue
if ($rules) {
    Write-Host "    ✓ Firewall rules exist ($($rules.Count) rules)" -ForegroundColor Green
} else {
    Write-Host "    ✗ No firewall rules found!" -ForegroundColor Red
    Write-Host "    Run: .\add_firewall_rule.ps1" -ForegroundColor Yellow
}

# 4. Check network interface
Write-Host "[4] Checking network interface..." -ForegroundColor Yellow
$ip = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object {$_.IPAddress -like "192.168.*" -or $_.IPAddress -like "10.*"} | Select-Object -First 1).IPAddress
if ($ip) {
    Write-Host "    ✓ Local IP: $ip" -ForegroundColor Green
} else {
    Write-Host "    ✗ No local network IP found!" -ForegroundColor Red
}

# 5. Check recent logs
Write-Host "[5] Checking recent logs..." -ForegroundColor Yellow
$recentLogs = Get-Content "output.log" -Tail 10 -ErrorAction SilentlyContinue
if ($recentLogs -match "Broadcasting") {
    Write-Host "    ✓ App is broadcasting on network" -ForegroundColor Green
} else {
    Write-Host "    ? Cannot verify broadcasting status" -ForegroundColor Yellow
}

# 6. Check for connection attempts
Write-Host "[6] Checking for recent connection attempts..." -ForegroundColor Yellow
$connections = Get-Content "output.log" -Tail 100 -ErrorAction SilentlyContinue | Select-String -Pattern "session|client connected|TCP.*accept"
if ($connections) {
    Write-Host "    Found connection attempts:" -ForegroundColor Green
    $connections | ForEach-Object { Write-Host "      $_" -ForegroundColor Gray }
} else {
    Write-Host "    No connection attempts found in recent logs" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Diagnosis Complete ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Make sure iPad and PC are on the same WiFi network" -ForegroundColor White
Write-Host "2. On iPad: Control Center → Screen Mirroring" -ForegroundColor White
Write-Host "3. Look for 'AuraCastPro' in the list" -ForegroundColor White
Write-Host "4. If you don't see it, try:" -ForegroundColor White
Write-Host "   - Restart AuraCastPro" -ForegroundColor Gray
Write-Host "   - Restart iPad WiFi (toggle off/on)" -ForegroundColor Gray
Write-Host "   - Check Windows Firewall settings" -ForegroundColor Gray
Write-Host ""
Write-Host "To monitor connection attempts in real-time:" -ForegroundColor Yellow
Write-Host "  Get-Content output.log -Wait -Tail 20" -ForegroundColor Gray
