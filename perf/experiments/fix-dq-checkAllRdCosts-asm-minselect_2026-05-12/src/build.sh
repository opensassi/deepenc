#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BASELINE_SRC="/home/pc/projects/deepenc"
BASELINE_BUILD="/home/pc/projects/deepenc/build/release-static"
BASELINE_LIB="$BASELINE_SRC/lib/release-static/libvvenc.a"

SRC="$SCRIPT_DIR/dq_microbench.cpp"
OUT="$SCRIPT_DIR/dq_microbench"

CXXFLAGS=(
    -std=gnu++14
    -O3
    -DNDEBUG
    -DTARGET_SIMD_X86=1
    -DVVENC_ENABLE_THIRDPARTY_JSON
    -DVVENC_SOURCE
    -DUSE_AVX2
    -fno-inline
    -mavx2
    -msse4.1
    -msse4.2
    -fPIC
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-ignored-attributes

    # Include paths (matching baseline cmake build)
    "-I$BASELINE_SRC/include"
    "-I$BASELINE_BUILD"
    "-I$BASELINE_SRC/source/Lib/vvenc"
    "-I$BASELINE_SRC/source/Lib"
    "-I$BASELINE_SRC/source/Lib/CommonLib"
    "-I$BASELINE_SRC/source/Lib/CommonLib/x86"
    "-I$BASELINE_SRC/source/Lib/CommonLib/arm"
    "-I$BASELINE_SRC/source/Lib/DecoderLib"
    "-I$BASELINE_SRC/source/Lib/EncoderLib"
    "-I$BASELINE_SRC/source/Lib/apputils"
    "-I$BASELINE_SRC/thirdparty/nlohmann_json/single_include"
    "-I$BASELINE_SRC/thirdparty"
    "-I$BASELINE_SRC"
)

echo "Compiling microbenchmark..."
g++ "${CXXFLAGS[@]}" -c "$SRC" -o "${SRC%.cpp}.o" 2>&1
echo "Linking..."
g++ -O3 "${SRC%.cpp}.o" "$BASELINE_LIB" -lpthread -ldl -o "$OUT" 2>&1
echo "Done: $OUT"
