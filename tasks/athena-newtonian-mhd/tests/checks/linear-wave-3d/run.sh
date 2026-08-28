#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 0 ]] || { printf 'this check has a no-argument contract\n' >&2; exit 2; }
CHECK_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$CHECK_DIR/../../.." && pwd)
RESULTS=${ATHENA_OUTPUT_DIR:-${RESULTS_DIR:-/app/results}}
exec python3 -B "$ROOT/tests/lib/run_case.py" \
  --case linear-wave-3d --problem linear_wave --eos adiabatic --flux hlld \
  --config "$CHECK_DIR/config/athinput.linear_wave3d" \
  --dimensions 8,8,8 --meshblock 8,8,8 --tlim 0.1 --output-dt 0.1 \
  --boundary periodic --results "$RESULTS"
