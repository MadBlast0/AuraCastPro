$dirs = @('src','cloud')
$replacements = @{
    [char]0x2192 = '->'      # →
    [char]0x2190 = '<-'      # ←
    [char]0x2014 = '--'      # — (em-dash)
    [char]0x2013 = '-'       # – (en-dash)
    [char]0x00D7 = 'x'       # ×
    [char]0x00F7 = '/'       # ÷
    [char]0x2018 = "'"        # '
    [char]0x2019 = "'"        # '
    [char]0x201C = '"'        # "
    [char]0x201D = '"'        # "
    [char]0x2026 = '...'     # …
    [char]0x221E = 'inf'     # ∞
}

$files = Get-ChildItem -Recurse -Path $dirs -Include '*.cpp','*.h','*.hpp'
$fixedCount = 0

foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw -Encoding UTF8
    $changed = $false
    foreach ($kv in $replacements.GetEnumerator()) {
        if ($content.Contains([string]$kv.Key)) {
            $content = $content.Replace([string]$kv.Key, $kv.Value)
            $changed = $true
        }
    }
    if ($changed) {
        Set-Content $file.FullName $content -Encoding UTF8 -NoNewline
        $fixedCount++
        Write-Host "Fixed: $($file.Name)"
    }
}

Write-Host ""
Write-Host "Total files fixed: $fixedCount"
