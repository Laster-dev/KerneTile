#!/bin/bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [ ! -f "build-web/ktui_gallery.html" ]; then
    echo "[1/3] Building WebAssembly artifacts..."
    bash scripts/build_web.sh
fi

python3 scripts/deploy_gh_pages.py
git push origin gh-pages
