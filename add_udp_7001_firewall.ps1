# Add firewall rule for AuraCastPro UDP 7001 (Timing Sync Port)
# This port is required for AirPlay timing synchronization (NTP)
# Without this rule, iPad cannot establish timing sync and won't send video packets

Write-Host "Adding firewall rule for UDP 7001 (AirPlay Timing Sync)..." -ForegroundColor Cyan

# Check if rule already exists
$existingRule = Get-NetFirewallRule -DisplayName "AuraCastPro UDP 7001" -ErrorAction SilentlyContinue

if ($existingRule) {
    Write-Host "Rule already exists. Removing old rule..." -ForegroundColor Yellow
    Remove-NetFirewallRule -DisplayName "AuraCastPro UDP 7001"
}

# Add the rule
New-NetFirewallRule -DisplayName "AuraCastPro UDP 7001" `
                    -Direction Inbound `
                    -Action Allow `
                    -Protocol UDP `
                    -LocalPort 7001 `
                    -Profile Any `
                    -Description "AuraCastPro - AirPlay Timing Sync (NTP) Port"

Write-Host "✓ Firewall rule added successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "AirPlay ports now configured:" -ForegroundColor Cyan
Write-Host "  • TCP 7000 - Control channel (RTSP)" -ForegroundColor White
Write-Host "  • UDP 7001 - Timing sync (NTP)" -ForegroundColor Green
Write-Host "  • UDP 7010 - Video data (RTP)" -ForegroundColor White
Write-Host "  • UDP 5353 - mDNS discovery" -ForegroundColor White
Write-Host ""
Write-Host "You can now connect your iPad to AuraCastPro!" -ForegroundColor Green
