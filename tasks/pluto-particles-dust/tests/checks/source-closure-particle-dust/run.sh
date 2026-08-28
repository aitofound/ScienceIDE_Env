#!/bin/sh
set -eu
[ "$#" -eq 0 ] || { echo 'run.sh accepts no arguments' >&2; exit 2; }
CHECK=source-closure-particle-dust
TESTS=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
SOURCE_ROOT=${MODULE_COVERAGE_SOURCE:-"$TESTS/../code/pluto"}
RUN_ID=$(date -u +%Y%m%d%H%M%S)-$$
ROOT=${MODULE_COVERAGE_OUTPUT:-"$TESTS/.coverage-output"}/$CHECK/$RUN_ID
mkdir -p "$ROOT"
exec python3 "$TESTS/module_coverage_probe.py" --row "$CHECK" --source-root "$SOURCE_ROOT" --output "$ROOT"
