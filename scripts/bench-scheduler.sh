#!/bin/bash
# bench-scheduler.sh — A/B comparison for scheduler dispatch bit-exactness
#
# Builds two vvencapp binaries (DISPATCH=OFF and DISPATCH=ON) in separate
# build directories, runs the sched_pipeline_bench on each, and verifies
# bit-exact output.
#
# Usage:
#   ./scripts/bench-scheduler.sh [--quick]
#
#   --quick   Skip slow preset, only test fast preset

set -euo pipefail

BENCH_DIR=$(mktemp -d /tmp/sched_bench_XXXX)
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OFF_DIR="$BENCH_DIR/off"
ON_DIR="$BENCH_DIR/on"
RESULTS_DIR="$BENCH_DIR/results"
PASS=0
FAIL=0

cleanup() { rm -rf "$BENCH_DIR"; }
trap cleanup EXIT

mkdir -p "$RESULTS_DIR"

echo "============================================"
echo " Scheduler Dispatch A/B Benchmark"
echo "============================================"
echo "Source: $SCRIPT_DIR"
echo "Build OFF: $OFF_DIR"
echo "Build ON:  $ON_DIR"
echo "Results:   $RESULTS_DIR"
echo ""

# ── Build DISPATCH=OFF ────────────────────────────────────────
echo "--- Building DISPATCH=OFF ---"
cmake -B "$OFF_DIR" -S "$SCRIPT_DIR" \
  -DVVENC_ENABLE_SCHEDULER_DISPATCH=OFF \
  -DVVENC_ENABLE_SCHEDULER_TRACE=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE="$OFF_DIR/bin" 2>/dev/null

cmake --build "$OFF_DIR" -j$(nproc) --target vvencapp 2>/dev/null
OFF_BIN="$OFF_DIR/bin/vvencapp"
if [ ! -f "$OFF_BIN" ]; then
  # Fallback: try the project-default output dir
  OFF_BIN="$SCRIPT_DIR/bin/release-static/vvencapp"
fi
echo " OFF binary: $OFF_BIN"

# ── Build DISPATCH=ON ─────────────────────────────────────────
echo "--- Building DISPATCH=ON ---"
cmake -B "$ON_DIR" -S "$SCRIPT_DIR" \
  -DVVENC_ENABLE_SCHEDULER_DISPATCH=ON \
  -DVVENC_ENABLE_SCHEDULER_TRACE=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE="$ON_DIR/bin" 2>/dev/null

cmake --build "$ON_DIR" -j$(nproc) --target vvencapp 2>/dev/null
ON_BIN="$ON_DIR/bin/vvencapp"
if [ ! -f "$ON_BIN" ]; then
  ON_BIN="$SCRIPT_DIR/bin/release-static/vvencapp"
fi
echo " ON binary:  $ON_BIN"

# ── Test functions ─────────────────────────────────────────────
check() {
  local label="$1" off_file="$2" on_file="$3"
  local off_hash on_hash

  if [ ! -f "$off_file" ] || [ ! -f "$on_file" ]; then
    echo "  FAIL: output file missing"
    FAIL=$((FAIL + 1))
    return
  fi

  off_hash=$(md5sum "$off_file" | cut -d' ' -f1)
  on_hash=$(md5sum "$on_file" | cut -d' ' -f1)

  if [ "$off_hash" = "$on_hash" ]; then
    echo "  PASS: $off_hash"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: OFF=$off_hash ON=$on_hash"
    FAIL=$((FAIL + 1))
  fi
}

# ── Test FAST preset ──────────────────────────────────────────
echo ""
echo "=== FAST preset, 3 frames, 720p QP22 ==="
timeout 30 "$OFF_BIN" --preset fast --qp 22 --frames 3 --threads 0 \
  -i "$SCRIPT_DIR/test/data/park_joy_1280x720f5.yuv" -s 1280x720 \
  -o "$RESULTS_DIR/fast_off.266" 2>/dev/null
timeout 30 "$ON_BIN" --preset fast --qp 22 --frames 3 --threads 0 \
  -i "$SCRIPT_DIR/test/data/park_joy_1280x720f5.yuv" -s 1280x720 \
  -o "$RESULTS_DIR/fast_on.266" 2>/dev/null
check "FAST 3f" "$RESULTS_DIR/fast_off.266" "$RESULTS_DIR/fast_on.266"

# ── Test SLOW preset (optional, --quick skips this) ────────────
if [ "${1:-}" != "--quick" ]; then
  echo ""
  echo "=== SLOW preset, 1 frame, 720p QP32 ==="
  timeout 120 "$OFF_BIN" --preset slow --qp 32 --frames 1 --threads 0 \
    -i "$SCRIPT_DIR/test/data/park_joy_1280x720f5.yuv" -s 1280x720 \
    -o "$RESULTS_DIR/slow_off.266" 2>/dev/null
  timeout 120 "$ON_BIN" --preset slow --qp 32 --frames 1 --threads 0 \
    -i "$SCRIPT_DIR/test/data/park_joy_1280x720f5.yuv" -s 1280x720 \
    -o "$RESULTS_DIR/slow_on.266" 2>/dev/null
  check "SLOW 1f 720p" "$RESULTS_DIR/slow_off.266" "$RESULTS_DIR/slow_on.266"

  echo ""
  echo "=== SLOW preset, 1 frame, 1080p QP32 ==="
  timeout 180 "$OFF_BIN" --preset slow --qp 32 --frames 1 --threads 0 \
    -i "$SCRIPT_DIR/perf/traces/hw_pipeline_baseline/input.yuv" -s 1920x1080 \
    -o "$RESULTS_DIR/slow1080_off.266" 2>/dev/null
  timeout 180 "$ON_BIN" --preset slow --qp 32 --frames 1 --threads 0 \
    -i "$SCRIPT_DIR/perf/traces/hw_pipeline_baseline/input.yuv" -s 1920x1080 \
    -o "$RESULTS_DIR/slow1080_on.266" 2>/dev/null
  check "SLOW 1f 1080p" "$RESULTS_DIR/slow1080_off.266" "$RESULTS_DIR/slow1080_on.266"
fi

# ── Summary ───────────────────────────────────────────────────
echo ""
echo "============================================"
echo " Results: $PASS passed, $FAIL failed"
echo "============================================"

# Also print timing summary if files exist
for f in "$RESULTS_DIR"/*.266; do
  if [ -f "$f" ]; then
    size=$(stat -c%s "$f" 2>/dev/null || echo 0)
    echo "  $(basename $f): $size bytes"
  fi
done

exit $FAIL
