# Add firewall rule for UDP 7010 (AirPlay video packets)
Write-Host "Adding firewall rule for UDP 7010..."

try {
    New-NetFirewallRule -DisplayName "AuraCastPro UDP 7010 Video" `
                        -Direction Inbound `
                        -Protocol UDP `
                        -LocalPort 7010 `
                        -Action Allow `
                        -Profile Any `
                        -ErrorAction Stop
    
    Write-Host "SUCCESS: Firewall rule added for UDP 7010" -ForegroundColor Green
    Write-Host "Video packets from iPad should now be received."
} catch {
    Write-Host "ERROR: Failed to add firewall rule: $_" -ForegroundColor Red
}

Write-Host "`nPress any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
