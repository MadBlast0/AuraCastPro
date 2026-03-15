# Test if port 7000 is accessible
Write-Host "Testing AirPlay port 7000..." -ForegroundColor Cyan

$result = Test-NetConnection -ComputerName 192.168.1.66 -Port 7000 -InformationLevel Detailed

if ($result.TcpTestSucceeded) {
    Write-Host "Port 7000 is OPEN and accepting connections" -ForegroundColor Green
} else {
    Write-Host "Port 7000 is BLOCKED or not listening" -ForegroundColor Red
    Write-Host "  This is why your iPad cannot connect!" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Firewall Status:" -ForegroundColor Cyan
Get-NetFirewallProfile | Select-Object Name, Enabled | Format-Table

Write-Host "Checking firewall rules for port 7000:" -ForegroundColor Cyan
$rules = Get-NetFirewallPortFilter | Where-Object { $_.LocalPort -eq 7000 }
if ($rules) {
    foreach ($filter in $rules) {
        $rule = Get-NetFirewallRule -AssociatedNetFirewallPortFilter $filter
        Write-Host "  Rule: $($rule.DisplayName) - Enabled: $($rule.Enabled) - Action: $($rule.Action)"
    }
} else {
    Write-Host "  No specific firewall rules found for port 7000" -ForegroundColor Yellow
}
