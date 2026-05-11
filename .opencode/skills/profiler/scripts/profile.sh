#!/usr/bin/env bash
# Profile: run perf record on encoder encode, produce flamegraph.
# Usage: ./profile.sh [--res WxH] [--preset NAME] [--frames N] [--events LIST]

source "$(dirname "$0")/common.sh"

# --- Parse args ---
RESOLUTION=$DEFAULT_RESOLUTION
PRESET=$DEFAULT_PRESET
FRAMES=$DEFAULT_FRAMES
EVENTS=$PERF_EVENTS_DEFAULT

while [[ $# -gt 0 ]]; do
    case "$1" in
        --res)    shift; RESOLUTION="$1" ;;
        --preset) shift; PRESET="$1" ;;
        --frames) shift; FRAMES="$1" ;;
        --events) shift; EVENTS="$1" ;;
        *) log_error "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

# --- Determine source file ---
if [[ "$RESOLUTION" == "$DEFAULT_RESOLUTION" ]]; then
    SOURCE="$DATA_DIR/park_joy_1080p${FRAMES}.yuv"
    LABEL="park_joy_1080p${FRAMES}_${PRESET}"
    RES_OPTS="--size 1920x1080"
else
    W="${RESOLUTION%%x*}"; H="${RESOLUTION##*x}"
    SOURCE="$DATA_DIR/park_joy_${RESOLUTION}f${FRAMES}.yuv"
    LABEL="park_joy_${RESOLUTION}f${FRAMES}_${PRESET}"
    RES_OPTS="--size ${W}x${H}"
fi

if [[ ! -f "$SOURCE" ]]; then
    log_error "Source not found: $SOURCE"
    log_error "Run setup.sh first."
    exit 1
fi

ENCODER="$(find_encoder)"
if [[ -z "$ENCODER" ]]; then
    log_error "Encoder binary not found. Build the Release target first."
    exit 1
fi

# --- Output dir ---
PERF_DIR="$OUTPUT_DIR/perf_archives/$LABEL"
mkdir -p "$PERF_DIR"
PERF_DATA="$PERF_DIR/perf.data"
PERF_STAT="$PERF_DIR/perf.stat"
FOLDED="$PERF_DIR/folded.txt"
FLAME="$PERF_DIR/flame.svg"
META="$PERF_DIR/meta.json"

# --- Encode command ---
ENCODE_CMD=("$ENCODER" "--input" "$SOURCE" $RES_OPTS "--frames" "$FRAMES" "--preset" "$PRESET" "--output" "/dev/null")

log_info "Profiling: $LABEL"
log_info "Encoder: ${ENCODE_CMD[*]}"
log_info "perf events: $EVENTS"

# --- perf record ---
perf record --call-graph fp -e "$EVENTS" -o "$PERF_DATA" -- "${ENCODE_CMD[@]}" 2>&1 | tee "$PERF_DIR/encode.log"
RESULT=${PIPESTATUS[0]}

# --- perf stat ---
perf stat -e "$EVENTS" -- "${ENCODE_CMD[@]}" > "$PERF_STAT" 2>&1

# --- Flamegraph generation ---
log_info "Generating flamegraph..."
perf script -i "$PERF_DATA" | "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" > "$FOLDED"
"$FLAMEGRAPH_DIR/flamegraph.pl" "$FOLDED" > "$FLAME"
log_info "Flamegraph: $FLAME"

# --- Meta ---
cat > "$META" <<METAEOF
{
  "label": "$LABEL",
  "timestamp": "$(timestamp)",
  "source": "$SOURCE",
  "preset": "$PRESET",
  "frames": $FRAMES,
  "perf_events": "$EVENTS",
  "encoder": "$ENCODER",
  "exit_code": $RESULT
}
METAEOF

log_info "Profile complete: $PERF_DIR"
