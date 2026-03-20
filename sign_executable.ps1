# Sign AuraCastPro.exe with a self-signed certificate
# This prevents Smart App Control from blocking the app

$certName = "AuraCastPro Development Certificate"
$exePath = "build\Release\AuraCastPro.exe"

# Check if certificate already exists
$cert = Get-ChildItem -Path Cert:\CurrentUser\My | Where-Object { $_.Subject -like "*$certName*" }

if (-not $cert) {
    Write-Host "Creating self-signed certificate..."
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=$certName" -CertStoreLocation Cert:\CurrentUser\My
    Write-Host "Certificate created: $($cert.Thumbprint)"
} else {
    Write-Host "Using existing certificate: $($cert.Thumbprint)"
}

# Sign the executable
if (Test-Path $exePath) {
    Write-Host "Signing $exePath..."
    Set-AuthenticodeSignature -FilePath $exePath -Certificate $cert -TimestampServer "http://timestamp.digicert.com"
    Write-Host "Executable signed successfully!"
    
    # Verify signature
    $signature = Get-AuthenticodeSignature -FilePath $exePath
    Write-Host "Signature status: $($signature.Status)"
} else {
    Write-Host "ERROR: $exePath not found. Build the project first."
}
