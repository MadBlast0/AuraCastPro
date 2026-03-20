#!/usr/bin/env pwsh
# Real-time connection monitor for AuraCastPro

Write-Host "=== AuraCastPro Connection Monitor ===" -ForegroundColor Cyan
Write-Host "Monitoring port 7100 for incoming connections..." -ForegroundColor Yellow
Write-Host "Try connecting from your iPad now!" -ForegroundColor Green
Write-Host ""

$logFile = "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log"
$lastSize = (Get-Item $logFile).Length

Write-Host "Press Ctrl+C to stop monitoring" -ForegroundColor Gray
Write-Host ""

while ($true) {
    Start-Sleep -Milliseconds 500
    
    $currentSize = (Get-Item $logFile).Length
    if ($currentSize -gt $lastSize) {
        # New log entries - read and display them
        $newContent = Get-Content $logFile -Tail 20 | Select-String -Pattern "(AirPlay2Host|Connection|accept|SESSION|PHASE|pair)" -CaseSensitive:$false
        
        if ($newContent) {
            foreach ($line in $newContent) {
                if ($line -match "ERROR|FAILED") {
                    Write-Host $line -ForegroundColor Red
                } elseif ($line -match "CONNECTION|ACCEPT|SESSION START") {
                    Write-Host $line -ForegroundColor Green
                } elseif ($line -match "PHASE") {
                    Write-Host $line -ForegroundColor Cyan
                } else {
                    Write-Host $line -ForegroundColor White
                }
            }
        }
        
        $lastSize = $currentSize
    }
    
    # Also check for new TCP connections on port 7100
    $connections = netstat -ano | Select-String "7100.*ESTABLISHED"
    if ($connections) {
        Write-Host ">>> ACTIVE CONNECTION DETECTED <<<" -ForegroundColor Magenta
        Write-Host $connections -ForegroundColor Magenta
    }
}
