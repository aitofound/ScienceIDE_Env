#!/usr/bin/env bash
# Check perturb-2lpt-lowres: the TEST half of the check.
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

# Upstream test this check reproduces:
# code/21cmfast/tests/test_perturb.py::TestPerturb::test_lowres_perturb[inputs_low]
# (inputs_low uses the default matter_options.PERTURB_ALGORITHM, which is "2LPT").
BUILD_START=$(date +%s)
PY=/opt/venv/bin/python3
if ! "$PY" -c "import py21cmfast" 2>/dev/null; then
  cp -R "$SOURCE_DIR/." "$WORK/src"
  CC=gcc SETUPTOOLS_SCM_PRETEND_VERSION_FOR_21CMFAST=4.2.0 \
    /opt/venv/bin/pip install --no-build-isolation --no-deps --no-index --no-cache-dir -q "$WORK/src"
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"

"$PY" - "$CHECK_DIR/ic/$INPUTS/params.json" "$OUT_DIR" <<'PYEOF'
import json, os, sys
from pathlib import Path
import numpy as np
import py21cmfast as p21c
from py21cmfast import InitialConditions, perturb_field
from py21cmfast.wrapper import cfuncs as cf

params_path, out_dir = sys.argv[1], Path(sys.argv[2])
out_dir.mkdir(parents=True, exist_ok=True)
cfg = json.loads(Path(params_path).read_text())
cfg["simulation_options"]["N_THREADS"] = int(os.environ.get("SAB_N_THREADS", "1"))
algorithm = cfg["algorithm"]
test_pt_z = cfg["test_pt_z"]

matter = dict(cfg["matter_options"])
if algorithm != "2LPT":
    matter["PERTURB_ALGORITHM"] = algorithm
inputs = p21c.InputParameters(
    random_seed=cfg["random_seed"],
    simulation_options=p21c.SimulationOptions(**cfg["simulation_options"]),
    matter_options=p21c.MatterOptions(**matter),
    cosmo_params=p21c.CosmoParams(**cfg["cosmo_params"]),
    astro_params=p21c.AstroParams.new(**cfg["astro_params"]),
    astro_options=p21c.AstroOptions(**cfg["astro_options"]),
    node_redshifts=tuple(cfg["node_redshifts"]),
)

# Reproduces tests/test_perturb.py::TestPerturb.get_fake_ics: a hand-constructed initial
# condition (two nonzero density cells; a uniform velocity field sized so the analytic
# displacement is exactly one HII_DIM cell) with a known first-principles answer, not a
# random realization -- this is what makes the upstream test's atol=1e-3 meaningful.
ics = InitialConditions.new(inputs=inputs)
d_z = cf.get_growth_factor(inputs=inputs, redshift=test_pt_z)
d_z_i = cf.get_growth_factor(inputs=inputs, redshift=inputs.simulation_options.INITIAL_REDSHIFT)
res_fac = int(inputs.simulation_options.HIRES_TO_LOWRES_FACTOR)
lo_dim = inputs.simulation_options.HII_DIM
hi_dim = inputs.simulation_options.DIM
fac_1lpt = inputs.simulation_options.cell_size / (d_z - d_z_i)
fac_2lpt = inputs.simulation_options.cell_size / ((-3.0 / 7.0) * (d_z**2 - d_z_i**2))
for name, array in ics.arrays.items():
    setattr(ics, name, array.initialize().computed())
fake_v = np.ones_like(ics.get("lowres_vx"))
ics.set("lowres_vx", 0 * fake_v)
ics.set("lowres_vy", fac_1lpt * fake_v)
ics.set("lowres_vz", 0 * fake_v)
if inputs.matter_options.PERTURB_ALGORITHM == "2LPT":
    ics.set("lowres_vx_2LPT", 0 * fake_v)
    ics.set("lowres_vy_2LPT", 0 * fake_v)
    ics.set("lowres_vz_2LPT", fac_2lpt * fake_v)
d_lo = np.zeros_like(ics.get("lowres_density"))
d_lo[0, 0, 0] = 1
d_lo[lo_dim // 2, lo_dim // 2, lo_dim // 2] = -1
ics.set("lowres_density", d_lo)
d_hi = np.zeros_like(ics.get("hires_density"))
d_hi[0, 0, 0] = res_fac**3
d_hi[hi_dim // 2, hi_dim // 2, hi_dim // 2] = -(res_fac**3)
ics.set("hires_density", d_hi)

pt = perturb_field(initial_conditions=ics, redshift=test_pt_z, regenerate=True, write=False)
np.save(out_dir / "density.npy", np.asarray(pt.get("density"), dtype=np.float32))
PYEOF
