# Add Windows Firewall rule for AuraCastPro UDP ports
# This script will auto-elevate to Administrator if needed

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "Not running as Administrator. Requesting elevation..." -ForegroundColor Yellow
    
    # Re-launch as administrator
    Start-Process powershell.exe -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
    exit
}

# Now running as Administrator
Write-Host "Running as Administrator" -ForegroundColor Green
Write-Host ""

$exePath = "C:\Users\MadBlast\Documents\GitHub\AuraCastPro\build\Debug\AuraCastPro.exe"

Write-Host "Adding firewall rule for AuraCastPro..."
Write-Host "Executable: $exePath"
Write-Host ""

# Remove existing rule if it exists
Write-Host "Removing any existing rule..."
netsh advfirewall firewall delete rule name="AuraCastPro UDP Ports" 2>$null | Out-Null

# Add new rule for UDP ports 7000, 7010, 7011
Write-Host "Adding new firewall rule..."
$result = netsh advfirewall firewall add rule name="AuraCastPro UDP Ports" dir=in action=allow protocol=UDP localport=7000,7010,7011 program="$exePath" enable=yes

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "✓ Firewall rule added successfully!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Allowed UDP ports:" -ForegroundColor Cyan
    Write-Host "  - 7000 (timing/control)" -ForegroundColor Cyan
    Write-Host "  - 7010 (video stream)" -ForegroundColor Cyan
    Write-Host "  - 7011 (audio stream)" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "You can now try connecting from your iPad!" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "✗ Failed to add firewall rule" -ForegroundColor Red
    Write-Host "Error code: $LASTEXITCODE" -ForegroundColor Red
}

Write-Host ""
Write-Host "Press any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
