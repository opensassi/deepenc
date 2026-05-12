#!/usr/bin/env bash
# profile-entry.sh — Run maximal perf capture against an encoder
#
# Usage:
#   bash .opencode/skills/asm-optimizer/scripts/profile-entry.sh \
#     --name <profile-name> \
#     --encoder <path-to-vvencapp> \
#     --source <yuv-path> \
#     [--preset fast|slow] [--frames 5|50] \
#     [--label custom-label]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKILL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT="$(cd "$SKILL_DIR/../../.." && pwd)"

# Defaults
PROFILE_NAME=""
ENCODER=""
SOURCE=""
PRESET="fast"
FRAMES=5
LABEL=""

# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    --name) PROFILE_NAME="$2"; shift 2 ;;
    --encoder) ENCODER="$2"; shift 2 ;;
    --source) SOURCE="$2"; shift 2 ;;
    --preset) PRESET="$2"; shift 2 ;;
    --frames) FRAMES="$2"; shift 2 ;;
    --label) LABEL="$2"; shift 2 ;;
    *) echo "Unknown: $1"; exit 1 ;;
  esac
done

# Validate
[ -n "$PROFILE_NAME" ] || { echo "ERROR: --name required"; exit 1; }
[ -n "$ENCODER" ] || { echo "ERROR: --encoder required"; exit 1; }
[ -x "$ENCODER" ] || { echo "ERROR: encoder not found or not executable: $ENCODER"; exit 1; }
[ -n "$SOURCE" ] || { echo "ERROR: --source required"; exit 1; }
[ -f "$SOURCE" ] || { echo "ERROR: source not found: $SOURCE"; exit 1; }

LABEL="${LABEL:-${PRESET}-${FRAMES}fr}"
OUTDIR="$PROJECT_ROOT/perf/baseline/profiles/$PROFILE_NAME/$LABEL"
mkdir -p "$OUTDIR"

ENCODER_NAME="$("$ENCODER" --version 2>&1 | head -1)"
ENCODE_CMD=("$ENCODER" --input "$SOURCE" --size 1280x720 \
            --frames "$FRAMES" --preset "$PRESET" --output /dev/null)

echo "=== Profile Entry ==="
echo "Name:    $PROFILE_NAME"
echo "Label:   $LABEL"
echo "Encoder: $ENCODER_NAME"
echo "Preset:  $PRESET"
echo "Frames:  $FRAMES"
echo "Output:  $OUTDIR"
echo ""

# Maximal perf counter set
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

# 1. Extended perf stat
echo "[1/4] perf stat (extended counters)..."
perf stat -e "$PERF_EVENTS" -- "${ENCODE_CMD[@]}" > "$OUTDIR/perf-stat.txt" 2>&1 || true

# 2. Top-down microarchitecture
echo "[2/4] perf top-down..."
perf stat --topdown -- "${ENCODE_CMD[@]}" > "$OUTDIR/perf-topdown.txt" 2>&1 || true

# 3. Flamegraph capture
echo "[3/4] perf record (flamegraph)..."
perf record --call-graph fp -e cycles -o "$OUTDIR/perf.data" \
  -- "${ENCODE_CMD[@]}" 2>&1 | tail -1

# 4. Hottest functions
echo "[4/4] perf report (hottest functions)..."
perf report -i "$OUTDIR/perf.data" --stdio --sort symbol \
  --no-call-graph --percent-limit 0.5 2>/dev/null \
  > "$OUTDIR/perf-hottest.txt" || true

# Flamegraph
if [ -f "$PROJECT_ROOT/scripts/FlameGraph/stackcollapse-perf.pl" ]; then
  echo "     generating flamegraph..."
  perf script -i "$OUTDIR/perf.data" 2>/dev/null | \
    "$PROJECT_ROOT/scripts/FlameGraph/stackcollapse-perf.pl" > "$OUTDIR/folded.txt" 2>/dev/null || true
  if [ -s "$OUTDIR/folded.txt" ] && [ -f "$PROJECT_ROOT/scripts/FlameGraph/flamegraph.pl" ]; then
    "$PROJECT_ROOT/scripts/FlameGraph/flamegraph.pl" "$OUTDIR/folded.txt" \
      > "$OUTDIR/flame.svg" 2>/dev/null || true
  fi
fi

# Meta
cat > "$OUTDIR/meta.json" <<METAEOF
{
  "profile_name": "$PROFILE_NAME",
  "label": "$LABEL",
  "preset": "$PRESET",
  "frames": $FRAMES,
  "encoder": "$ENCODER_NAME",
  "source": "$SOURCE",
  "timestamp": "$(date -Iseconds)"
}
METAEOF

echo ""
echo "Done. Profile saved to $OUTDIR"
echo "  perf-stat.txt  — extended hardware counters"
echo "  perf-topdown.txt — top-down microarchitecture"
echo "  perf-hottest.txt — hottest functions"
echo "  perf.data      — raw perf data"
echo "  flame.svg      — interactive flamegraph"
