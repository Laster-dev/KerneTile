#!/usr/bin/env bash
# ==============================================================================
# KerneTile / KTUI Linux Build & Test Bootstrap Script
# Supported Platforms: Ubuntu 20.04+, Debian 11+, Arch, Fedora
# ==============================================================================
set -e

echo "[1/4] Checking and installing dependencies..."
if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y build-essential cmake ninja-build g++ libc6-dev libx11-dev fontconfig fonts-wqy-microhei
elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -Syu --noconfirm base-devel cmake ninja gcc libx11 fontconfig wqy-microhei
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y gcc-c++ cmake ninja-build glibc-static libstdc++-static libX11-devel fontconfig wqy-microhei-fonts
else
    echo "Notice: Package manager not recognized, ensure C++20 compiler and CMake are installed."
fi

echo "[2/4] Configuring KerneTile for Linux (Pure C++20 + POSIX PAL + Headless/Universal HAL)..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

cmake -B build-linux -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=20 \
    -DBUILD_TESTING=ON

echo "[3/4] Building KerneTile & KTUI..."
cmake --build build-linux --parallel $(nproc)

echo "[4/4] Executing Cross-Platform Automated Unit & Integration Tests..."
ctest --test-dir build-linux --output-on-failure

echo "=============================================================================="
echo " KerneTile & KTUI Linux Build and Verification Complete! All tests PASSED. "
echo "=============================================================================="
