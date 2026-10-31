#!/bin/sh
# Build the WebAssembly part of the browser demo (web/guy.wasm) with Zig: pip install ziglang
set -e
cd "$(dirname "$0")"
python3 -m ziglang c++ --target=wasm32-wasi -Os -s -mexec-model=reactor -fno-exceptions -I../src guy_wasm.cpp -o guy.wasm
echo "web/guy.wasm is ready; serve this folder, e.g. python3 -m http.server -d web"
