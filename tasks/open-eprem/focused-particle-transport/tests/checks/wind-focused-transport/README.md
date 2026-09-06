# wind-focused-transport

Upstream test: `code/open-eprem/examples/wind.cfg`. Policy: `pointwise` (provisional).

## The test

`run.sh nominal` copies the read-only pinned EPREM source, performs an out-of-tree MPI C build with `-O3`, and runs the official non-shock solar-wind deck. The graded defaults are two MPI ranks, `simStopTime=0.2` day, `tDel=0.01` day, 500 nodes per stream, 1 row by 1 column on each of six faces (6 streams), 20 energy steps, 7 pitch-angle steps and four point observers. These defaults keep the official deck values unchanged and exercise the focused energetic-particle transport path as the non-shock control. After EPREM exits successfully and prints its completion banner, `extract.py` reads the required stream and point-observer NetCDF files, keys streams by physical `(face,row,col)` identity, selects the final physical sample, and writes only the deterministic named arrays in `transport.npz`. The measured planning runtime is approximately 8 seconds on two MPI ranks; `expected_runtime_s` excludes the per-invocation build.

## The two initial conditions

`ic/nominal/wind.cfg` is the official wind configuration at the graded defaults. `ic/variant/wind.cfg` differs in exactly one initial-condition value: `lamo` is `0.1` nominally and `0.10000000000000003` in the variant, exactly two upward binary64 ULPs. Because `mfpInverseB=1`, `src/meanFreePath.c:24-26` multiplies the parallel mean free path directly by `lamo`; the variant therefore supplies a small physical perturbation for later pointwise sensitivity calibration. There is no alternative build: `run.sh` rejects `altbuild`, no independently justified altbuild has been authored, and no altbuild run has been performed.

## The pass policy

The provisional policy grades exactly one deterministic artifact, `transport.npz`. The physical identity fields `stream_face`, `stream_row`, `stream_col` and `point_observer` must match exactly. The named floating physical fields are `final_time_day`, `energy_mev`, `speed_km_s`, `pitch_angle_mu`, `mass_nucleon`, `charge_e`, `stream_mfp_au`, `stream_flux`, `point_mfp_au` and `point_flux`; each will be compared pointwise as `|candidate-reference| <= atol + rtol*|reference|`. Raw NetCDF bytes, attributes and layout, record or file ordering, adaptive step counts, MPI decomposition, logs and timings are not graded. Pointwise status and every per-field tolerance are explicitly provisional: all floating `atol` and `rtol` values are null, so `validate.py` fails closed until calibration is measured and the human curator finalizes the bounds. Mean-free-path or focused-transport faults are expected to affect these physical arrays, but no fault-rejection margin is claimed yet.

## Evidence

The default runtime measurement available for planning is approximately 8 seconds on two MPI ranks. No altbuild comparison, repeat/variant calibration, task selfcheck, or wrong-implementation or wrong-port probe has run. Consequently no measured floor, self-validation spread, bound fraction, final tolerance or wrong-port rejection evidence is reported. The human curator must review calibration results and finalize every field bound before a successful selfcheck can establish this check's numerical equivalence policy.
