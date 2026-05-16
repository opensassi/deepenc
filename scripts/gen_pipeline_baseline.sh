#!/usr/bin/env bash
#
# Extract one 1080p YUV frame from the test data.
# The reference bitstream is generated automatically by the first
# run of hw_pipeline_bench (self-referencing approach).
#
# Usage: bash scripts/gen_pipeline_baseline.sh
# Output: perf/traces/hw_pipeline_baseline/input.yuv
#
set -euo pipefail

TOP="$(cd "$(dirname "$0")/.." && pwd)"
OUTDIR="$TOP/perf/traces/hw_pipeline_baseline"
mkdir -p "$OUTDIR"

INPUT_Y4M="$TOP/test/data/park_joy_1080p.y4m"
INPUT_YUV="$OUTDIR/input.yuv"

[ -f "$INPUT_Y4M" ] || { echo "ERROR: $INPUT_Y4M not found"; exit 1; }

echo "=== Extracting first frame ==="
ffmpeg -y -i "$INPUT_Y4M" -frames:v 1 -f rawvideo \
  -pix_fmt yuv420p -s 1920x1080 "$INPUT_YUV" 2>/dev/null

FILESIZE=$(stat -c%s "$INPUT_YUV" 2>/dev/null || stat -f%z "$INPUT_YUV" 2>/dev/null)
[ "$FILESIZE" -eq $((1920*1080*3/2)) ] || { echo "ERROR: wrong size"; exit 1; }

echo "  input.yuv: $FILESIZE bytes OK"
echo ""
echo "Build hw_pipeline_bench with VVENC_ENABLE_HW_PREANALYSIS=ON, then run:"
echo "  ./bin/release-static/hw_pipeline_bench"
echo ""
echo "The first run creates the reference output.266 automatically."
echo "Subsequent runs verify bit-exactness against it."
