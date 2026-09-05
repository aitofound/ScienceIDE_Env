# pkpm-alfven-soliton-p2

Upstream test: `code/gkeyll/pkpm/creg/rt_pkpm_alf_soliton_1x_p2.c`. Policy: `pointwise`.

## The test

`run.sh` builds and executes the unchanged upstream two-species P2 1x1v Alfvén-soliton driver at 16 configuration cells and 32 parallel-velocity cells per species through the full `t=10000/omegaCi` window on one CPU. It exercises periodic P2 reduced-kinetic transport, perpendicular moments, collisions, current coupling, and the self-consistent electromagnetic field. The graded defaults retain the full upstream window and resolution; `SAB_STEPS`, `SAB_XCELLS`, and `SAB_VX_CELLS` are iteration-only overrides. The separate native survey measured 25.93 s of physical run time.

## The two initial conditions

The nominal case uses the upstream soliton amplitude `a=0.01`. The variant uses `0.010000000000000004`, exactly two upward binary64 ULP, so it probes the numerical floor while remaining in the same weakly nonlinear Alfvénic regime.

## The pass policy

Every binary64 coefficient in the final electron and ion distributions, their perpendicular-fluid states, and the electromagnetic field is compared pointwise on identical grids. The finalized bounds are `atol=2e-10` and `rtol=1e-11`.

## Evidence

No separate same-input cross-build floor is available. The consented nominal-versus-variant Docker calibration measured a maximum spread of `1.0283811909678198e-10`, so the finalized absolute bound leaves 1.94 times headroom.
