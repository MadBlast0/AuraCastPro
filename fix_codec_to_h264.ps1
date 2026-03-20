# Fix: Change codec preference from H.265 to H.264
# H.265/HEVC decoder is not available on this system
# H.264/AVC works and is more compatible

$settingsPath = "$env:APPDATA\AuraCastPro\AuraCastPro\settings.json"

Write-Host "Fixing codec preference..." -ForegroundColor Cyan

if (Test-Path $settingsPath) {
    $settings = Get-Content $settingsPath | ConvertFrom-Json
    
    if ($settings.video.preferredCodec -eq "h265" -or $settings.video.preferredCodec -eq "hevc") {
        Write-Host "Current codec: $($settings.video.preferredCodec)" -ForegroundColor Yellow
        $settings.video.preferredCodec = "h264"
        $settings | ConvertTo-Json -Depth 10 | Set-Content $settingsPath
        Write-Host "✓ Changed to H.264" -ForegroundColor Green
    } else {
        Write-Host "Already using: $($settings.video.preferredCodec)" -ForegroundColor Green
    }
} else {
    Write-Host "Settings file not found - will use H.264 by default" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Now restart AuraCastPro and reconnect iPad" -ForegroundColor Cyan
Write-Host "The iPad will send H.264 video which can be decoded" -ForegroundColor White
