#!/usr/bin/env bash
# Check ic-density-reproducibility-from-array: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_N_THREADS "1" "OpenMP thread count (simulation_options.N_THREADS); the FFT/filtering kernels parallelize with more threads"
ALTBUILD=""
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT

# Upstream test this check reproduces: code/21cmfast/tests/test_initial_conditions.py::test_initial_density_array
BUILD_START=$(date +%s)
PY=/opt/venv/bin/python3
if ! "$PY" -c "import py21cmfast" 2>/dev/null; then
  cp -R "$SOURCE_DIR/." "$WORK/src"
  CC=gcc SETUPTOOLS_SCM_PRETEND_VERSION_FOR_21CMFAST=4.2.0 \
    /opt/venv/bin/pip install --no-build-isolation --no-deps --no-index --no-cache-dir -q "$WORK/src"
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"

"$PY" - "$CHECK_DIR/ic/$INPUTS" "$OUT_DIR" <<'PYEOF'
import json, os, sys
from pathlib import Path
import numpy as np
import py21cmfast as p21c

ic_dir, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
out_dir.mkdir(parents=True, exist_ok=True)
cfg = json.loads((ic_dir / "params.json").read_text())
cfg["simulation_options"]["N_THREADS"] = int(os.environ.get("SAB_N_THREADS", "1"))
initial_density = np.load(ic_dir / "initial_density.npy")

inputs = p21c.InputParameters(
    random_seed=cfg["random_seed"],
    simulation_options=p21c.SimulationOptions(**cfg["simulation_options"]),
    matter_options=p21c.MatterOptions(**cfg["matter_options"]),
    cosmo_params=p21c.CosmoParams(**cfg["cosmo_params"]),
    astro_params=p21c.AstroParams.new(),
    astro_options=p21c.AstroOptions(**cfg["astro_options"]),
    node_redshifts=tuple(cfg["node_redshifts"]),
)
ic = p21c.compute_initial_conditions(inputs=inputs, initial_density=initial_density)

for name in ["lowres_density", "hires_density", "lowres_vx", "lowres_vy", "lowres_vz",
             "lowres_vx_2LPT", "lowres_vy_2LPT", "lowres_vz_2LPT"]:
    np.save(out_dir / f"{name}.npy", np.asarray(getattr(ic, name).value, dtype=np.float32))
PYEOF
