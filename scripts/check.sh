#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_root"

python3 -m py_compile tools/rover_control.py

control_mix_test_binary="$(mktemp "${TMPDIR:-/tmp}/tumbller-control-mix.XXXXXX")"
trap 'rm -f "$control_mix_test_binary"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -Iinclude test/control_mix_test.cpp -o "$control_mix_test_binary"
"$control_mix_test_binary"

platformio run
git diff --check
