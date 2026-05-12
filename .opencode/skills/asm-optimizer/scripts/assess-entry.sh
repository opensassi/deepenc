#!/usr/bin/env bash
# assess-entry.sh — Evaluate one dispatch table entry for ASM potential
#
# Reads from:
#   - Primitives.spec.md §3 (dispatch table catalog)
#   - Baseline profile (perf stat, hottest functions, top-down)
#   - x265-reference.md for cross-reference
#
# Usage:
#   bash .opencode/skills/asm-optimizer/scripts/assess-entry.sh \
#     --entry g_vvenc.dq.updateStates \
#     [--baseline-profile default] [--baseline-label slow-50fr]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKILL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT="$(cd "$SKILL_DIR/../../.." && pwd)"

ENTRY=""
BASELINE_PROFILE="default"
BASELINE_LABEL="slow-50fr"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --entry) ENTRY="$1"; shift 1 ;;  # Handled below
    --entry) ENTRY="$2"; shift 2 ;;
    --baseline-profile) BASELINE_PROFILE="$2"; shift 2 ;;
    --baseline-label) BASELINE_LABEL="$2"; shift 2 ;;
    *) echo "Unknown: $1"; exit 1 ;;
  esac
done

[ -n "$ENTRY" ] || { echo "ERROR: --entry required (e.g. g_vvenc.dq.updateStates)"; exit 1; }

PROFILE_DIR="$PROJECT_ROOT/perf/baseline/profiles/$BASELINE_PROFILE/$BASELINE_LABEL"

echo "=== Assessment: $ENTRY ==="
echo "Baseline profile: $BASELINE_PROFILE/$BASELINE_LABEL"
echo ""

# ---- 1. Extract perf counters ----
STAT_FILE="$PROFILE_DIR/perf-stat.txt"
if [ -f "$STAT_FILE" ]; then
  cycles=$(grep -oP '[\d,]+(?=\s+cycles\s+)' "$STAT_FILE" 2>/dev/null | tr -d ',' | head -1)
  instr=$(grep -oP '[\d,]+(?=\s+instructions\s+)' "$STAT_FILE" 2>/dev/null | tr -d ',' | head -1)
  llc_misses=$(grep -oP '[\d,]+(?=\s+LLC-load-misses\s+)' "$STAT_FILE" 2>/dev/null | tr -d ',' | head -1)
  llc_loads=$(grep -oP '[\d,]+(?=\s+LLC-loads\s+)' "$STAT_FILE" 2>/dev/null | tr -d ',' | head -1)
  br_misses=$(grep -oP '[\d,]+(?=\s+branch-misses\s+)' "$STAT_FILE" 2>/dev/null | tr -d ',' | head -1)
  br_total=$(grep -oP '[\d,]+(?=\s+branches\s+)' "$STAT_FILE" 2>/dev/null | tr -d ',' | head -1)
  l1_misses=$(grep -oP '[\d,]+(?=\s+L1-dcache-load-misses\s+)' "$STAT_FILE" 2>/dev/null | tr -d ',' | head -1)
  l1_loads=$(grep -oP '[\d,]+(?=\s+L1-dcache-loads\s+)' "$STAT_FILE" 2>/dev/null | tr -d ',' | head -1)

  # IPC
  if [ -n "$cycles" ] && [ -n "$instr" ] && [ "$cycles" -gt 0 ]; then
    ipc=$(echo "scale=3; $instr / $cycles" | bc 2>/dev/null || echo "N/A")
  else
    ipc="N/A"
  fi

  # LLC miss rate
  if [ -n "$llc_loads" ] && [ -n "$llc_misses" ] && [ "$llc_loads" -gt 0 ]; then
    llc_miss_pct=$(echo "scale=2; 100 * $llc_misses / $llc_loads" | bc 2>/dev/null || echo "N/A")
  else
    llc_miss_pct="N/A"
  fi

  # Branch mispredict rate
  if [ -n "$br_total" ] && [ -n "$br_misses" ] && [ "$br_total" -gt 0 ]; then
    br_miss_pct=$(echo "scale=2; 100 * $br_misses / $br_total" | bc 2>/dev/null || echo "N/A")
  else
    br_miss_pct="N/A"
  fi

  # L1 miss rate
  if [ -n "$l1_loads" ] && [ -n "$l1_misses" ] && [ "$l1_loads" -gt 0 ]; then
    l1_miss_pct=$(echo "scale=2; 100 * $l1_misses / $l1_loads" | bc 2>/dev/null || echo "N/A")
  else
    l1_miss_pct="N/A"
  fi

  echo "--- Perf Counters ---"
  echo "IPC:                $ipc"
  echo "L1 DCache miss:     ${l1_miss_pct}%"
  echo "LLC miss:           ${llc_miss_pct}%"
  echo "Branch mispredict:  ${br_miss_pct}%"
else
  echo "--- Perf Counters ---"
  echo "No baseline profile found at $PROFILE_DIR"
  echo "Run 'setup-baseline' or 'profile' first."
fi

# ---- 2. Top-down ----
TOPDOWN_FILE="$PROFILE_DIR/perf-topdown.txt"
if [ -f "$TOPDOWN_FILE" ]; then
  frontend_bound=$(grep -oP 'tma_frontend_bound\s+\K[\d.]+' "$TOPDOWN_FILE" 2>/dev/null | head -1)
  backend_bound=$(grep -oP 'tma_backend_bound\s+\K[\d.]+' "$TOPDOWN_FILE" 2>/dev/null | head -1)
  bad_spec=$(grep -oP 'tma_bad_speculation\s+\K[\d.]+' "$TOPDOWN_FILE" 2>/dev/null | head -1)
  retiring=$(grep -oP 'tma_retiring\s+\K[\d.]+' "$TOPDOWN_FILE" 2>/dev/null | head -1)

  echo ""
  echo "--- Top-Down Microarchitecture ---"
  [ -n "$retiring" ] && echo "Retiring:        ${retiring}%"
  [ -n "$frontend_bound" ] && echo "Frontend Bound:  ${frontend_bound}%"
  [ -n "$backend_bound" ] && echo "Backend Bound:   ${backend_bound}%"
  [ -n "$bad_spec" ] && echo "Bad Speculation: ${bad_spec}%"
fi

# ---- 3. Hottest functions ----
HOT_FILE="$PROFILE_DIR/perf-hottest.txt"
if [ -f "$HOT_FILE" ]; then
  entry_share=$(grep -F "$ENTRY" "$HOT_FILE" 2>/dev/null | awk '{print $1}' | head -1)
  echo ""
  echo "--- Dispatch Table Entry in Hotspots ---"
  [ -n "$entry_share" ] && echo "Perf share: ${entry_share}%" || echo "Perf share: not in top hotspots"
fi

# ---- 4. x265 cross-reference ----
X265_REF="$PROJECT_ROOT/external/x265-reference.md"
if [ -f "$X265_REF" ]; then
  # Extract module name from entry path (g_vvenc.dq.updateStates -> dq)
  module=$(echo "$ENTRY" | cut -d. -f2)
  # Find the relevant section in the reference doc
  x265_match=$(grep -i -B2 -A2 "$module" "$X265_REF" 2>/dev/null | head -10 || true)
  echo ""
  echo "--- x265 Cross-Reference ---"
  if [ -n "$x265_match" ]; then
    echo "$x265_match"
  else
    echo "No direct x265 mapping found for module '$module'"
    echo "See: $X265_REF"
  fi
fi

# ---- 5. Optimization Potential Score ----
echo ""
echo "--- Optimization Potential Assessment ---"
score=0
max_score=9

[ "$(echo "$ipc < 1.5" | bc 2>/dev/null)" = "1" ] && score=$((score + 2)) && echo "  +2 IPC < 1.5"
[ "$(echo "${llc_miss_pct:-0} > 5" | bc 2>/dev/null)" = "1" ] && score=$((score + 2)) && echo "  +2 LLC miss > 5%"
[ "$(echo "${br_miss_pct:-0} > 2" | bc 2>/dev/null)" = "1" ] && score=$((score + 1)) && echo "  +1 Branch mispredict > 2%"
[ "$(echo "${frontend_bound:-0} > 15" | bc 2>/dev/null)" = "1" ] && score=$((score + 2)) && echo "  +2 Frontend bound > 15%"
[ "$(echo "${backend_bound:-0} > 20" | bc 2>/dev/null)" = "1" ] && score=$((score + 1)) && echo "  +1 Backend bound > 20%"
[ -n "$entry_share" ] && score=$((score + 1)) && echo "  +1 In hotspot list"

echo ""
case $score in
  0-2) echo "Score: $score/$max_score — Priority: Low" ;;
  3-4) echo "Score: $score/$max_score — Priority: Medium" ;;
  5-6) echo "Score: $score/$max_score — Priority: High" ;;
  *)   echo "Score: $score/$max_score — Priority: Critical" ;;
esac
