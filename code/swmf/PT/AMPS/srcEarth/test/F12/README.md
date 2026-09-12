# F12 — day/night spatial boundary anisotropy identities

F12 validates the `DAYSIDE_NIGHTSIDE` spatial boundary-weight model in the
`GRIDLESS` density/spectrum path.

The test uses a deliberately simple exact geometry:

```text
FIELD_MODEL NONE
EFIELD_MODEL NONE
R_INNER = 0
DS_BOUNDARY_MODE = ANISOTROPIC
BA_PAD_MODEL = ISOTROPIC
OUTPUT points have X = 0
```

With no magnetic field and no absorbing inner sphere, every sampled direction is
allowed.  Since every diagnostic point lies on the GSM Y-Z plane, the deterministic
AMPS direction grid is paired under `x -> -x`: exactly half of the trajectories
exit through `x_exit > 0` and half through `x_exit <= 0`.

The exact reference identities are therefore:

```text
DAYSIDE_NIGHTSIDE(1,1) = UNIFORM
DAY_ONLY(1,0)          = 0.5 * UNIFORM
NIGHT_ONLY(0,1)        = 0.5 * UNIFORM
DAY_ONLY + NIGHT_ONLY  = UNIFORM
DAYSIDE_NIGHTSIDE(2,0.5) = 1.25 * UNIFORM
```

The runner checks these identities for total density, total omnidirectional flux,
all requested flux channels, saved transmission `T(E)`, boundary spectrum, and
local spectrum.

## Run

From the directory containing the AMPS executable:

```bash
python srcEarth/test/F12/run_F12.py -np 4 -nt 16
```

Useful variants:

```bash
python srcEarth/test/F12/run_F12.py -np 1 -nt 1
python srcEarth/test/F12/run_F12.py --ratio-tol 1e-12 --spectrum-tol 1e-12
python srcEarth/test/F12/run_F12.py --nintervals 160
python srcEarth/test/F12/run_F12.py --dry-run --workdir test_output/F12_dryrun
python srcEarth/test/F12/run_F12.py --skip-run --workdir test_output/F12_gridless
```

## Output

The default output directory is `test_output/F12_gridless`.  It contains one
subdirectory per case:

```text
UNIFORM/
DN_1_1/
DAY_ONLY/
NIGHT_ONLY/
DAY2_NIGHT05/
```

Each case directory contains the rendered `AMPS_PARAM_F12.in`, the AMPS log, and
the usual gridless density/spectrum/flux output files.  The top-level directory
contains:

```text
F12_summary.csv
F12_result.json
reference_F12_daynight_step_used.csv
```

The repository reference table is `reference_F12_daynight_step.csv`.
