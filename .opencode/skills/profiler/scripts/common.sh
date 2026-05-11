#!/usr/bin/env bash
# Common configuration and paths for the profiler skill.
# Source this from other scripts: source "$(dirname "$0")/common.sh"

set -euo pipefail

# --- Paths ---
PROJECT_ROOT="$(cd "$(dirname "$0")/../../../../" && pwd)"
SCRIPTS_DIR="$PROJECT_ROOT/.opencode/skills/profiler/scripts"
OUTPUT_DIR="$PROJECT_ROOT/.profiler"
DATA_DIR="$PROJECT_ROOT/test/data"
FLAMEGRAPH_DIR="$PROJECT_ROOT/scripts/FlameGraph"

# --- Default workload ---
DEFAULT_SOURCE="park_joy"
DEFAULT_RESOLUTION="1920x1080"
DEFAULT_FRAMES=50
DEFAULT_PRESET="medium"

# --- Benchmark defaults ---
DEFAULT_ITERATIONS=5
DEFAULT_METRICS="psnr"

# --- Regression thresholds ---
THRESHOLD_TIME_PCT=2.0        # time increase > 2% flags warning
THRESHOLD_PSNR_Y_DB=0.1       # PSNR Y drop > 0.1 dB flags warning
THRESHOLD_BITRATE_PCT=5.0     # bitrate increase > 5% flags warning

# --- perf defaults ---
PERF_EVENTS_DEFAULT="cycles,cache-misses,branch-misses"

# --- Encoder binary discovery ---
find_encoder() {
    local paths=(
        "$PROJECT_ROOT/bin/release-static/vvencapp"
        "$PROJECT_ROOT/bin/vvencapp"
        "$PROJECT_ROOT/build/bin/vvencapp"
        "$PROJECT_ROOT/build/release/bin/vvencapp"
    )
    for p in "${paths[@]}"; do
        if [[ -x "$p" ]]; then
            echo "$p"
            return 0
        fi
    done
    echo ""
    return 1
}

# --- Timestamp ---
timestamp() {
    date +%Y%m%dT%H%M%S
}

# --- Logging ---
log_info()  { echo "[INFO]  $*"; }
log_warn()  { echo "[WARN]  $*" >&2; }
log_error() { echo "[ERROR] $*" >&2; }
