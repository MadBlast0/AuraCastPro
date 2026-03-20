# Reset AuraCastPro settings to safe defaults for iPad connection

$settingsPath = "$env:LOCALAPPDATA\AuraCastPro\settings.json"

Write-Host "Resetting AuraCastPro settings to safe defaults..." -ForegroundColor Yellow
Write-Host "Settings file: $settingsPath" -ForegroundColor Cyan

if (Test-Path $settingsPath) {
    # Backup existing settings
    $backupPath = "$settingsPath.backup_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
    Copy-Item $settingsPath $backupPath
    Write-Host "Backed up existing settings to: $backupPath" -ForegroundColor Green
}

# Create safe default settings
$safeDefaults = @{
    # Identity
    deviceName = "AuraCastPro"
    firstRunCompleted = $true
    
    # Video - SAFE DEFAULTS FOR iPAD
    maxWidth = 1920
    maxHeight = 1080
    maxFps = 60
    maxResolutionIndex = 0  # Native (Auto) - advertises 1080p as safe default
    fpsCapIndex = 0         # Auto (60 FPS)
    hdrEnabled = $false
    hardwareDecodeEnabled = $true
    preferredCodec = "h265"
    colorSpace = "sRGB"
    hardwareEncoder = "Auto"
    maxBitrateKbps = 20000  # 20 Mbps
    
    # Audio
    audioEnabled = $true
    audioVolume = 100
    audioLatencyMs = 200
    
    # Recording - Safe defaults
    recordingFolder = "$env:USERPROFILE\Videos\AuraCastPro"
    videoCodec = "h265"
    recordingFormat = "mp4"
    videoEncoder = "Stream Copy (No Re-encode)"
    audioEncoder = "AAC"
    bitrate = 20000
    audioBitrate = 192
    rateControl = "Constant Bitrate (CBR)"
    encoderPreset = "P4 - Medium"
    encoderProfile = "main"
    encoderTuning = "High Quality"
    multipassMode = "Single Pass"
    keyframeInterval = 0
    lookAhead = $true
    adaptiveQuantization = $true
    bFrames = 2
    customEncoderOptions = ""
    customMuxerSettings = ""
    audioTrackMask = 1
    rescaleFilter = "Lanczos (Best Quality)"
    rescaleResolution = "Source"  # Record at source resolution (no rescaling)
    autoFileSplitting = $false
    fileSplitMode = "By Size"
    fileSplitSizeMB = 2000
    fileSplitTimeMinutes = 60
    generateFileNameWithoutSpace = $false
    
    # Network
    autoReconnect = $true
    reconnectDelayMs = 3000
    maxReconnectAttempts = 5
    
    # UI
    theme = "dark"
    language = "en"
    showFps = $true
    showBitrate = $true
    showLatency = $true
    minimizeToTray = $true
    startMinimized = $false
    
    # Advanced
    logLevel = "info"
    enableTelemetry = $false
    checkForUpdates = $true
    hardwareAcceleration = $true
    lowLatencyMode = $false
    bufferSize = 4
}

# Convert to JSON and save
$json = $safeDefaults | ConvertTo-Json -Depth 10
$json | Out-File -FilePath $settingsPath -Encoding UTF8

Write-Host ""
Write-Host "✓ Settings reset to safe defaults!" -ForegroundColor Green
Write-Host ""
Write-Host "Key changes:" -ForegroundColor Yellow
Write-Host "  - Mirroring Resolution: Native (Auto) - advertises 1080p" -ForegroundColor White
Write-Host "  - Mirroring Frame Rate: Auto (60 FPS)" -ForegroundColor White
Write-Host "  - Mirroring Bitrate: 20 Mbps" -ForegroundColor White
Write-Host "  - Recording Resolution: Source (no rescaling)" -ForegroundColor White
Write-Host "  - Recording Encoder: Stream Copy (zero CPU)" -ForegroundColor White
Write-Host "  - Recording Codec: H.265" -ForegroundColor White
Write-Host ""
Write-Host "Now restart AuraCastPro and try connecting from iPad." -ForegroundColor Cyan
Write-Host ""
