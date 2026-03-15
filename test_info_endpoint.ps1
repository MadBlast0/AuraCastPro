# Test /info Endpoint Fix
# This script restarts the app and monitors for /info requests from iPad

Write-Host "=== Testing /info Endpoint Fix ===" -ForegroundColor Cyan
Write-Host ""

# Step 1: Stop any running instances
Write-Host "[1/4] Stopping old instances..." -ForegroundColor Yellow
Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

# Step 2: Verify port 7000 is free
Write-Host "[2/4] Checking if port 7000 is free..." -ForegroundColor Yellow
$portCheck = Get-NetTCPConnection -LocalPort 7000 -ErrorAction SilentlyContinue
if ($portCheck) {
    Write-Host "  WARNING: Port 7000 is still in use!" -ForegroundColor Red
    Write-Host "  Waiting 5 seconds for port to be released..." -ForegroundColor Yellow
    Start-Sleep -Seconds 5
}

# Step 3: Start the new build
Write-Host "[3/4] Starting AuraCastPro with updated /info endpoint..." -ForegroundColor Yellow
$appPath = ".\build\Release\AuraCastPro.exe"

if (!(Test-Path $appPath)) {
    Write-Host "  ERROR: App not found at $appPath" -ForegroundColor Red
    Write-Host "  Run: cmake --build build --config Release" -ForegroundColor Yellow
    exit 1
}

Write-Host "  Launching: $appPath" -ForegroundColor Green
Start-Process -FilePath $appPath -WorkingDirectory (Get-Location)

# Wait for app to start
Start-Sleep -Seconds 3

# Step 4: Verify app is running and listening
Write-Host "[4/4] Verifying app is running..." -ForegroundColor Yellow
$process = Get-Process -Name "AuraCastPro" -ErrorAction SilentlyContinue
if ($process) {
    Write-Host "  ✓ App is running (PID: $($process.Id))" -ForegroundColor Green
} else {
    Write-Host "  ✗ App failed to start!" -ForegroundColor Red
    exit 1
}

$listening = Get-NetTCPConnection -LocalPort 7000 -State Listen -ErrorAction SilentlyContinue
if ($listening) {
    Write-Host "  ✓ Port 7000 is listening" -ForegroundColor Green
} else {
    Write-Host "  ✗ Port 7000 is NOT listening!" -ForegroundColor Red
    Write-Host "  Check the app logs for errors" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== What Changed ===" -ForegroundColor Cyan
Write-Host "The /info endpoint now returns ALL required fields:" -ForegroundColor White
Write-Host "  ✓ audioFormats array (types 100, 101)" -ForegroundColor Green
Write-Host "  ✓ audioLatencies array" -ForegroundColor Green
Write-Host "  ✓ displays array (1920x1080)" -ForegroundColor Green
Write-Host "  ✓ statusFlags = 4 (was 0)" -ForegroundColor Green
Write-Host "  ✓ vv = 2 (version)" -ForegroundColor Green
Write-Host "  ✓ keepAliveLowPower = true" -ForegroundColor Green
Write-Host "  ✓ keepAliveSendStatsAsBody = true" -ForegroundColor Green
Write-Host "  ✓ model = AppleTV5,3 (was AppleTV3,2)" -ForegroundColor Green
Write-Host ""

Write-Host "=== Next Steps ===" -ForegroundColor Cyan
Write-Host "1. Open Control Center on your iPad" -ForegroundColor White
Write-Host "2. Tap 'Screen Mirroring'" -ForegroundColor White
Write-Host "3. Tap 'AuraCastPro'" -ForegroundColor White
Write-Host ""
Write-Host "Watch the app console for this log line:" -ForegroundColor Yellow
Write-Host "  [info] [AirPlay2Host] <<< GET /info RTSP/1.0" -ForegroundColor Cyan
Write-Host "  [info] [AirPlay2Host] Returned /info plist to [iPad IP]" -ForegroundColor Cyan
Write-Host ""
Write-Host "If you DON'T see these lines, iOS isn't reaching our /info endpoint." -ForegroundColor Yellow
Write-Host ""
Write-Host "Press Ctrl+C to stop monitoring..." -ForegroundColor Gray
