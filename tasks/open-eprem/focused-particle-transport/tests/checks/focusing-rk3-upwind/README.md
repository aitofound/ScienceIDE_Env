# focusing-rk3-upwind

Upstream test: `custom: No official deck selects adiabaticFocusAlg=2, so no official deck reaches the three-stage Runge-Kutta adiabatic-focusing subcycle with its upwind operator in src/energeticParticles.c:1629-1754.`. Policy: `pointwise`.

## The test

Adiabatic focusing is transport across pitch angle, the cosine of the angle between particle motion and the magnetic field, as field strength and plasma flow change. This custom case selects `adiabaticFocusAlg=2`: the three-stage Runge-Kutta subcycle at `src/energeticParticles.c:1629-1690` calls the upwind pitch-angle-space flux operator at line 1754 during every stage. The deck uses two MPI ranks, a 0.2-day window, a 0.01-day outer step, 500 nodes per stream, six streams, 20 energy steps, 7 pitch-angle steps and four point observers. `extract.py` writes `transport.npz` in the `eprem-final-named-arrays-v1` format. `run.sh altbuild` uses the nominal deck and the same compiler and libraries, with `-O1` instead of `-O3`.

## The two initial conditions

Nominal `case.cfg` contains the complete official `wind.cfg` deck followed by exactly `adiabaticFocusAlg=2`. Variant `case.cfg` differs only in `lamo`, which changes from `0.1` to `0.10000000000000003`, exactly two upward binary64 ULPs. The variant measures numerical sensitivity to a very small active change in the parallel mean free path.

## The pass policy

The finalized pointwise policy compares final named coordinate, parallel-mean-free-path and particle-flux arrays. The integer fields `stream_face`, `stream_row`, `stream_col` and `point_observer` define physical identity and must match exactly. Coordinate and parameter fields use `atol=rtol=1e-12`; mean-free-path fields use `atol=1e-17` and `rtol=1e-12`. Each flux field uses `atol=1e-14`, plus `1e-9` times that reference field's peak, plus `rtol=1e-10` times the reference cell. The peak term prevents effectively zero flux from dominating a relative comparison. Raw NetCDF storage, file order, MPI layout, adaptive bookkeeping, logs and timings are not graded.

## Evidence

Calibration run `cal3` began on 2026-09-08 on arm64 macOS under Colima with gcc 14.2. The two-ULP variant moved `stream_flux` by at most `1.8189894035458565e-12` and used `0.0006412549176847409` of the worst finalized bound. The `-O1` build moved `stream_flux` by at most `2.9668001388927223e-10` and used `0.0012099207440976264` of the worst bound.

Against the nominal wind deck, selecting RK3 upwind focusing changed `stream_flux` by at most `2.422e+00` in 59,880 of 60,000 cells and `point_flux` by `6.757e-05` in all 80 cells, while `stream_mfp_au` was identical. The selector therefore reaches the requested transport path and changes graded flux; mean free path does not depend on the transport algorithm. These are differences between two physics configurations, not tolerances.

Grading runs on x86, so this same-host compiler comparison does not establish a cross-architecture or GPU floor. A GPU or other port may differ more than this same-host calibration.
