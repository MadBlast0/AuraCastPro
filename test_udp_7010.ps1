# Test if UDP packets are arriving on port 7010
Write-Host "Testing UDP port 7010..." -ForegroundColor Cyan

# Check if port is listening
$udpConnections = Get-NetUDPEndpoint -LocalPort 7010 -ErrorAction SilentlyContinue
if ($udpConnections) {
    Write-Host "✓ Port 7010 is listening" -ForegroundColor Green
    $udpConnections | Format-Table LocalAddress, LocalPort, OwningProcess
} else {
    Write-Host "✗ Port 7010 is NOT listening" -ForegroundColor Red
}

# Check firewall rule
Write-Host "`nChecking firewall rule..." -ForegroundColor Cyan
$fwRule = Get-NetFirewallRule -DisplayName "*7010*" -ErrorAction SilentlyContinue
if ($fwRule) {
    Write-Host "✓ Firewall rule exists" -ForegroundColor Green
    $fwRule | Select-Object DisplayName, Enabled, Direction, Action | Format-Table
} else {
    Write-Host "✗ No firewall rule for port 7010" -ForegroundColor Red
}

# Check if Windows Firewall is enabled
Write-Host "`nFirewall status:" -ForegroundColor Cyan
Get-NetFirewallProfile | Select-Object Name, Enabled | Format-Table

Write-Host "`nTo capture UDP packets, you can use:" -ForegroundColor Yellow
Write-Host "  netsh trace start capture=yes tracefile=udp_capture.etl" -ForegroundColor White
Write-Host "  (connect iPad)" -ForegroundColor White
Write-Host "  netsh trace stop" -ForegroundColor White

Write-Host "`nPress any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
