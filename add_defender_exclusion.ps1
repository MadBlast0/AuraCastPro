# Add AuraCastPro to Windows Defender exclusions
# Run as Administrator

$exePath = (Get-Location).Path + "\build\Release\AuraCastPro.exe"

Write-Host "Adding exclusion for: $exePath"

try {
    Add-MpPreference -ExclusionPath $exePath
    Write-Host "Successfully added to Windows Defender exclusions!"
    Write-Host "You can now run AuraCastPro.exe without Smart App Control blocking it."
} catch {
    Write-Host "ERROR: Failed to add exclusion. Make sure you're running as Administrator."
    Write-Host $_.Exception.Message
}

# Also add the entire build directory
$buildDir = (Get-Location).Path + "\build"
Write-Host "`nAdding exclusion for build directory: $buildDir"

try {
    Add-MpPreference -ExclusionPath $buildDir
    Write-Host "Successfully added build directory to exclusions!"
} catch {
    Write-Host "ERROR: Failed to add build directory exclusion."
    Write-Host $_.Exception.Message
}
