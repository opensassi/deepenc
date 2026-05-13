#!/usr/bin/env bash
set -euo pipefail

# Ubuntu 24.04.2 LTS (noble) — full development environment for deepenc
# Usage: bash scripts/install/linux/ubuntu-noble-24.04/install.sh
UBUNTU_CODENAME="$(lsb_release -c -s 2>/dev/null || echo 'noble')"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

PACKAGES=(
  # ---- Build toolchain ----
  build-essential
  cmake
  nasm
  git

  # ---- Artifact pipeline ----
  nodejs
  npm

  # ---- Code search & navigation ----
  ripgrep
  fd-find
  bat
  fzf

  # ---- System monitoring ----
  htop
  duf

  # ---- Performance & benchmarking ----
  hyperfine
  linux-tools-common
  linux-tools-generic

  # ---- Network tools ----
  httpie
  whois
  net-tools
  dnsutils
  traceroute
  nmap
  mtr

  # ---- Development utilities ----
  tmux
  parallel
  pv
  entr
  tree
  strace
  ltrace
  jq
  zip
  unzip
  pigz
  rsync
  curl
  wget
  ssh

  # ---- Debugging ----
  gdb
  python3-pip
)

echo "==> deepenc: installing ${#PACKAGES[@]} packages for Ubuntu $UBUNTU_CODENAME..."

sudo apt update
sudo apt install -y "${PACKAGES[@]}"

echo "==> Installing gdb-mcp-server (structured GDB interface for agents)..."
pip3 install gdb-mcp-server

if [ -f "$REPO_ROOT/package.json" ]; then
  echo "==> Installing npm dependencies..."
  cd "$REPO_ROOT" && npm install
fi

echo ""
echo "=============================="
echo "  Toolchain Verification"
echo "=============================="
printf "%-20s %s\n" "g++"      "$(g++ --version | head -1)"
printf "%-20s %s\n" "cmake"    "$(cmake --version | head -1)"
printf "%-20s %s\n" "nasm"     "$(nasm --version | head -1)"
printf "%-20s %s\n" "node"     "$(node --version)"
printf "%-20s %s\n" "perf"     "$(perf --version | head -1)"
printf "%-20s %s\n" "gdb"      "$(gdb --version | head -1)"
printf "%-20s %s\n" "rg"       "$(rg --version | head -1)"
printf "%-20s %s\n" "gdb-mcp"  "$(pip3 show gdb-mcp-server 2>/dev/null | grep Version || echo 'not found')"
echo ""
echo "Installation complete."
echo "Run 'gh auth login' for GitHub CLI (issue management)."
