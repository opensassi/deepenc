#!/usr/bin/env bash
# Benchmark: run N iterations of encoder encode with quality metrics.
# Usage: ./benchmark.sh [--iter N] [--res WxH] [--preset NAME] [--frames N] [--psnr] [--ssim] [--vmaf]

source "$(dirname "$0")/common.sh"

# --- Parse args ---
ITERATIONS=$DEFAULT_ITERATIONS
RESOLUTION=$DEFAULT_RESOLUTION
PRESET=$DEFAULT_PRESET
FRAMES=$DEFAULT_FRAMES
DO_PSNR=false
DO_SSIM=false
DO_VMAF=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --iter)   shift; ITERATIONS="$1" ;;
        --res)    shift; RESOLUTION="$1" ;;
        --preset) shift; PRESET="$1" ;;
        --frames) shift; FRAMES="$1" ;;
        --psnr)   DO_PSNR=true ;;
        --ssim)   DO_SSIM=true ;;
        --vmaf)   DO_VMAF=true ;;
        *) log_error "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

# Default to --psnr if no metric flag given
$DO_PSNR || $DO_SSIM || $DO_VMAF || DO_PSNR=true

# --- Determine source file ---
W="${RESOLUTION%%x*}"; H="${RESOLUTION##*x}"
SOURCE="$DATA_DIR/park_joy_${RESOLUTION}f${FRAMES}.yuv"
LABEL="park_joy_${RESOLUTION}f${FRAMES}_${PRESET}"

if [[ ! -f "$SOURCE" ]]; then
    log_error "Source not found: $SOURCE. Run setup.sh first."
    exit 1
fi

ENCODER="$(find_encoder)"
if [[ -z "$ENCODER" ]]; then
    log_error "Encoder binary not found."
    exit 1
fi

# --- Metrics flags ---
# PSNR is always printed in vvencapp stats by default; no flag needed.
# SSIM may be available via config file; no CLI flag for it in vvencapp.
METRICS_FLAGS=()

# --- VMAF check ---
if $DO_VMAF; then
    if ! command -v ffmpeg &>/dev/null; then
        log_error "--vmaf requires ffmpeg (with libvvenc support). Not found."
        exit 1
    fi
    if ! command -v vmaf &>/dev/null; then
        log_error "--vmaf requires vmaf tool. Not found."
        exit 1
    fi
fi

# --- Iteration loop ---
RESULTS_FILE="$OUTPUT_DIR/benchmarks/benchmark-$(timestamp).json"
BITSTREAM="/tmp/park_joy_bench.266"

declare -a ITER_DATA
for ((i=1; i<=ITERATIONS; i++)); do
    log_info "Iteration $i / $ITERATIONS ..."

    START_MS=$(date +%s%3N)
    "$ENCODER" --input "$SOURCE" --size "${W}x${H}" --frames "$FRAMES" --preset "$PRESET" --output "$BITSTREAM" 2>&1 | tee "$OUTPUT_DIR/benchmarks/iter_${i}.log"
    END_MS=$(date +%s%3N)
    WALL_MS=$((END_MS - START_MS))
    FPS=$(echo "scale=3; $FRAMES * 1000 / $WALL_MS" | bc 2>/dev/null || echo "0")

    # Parse encoder output for metrics
    # Parse encoder output for metrics (vvencapp stats line format)
    PSNR_Y=$(grep -oP 'Y-PSNR\s+\K[\d.]+' "$OUTPUT_DIR/benchmarks/iter_${i}.log" | tail -1 || echo "null")
    PSNR_U=$(grep -oP 'U-PSNR\s+\K[\d.]+' "$OUTPUT_DIR/benchmarks/iter_${i}.log" | tail -1 || echo "null")
    PSNR_V=$(grep -oP 'V-PSNR\s+\K[\d.]+' "$OUTPUT_DIR/benchmarks/iter_${i}.log" | tail -1 || echo "null")
    BITRATE=$(grep -oP 'Bitrate\s+\K[\d.]+' "$OUTPUT_DIR/benchmarks/iter_${i}.log" | tail -1 || echo "null")
    SSIM_VAL="null"
    $DO_SSIM && SSIM_VAL=$(grep -oP 'SSIM\s*:\s*\K[\d.]+' "$OUTPUT_DIR/benchmarks/iter_${i}.log" | tail -1 || echo "null")

    # VMAF
    VMAF_VAL="null"
    if $DO_VMAF; then
        RECON_YUV="/tmp/park_joy_recon_${i}.yuv"
        ffmpeg -c:v libvvenc -i "$BITSTREAM" "$RECON_YUV" -y 2>/dev/null
        VMAF_JSON=$(vmaf -r "$SOURCE" -d "$RECON_YUV" --width "$W" --height "$H" --pixel_format 420 --bitdepth 8 --model path=/usr/share/model/vmaf_v0.6.1.json --json 2>/dev/null || echo "")
        VMAF_VAL=$(echo "$VMAF_JSON" | grep -oP '"score":\s*\K[\d.]+' | tail -1 || echo "null")
        rm -f "$RECON_YUV"
    fi

    ITEM=$(cat <<ITEMEOF
    {
      "iter": $i,
      "wall_time_ms": $WALL_MS,
      "fps": $FPS,
      "bitrate_kbps": $BITRATE,
      "psnr_y": $PSNR_Y,
      "psnr_u": $PSNR_U,
      "psnr_v": $PSNR_V,
      "ssim": $SSIM_VAL,
      "vmaf": $VMAF_VAL
    }
ITEMEOF
)
    ITER_DATA+=("$ITEM")
done

# --- Compute summary ---
# Build JSON
JSON=$(cat <<JSONEOF
{
  "label": "$LABEL",
  "timestamp": "$(timestamp)",
  "iterations": [
    $(IFS=,; echo "${ITER_DATA[*]}")
  ],
  "config": {
    "source": "$SOURCE",
    "preset": "$PRESET",
    "frames": $FRAMES,
    "metrics": [$($DO_PSNR && echo '"psnr"')$($DO_SSIM && echo ',"ssim"')$($DO_VMAF && echo ',"vmaf"')],
    "encoder": "$ENCODER",
    "iterations": $ITERATIONS
  }
}
JSONEOF
)

echo "$JSON" > "$RESULTS_FILE"
log_info "Benchmark saved: $RESULTS_FILE"
