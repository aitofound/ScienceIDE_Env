# euler-axi-sodshock

Upstream test: `code/gkeyll/moments/creg/rt_euler_axi_sodshock.c`. Policy: `pointwise`.

## The test

Upstream axisymmetric Sod-type shock tube in polar coordinates for the five-moment Euler equations on a mapped grid (64x64 cells, t=0.1). `run.sh` builds the driver against the prebuilt `moments` library in the image and runs it with no command-line overrides; `SAB_STEPS`, `SAB_XCELLS` and `SAB_YCELLS` are iteration-only knobs.

## The two initial conditions

`ic/nominal/value.txt` contains rhol = 3.0. The variant contains 3.000000000000001, exactly two representable binary64 values upward. The run script changes only the first numeric-literal declaration of that parameter (the driver's context value) in its private source copy, so the upstream deck remains otherwise unchanged. A third run, `run.sh altbuild`, takes the nominal inputs on the same pinned source compiled strict-IEEE with the same gcc (`-O2`, no `-ffast-math`, `-ffp-contract=off`, no `-march=native`) into `build-ieee/`; a correct port compiled without fast-math or FMA contraction is such a build, and its distance from the default `-O3 -ffast-math -march=native` build is the floor the bound must clear.

## The pass policy

Every binary64 payload value of the integrated Euler diagnostic history on the mapped polar grid (198 values) is compared pointwise after ignoring timestamps, with exact length required, against `1e-11 + 1e-11*|reference|`. A wrong coordinate mapping, geometric source term or mapped flux (moments/apps/moment.c, moments/zero/wv_euler.c) moves the history at the 1e-6 level or more.

## Evidence

Run time on x86 (one core, 2026-09-05): 1.1 s. The two-ULP variant moves the history by at most 4.4e-15 absolute (5.7e-16 relative on a value of order 7.9); the strict-IEEE build differs by 7.1e-15 absolute (3.6e-15 at the value of order 7.9 that uses the largest fraction of the bound). The largest fraction of the bound used is 5.0e-05, so the bound is 20000x over the floor.
