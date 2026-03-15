# Fix Network Profile - Set to Private for mDNS to work
# Run this as Administrator

Write-Host "Fixing Network Profile for AirPlay Discovery..." -ForegroundColor Cyan
Write-Host ""

# Get current profile
$profile = Get-NetConnectionProfile | Where-Object { $_.IPv4Connectivity -eq "Internet" } | Select-Object -First 1

if ($profile) {
    Write-Host "Current Network: $($profile.Name)" -ForegroundColor Yellow
    Write-Host "Current Category: $($profile.NetworkCategory)" -ForegroundColor Yellow
    Write-Host ""
    
    if ($profile.NetworkCategory -eq "Public") {
        Write-Host "Changing network category to Private..." -ForegroundColor Cyan
        Set-NetConnectionProfile -Name $profile.Name -NetworkCategory Private
        Write-Host "✓ Network category changed to Private!" -ForegroundColor Green
        Write-Host ""
        Write-Host "IMPORTANT: Now restart AuraCastPro and try connecting from your iPad" -ForegroundColor Yellow
    } else {
        Write-Host "✓ Network is already set to Private" -ForegroundColor Green
    }
} else {
    Write-Host "✗ Could not find active network connection" -ForegroundColor Red
}

Write-Host ""
Write-Host "Press any key to continue..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
