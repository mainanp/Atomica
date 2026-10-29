#!/usr/bin/env bash
# One-shot setup for Ubuntu 24.04: installs build tools, builds, runs the tests.
set -euo pipefail
cd "$(dirname "$0")/.."

sudo apt update
sudo apt install -y build-essential cmake git libglfw3-dev libgl1-mesa-dev mesa-utils

cmake -B build -S .
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

echo
echo "Build OK.  Run:  ./build/atomica          (or ./build/atomica --stage 4)"
