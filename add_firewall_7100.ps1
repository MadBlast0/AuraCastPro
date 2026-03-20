# Add firewall rule for port 7100
# Run as Administrator

Write-Host "Adding firewall rule for AuraCastPro port 7100..." -ForegroundColor Yellow

# Remove old rules if they exist
Remove-NetFirewallRule -DisplayName "AuraCastPro Port 7100" -ErrorAction SilentlyContinue

# Add new rule for TCP port 7100
New-NetFirewallRule -DisplayName "AuraCastPro Port 7100" `
                    -Direction Inbound `
                    -Protocol TCP `
                    -LocalPort 7100 `
                    -Action Allow `
                    -Profile Any `
                    -Enabled True

Write-Host "Firewall rule added successfully!" -ForegroundColor Green
Write-Host "iPad should now be able to connect on port 7100" -ForegroundColor Cyan
