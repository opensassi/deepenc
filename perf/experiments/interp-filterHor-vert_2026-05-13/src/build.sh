#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/relwithdebinfo-static"
LIB_DIR="$REPO_ROOT/lib/relwithdebinfo-static"

SRC="$SCRIPT_DIR/interp_microbench.cpp"
OUT="$SCRIPT_DIR/interp_microbench"

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

    "-I$REPO_ROOT/include"
    "-I$BUILD_DIR"
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

echo "Compiling microbenchmark..."
g++ "${CXXFLAGS[@]}" -c "$SRC" -o "${SRC%.cpp}.o" 2>&1
echo "Linking..."
g++ -O3 "${SRC%.cpp}.o" "$LIB_DIR/libvvenc.a" -lpthread -ldl -o "$OUT" 2>&1
echo "Done: $OUT"
