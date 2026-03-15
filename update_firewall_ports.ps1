# Update AirPlay Firewall Rules to use standard iOS ports
# Run this as Administrator

Write-Host "Updating firewall rules for AirPlay standard ports..." -ForegroundColor Cyan

# Remove old rules with port 7236
$oldRules = Get-NetFirewallRule -DisplayName "*AuraCastPro*" -ErrorAction SilentlyContinue
foreach ($rule in $oldRules) {
    Write-Host "Removing old rule: $($rule.DisplayName)" -ForegroundColor Yellow
    Remove-NetFirewallRule -Name $rule.Name -ErrorAction SilentlyContinue
}

# Add new rules with correct ports
$appName = "AuraCastPro"
$rules = @(
    @{ Name="$appName AirPlay RTSP"; Protocol="TCP"; Port=7000; Direction="Inbound" },
    @{ Name="$appName AirPlay Timing 1"; Protocol="UDP"; Port=7001; Direction="Inbound" },
    @{ Name="$appName AirPlay Timing 2"; Protocol="UDP"; Port=7002; Direction="Inbound" },
    @{ Name="$appName AirPlay RTP Video"; Protocol="UDP"; Port=7010; Direction="Inbound" },
    @{ Name="$appName AirPlay RTP Audio"; Protocol="UDP"; Port=7011; Direction="Inbound" },
    @{ Name="$appName Cast TCP"; Protocol="TCP"; Port=8009; Direction="Inbound" },
    @{ Name="$appName Cast UDP"; Protocol="UDP"; Port=8009; Direction="Inbound" },
    @{ Name="$appName mDNS"; Protocol="UDP"; Port=5353; Direction="Inbound" }
)

foreach ($rule in $rules) {
    Write-Host "Adding rule: $($rule.Name) - $($rule.Protocol) port $($rule.Port)" -ForegroundColor Green
    New-NetFirewallRule -DisplayName $rule.Name `
        -Direction $rule.Direction `
        -Protocol $rule.Protocol `
        -LocalPort $rule.Port `
        -Action Allow `
        -Profile Private,Domain `
        -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "✓ Firewall rules updated successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "Standard iOS AirPlay ports are now configured:" -ForegroundColor Cyan
Write-Host "  • Port 7000 (TCP) - AirPlay RTSP control" -ForegroundColor White
Write-Host "  • Port 7001 (UDP) - Timing sync" -ForegroundColor White
Write-Host "  • Port 7002 (UDP) - Timing sync" -ForegroundColor White
Write-Host "  • Port 5353 (UDP) - mDNS discovery" -ForegroundColor White
Write-Host ""
Write-Host "Now restart AuraCastPro and try connecting from your iPad." -ForegroundColor Yellow
