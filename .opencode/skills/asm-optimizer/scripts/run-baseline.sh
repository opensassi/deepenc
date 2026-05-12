#!/usr/bin/env bash
# run-baseline.sh — Build baseline vvenc-1.14.0 and run full profiling matrix
#
# Creates:
#   perf/baseline/vvenc-1.14.0/          ← release tag build
#   perf/baseline/profiles/<name>/        ← perf counter dumps
#   perf/baseline/reports/                ← aggregated summaries
#
# Usage: bash .opencode/skills/asm-optimizer/scripts/run-baseline.sh [--profile-name default]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKILL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT="$(cd "$SKILL_DIR/../../.." && pwd)"

PROFILE_NAME="${1:-default}"
BASELINE_DIR="$PROJECT_ROOT/perf/baseline"
PROFILES_DIR="$BASELINE_DIR/profiles/$PROFILE_NAME"
REPORTS_DIR="$BASELINE_DIR/reports"
VERSION_DIR="$BASELINE_DIR/vvenc-1.14.0"
PARK_JOY="$PROJECT_ROOT/test/data/park_joy_1280x720f50.yuv"

# ---- Configuration ----
PRESETS=("fast" "slow")
FRAME_COUNTS=(5 50)

# Maximal perf event set
PERF_EVENTS="cycles,instructions,branches,branch-misses,"
PERF_EVENTS+="cache-references,cache-misses,"
PERF_EVENTS+="L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,"
PERF_EVENTS+="L1-icache-loads,L1-icache-load-misses,"
PERF_EVENTS+="LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,"
PERF_EVENTS+="dTLB-loads,dTLB-load-misses,dTLB-stores,dTLB-store-misses,"
PERF_EVENTS+="iTLB-loads,iTLB-load-misses,"
PERF_EVENTS+="node-loads,node-load-misses,node-stores,node-store-misses,"
PERF_EVENTS+="alignment-faults,"
PERF_EVENTS+="context-switches,cpu-migrations,page-faults,"
PERF_EVENTS+="stalled-cycles-frontend,stalled-cycles-backend"

# Check prerequisites
command -v perf >/dev/null 2>&1 || { echo "ERROR: perf not found"; exit 1; }
command -v git >/dev/null 2>&1 || { echo "ERROR: git not found"; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake not found"; exit 1; }

echo "=== asm-optimizer: Baseline Setup ==="
echo "Profile name:  $PROFILE_NAME"
echo "Baseline dir:  $BASELINE_DIR"
echo "Encoder:       $VERSION_DIR"
echo "Test clip:     $PARK_JOY"
echo ""

# ---- Step 1: Create directory structure ----
mkdir -p "$PROFILES_DIR"
mkdir -p "$REPORTS_DIR"
mkdir -p "$BASELINE_DIR"

# ---- Step 2: Clone baseline encoder ----
if [ ! -d "$VERSION_DIR/.git" ]; then
  echo "[1/5] Cloning vvenc-1.14.0..."
  git clone --branch vvenc-1.14.0 --depth 1 \
    https://github.com/fraunhoferhhi/vvenc.git \
    "$VERSION_DIR" 2>&1 | tail -3
else
  echo "[1/5] vvenc-1.14.0 already cloned"
fi

# ---- Step 3: Build baseline encoder ----
BASELINE_BIN="$VERSION_DIR/bin/release-static/vvencapp"
if [ ! -x "$BASELINE_BIN" ]; then
  echo "[2/5] Building vvenc-1.14.0 Release..."
  cmake -B "$VERSION_DIR/build" -S "$VERSION_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DVVENC_ENABLE_LINK_TIME_OPT=OFF \
    -DVVENC_ENABLE_ML_LIGHTGBM=OFF 2>&1 | tail -3
  cmake --build "$VERSION_DIR/build" -j"$(nproc)" --target vvencapp 2>&1 | tail -5
else
  echo "[2/5] Baseline encoder already built: $BASELINE_BIN"
fi

# Verify
"$BASELINE_BIN" --version 2>&1 | head -1

# ---- Step 4: Run profiling matrix ----
echo "[3/5] Running profiling matrix..."

for preset in "${PRESETS[@]}"; do
  for frames in "${FRAME_COUNTS[@]}"; do
    label="${preset}-${frames}fr"
    profile_dir="$PROFILES_DIR/$label"
    mkdir -p "$profile_dir"

    echo ""
    echo "--- Profiling: $label ---"

    # Determine source: use 50fr file, cap at requested frames
    if [ "$frames" -eq 5 ]; then
      SRC="$PROJECT_ROOT/test/data/park_joy_1280x720f5.yuv"
    else
      SRC="$PARK_JOY"
    fi

    ENCODE_CMD=("$BASELINE_BIN" --input "$SRC" --size 1280x720 \
                --frames "$frames" --preset "$preset" --output /dev/null)

    # Extended perf stat
    echo "  perf stat (maximal counters)..."
    perf stat -e "$PERF_EVENTS" \
      -- "${ENCODE_CMD[@]}" > "$profile_dir/perf-stat.txt" 2>&1 || true

    # Top-down microarchitecture
    echo "  perf top-down..."
    perf stat --topdown \
      -- "${ENCODE_CMD[@]}" > "$profile_dir/perf-topdown.txt" 2>&1 || true

    # Flamegraph capture
    echo "  perf record (flamegraph)..."
    perf record --call-graph fp -e cycles -o "$profile_dir/perf.data" \
      -- "${ENCODE_CMD[@]}" 2>&1 | tail -1

    # Hottest functions
    echo "  perf report (hottest functions)..."
    perf report -i "$profile_dir/perf.data" --stdio --sort symbol \
      --no-call-graph --percent-limit 0.5 2>/dev/null \
      > "$profile_dir/perf-hottest.txt" || touch "$profile_dir/perf-hottest.txt"

    # Folded stack for flamegraph
    if [ -f "$PROJECT_ROOT/scripts/FlameGraph/stackcollapse-perf.pl" ]; then
      echo "  generating flamegraph..."
      perf script -i "$profile_dir/perf.data" 2>/dev/null | \
        "$PROJECT_ROOT/scripts/FlameGraph/stackcollapse-perf.pl" > "$profile_dir/folded.txt" 2>/dev/null || true
      if [ -s "$profile_dir/folded.txt" ] && [ -f "$PROJECT_ROOT/scripts/FlameGraph/flamegraph.pl" ]; then
        "$PROJECT_ROOT/scripts/FlameGraph/flamegraph.pl" "$profile_dir/folded.txt" \
          > "$profile_dir/flame.svg" 2>/dev/null || true
      fi
    fi

    # Meta info
    cat > "$profile_dir/meta.json" <<METAEOF
{
  "profile_name": "$PROFILE_NAME",
  "label": "$label",
  "preset": "$preset",
  "frames": $frames,
  "encoder": "vvenc-1.14.0",
  "source": "$SRC",
  "timestamp": "$(date -Iseconds)",
  "encoder_version": "$("$BASELINE_BIN" --version 2>&1 | head -1)"
}
METAEOF

    echo "  -> $profile_dir"
  done
done

# ---- Step 5: Generate aggregate summary ----
echo "[4/5] Generating aggregate report..."

SUMMARY_FILE="$REPORTS_DIR/profile-summary.json"
echo '{' > "$SUMMARY_FILE"
echo '  "profile_name": "'"$PROFILE_NAME"'",' >> "$SUMMARY_FILE"
echo '  "created": "'"$(date -Iseconds)"'",' >> "$SUMMARY_FILE"
echo '  "profiles": [' >> "$SUMMARY_FILE"

first=true
for preset in "${PRESETS[@]}"; do
  for frames in "${FRAME_COUNTS[@]}"; do
    $first || echo ',' >> "$SUMMARY_FILE"
    first=false
    label="${preset}-${frames}fr"
    stat_file="$PROFILES_DIR/$label/perf-stat.txt"

    fps=$(grep -oP 'fps=\s*\K[0-9.]+' "$PROFILES_DIR/$label/perf-stat.txt" 2>/dev/null || echo "0")
    user_time=$(grep -oP '[\d.]+ seconds user' "$stat_file" 2>/dev/null | awk '{print $1}' || echo "0")

    cat >> "$SUMMARY_FILE" <<ENTRY
    {
      "label": "$label",
      "preset": "$preset",
      "frames": $frames,
      "fps": $fps,
      "user_time_sec": $user_time,
      "dir": "profiles/$PROFILE_NAME/$label"
    }
ENTRY
  done
done

echo '  ]' >> "$SUMMARY_FILE"
echo '}' >> "$SUMMARY_FILE"

echo ""
echo "[5/5] Done!"
echo "  Baseline encoder: $BASELINE_BIN"
echo "  Profiles:         $PROFILES_DIR"
echo "  Summary:          $SUMMARY_FILE"
echo ""
echo "Next step: run 'profile <name>' or 'assess <entry>' to analyze."
