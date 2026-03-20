# Temporarily disable Windows Firewall for testing
Write-Host "Disabling Windows Firewall for all profiles..." -ForegroundColor Yellow
Write-Host "This is for TESTING ONLY - remember to re-enable it!" -ForegroundColor Red

try {
    Set-NetFirewallProfile -Profile Domain,Public,Private -Enabled False
    Write-Host "`nFirewall DISABLED" -ForegroundColor Green
    Write-Host "Now try connecting your iPad to AuraCastPro"
    Write-Host "`nPress any key when done testing..."
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    
    Write-Host "`nRe-enabling Windows Firewall..." -ForegroundColor Yellow
    Set-NetFirewallProfile -Profile Domain,Public,Private -Enabled True
    Write-Host "Firewall RE-ENABLED" -ForegroundColor Green
} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
}

Write-Host "`nPress any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
