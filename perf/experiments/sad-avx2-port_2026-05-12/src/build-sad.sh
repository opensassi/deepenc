#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BASELINE_LIB="$REPO_ROOT/lib/release-static/libvvenc.a"
BASELINE_BUILD="$REPO_ROOT/build/release-static"

SRC="$SCRIPT_DIR/sad_microbench.cpp"
OUT="$SCRIPT_DIR/sad_microbench"

CXXFLAGS=(
    -std=gnu++14
    -O3
    -DNDEBUG
    -DTARGET_SIMD_X86=1
    -DVVENC_ENABLE_THIRDPARTY_JSON
    -DVVENC_SOURCE
    -DUSE_AVX2
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

    # Include paths
    "-I$REPO_ROOT/include"
    "-I$BASELINE_BUILD"
    "-I$REPO_ROOT/source/Lib/vvenc"
    "-I$REPO_ROOT/source/Lib"
    "-I$REPO_ROOT/source/Lib/CommonLib"
    "-I$REPO_ROOT/source/Lib/CommonLib/x86"
    "-I$REPO_ROOT/source/Lib/CommonLib/arm"
    "-I$REPO_ROOT/source/Lib/DecoderLib"
    "-I$REPO_ROOT/source/Lib/EncoderLib"
    "-I$REPO_ROOT/source/Lib/apputils"
    "-I$REPO_ROOT/thirdparty/nlohmann_json/single_include"
    "-I$REPO_ROOT/thirdparty"
    "-I$REPO_ROOT"
)

echo "Compiling SAD microbenchmark..."
g++ "${CXXFLAGS[@]}" -c "$SRC" -o "${SRC%.cpp}.o" 2>&1
echo "Linking..."
g++ -O3 "${SRC%.cpp}.o" "$BASELINE_LIB" -lpthread -ldl -o "$OUT" 2>&1
echo "Done: $OUT"
