#!/usr/bin/env bash
set -e

echo "=============================================================================="
echo " KerneTile / KTUI Web (WebAssembly) Build & Package Script"
echo "=============================================================================="

# 1. æ£€æŸ?emcc ç¼–è¯‘å™?
if ! command -v emcc >/dev/null 2>&1; then
    if [ -n "$EMSDK" ] && [ -f "$EMSDK/emsdk_env.sh" ]; then
        source "$EMSDK/emsdk_env.sh"
    else
        echo "[ERROR] Emscripten compiler (emcc) not found in PATH or EMSDK!"
        echo "Please install Emscripten SDK: https://emscripten.org/docs/getting_started/downloads.html"
        exit 1
    fi
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

echo "[1/3] Configuring KerneTile for Web..."
emcmake cmake -B build-web -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=20

echo "[2/3] Compiling WebAssembly artifacts..."
cmake --build build-web --target ktui_gallery --parallel $(nproc 2>/dev/null || echo 4)

echo "[3/3] Executing Node.js Headless Smoke Test..."
cd build-web
node ktui_gallery.js --capture-only
cd "$ROOT_DIR"

echo "=============================================================================="
echo " KerneTile KTUI Web Build Complete!"
echo " Artifacts: build-web/ktui_gallery.html, .js, .wasm"
echo " Run: python3 -m http.server 8080 --directory build-web"
echo "=============================================================================="
