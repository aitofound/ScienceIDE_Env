# pkpm-alfven-soliton-p2

Upstream test: `code/gkeyll/pkpm/creg/rt_pkpm_alf_soliton_1x_p2.c`. Policy: `pointwise`.

## The test

`run.sh` builds and executes the unchanged upstream two-species P2 1x1v Alfvén-soliton driver at 16 configuration cells and 32 parallel-velocity cells per species through the full `t=10000/omegaCi` window on one CPU. It exercises periodic P2 reduced-kinetic transport, perpendicular moments, collisions, current coupling, and the self-consistent electromagnetic field. The graded defaults retain the full upstream window and resolution; `SAB_STEPS`, `SAB_XCELLS`, and `SAB_VX_CELLS` are iteration-only overrides. The separate native survey measured 25.93 s; the x86 calibration selfcheck of 2026-09-05 measured 45.4 s of physical run time with the per-check driver build excluded.

## The two initial conditions

The nominal case uses the upstream soliton amplitude `a=0.01`. The variant uses `0.010000000000000004`, exactly two upward binary64 ULP, so it probes the numerical floor while remaining in the same weakly nonlinear Alfvénic regime. This driver derives the domain half-width from the amplitude itself (`Lx = 10*(di/a)`), so the two-ULP change also moves the grid's lower/upper extents at the same numerical scale; the pass policy grades those together with the payload. `run.sh altbuild` runs the same nominal inputs on a strict-IEEE build of the same source (gcc -O2, no -ffast-math, -ffp-contract=off, no -march=native); self-validation grades it against the default build and records the distance as this check's floor.

## The pass policy

Every binary64 coefficient in the final electron and ion distributions, their perpendicular-fluid states, and the electromagnetic field, plus each output grid's extents, is compared pointwise. The bounds are `atol=2e-08` and `rtol=1e-11`.

## Evidence

The consented x86 Docker calibration of 2026-09-05 measured a two-ULP variant spread of `1.0783759992692146e-10` and a strict-IEEE altbuild floor of `1.1090151019743644e-10`, both on a perpendicular-fluid-moment value several orders of magnitude below the run's largest values; neither run was identical. The bound `atol=2e-08`, `rtol=1e-11`, widened at revision on 2026-09-05 from `atol=2e-10` (a 1.8x margin), is about 182 times the larger of the two.
