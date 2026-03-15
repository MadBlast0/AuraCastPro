# Fix AirPlay Firewall Rules
# Run this script as Administrator to allow AirPlay connections

Write-Host "Adding Windows Firewall rules for AirPlay..." -ForegroundColor Cyan

# mDNS port (5353 UDP) - for device discovery
New-NetFirewallRule -DisplayName "AuraCastPro - mDNS Discovery" `
    -Direction Inbound `
    -Protocol UDP `
    -LocalPort 5353 `
    -Action Allow `
    -Profile Private,Domain `
    -ErrorAction SilentlyContinue

# AirPlay RAOP port (7000 TCP) - for audio/video streaming
New-NetFirewallRule -DisplayName "AuraCastPro - AirPlay RAOP" `
    -Direction Inbound `
    -Protocol TCP `
    -LocalPort 7000 `
    -Action Allow `
    -Profile Private,Domain `
    -ErrorAction SilentlyContinue

# AirPlay control port (7100 TCP)
New-NetFirewallRule -DisplayName "AuraCastPro - AirPlay Control" `
    -Direction Inbound `
    -Protocol TCP `
    -LocalPort 7100 `
    -Action Allow `
    -Profile Private,Domain `
    -ErrorAction SilentlyContinue

# AirPlay timing port (7001 UDP)
New-NetFirewallRule -DisplayName "AuraCastPro - AirPlay Timing" `
    -Direction Inbound `
    -Protocol UDP `
    -LocalPort 7001 `
    -Action Allow `
    -Profile Private,Domain `
    -ErrorAction SilentlyContinue

# Allow the executable itself
$exePath = Join-Path $PSScriptRoot "build\release_x64\Release\AuraCastPro.exe"
if (Test-Path $exePath) {
    New-NetFirewallRule -DisplayName "AuraCastPro Application" `
        -Direction Inbound `
        -Program $exePath `
        -Action Allow `
        -Profile Private,Domain `
        -ErrorAction SilentlyContinue
    
    Write-Host "✓ Firewall rules added successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Make sure:" -ForegroundColor Yellow
    Write-Host "  1. Your PC and iPad are on the SAME Wi-Fi network" -ForegroundColor White
    Write-Host "  2. Your Wi-Fi is set to 'Private' network (not Public)" -ForegroundColor White
    Write-Host "  3. Network Discovery is enabled in Windows Settings" -ForegroundColor White
    Write-Host ""
    Write-Host "Now starting AuraCastPro..." -ForegroundColor Cyan
    Start-Process $exePath
} else {
    Write-Host "✗ Could not find AuraCastPro.exe at: $exePath" -ForegroundColor Red
    Write-Host "Please build the application first." -ForegroundColor Yellow
}
