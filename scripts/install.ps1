#Requires -RunAsAdministrator

# deepenc environment installer — Windows detection and dispatch
# Usage (as Administrator):
#   powershell -ExecutionPolicy Bypass -File scripts\install.ps1

$ErrorActionPreference = "Stop"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "==> deepenc: detected Windows"

# Check if WSL is available
$wslFeature = Get-WindowsOptionalFeature -Online -FeatureName "Microsoft-Windows-Subsystem-Linux" -ErrorAction SilentlyContinue
$hasWsl = ($wslFeature -and $wslFeature.State -eq "Enabled")

if (-not $hasWsl) {
    Write-Host "==> WSL is not installed. Running WSL2 installer..."
    & "$SCRIPT_DIR\install\windows\wsl2\install.ps1"
} else {
    # WSL is already enabled — check if Ubuntu distro exists
    $distros = wsl --list --quiet 2>$null
    if ($distros -contains "Ubuntu") {
        Write-Host "==> WSL2 + Ubuntu detected. Running Ubuntu installer inside WSL..."
        wsl -d Ubuntu -e bash -c "
            cd ~/deepenc 2>/dev/null || mkdir -p ~/deepenc
            bash scripts/install/linux/ubuntu-noble-24.04/install.sh
        "
    } else {
        Write-Host "==> WSL enabled but Ubuntu not installed. Running WSL2 installer..."
        & "$SCRIPT_DIR\install\windows\wsl2\install.ps1"
    }
}
