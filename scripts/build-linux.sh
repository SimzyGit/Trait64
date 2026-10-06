#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
set -euo pipefail
trait64_source="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cmake -S "${trait64_source}" -B "${trait64_source}/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${trait64_source}/build" --parallel "${T64_BUILD_JOBS:-2}"
ctest --test-dir "${trait64_source}/build" --output-on-failure
