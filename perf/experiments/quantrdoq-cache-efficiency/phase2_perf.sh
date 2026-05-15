#!/usr/bin/env bash
# Phase 2: perf stat for all mode × TU size combinations (hybrid CPU fix)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULTS_DIR="$SCRIPT_DIR/results"
BUILD_DIR="$SCRIPT_DIR/build"

PERF_EVENTS="cycles,instructions,"
PERF_EVENTS+="L1-dcache-loads,L1-dcache-load-misses,"
PERF_EVENTS+="LLC-loads,LLC-load-misses,"
PERF_EVENTS+="cache-misses,cache-references"

TU_SIZES=(4 8 16 32 64)
MODES=(baseline subarray earlyexit t4x4 t8x8 t16x16 t32x32 t64x64)
PERF_ITER=5000

parse_event() {
    local event=$1
    local output="$2"
    # Hybrid CPUs report as cpu_core/<event>/ or cpu_atom/<event>/
    # Take the cpu_core count if available, else the non-prefixed count
    local val=$(echo "$output" | grep "cpu_core/$event/" | awk '{print $1}' | head -1 | sed 's/,//g')
    if [ -z "$val" ] || [[ "$val" =~ [^0-9] ]]; then
        val=$(echo "$output" | grep -E "[0-9,]+[[:space:]]+$event" | grep -v "cpu_" | awk '{print $1}' | head -1 | sed 's/,//g')
    fi
    val="${val//,/}"
    if [[ ! "$val" =~ ^[0-9]+$ ]]; then val=0; fi
    echo "$val"
}

echo "mode,tu_size,tu_area,active_area,iterations,cycles,instructions,ipc,l1_loads,l1_misses,l1_miss_rate,llc_loads,llc_misses,llc_miss_rate,cache_refs,cache_misses,cache_miss_rate,wall_s,ns_per_iter" \
    > "$RESULTS_DIR/perf_phase2.csv"

for mode in "${MODES[@]}"; do
    for tu in "${TU_SIZES[@]}"; do
        area=$((tu * tu))
        echo -n "perf $mode ${tu}x${tu}..."

        perf_out=$(perf stat -e "$PERF_EVENTS" \
            taskset -c 0 \
            "$BUILD_DIR/microbench" \
                --mode "$mode" --tu-size "$tu" --iterations "$PERF_ITER" \
            2>&1 > /dev/null)

        wall_s=$(echo "$perf_out" | grep "seconds time elapsed" | awk '{print $1}' | sed 's/,/./g')
        cycles=$(parse_event "cycles" "$perf_out")
        instr=$(parse_event "instructions" "$perf_out")
        l1_loads=$(parse_event "L1-dcache-loads" "$perf_out")
        l1_miss=$(parse_event "L1-dcache-load-misses" "$perf_out")
        llc_loads=$(parse_event "LLC-loads" "$perf_out")
        llc_miss=$(parse_event "LLC-load-misses" "$perf_out")
        cache_refs=$(parse_event "cache-references" "$perf_out")
        cache_miss=$(parse_event "cache-misses" "$perf_out")

        ipc=$(echo "scale=2; $instr / $cycles" | bc 2>/dev/null || echo "0")
        l1_rate=$(echo "scale=2; $l1_miss * 100 / $l1_loads" | bc 2>/dev/null || echo "0")
        llc_rate=$(echo "scale=2; $llc_miss * 100 / $llc_loads" | bc 2>/dev/null || echo "0")
        cache_rate=$(echo "scale=2; $cache_miss * 100 / $cache_refs" | bc 2>/dev/null || echo "0")
        ns_per_iter=$(echo "scale=2; $wall_s * 1000000000 / $PERF_ITER" | bc 2>/dev/null || echo "0")

        echo "$mode,$tu,$area,$area,$PERF_ITER,$cycles,$instr,$ipc,$l1_loads,$l1_miss,$l1_rate,$llc_loads,$llc_miss,$llc_rate,$cache_refs,$cache_miss,$cache_rate,$wall_s,$ns_per_iter" \
            >> "$RESULTS_DIR/perf_phase2.csv"

        echo "ipc=$ipc l1_miss=$l1_miss llc_miss=$llc_miss wall=${wall_s}s"
    done
done

echo ""
echo "=== Phase 2 Complete ==="
echo "Results: $RESULTS_DIR/perf_phase2.csv"
