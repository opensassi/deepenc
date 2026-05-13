#Requires -RunAsAdministrator

# WSL2 — full development environment for deepenc
# Usage (as Administrator):
#   powershell -ExecutionPolicy Bypass -File scripts\install\windows\wsl2\install.ps1

$ErrorActionPreference = "Stop"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$REPO_ROOT = Resolve-Path "$SCRIPT_DIR\..\..\..\.."

Write-Host "==> deepenc: installing for Windows (WSL2)..."

# ---- 1. Check if WSL feature is enabled ----
$wslFeature = Get-WindowsOptionalFeature -Online -FeatureName "Microsoft-Windows-Subsystem-Linux"
$vmpFeature = Get-WindowsOptionalFeature -Online -FeatureName "VirtualMachinePlatform"

if ($wslFeature.State -ne "Enabled" -or $vmpFeature.State -ne "Enabled") {
    Write-Host "==> Enabling WSL and Virtual Machine Platform..."
    Enable-WindowsOptionalFeature -Online -FeatureName "Microsoft-Windows-Subsystem-Linux" -NoRestart
    Enable-WindowsOptionalFeature -Online -FeatureName "VirtualMachinePlatform" -NoRestart
    Write-Host ""
    Write-Host "WARNING: A system reboot is required before continuing."
    Write-Host "After rebooting, run this script again to complete setup."
    Write-Host ""
    pause
    exit
}
Write-Host "  ✓ WSL feature is enabled"

# ---- 2. Install/update WSL2 kernel ----
Write-Host "==> Updating WSL2 kernel..."
wsl --update

# ---- 3. Set WSL2 as default ----
Write-Host "==> Setting WSL2 as default..."
wsl --set-default-version 2

# ---- 4. Install Ubuntu distro if not present ----
$distros = wsl --list --quiet
if ($distros -notcontains "Ubuntu") {
    Write-Host "==> Installing Ubuntu distribution..."
    wsl --install -d Ubuntu
    Write-Host ""
    Write-Host "Ubuntu installed. It will start automatically to complete setup."
    Write-Host "Create a Linux username/password when prompted, then re-run this script."
    Write-Host ""
    pause
    exit
}
Write-Host "  ✓ Ubuntu distribution found"

# ---- 5. Set up the repo inside WSL ----
$WSL_HOME = "~"
$WSL_REPO_DIR = "deepenc"

# Check if the repo already exists in WSL
$repoExists = wsl -d Ubuntu -e bash -c "test -d $WSL_HOME/$WSL_REPO_DIR && echo 'exists' || echo ''"
if (-not $repoExists) {
    # Check if we're inside a git repo (clone scenario)
    if (Test-Path "$REPO_ROOT\.git") {
        $repoUrl = (git -C $REPO_ROOT remote get-url origin 2>$null)
        if ($repoUrl) {
            Write-Host "==> Cloning existing repository inside WSL..."
            wsl -d Ubuntu -e bash -c "cd $WSL_HOME && git clone $repoUrl $WSL_REPO_DIR"
        } else {
            Write-Host "==> Copying files to WSL (no remote origin)..."
            wsl -d Ubuntu -e bash -c "mkdir -p $WSL_HOME/$WSL_REPO_DIR"
            # Copy files from current directory into WSL (excluding node_modules, .git, etc.)
            wsl -d Ubuntu -e bash -c "rsync -a --exclude='node_modules' --exclude='.git' '$REPO_ROOT/' '$WSL_HOME/$WSL_REPO_DIR/'"
        }
    } else {
        # Starter scenario — create directory structure
        Write-Host "==> Creating project structure inside WSL..."
        wsl -d Ubuntu -e bash -c "mkdir -p $WSL_HOME/$WSL_REPO_DIR"
        if (Test-Path $REPO_ROOT) {
            wsl -d Ubuntu -e bash -c "rsync -a '$REPO_ROOT/' '$WSL_HOME/$WSL_REPO_DIR/'"
        }
    }
}
Write-Host "  ✓ Repository set up at ~/$WSL_REPO_DIR inside WSL"

# ---- 6. Run the Ubuntu install script inside WSL ----
Write-Host "==> Running Ubuntu install script inside WSL..."
wsl -d Ubuntu -e bash -c "
    cd ~/$WSL_REPO_DIR
    bash scripts/install/linux/ubuntu-noble-24.04/install.sh
"

# ---- 7. Done ----
Write-Host ""
Write-Host "=============================="
Write-Host "  WSL2 Setup Complete"
Write-Host "=============================="
Write-Host "  Enter WSL:        wsl -d Ubuntu"
Write-Host "  Project path:     ~/$WSL_REPO_DIR"
Write-Host ""
Write-Host "  Run 'gh auth login' inside WSL for GitHub CLI access."
Write-Host ""
