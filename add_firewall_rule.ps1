# Add Windows Firewall rule for AuraCastPro UDP ports
# This script must be run as Administrator

$exePath = "C:\Users\MadBlast\Documents\GitHub\AuraCastPro\build\Debug\AuraCastPro.exe"

Write-Host "Adding firewall rule for AuraCastPro..."
Write-Host "Executable: $exePath"

# Remove existing rule if it exists
netsh advfirewall firewall delete rule name="AuraCastPro UDP Ports" 2>$null

# Add new rule for UDP ports 7000, 7010, 7011
netsh advfirewall firewall add rule name="AuraCastPro UDP Ports" dir=in action=allow protocol=UDP localport=7000,7010,7011 program="$exePath" enable=yes

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Firewall rule added successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Allowed UDP ports: 7000 (timing), 7010 (video), 7011 (audio)"
} else {
    Write-Host "✗ Failed to add firewall rule" -ForegroundColor Red
    Write-Host "Please run this script as Administrator (right-click -> Run as Administrator)"
}

Write-Host ""
Write-Host "Press any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
