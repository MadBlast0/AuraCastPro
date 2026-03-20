# Verify AirPlay Port Configuration
# Checks firewall rules and listening ports for AuraCastPro

Write-Host "`n=== AuraCastPro Port Verification ===" -ForegroundColor Cyan
Write-Host ""

# Required ports
$requiredPorts = @(
    @{Port=7000; Protocol="TCP"; Purpose="AirPlay Control (RTSP)"},
    @{Port=7001; Protocol="UDP"; Purpose="Timing Sync (NTP)"},
    @{Port=7010; Protocol="UDP"; Purpose="Video Data (RTP)"},
    @{Port=5353; Protocol="UDP"; Purpose="mDNS Discovery"}
)

Write-Host "Checking Firewall Rules..." -ForegroundColor Yellow
Write-Host ""

$allRulesOk = $true

foreach ($portInfo in $requiredPorts) {
    $port = $portInfo.Port
    $protocol = $portInfo.Protocol
    $purpose = $portInfo.Purpose
    
    # Check firewall rule
    $ruleName = "AuraCastPro $protocol $port"
    $rule = Get-NetFirewallRule -DisplayName $ruleName -ErrorAction SilentlyContinue
    
    if ($rule) {
        Write-Host "  ✓ $protocol $port - $purpose" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $protocol $port - $purpose (MISSING RULE!)" -ForegroundColor Red
        $allRulesOk = $false
    }
}

Write-Host ""
Write-Host "Checking Listening Ports..." -ForegroundColor Yellow
Write-Host ""

# Check if AuraCastPro is running
$process = Get-Process AuraCastPro -ErrorAction SilentlyContinue

if ($process) {
    Write-Host "  ✓ AuraCastPro is running (PID: $($process.Id))" -ForegroundColor Green
    Write-Host ""
    
    # Check TCP 7000
    $tcp7000 = Get-NetTCPConnection -LocalPort 7000 -State Listen -ErrorAction SilentlyContinue
    if ($tcp7000) {
        Write-Host "  ✓ TCP 7000 listening" -ForegroundColor Green
    } else {
        Write-Host "  ✗ TCP 7000 NOT listening" -ForegroundColor Red
    }
    
    # Check UDP ports
    $udp7001 = Get-NetUDPEndpoint -LocalPort 7001 -ErrorAction SilentlyContinue
    if ($udp7001) {
        Write-Host "  ✓ UDP 7001 listening" -ForegroundColor Green
    } else {
        Write-Host "  ✗ UDP 7001 NOT listening" -ForegroundColor Red
    }
    
    $udp7010 = Get-NetUDPEndpoint -LocalPort 7010 -ErrorAction SilentlyContinue
    if ($udp7010) {
        Write-Host "  ✓ UDP 7010 listening" -ForegroundColor Green
    } else {
        Write-Host "  ✗ UDP 7010 NOT listening" -ForegroundColor Red
    }
    
    $udp5353 = Get-NetUDPEndpoint -LocalPort 5353 -ErrorAction SilentlyContinue
    if ($udp5353) {
        Write-Host "  ✓ UDP 5353 listening" -ForegroundColor Green
    } else {
        Write-Host "  ✗ UDP 5353 NOT listening" -ForegroundColor Red
    }
} else {
    Write-Host "  ✗ AuraCastPro is NOT running" -ForegroundColor Red
    Write-Host "    Start the app to check listening ports" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Cyan

if ($allRulesOk) {
    Write-Host "✓ All firewall rules are configured correctly!" -ForegroundColor Green
} else {
    Write-Host "✗ Some firewall rules are missing!" -ForegroundColor Red
    Write-Host ""
    Write-Host "To fix, run as Administrator:" -ForegroundColor Yellow
    Write-Host "  .\add_udp_7001_firewall.ps1" -ForegroundColor White
}

Write-Host ""

Write-Host ""
