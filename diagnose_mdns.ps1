# AuraCastPro mDNS Diagnostic Script
Write-Host "=== AuraCastPro mDNS Diagnostics ===" -ForegroundColor Cyan
Write-Host ""

# Check if app is running
Write-Host "[1] Checking if AuraCastPro is running..." -ForegroundColor Yellow
$process = Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue
if ($process) {
    Write-Host "    ✅ AuraCastPro is running (PID: $($process.Id))" -ForegroundColor Green
} else {
    Write-Host "    ❌ AuraCastPro is NOT running!" -ForegroundColor Red
    Write-Host "    Please start the app first." -ForegroundColor Red
    exit
}

# Check ports
Write-Host ""
Write-Host "[2] Checking if ports are listening..." -ForegroundColor Yellow
$port5000 = netstat -an | Select-String "0.0.0.0:5000.*LISTENING"
$port7000 = netstat -an | Select-String "0.0.0.0:7000.*LISTENING"
$port5353 = netstat -an | Select-String ":5353"

if ($port5000) {
    Write-Host "    ✅ Port 5000 (RAOP) is listening" -ForegroundColor Green
} else {
    Write-Host "    ❌ Port 5000 (RAOP) is NOT listening!" -ForegroundColor Red
}

if ($port7000) {
    Write-Host "    ✅ Port 7000 (AirPlay) is listening" -ForegroundColor Green
} else {
    Write-Host "    ❌ Port 7000 (AirPlay) is NOT listening!" -ForegroundColor Red
}

if ($port5353) {
    Write-Host "    ✅ Port 5353 (mDNS) is active" -ForegroundColor Green
} else {
    Write-Host "    ⚠️  Port 5353 (mDNS) may not be active" -ForegroundColor Yellow
}

# Check network
Write-Host ""
Write-Host "[3] Checking network configuration..." -ForegroundColor Yellow
$ip = Get-NetIPAddress -AddressFamily IPv4 | Where-Object {$_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.*"} | Select-Object -First 1
if ($ip) {
    Write-Host "    ✅ PC IP Address: $($ip.IPAddress)" -ForegroundColor Green
    Write-Host "    ✅ Interface: $($ip.InterfaceAlias)" -ForegroundColor Green
    
    if ($ip.IPAddress -match "^192\.168\.1\.") {
        Write-Host "    ✅ On 192.168.1.x network" -ForegroundColor Green
        Write-Host "    📱 Make sure your iPad is also on 192.168.1.x" -ForegroundColor Cyan
    } else {
        Write-Host "    ⚠️  Not on 192.168.1.x network" -ForegroundColor Yellow
        Write-Host "    📱 Make sure your iPad is on the same network: $($ip.IPAddress -replace '\.\d+$', '.x')" -ForegroundColor Cyan
    }
} else {
    Write-Host "    ❌ No network connection found!" -ForegroundColor Red
}

# Check Bonjour
Write-Host ""
Write-Host "[4] Checking Bonjour service..." -ForegroundColor Yellow
$bonjour = Get-Service -Name "Bonjour*" -ErrorAction SilentlyContinue
if ($bonjour -and $bonjour.Status -eq "Running") {
    Write-Host "    ✅ Bonjour Service is running" -ForegroundColor Green
} else {
    Write-Host "    ⚠️  Bonjour Service not found or not running" -ForegroundColor Yellow
    Write-Host "    This is OK - AuraCastPro has built-in mDNS" -ForegroundColor Cyan
}

# Check firewall
Write-Host ""
Write-Host "[5] Checking Windows Firewall..." -ForegroundColor Yellow
$fwRules = Get-NetFirewallRule -DisplayName "*AuraCast*" -ErrorAction SilentlyContinue
if ($fwRules) {
    $enabledCount = ($fwRules | Where-Object {$_.Enabled -eq $true}).Count
    Write-Host "    ✅ Found $($fwRules.Count) firewall rules ($enabledCount enabled)" -ForegroundColor Green
} else {
    Write-Host "    ⚠️  No firewall rules found for AuraCastPro" -ForegroundColor Yellow
}

# Check firewall profiles
$profiles = Get-NetFirewallProfile
$allEnabled = ($profiles | Where-Object {$_.Enabled -eq $true}).Count -eq 3
if ($allEnabled) {
    Write-Host "    ✅ All firewall profiles are enabled" -ForegroundColor Green
} else {
    Write-Host "    ⚠️  Some firewall profiles are disabled" -ForegroundColor Yellow
}

# Check logs
Write-Host ""
Write-Host "[6] Checking recent logs..." -ForegroundColor Yellow
if (Test-Path "build\Release\output.log") {
    $logs = Get-Content "build\Release\output.log" -Tail 5
    $hasMdns = $logs | Select-String "mDNS|Broadcasting"
    if ($hasMdns) {
        Write-Host "    ✅ mDNS broadcasting detected in logs" -ForegroundColor Green
        Write-Host "    Last log entries:" -ForegroundColor Cyan
        $logs | ForEach-Object { Write-Host "      $_" -ForegroundColor Gray }
    } else {
        Write-Host "    ⚠️  No mDNS activity in recent logs" -ForegroundColor Yellow
    }
} else {
    Write-Host "    ⚠️  No log file found" -ForegroundColor Yellow
}

# Summary
Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "If everything above shows ✅, then:" -ForegroundColor White
Write-Host "1. Make sure your iPad is on the same Wi-Fi network" -ForegroundColor White
Write-Host "2. Wait 30 seconds for mDNS to propagate" -ForegroundColor White
Write-Host "3. On iPad: Toggle Wi-Fi off/on to refresh" -ForegroundColor White
Write-Host "4. Open Control Center → Screen Mirroring" -ForegroundColor White
Write-Host "5. Look for 'AuraCastPro' in the list" -ForegroundColor White
Write-Host ""
Write-Host "If you still don't see it:" -ForegroundColor Yellow
Write-Host "- Check router settings for 'AP Isolation' (should be OFF)" -ForegroundColor Yellow
Write-Host "- Try restarting your router" -ForegroundColor Yellow
Write-Host "- Check if VPN is active on either device (should be OFF)" -ForegroundColor Yellow
Write-Host ""
Write-Host "Press any key to exit..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
