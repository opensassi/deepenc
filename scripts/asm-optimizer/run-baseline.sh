#!/usr/bin/env bash
set -euo pipefail

# run-baseline.sh
# Build vvenc-v1.14.0 baseline and run the full profiling matrix.
#
# Usage: ./scripts/asm-optimizer/run-baseline.sh [--rebuild]
#   --rebuild  Force rebuild even if baseline binaries exist

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BASELINE_DIR="$REPO_ROOT/perf/baseline"
BASELINE_SRC="$BASELINE_DIR/vvenc-1.14.0"
BASELINE_BUILD="$BASELINE_DIR/build"
BASELINE_BIN="$BASELINE_SRC/bin/release-static"
PROFILES_DIR="$BASELINE_DIR/profiles/default"
REPORTS_DIR="$BASELINE_DIR/reports"
TEST_SEQ="$REPO_ROOT/test/data/park_joy_832x480f50.yuv"

# Output tree:
#   perf/baseline/
#   ├── vvenc-1.14.0/          ← git worktree checkout
#   ├── build/                  ← Release cmake build
#   ├── profiles/default/
#   │   ├── fast-5fr/
#   │   ├── fast-50fr/
#   │   ├── slow-5fr/
#   │   └── slow-50fr/
#   └── reports/
#       └── profile-summary.json

# Encoder parameters
WIDTH=832
HEIGHT=480
FPS=50
PRESETS=("fast" "slow")
FRAME_COUNTS=(5 50)
QP=22

# Perf counter groups
# Pass 1: comprehensive via -d -d -d (cycles, instr, cache, TLB, etc.)
# Pass 2: SIMD-specific counters
# Pass 3: top-down microarchitecture analysis
PERF_COMPREHENSIVE="-d -d -d"
PERF_SIMD="-e fp_arith_inst_retired.256b_packed_single,fp_arith_inst_retired.128b_packed_single,fp_arith_inst_retired.scalar_single"
PERF_MEM_UOP="-e mem_load_uops_retired.l1_hit,mem_load_uops_retired.l1_miss,mem_load_uops_retired.l2_hit,mem_load_uops_retired.l2_miss,mem_load_uops_retired.l3_hit,mem_load_uops_retired_misc.l3_miss"

run_perf_pass() {
    local label="$1"
    local preset="$2"
    local frames="$3"
    local events="$4"
    local suffix="$5"

    local outdir="$PROFILES_DIR/$label"
    mkdir -p "$outdir"
    local output="$outdir/perf-${suffix}.txt"
    local bitstream="$outdir/out.265"

    echo "  [perf:$suffix] $preset ${frames}fr ..."
    perf stat $events -o "$output" \
        "$BASELINE_BIN/vvencapp" \
        -i "$TEST_SEQ" -s "${WIDTH}x${HEIGHT}" -r "$FPS" \
        -f "$frames" \
        --preset "$preset" \
        -q "$QP" \
        -o "$bitstream" \
        2>/dev/null || true

    if [ -f "$output" ]; then
        sed -i "1i# RUN: preset=$preset frames=$frames qp=$QP seq=$(basename $TEST_SEQ) date=$(date -Iseconds)" "$output"
    fi
    rm -f "$bitstream"
}

run_timing_pass() {
    local label="$1"
    local preset="$2"
    local frames="$3"

    local outdir="$PROFILES_DIR/$label"
    mkdir -p "$outdir"
    local output="$outdir/timing.txt"
    local bitstream="$outdir/out.265"

    echo "  [timing] $preset ${frames}fr ..."
    for i in 1 2 3; do
        /usr/bin/time -f "real %e user %U sys %S" -a -o "$output" \
            "$BASELINE_BIN/vvencapp" \
            -i "$TEST_SEQ" -s "${WIDTH}x${HEIGHT}" -r "$FPS" \
            -f "$frames" \
            --preset "$preset" \
            -q "$QP" \
            -o "$bitstream" \
            2>/dev/null
        rm -f "$bitstream"
    done

    sed -i "1i# RUN: preset=$preset frames=$frames qp=$QP seq=$(basename $TEST_SEQ) date=$(date -Iseconds)" "$output"
}

# ----------------------------------------------------------------

step() { echo ""; echo "==> $1"; }

step "1. Create baseline directory structure"
mkdir -p "$BASELINE_DIR" "$PROFILES_DIR" "$REPORTS_DIR"

step "2. Checkout vvenc-v1.14.0 via git worktree"
if [ ! -d "$BASELINE_SRC" ] || [ ! -f "$BASELINE_SRC/CMakeLists.txt" ]; then
    git -C "$REPO_ROOT" worktree add -f "$BASELINE_SRC" v1.14.0
    echo "  checked out v1.14.0 at $BASELINE_SRC"
else
    echo "  vvenc-1.14.0 already exists"
fi

step "3. Build Release encoder"
if [ ! -f "$BASELINE_BIN/vvencapp" ] || [ "${1:-}" == "--rebuild" ]; then
    cmake -S "$BASELINE_SRC" -B "$BASELINE_BUILD" \
        -DCMAKE_BUILD_TYPE=Release \
        -DVVENC_ENABLE_X86_SIMD=ON \
        -DVVENC_ENABLE_LINK_TIME_OPT=ON \
        -DVVENC_LIBRARY_ONLY=OFF
    cmake --build "$BASELINE_BUILD" -j "$(nproc)" --target vvencapp
    echo "  build complete: $BASELINE_BIN/vvencapp"
else
    echo "  baseline binary already exists"
fi

# Verify binary
"$BASELINE_BIN/vvencapp" --version

step "4. Run profiling matrix"

for preset in "${PRESETS[@]}"; do
    for frames in "${FRAME_COUNTS[@]}"; do
        label="${preset}-${frames}fr"
        echo "--- Profiling $label ---"

        run_perf_pass "$label" "$preset" "$frames" "$PERF_COMPREHENSIVE" "comprehensive"
        run_perf_pass "$label" "$preset" "$frames" "$PERF_SIMD"          "simd"
        run_perf_pass "$label" "$preset" "$frames" "$PERF_MEM_UOP"       "mem-uop"
        run_timing_pass  "$label" "$preset" "$frames"
    done
done

step "5. Generate summary report"
cat > "$REPORTS_DIR/profile-summary.json" << 'REPORT_EOF'
{
  "baseline": "v1.14.0",
  "date": "REPLACE_DATE",
  "profiles": {
    "fast-5fr": {
      "preset": "fast", "frames": 5, "qp": 22,
      "sequence": "park_joy_832x480f50"
    },
    "fast-50fr": {
      "preset": "fast", "frames": 50, "qp": 22,
      "sequence": "park_joy_832x480f50"
    },
    "slow-5fr": {
      "preset": "slow", "frames": 5, "qp": 22,
      "sequence": "park_joy_832x480f50"
    },
    "slow-50fr": {
      "preset": "slow", "frames": 50, "qp": 22,
      "sequence": "park_joy_832x480f50"
    }
  },
  "counter_groups": [
    "comprehensive", "simd", "mem-uop"
  ]
}
REPORT_EOF

sed -i "s/REPLACE_DATE/$(date -Iseconds)/" "$REPORTS_DIR/profile-summary.json"

echo ""
echo "=========================================="
echo "Baseline setup complete!"
echo "  Source: $BASELINE_SRC"
echo "  Build:  $BASELINE_BUILD"
echo "  Binary: $BASELINE_BIN/vvencapp"
echo "  Profiles: $PROFILES_DIR/"
echo "  Report:  $REPORTS_DIR/profile-summary.json"
echo "=========================================="
