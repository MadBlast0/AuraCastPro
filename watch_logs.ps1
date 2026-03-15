# Watch AuraCastPro logs in real-time
Get-Content "$env:APPDATA\AuraCastPro\AuraCastPro\logs\auracastpro.log" -Wait -Tail 20
