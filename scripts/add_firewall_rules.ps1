# ==============================================================================
# add_firewall_rules.ps1 — Add Windows Firewall rules for AuraCastPro
#
# This script is called by the NSIS installer with elevated privileges.
# It adds inbound firewall rules for AirPlay (TCP 7000, UDP 7001/7002),
# Google Cast (TCP/UDP 8009), and mDNS (UDP 5353).
#
# Safe to run multiple times — rules are removed before re-adding.
# ==============================================================================

param(
    [switch]$Remove  # Pass -Remove to uninstall the rules
)

# Require administrator
$currentUser = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $currentUser.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "This script must be run as Administrator."
    exit 1
}

$appName = "AuraCastPro"
$rules = @(
    @{ Name="$appName AirPlay RTSP"; Protocol="TCP"; Port=7000; Direction="Inbound" },  # RTSP control (standard iOS port)
    @{ Name="$appName AirPlay Timing 1"; Protocol="UDP"; Port=7001; Direction="Inbound" },  # Timing sync
    @{ Name="$appName AirPlay Timing 2"; Protocol="UDP"; Port=7002; Direction="Inbound" },  # Timing sync
    @{ Name="$appName AirPlay RTP Video"; Protocol="UDP"; Port=7010; Direction="Inbound" },  # Video RTP
    @{ Name="$appName AirPlay RTP Audio"; Protocol="UDP"; Port=7011; Direction="Inbound" },  # Audio RTP
    @{ Name="$appName Cast TCP";     Protocol="TCP"; Port=8009; Direction="Inbound" },
    @{ Name="$appName Cast UDP";     Protocol="UDP"; Port=8009; Direction="Inbound" },
    @{ Name="$appName mDNS";         Protocol="UDP"; Port=5353; Direction="Inbound" },
    @{ Name="$appName ADB TCP";      Protocol="TCP"; Port=5555; Direction="Inbound" }
)

if ($Remove) {
    Write-Host "Removing AuraCastPro firewall rules..."
    foreach ($rule in $rules) {
        Remove-NetFirewallRule -DisplayName $rule.Name -ErrorAction SilentlyContinue
        Write-Host "  Removed: $($rule.Name)"
    }
    Write-Host "Done."
    exit 0
}

Write-Host "Adding AuraCastPro firewall rules..."
foreach ($rule in $rules) {
    # Remove existing rule first to avoid duplicates
    Remove-NetFirewallRule -DisplayName $rule.Name -ErrorAction SilentlyContinue

    $params = @{
        DisplayName = $rule.Name
        Direction   = $rule.Direction
        Protocol    = $rule.Protocol
        LocalPort   = $rule.Port
        Action      = "Allow"
        Profile     = "Private,Domain"  # Only on trusted networks, not Public
        Enabled     = "True"
    }

    try {
        New-NetFirewallRule @params | Out-Null
        Write-Host "  Added: $($rule.Name) ($($rule.Protocol)/$($rule.Port))"
    }
    catch {
        Write-Warning "  Failed to add rule '$($rule.Name)': $_"
    }
}

Write-Host "Firewall rules configured successfully."
