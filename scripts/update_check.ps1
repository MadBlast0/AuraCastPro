# =============================================================================
# update_check.ps1 — Check for AuraCastPro updates from the update server
# Called by UpdateService.cpp on a 24-hour timer, or manually by the user.
# =============================================================================
param(
    [string]$CurrentVersion = "0.1.0",
    [string]$UpdateEndpoint = "https://auracastpro.com/api/version",
    [switch]$Silent
)

$ErrorActionPreference = "SilentlyContinue"

function Compare-Version {
    param([string]$v1, [string]$v2)
    $a = [System.Version]$v1
    $b = [System.Version]$v2
    return $a.CompareTo($b)
}

try {
    if (-not $Silent) { Write-Host "Checking for AuraCastPro updates..." }

    $response = Invoke-RestMethod -Uri $UpdateEndpoint `
        -Method GET `
        -Headers @{ "User-Agent" = "AuraCastPro/$CurrentVersion (Windows)" } `
        -TimeoutSec 10

    $latestVersion  = $response.version
    $downloadUrl    = $response.download_url
    $releaseNotes   = $response.release_notes
    $isCritical     = $response.is_critical -eq $true

    if (Compare-Version $latestVersion $CurrentVersion -gt 0) {
        if (-not $Silent) {
            Write-Host ""
            Write-Host "UPDATE AVAILABLE: v$latestVersion (current: v$CurrentVersion)" -ForegroundColor Green
            if ($isCritical) {
                Write-Host "*** CRITICAL UPDATE — includes important security fixes ***" -ForegroundColor Red
            }
            Write-Host "Release notes: $releaseNotes"
            Write-Host "Download: $downloadUrl"
        }
        # Write result to a temp file so UpdateService.cpp can read it
        $result = @{ available=$true; version=$latestVersion; url=$downloadUrl; critical=$isCritical }
        $result | ConvertTo-Json | Out-File "$env:TEMP\AuraCastPro_update.json" -Encoding UTF8
        exit 1  # exit code 1 = update available
    } else {
        if (-not $Silent) { Write-Host "AuraCastPro is up to date (v$CurrentVersion)." }
        exit 0  # exit code 0 = up to date
    }
} catch {
    if (-not $Silent) { Write-Host "Update check failed: $_" -ForegroundColor Yellow }
    exit 2  # exit code 2 = network error
}
