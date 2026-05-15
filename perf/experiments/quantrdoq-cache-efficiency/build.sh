#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR/build"
RESULTS_DIR="$SCRIPT_DIR/results"

CXX=g++
CXXFLAGS="-std=gnu++14 -O3 -mavx2 -msse4.1 -DNDEBUG -I$SRC_DIR"
LDFLAGS="-lpthread"

mkdir -p "$BUILD_DIR" "$RESULTS_DIR"

echo "=== Building microbench ==="
$CXX $CXXFLAGS \
    "$SRC_DIR/microbench.cpp" \
    "$SRC_DIR/mode_baseline.cpp" \
    "$SRC_DIR/mode_subarray.cpp" \
    "$SRC_DIR/mode_earlyexit.cpp" \
    "$SRC_DIR/mode_template.cpp" \
    $LDFLAGS \
    -o "$BUILD_DIR/microbench"
echo "=== Build OK ==="

# Removed mem_load_uops_retired.l3_hit/l3_miss - these vary by CPU generation
PERF_EVENTS="cycles,instructions,"
PERF_EVENTS+="L1-dcache-loads,L1-dcache-load-misses,"
PERF_EVENTS+="LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,"
PERF_EVENTS+="cache-misses,cache-references,"
PERF_EVENTS+="stalled-cycles-frontend,stalled-cycles-backend"

TU_SIZES=(4 8 16 32 64)
MODES=(baseline subarray earlyexit t4x4 t8x8 t16x16 t32x32 t64x64)

TIMING_ITER=50000
PERF_ITER=3000

echo ""
echo "=== Phase 1: Wall-clock timing ($TIMING_ITER iters) ==="
# Header matches microbench output: mode,tu_size,tu_area,active_area,iterations,wall_ns,checksum,alloc_bytes
echo "mode,tu_size,tu_area,active_area,iterations,wall_ns,checksum,alloc_bytes" > "$RESULTS_DIR/timing.csv"
for mode in "${MODES[@]}"; do
    for tu in "${TU_SIZES[@]}"; do
        echo -n "  timing $mode ${tu}x${tu}... "
        "$BUILD_DIR/microbench" \
            --mode "$mode" --tu-size "$tu" --iterations "$TIMING_ITER" \
            2>/dev/null | tail -1 >> "$RESULTS_DIR/timing.csv"
        echo "done"
    done
done

echo ""
echo "=== Phase 2: Perf stat cache counters ($PERF_ITER iters) ==="
echo "mode,tu_size,tu_area,active_area,iterations,wall_ns,checksum,alloc_bytes" > "$RESULTS_DIR/perf.csv"
echo "mode,tu_size,perf_event,count" > "$RESULTS_DIR/perf_raw.csv"
for mode in "${MODES[@]}"; do
    for tu in "${TU_SIZES[@]}"; do
        echo -n "  perf $mode ${tu}x${tu}... "
        perf_stat_file="$RESULTS_DIR/${mode}_${tu}x${tu}_perf.txt"
        perf stat -e "$PERF_EVENTS" \
            taskset -c 0 \
            "$BUILD_DIR/microbench" \
                --mode "$mode" --tu-size "$tu" --iterations "$PERF_ITER" \
            2>"$perf_stat_file" > /dev/null || true

        # Parse each perf counter
        for event in cycles instructions L1-dcache-loads L1-dcache-load-misses LLC-loads LLC-load-misses LLC-stores LLC-store-misses cache-misses cache-references stalled-cycles-frontend stalled-cycles-backend; do
            count=$(grep "$event" "$perf_stat_file" | head -1 | awk '{print $1}' | sed 's/,//g' || echo "0")
            echo "$mode,$tu,$((tu*tu)),$((tu*tu)),$PERF_ITER,$event,$count" >> "$RESULTS_DIR/perf_raw.csv"
        done

        # Also record the microbench's own timing under perf
        wall_ns=$(grep "^$mode," "$perf_stat_file" 2>/dev/null || echo "")
        if [ -z "$wall_ns" ]; then
            # perf stat suppresses stdout, so re-run without perf for timing
            wall_ns=$("$BUILD_DIR/microbench" --mode "$mode" --tu-size "$tu" --iterations "$PERF_ITER" 2>/dev/null | tail -1 | awk -F, '{print $6}')
        fi
        echo "$mode,$tu,$((tu*tu)),$((tu*tu)),$PERF_ITER,$wall_ns,0,0" >> "$RESULTS_DIR/perf.csv"
        echo "done"
    done
done

echo ""
echo "=== Results ==="
echo ""
echo "--- Timing ---"
column -t -s, "$RESULTS_DIR/timing.csv"
echo ""
echo "--- Perf (aggregated) ---"
echo ""
# Derive ns/coeff from timing.csv for easier comparison
python3 -c "
import csv
rows = []
with open('$RESULTS_DIR/timing.csv') as f:
    reader = csv.DictReader(f)
    for row in reader:
        ns = int(row['wall_ns'])
        iters = int(row['iterations'])
        area = int(row['active_area'])
        ns_per_coeff = ns / (iters * area) if area > 0 else 0
        rows.append((row['mode'], int(row['tu_size']), area, iters, int(ns), ns_per_coeff, int(row['alloc_bytes'])))

# Print summary table
print(f'{\"Mode\":<20} {\"TU\":<6} {\"Area\":<6} {\"Iters\":<8} {\"Wall(s)\":<10} {\"ns/coeff\":<10} {\"Alloc(KB)\":<10}')
print('-' * 75)
for r in sorted(rows, key=lambda x: (x[0], x[1])):
    wall_s = r[4] / 1e9
    print(f'{r[0]:<20} {r[1]:<6} {r[2]:<6} {r[3]:<8} {wall_s:<10.3f} {r[5]:<10.1f} {r[6]/1024:<10.1f}')
" 2>&1 || echo "(python3 not available, showing raw)"
