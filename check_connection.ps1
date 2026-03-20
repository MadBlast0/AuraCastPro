# Diagnostic script to check AirPlay connection issues

Write-Host "=== AirPlay Connection Diagnostics ===" -ForegroundColor Cyan
Write-Host ""

# 1. Check if app is running
Write-Host "1. Checking if AuraCastPro is running..." -ForegroundColor Yellow
$process = Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue
if ($process) {
    Write-Host "   ✓ AuraCastPro.exe is running (PID: $($process.Id))" -ForegroundColor Green
} else {
    Write-Host "   ✗ AuraCastPro.exe is NOT running" -ForegroundColor Red
    Write-Host "   → Start the application first!" -ForegroundColor Yellow
    exit
}

# 2. Check if port 7000 is listening
Write-Host ""
Write-Host "2. Checking if AirPlay port 7000 is listening..." -ForegroundColor Yellow
$port7000 = Get-NetTCPConnection -LocalPort 7000 -State Listen -ErrorAction SilentlyContinue
if ($port7000) {
    Write-Host "   ✓ Port 7000 is listening" -ForegroundColor Green
} else {
    Write-Host "   ✗ Port 7000 is NOT listening" -ForegroundColor Red
    Write-Host "   → AirPlay2Host may have failed to start" -ForegroundColor Yellow
}

# 3. Check firewall rules
Write-Host ""
Write-Host "3. Checking Windows Firewall rules..." -ForegroundColor Yellow
$fwRules = Get-NetFirewallRule -DisplayName "*AuraCastPro*" -ErrorAction SilentlyContinue
if ($fwRules) {
    Write-Host "   ✓ Firewall rules exist:" -ForegroundColor Green
    foreach ($rule in $fwRules) {
        Write-Host "     - $($rule.DisplayName) [$($rule.Enabled ? 'Enabled' : 'Disabled')]"
    }
} else {
    Write-Host "   ✗ No firewall rules found" -ForegroundColor Red
    Write-Host "   → Run add_firewall_rule.ps1 as Administrator" -ForegroundColor Yellow
}

# 4. Check mDNS service
Write-Host ""
Write-Host "4. Checking mDNS service..." -ForegroundColor Yellow
$mdnsService = Get-Service -Name "Bonjour Service" -ErrorAction SilentlyContinue
if ($mdnsService) {
    if ($mdnsService.Status -eq "Running") {
        Write-Host "   ✓ Bonjour Service is running" -ForegroundColor Green
    } else {
        Write-Host "   ✗ Bonjour Service is stopped" -ForegroundColor Red
        Write-Host "   → Start the service: Start-Service 'Bonjour Service'" -ForegroundColor Yellow
    }
} else {
    Write-Host "   ⚠ Bonjour Service not found (using built-in mDNS)" -ForegroundColor Yellow
}

# 5. Check network connectivity
Write-Host ""
Write-Host "5. Checking network interfaces..." -ForegroundColor Yellow
$adapters = Get-NetAdapter | Where-Object { $_.Status -eq "Up" }
foreach ($adapter in $adapters) {
    $ip = (Get-NetIPAddress -InterfaceIndex $adapter.ifIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue).IPAddress
    if ($ip) {
        Write-Host "   ✓ $($adapter.Name): $ip" -ForegroundColor Green
    }
}

# 6. Check if iPad is on same network
Write-Host ""
Write-Host "6. Network requirements:" -ForegroundColor Yellow
Write-Host "   - iPad and PC must be on the SAME Wi-Fi network" -ForegroundColor White
Write-Host "   - iPad must have Wi-Fi enabled (not cellular only)" -ForegroundColor White
Write-Host "   - Router must allow device-to-device communication" -ForegroundColor White

# 7. Check recent log file
Write-Host ""
Write-Host "7. Checking for recent errors in logs..." -ForegroundColor Yellow
$logPath = "$env:LOCALAPPDATA\AuraCastPro\logs"
if (Test-Path $logPath) {
    $latestLog = Get-ChildItem $logPath -Filter "*.log" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($latestLog) {
        Write-Host "   Latest log: $($latestLog.Name)" -ForegroundColor Cyan
        $errors = Select-String -Path $latestLog.FullName -Pattern "ERROR|CRITICAL|failed" -SimpleMatch | Select-Object -Last 5
        if ($errors) {
            Write-Host "   Recent errors:" -ForegroundColor Red
            foreach ($error in $errors) {
                Write-Host "     $($error.Line)" -ForegroundColor Red
            }
        } else {
            Write-Host "   ✓ No recent errors found" -ForegroundColor Green
        }
    }
} else {
    Write-Host "   ⚠ Log directory not found: $logPath" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Troubleshooting Steps ===" -ForegroundColor Cyan
Write-Host "1. Make sure iPad and PC are on the same Wi-Fi network"
Write-Host "2. On iPad: Settings → General → AirPlay & Handoff → Enable AirPlay"
Write-Host "3. On iPad: Control Center → Screen Mirroring → Look for 'AuraCastPro'"
Write-Host "4. If not visible, restart the app and check logs"
Write-Host "5. Check Settings → Mirroring tab → Try changing resolution to 1080p"
Write-Host ""
