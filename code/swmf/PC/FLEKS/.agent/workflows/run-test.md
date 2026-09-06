---
description: How to run the most common FLEKS test (test16_3d) via SWMF
---

# Run test16_3d (GM-PC/FLEKS 3D periodic reconnection)

All commands are run from the **SWMF root** directory (`../../` from FLEKS).

## Full automated run

1. `cd ../../ && make test16_3d`

## Step-by-step run

1. Compile: `cd ../../ && make test16_3d_compile`
2. Create run directory: `make test16_3d_rundir`
3. Run simulation: `make test16_3d_run`
4. Check results: `make test16_3d_check`

## Verify

5. Check the diff file: `ls -l test16_3d_gmpc.diff`
   - Empty or near-empty file = **PASS**
   - Non-trivial differences = **FAIL** (investigate `run_test/runlog`)

## Notes

- Default MPI: `mpiexec -n 2`. Override with `NP=4`.
- To bless new reference results: `make test16_3d_check BLESS=YES`
- Results land in `run_test/RESULTS/3d/PC/`
- Reference files are in `output/test16/`
