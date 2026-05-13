#!/usr/bin/env bash
set -euo pipefail

# deepenc environment installer — OS detection and dispatch
# Usage: bash scripts/install.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OS="$(uname -s)"

detect_macos_codename() {
  local ver="$1"
  if [[ $ver == 15.* ]]; then   echo "sequoia"
  elif [[ $ver == 14.* ]]; then echo "sonoma"
  elif [[ $ver == 13.* ]]; then echo "ventura"
  elif [[ $ver == 12.* ]]; then echo "monterey"
  elif [[ $ver == 11.* ]]; then echo "big-sur"
  else                         echo "unknown"
  fi
}

case "$OS" in
  Darwin)
    VERSION=$(sw_vers -productVersion)
    CODENAME=$(detect_macos_codename "$VERSION")
    DIR="macos-${CODENAME}-${VERSION}"
    INSTALLER="$SCRIPT_DIR/install/osx/$DIR/install.sh"

    if [ -f "$INSTALLER" ]; then
      echo "==> deepenc: detected macOS ${VERSION} (${CODENAME})"
      bash "$INSTALLER"
    else
      echo "==> deepenc: no installer for macOS ${VERSION} (${CODENAME})"
      echo "   Expected: $INSTALLER"
      echo "   Try running with a supported macOS version (11.0 or later)."
      exit 1
    fi
    ;;

  Linux)
    # Check for WSL (Windows Subsystem for Linux)
    if grep -qi "microsoft\|wsl" /proc/version 2>/dev/null; then
      echo "==> deepenc: WSL detected"
      echo "   Run the Windows installer from PowerShell as Administrator:"
      echo ""
      echo "     powershell -ExecutionPolicy Bypass -File scripts\\install.ps1"
      echo ""
      echo "   If scripts/install.ps1 is not available, run the WSL2 installer directly:"
      echo "     powershell -ExecutionPolicy Bypass -File scripts\\install\\windows\\wsl2\\install.ps1"
      echo ""
      exit 0
    fi

    # Read distro info
    if [ ! -f /etc/os-release ]; then
      echo "==> deepenc: /etc/os-release not found — cannot detect Linux distro"
      exit 1
    fi

    source /etc/os-release
    NAME_LOWER=$(echo "${NAME%% *}" | tr '[:upper:]' '[:lower:]')
    CODENAME="${VERSION_CODENAME:-$UBUNTU_CODENAME}"
    VERSION="${VERSION_ID}"
    DIR="${NAME_LOWER}-${CODENAME}-${VERSION}"
    INSTALLER="$SCRIPT_DIR/install/linux/$DIR/install.sh"

    if [ -f "$INSTALLER" ]; then
      echo "==> deepenc: detected ${NAME} ${VERSION} (${CODENAME})"
      bash "$INSTALLER"
    else
      echo "==> deepenc: no installer for ${NAME} ${VERSION} (${CODENAME})"
      echo "   Expected: $INSTALLER"
      echo "   Supported distributions are listed under scripts/install/linux/"
      exit 1
    fi
    ;;

  *)
    echo "==> deepenc: unsupported operating system: $OS"
    echo "   Supported: macOS, Linux (Ubuntu, Debian, Fedora, etc.)"
    echo "   Windows: use WSL2 + the PowerShell installer"
    exit 1
    ;;
esac
