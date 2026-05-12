#!/usr/bin/env bash
# Setup: download park_joy test data, create .profiler/ dir, clone FlameGraph.
# Usage: ./setup.sh [--frames N] [--resize WxH]...

source "$(dirname "$0")/common.sh"

# --- Parse args ---
FRAMES=$DEFAULT_FRAMES
RESIZE_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --frames) FRAMES="$2"; shift 2 ;;
        --resize) RESIZE_ARGS+=("$2"); shift 2 ;;
        *) log_error "Unknown option: $1"; exit 1 ;;
    esac
done

# --- Ensure output dirs ---
mkdir -p "$OUTPUT_DIR"/{flamegraphs,benchmarks,perf_archives,reports}

# --- Ensure .gitignore entries ---
GITIGNORE="$PROJECT_ROOT/.gitignore"
for pattern in "test/data/park_joy_*.yuv" ".profiler/"; do
    if ! grep -qxF "$pattern" "$GITIGNORE" 2>/dev/null; then
        echo "$pattern" >> "$GITIGNORE"
        log_info "Added '$pattern' to .gitignore"
    fi
done

# --- Download park_joy 1080p ---
Y4M_URL="https://media.xiph.org/video/derf/y4m/park_joy_1080p50.y4m"
Y4M_FILE="$DATA_DIR/park_joy_1080p.y4m"
SOURCE_FILE="$DATA_DIR/park_joy_1080p${FRAMES}.yuv"

if [[ ! -f "$SOURCE_FILE" ]]; then
    log_info "Downloading park_joy 1080p Y4M (1.5 GB) ..."
    mkdir -p "$DATA_DIR"
    if [[ ! -f "$Y4M_FILE" ]]; then
        wget -O "$Y4M_FILE" "$Y4M_URL" || {
            log_error "Download failed. Try manual download from:"
            log_error "  $Y4M_URL"
            log_error "Place it at: $Y4M_FILE"
            exit 1
        }
    else
        log_info "Y4M already cached: $Y4M_FILE"
    fi

    log_info "Converting Y4M to raw YUV (first $FRAMES frames at 1920x1080; use --resize for lower resolutions)..."
    ffmpeg -i "$Y4M_FILE" -vframes "$FRAMES" -f rawvideo -pix_fmt yuv420p -y "$SOURCE_FILE" 2>/dev/null || {
        log_error "ffmpeg conversion failed. Is ffmpeg installed?"
        exit 1
    }
    log_info "Saved: $SOURCE_FILE ($(du -h "$SOURCE_FILE" | cut -f1))"
else
    log_info "Already exists: $SOURCE_FILE"
fi

# --- Resize variants (from the raw source YUV) ---
for RES in "${RESIZE_ARGS[@]}"; do
    RES_FILE="$DATA_DIR/park_joy_${RES}f${FRAMES}.yuv"
    if [[ ! -f "$RES_FILE" ]]; then
        W="${RES%%x*}"
        H="${RES##*x}"
        log_info "Resizing to ${W}x${H} ..."
        ffmpeg -f rawvideo -s 1920x1080 -pix_fmt yuv420p -i "$SOURCE_FILE" \
               -vf "scale=$W:$H" -frames "$FRAMES" -f rawvideo -pix_fmt yuv420p "$RES_FILE" -y 2>/dev/null || {
            log_error "ffmpeg resize failed for ${W}x${H}."
            exit 1
        }
        log_info "Saved: $RES_FILE ($(du -h "$RES_FILE" | cut -f1))"
    else
        log_info "Already exists: $RES_FILE"
    fi
done

# --- FlameGraph scripts ---
if [[ ! -f "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" ]]; then
    log_info "FlameGraph scripts not found. Cloning..."
    git clone --depth=1 https://github.com/brendangregg/FlameGraph.git "$FLAMEGRAPH_DIR" || {
        log_error "Failed to clone FlameGraph. Clone manually:"
        log_error "  git clone --depth=1 https://github.com/brendangregg/FlameGraph.git $FLAMEGRAPH_DIR"
        exit 1
    }
    log_info "FlameGraph scripts installed at $FLAMEGRAPH_DIR"
else
    log_info "FlameGraph scripts already present"
fi

log_info "Setup complete."
