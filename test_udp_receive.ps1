# Simple UDP receiver test to see if packets arrive on port 7010
# This will help diagnose if the iPad is sending packets

$port = 7010
$endpoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, $port)
$udpClient = New-Object System.Net.Sockets.UdpClient $port

Write-Host "Listening for UDP packets on port $port..."
Write-Host "Connect from your iPad now..."
Write-Host "Press Ctrl+C to stop"
Write-Host ""

$packetCount = 0
$startTime = Get-Date

try {
    while ($true) {
        if ($udpClient.Available -gt 0) {
            $data = $udpClient.Receive([ref]$endpoint)
            $packetCount++
            $elapsed = ((Get-Date) - $startTime).TotalSeconds
            Write-Host "[$elapsed s] Packet #$packetCount from $($endpoint.Address):$($endpoint.Port) - $($data.Length) bytes"
            
            # Show first 32 bytes in hex
            $hex = ($data[0..[Math]::Min(31, $data.Length-1)] | ForEach-Object { $_.ToString("X2") }) -join " "
            Write-Host "  Data: $hex"
        }
        Start-Sleep -Milliseconds 100
    }
} finally {
    $udpClient.Close()
    Write-Host ""
    Write-Host "Total packets received: $packetCount"
}
