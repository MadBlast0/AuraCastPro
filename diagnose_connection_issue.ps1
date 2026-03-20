# Comprehensive diagnostic for AirPlay connection issues

Write-Host "=== AuraCastPro Connection Diagnostics ===" -ForegroundColor Cyan
Write-Host ""

# Check if app is running
Write-Host "1. Checking if app is running..." -ForegroundColor Yellow
$process = Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue
if ($process) {
    Write-Host "   ✗ App is still running (PID: $($process.Id))" -ForegroundColor Red
    Write-Host "   → Please close the app first" -ForegroundColor Yellow
    exit
} else {
    Write-Host "   ✓ App is closed" -ForegroundColor Green
}

# Check settings file
Write-Host ""
Write-Host "2. Checking settings..." -ForegroundColor Yellow
$settingsPath = "$env:LOCALAPPDATA\AuraCastPro\settings.json"
if (Test-Path $settingsPath) {
    Write-Host "   Settings file exists: $settingsPath" -ForegroundColor Cyan
    try {
        $settings = Get-Content $settingsPath | ConvertFrom-Json
        Write-Host "   - maxResolutionIndex: $($settings.maxResolutionIndex) (0=Native, 1=4K, 2=2K, 3=1080p)" -ForegroundColor White
        Write-Host "   - fpsCapIndex: $($settings.fpsCapIndex) (0=Auto/60fps)" -ForegroundColor White
        Write-Host "   - maxBitrateKbps: $($settings.maxBitrateKbps)" -ForegroundColor White
        Write-Host "   - airplayPort: $($settings.airplayPort)" -ForegroundColor White
        
        if ($settings.maxResolutionIndex -ne 0) {
            Write-Host "   ⚠ Resolution is not Native (Auto)" -ForegroundColor Yellow
            Write-Host "   → Recommend changing to 0 (Native)" -ForegroundColor Yellow
        }
    } catch {
        Write-Host "   ✗ Failed to parse settings file" -ForegroundColor Red
    }
} else {
    Write-Host "   ✓ No settings file - will use defaults (Native, Auto, 20Mbps)" -ForegroundColor Green
}

# Check logs
Write-Host ""
Write-Host "3. Checking for recent logs..." -ForegroundColor Yellow
$logPath = "$env:LOCALAPPDATA\AuraCastPro\logs"
if (Test-Path $logPath) {
    $latestLog = Get-ChildItem $logPath -Filter "*.log" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($latestLog) {
        Write-Host "   Latest log: $($latestLog.Name)" -ForegroundColor Cyan
        Write-Host "   Last modified: $($latestLog.LastWriteTime)" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "   Checking for connection attempts..." -ForegroundColor Yellow
        
        # Look for /info requests
        $infoRequests = Select-String -Path $latestLog.FullName -Pattern "/info request" -SimpleMatch
        if ($infoRequests) {
            Write-Host "   ✓ Found $($infoRequests.Count) /info requests (iPad tried to connect)" -ForegroundColor Green
        } else {
            Write-Host "   ✗ No /info requests found (iPad never tried to connect)" -ForegroundColor Red
            Write-Host "   → iPad might not see AuraCastPro in Screen Mirroring list" -ForegroundColor Yellow
        }
        
        # Look for errors
        Write-Host ""
        Write-Host "   Recent errors:" -ForegroundColor Yellow
        $errors = Select-String -Path $latestLog.FullName -Pattern "ERROR|CRITICAL|failed" -SimpleMatch | Select-Object -Last 10
        if ($errors) {
            foreach ($error in $errors) {
                Write-Host "   $($error.Line)" -ForegroundColor Red
            }
        } else {
            Write-Host "   ✓ No errors found" -ForegroundColor Green
        }
        
        # Look for AirPlay2Host initialization
        Write-Host ""
        Write-Host "   Checking AirPlay2Host initialization..." -ForegroundColor Yellow
        $airplayInit = Select-String -Path $latestLog.FullName -Pattern "AirPlay2Host.*init|Display capabilities set" -SimpleMatch | Select-Object -Last 5
        if ($airplayInit) {
            foreach ($line in $airplayInit) {
                Write-Host "   $($line.Line)" -ForegroundColor Cyan
            }
        } else {
            Write-Host "   ✗ AirPlay2Host initialization not found" -ForegroundColor Red
        }
        
        # Look for mDNS
        Write-Host ""
        Write-Host "   Checking mDNS advertisement..." -ForegroundColor Yellow
        $mdns = Select-String -Path $latestLog.FullName -Pattern "mDNS|MDNSService|_airplay" -SimpleMatch | Select-Object -Last 5
        if ($mdns) {
            foreach ($line in $mdns) {
                Write-Host "   $($line.Line)" -ForegroundColor Cyan
            }
        } else {
            Write-Host "   ⚠ No mDNS logs found" -ForegroundColor Yellow
        }
    } else {
        Write-Host "   ✗ No log files found" -ForegroundColor Red
    }
} else {
    Write-Host "   ✗ Log directory doesn't exist: $logPath" -ForegroundColor Red
    Write-Host "   → App may not have run yet, or logging is disabled" -ForegroundColor Yellow
}

# Check firewall
Write-Host ""
Write-Host "4. Checking firewall rules..." -ForegroundColor Yellow
$fwRules = Get-NetFirewallRule -DisplayName "*AuraCastPro*" -ErrorAction SilentlyContinue
if ($fwRules) {
    Write-Host "   ✓ Firewall rules exist:" -ForegroundColor Green
    foreach ($rule in $fwRules) {
        $enabled = if ($rule.Enabled -eq "True") { "Enabled" } else { "Disabled" }
        Write-Host "     - $($rule.DisplayName) [$enabled]" -ForegroundColor White
    }
} else {
    Write-Host "   ✗ No firewall rules found" -ForegroundColor Red
    Write-Host "   → Run add_firewall_rule.ps1 as Administrator" -ForegroundColor Yellow
}

# Check network
Write-Host ""
Write-Host "5. Checking network interfaces..." -ForegroundColor Yellow
$adapters = Get-NetAdapter | Where-Object { $_.Status -eq "Up" }
foreach ($adapter in $adapters) {
    $ip = (Get-NetIPAddress -InterfaceIndex $adapter.ifIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue).IPAddress
    if ($ip) {
        Write-Host "   ✓ $($adapter.Name): $ip" -ForegroundColor Green
    }
}

# Check build
Write-Host ""
Write-Host "6. Checking executable..." -ForegroundColor Yellow
$exePath = "build\Release\AuraCastPro.exe"
if (Test-Path $exePath) {
    $exe = Get-Item $exePath
    Write-Host "   ✓ Executable exists" -ForegroundColor Green
    Write-Host "   - Size: $([math]::Round($exe.Length / 1MB, 2)) MB" -ForegroundColor White
    Write-Host "   - Last modified: $($exe.LastWriteTime)" -ForegroundColor White
    
    if ($exe.LastWriteTime -lt (Get-Date).AddHours(-1)) {
        Write-Host "   ⚠ Executable is more than 1 hour old" -ForegroundColor Yellow
        Write-Host "   → Rebuild if you made recent changes" -ForegroundColor Yellow
    }
} else {
    Write-Host "   ✗ Executable not found: $exePath" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Recommendations ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Based on the diagnostics above:" -ForegroundColor Yellow
Write-Host ""
Write-Host "If no /info requests found:" -ForegroundColor White
Write-Host "  → iPad can't see AuraCastPro in Screen Mirroring list" -ForegroundColor White
Write-Host "  → Check: Same Wi-Fi network? Firewall rules? mDNS working?" -ForegroundColor White
Write-Host ""
Write-Host "If /info requests found but connection fails:" -ForegroundColor White
Write-Host "  → iPad sees AuraCastPro but can't connect" -ForegroundColor White
Write-Host "  → Check error messages in logs" -ForegroundColor White
Write-Host "  → Try changing resolution to Native (Auto)" -ForegroundColor White
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Add firewall rules if missing: .\add_firewall_rule.ps1 (as Admin)" -ForegroundColor White
Write-Host "  2. Delete settings to use defaults: Remove-Item '$settingsPath'" -ForegroundColor White
Write-Host "  3. Start app and check logs in: $logPath" -ForegroundColor White
Write-Host "  4. Try connecting from iPad" -ForegroundColor White
Write-Host "  5. Run this script again to see what happened" -ForegroundColor White
Write-Host ""
