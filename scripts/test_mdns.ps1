# Test if mDNS is broadcasting properly
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "mDNS Broadcast Test" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Check if port 5353 is listening
Write-Host "1. Checking if port 5353 (mDNS) is listening..." -ForegroundColor Yellow
$mdnsListeners = netstat -ano | Select-String "UDP.*:5353"
if ($mdnsListeners) {
    Write-Host "   OK: Port 5353 is listening" -ForegroundColor Green
    $mdnsListeners | ForEach-Object { Write-Host "   $_" -ForegroundColor Gray }
} else {
    Write-Host "   ERROR: Port 5353 is NOT listening!" -ForegroundColor Red
    Write-Host "   AuraCastPro may not be running or failed to start mDNS" -ForegroundColor Red
}

Write-Host "`n2. Checking network interfaces..." -ForegroundColor Yellow
$adapters = Get-NetAdapter | Where-Object {$_.Status -eq "Up"}
foreach ($adapter in $adapters) {
    $ip = (Get-NetIPAddress -InterfaceIndex $adapter.ifIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue).IPAddress
    if ($ip) {
        Write-Host "   $($adapter.Name): $ip" -ForegroundColor Green
    }
}

Write-Host "`n3. Testing mDNS multicast group membership..." -ForegroundColor Yellow
Write-Host "   Multicast address: 224.0.0.251:5353" -ForegroundColor Gray
Write-Host "   (This is where AirPlay/Cast devices listen)" -ForegroundColor Gray

Write-Host "`n4. Checking Windows Firewall..." -ForegroundColor Yellow
$rules = Get-NetFirewallRule | Where-Object {$_.DisplayName -like "*AuraCast*" -or $_.DisplayName -like "*5353*"}
if ($rules) {
    Write-Host "   Found firewall rules:" -ForegroundColor Green
    $rules | ForEach-Object { Write-Host "   - $($_.DisplayName)" -ForegroundColor Gray }
} else {
    Write-Host "   WARNING: No AuraCastPro firewall rules found!" -ForegroundColor Yellow
    Write-Host "   Run scripts\fix_firewall.bat as Administrator" -ForegroundColor Yellow
}

Write-Host "`n5. Simulating mDNS query (requires nmap or similar)..." -ForegroundColor Yellow
Write-Host "   To test from your phone:" -ForegroundColor Cyan
Write-Host "   - iPhone: Settings → Wi-Fi → (i) → Check IP is 192.168.x.x" -ForegroundColor Cyan
Write-Host "   - Android: Settings → Wi-Fi → Advanced → Check IP is 192.168.x.x" -ForegroundColor Cyan
Write-Host "   - Both devices must be on SAME subnet as PC" -ForegroundColor Cyan

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Test Complete!" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Make sure AuraCastPro is running" -ForegroundColor White
Write-Host "2. Check logs for 'mDNS responder started' message" -ForegroundColor White
Write-Host "3. On iPhone: Control Center → Screen Mirroring" -ForegroundColor White
Write-Host "4. On Android: Quick Settings → Cast" -ForegroundColor White
Write-Host "5. Look for 'AuraCastPro' in the list`n" -ForegroundColor White

pause
