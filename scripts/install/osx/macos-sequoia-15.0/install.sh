#!/usr/bin/env bash
set -euo pipefail

# macOS — full development environment for deepenc
# Usage: bash scripts/install/osx/macos-sequoia-15.0/install.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# ---- Version detection ----
OS_NAME="macos"
OS_VERSION=$(sw_vers -productVersion 2>/dev/null || echo "0.0")

# Codename lookup (newest → oldest)
if [[ $OS_VERSION == 15.* ]]; then   OS_CODENAME="sequoia"
elif [[ $OS_VERSION == 14.* ]]; then OS_CODENAME="sonoma"
elif [[ $OS_VERSION == 13.* ]]; then OS_CODENAME="ventura"
elif [[ $OS_VERSION == 12.* ]]; then OS_CODENAME="monterey"
elif [[ $OS_VERSION == 11.* ]]; then OS_CODENAME="big-sur"
else                                 OS_CODENAME="unknown"
fi

echo "==> deepenc: installing for macOS ${OS_VERSION} (${OS_CODENAME})..."

# ---- 1. Xcode Command Line Tools ----
if ! xcode-select -p &>/dev/null; then
  echo "==> Installing Xcode Command Line Tools..."
  xcode-select --install
  echo "   Please complete the installation dialog, then re-run this script."
  exit 0
fi
echo "  ✓ Xcode Command Line Tools found"

# ---- 2. Homebrew ----
if ! command -v brew &>/dev/null; then
  echo "==> Installing Homebrew..."
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi
echo "  ✓ Homebrew found"

# ---- 3. Install packages ----
BREW_PACKAGES=(
  # Build toolchain
  cmake
  nasm
  git
  node

  # Code search & navigation
  ripgrep
  fd
  bat
  fzf

  # System monitoring
  htop
  duf

  # Performance & benchmarking
  hyperfine

  # Network tools
  httpie
  whois
  nmap
  mtr

  # Development utilities
  tmux
  parallel
  pv
  entr
  tree
  jq
  pigz
  wget

  # Debugging
  python
)

echo "==> Installing ${#BREW_PACKAGES[@]} Homebrew packages..."
brew install "${BREW_PACKAGES[@]}"

# ---- 4. Python + GDB MCP ----
echo "==> Installing gdb-mcp-server..."
pip3 install gdb-mcp-server

# ---- 5. npm ----
if [ -f "$REPO_ROOT/package.json" ]; then
  echo "==> Installing npm dependencies..."
  cd "$REPO_ROOT" && npm install
fi

# ---- 6. Verification ----
echo ""
echo "=============================="
echo "  Toolchain Verification"
echo "=============================="
printf "%-20s %s\n" "clang++"  "$(clang++ --version | head -1)"
printf "%-20s %s\n" "cmake"    "$(cmake --version | head -1)"
printf "%-20s %s\n" "nasm"     "$(nasm --version | head -1)"
printf "%-20s %s\n" "node"     "$(node --version)"
printf "%-20s %s\n" "lldb"     "$(lldb --version | head -1)"
printf "%-20s %s\n" "rg"       "$(rg --version | head -1)"
printf "%-20s %s\n" "gdb-mcp"  "$(pip3 show gdb-mcp-server 2>/dev/null | grep Version || echo 'not found')"
echo ""
echo "Installation complete."
echo ""
echo "Notes:"
echo "  - perf/strace/ltrace are Linux-specific and not available on macOS."
echo "  - Use Xcode Instruments.app for profiling."
echo "  - Use lldb for debugging (Xcode CLT provides it)."
echo "  - Run 'gh auth login' for GitHub CLI (issue management)."
echo ""
