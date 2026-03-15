# Test mDNS Discovery
Write-Host "Testing mDNS Discovery..." -ForegroundColor Cyan
Write-Host "Listening for mDNS packets on 224.0.0.251:5353..." -ForegroundColor Yellow
Write-Host "Press Ctrl+C to stop" -ForegroundColor Gray
Write-Host ""

$udpClient = New-Object System.Net.Sockets.UdpClient
$udpClient.Client.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::Socket, [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true)
$udpClient.Client.Bind([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 5353))

$multicastAddress = [System.Net.IPAddress]::Parse("224.0.0.251")
$udpClient.JoinMulticastGroup($multicastAddress)

Write-Host "Joined mDNS multicast group. Waiting for packets..." -ForegroundColor Green

$count = 0
$foundAuraCast = $false

try {
    while ($count -lt 30) {
        $remoteEP = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
        $data = $udpClient.Receive([ref]$remoteEP)
        
        $text = [System.Text.Encoding]::ASCII.GetString($data)
        
        if ($text -match "AuraCastPro|airplay|raop") {
            Write-Host "Found AuraCastPro mDNS packet from $($remoteEP.Address)" -ForegroundColor Green
            $foundAuraCast = $true
            
            if ($text -match "_airplay") {
                Write-Host "  - Contains _airplay._tcp service" -ForegroundColor Cyan
            }
            if ($text -match "_raop") {
                Write-Host "  - Contains _raop._tcp service" -ForegroundColor Cyan
            }
            if ($text -match "_googlecast") {
                Write-Host "  - Contains _googlecast._tcp service" -ForegroundColor Cyan
            }
            
            break
        }
        
        $count++
        if ($count % 10 -eq 0) {
            Write-Host "  Received $count packets..." -ForegroundColor Gray
        }
    }
    
    if (-not $foundAuraCast) {
        Write-Host "Did not find AuraCastPro in mDNS packets" -ForegroundColor Red
        Write-Host "  This means the mDNS service may not be broadcasting correctly" -ForegroundColor Yellow
    }
}
finally {
    $udpClient.Close()
}

Write-Host ""
Write-Host "Test complete." -ForegroundColor Cyan
