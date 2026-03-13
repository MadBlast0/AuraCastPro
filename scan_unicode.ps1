$dirs = @('src','cloud')
Get-ChildItem -Recurse -Path $dirs -Include '*.cpp','*.h' | ForEach-Object {
    $file = $_
    $lines = Get-Content $file.FullName -Encoding UTF8
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '[^\x00-\x7F]') {
            Write-Host "$($file.Name):$($i+1): $($lines[$i].Trim())"
        }
    }
}
