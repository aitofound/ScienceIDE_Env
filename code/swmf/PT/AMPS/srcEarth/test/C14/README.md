# C14 — Mode3D versus gridless cross-solver consistency

C14 runs the same centered, aligned-dipole vertical-cutoff problem through the
standalone Mode3D and gridless solver paths. Its primary purpose is to detect
solver-path inconsistencies: both branches must use the same particle mover,
cutoff search, finite trajectory limits, domain, species, field model, and shell
locations and should therefore return compatible cutoff rigidities.

C14 also selects the corrected cutoff trajectory integrator by default:

```text
CUTOFF_TRACE_POLICY = ACCURATE
```

This policy uses the same upper-bound-only timestep selection and exact
segment/boundary event handling introduced for F3. The global executable default
remains `LEGACY`, so older C1/C2/C3/C11 references are not changed unless their
runners explicitly request `ACCURATE`.

The runner also calculates the analytical vertical Størmer cutoff,

```text
Rc = R0 cos^4(lambda) / r_RE^2,
```

but the analytical formula assumes asymptotic escape to infinity. The numerical
standalone problem terminates escape at a finite Cartesian box. Those are not
identical boundary-value problems. This distinction is especially important for
near-separatrix trajectories at the 500-km shell and for low-rigidity,
high-latitude trajectories.

## Check profiles

### `CROSS_SOLVER` — default

This profile matches the principal purpose of C14.

Hard checks are:

1. Point-by-point Mode3D versus gridless agreement.
2. The 90-degree rotational symmetry that is actually preserved by the centered
   Cartesian escape box.
3. North/south symmetry.

The following are still calculated and written to `C14_summary.csv`, but are
reported as diagnostics rather than hard failures:

- Mode3D versus infinite-domain Størmer.
- Gridless versus infinite-domain Størmer.
- Continuous longitude invariance across all longitudes.

Continuous longitude invariance is not an exact property of a finite cube. A
cube is invariant under rotations by 90 degrees about the Z axis, but not under
an arbitrary 30-degree rotation. Therefore the hard finite-domain symmetry test
groups longitudes by `lon mod 90 deg`:

```text
0, 90, 180, 270
30, 120, 210, 300
60, 150, 240, 330
```

Use:

```bash
python srcEarth/test/C14/run_C14.py -np 4 -nt 16
```

### `STRICT_STORMER`

This profile promotes the Størmer residuals to hard pass/fail checks. Full
continuous-longitude invariance remains diagnostic because a finite Cartesian
box does not have that symmetry. The strict profile uses larger defaults:

```text
DOMAIN half-size = 100 Re
UPPER_SCAN points = 200
```

Run:

```bash
python srcEarth/test/C14/run_C14.py \
  --check-profile STRICT_STORMER \
  --mode3d-field-eval ANALYTIC \
  -np 4 -nt 16
```

`--strict-stormer` is an alias for `--check-profile STRICT_STORMER`.

A strict result should be treated as a domain-convergence result, not just a
single run. Repeat with a larger box, for example 150 Re, and verify that the
cutoff changes negligibly:

```bash
python srcEarth/test/C14/run_C14.py \
  --strict-stormer \
  --domain-half-size-re 150 \
  --cutoff-scan-n 300 \
  --mode3d-field-eval ANALYTIC \
  -np 4 -nt 16
```

## Interpreting the reported failure pattern

A residual such as

```text
Mode3D/Størmer   ~= 25%
Gridless/Størmer ~= 25%
Mode3D/gridless  ~= small
```

is not a Mode3D-versus-gridless inconsistency, but it is also too large to accept
as a strict Størmer result. In this code line, that pattern is characteristic of
running C14 with the backward-compatible `LEGACY` cutoff integration policy. The
legacy policy imposes a 100-km minimum displacement per full-orbit step; that can
override the gyro-angle accuracy limit and shift the vertical cutoff by tens of
percent while affecting Mode3D and gridless in almost exactly the same way.

The C14 runner therefore defaults to:

```bash
--cutoff-trace-policy ACCURATE
```

Use `LEGACY` only for an explicit compatibility experiment. Changing the policy
requires rerunning AMPS; `--skip-run` cannot repair output generated with the
legacy integrator.

Similarly, longitude differences that repeat in 90-degree groups are consistent
with the symmetry of a Cartesian box. They should not be interpreted as a
violation of the centered-dipole physics.

By contrast, a large point-by-point Mode3D/gridless difference remains a real
C14 failure. With Mode3D `MESH`, high-latitude cutoffs are small and sensitive to
mesh interpolation and final trajectory classification. The default mesh-mode
high-latitude tolerance is therefore 0.15, while the analytical-field tolerance
remains 0.05. These tolerances can be overridden explicitly.

## Particle mover selection

Use `--mover NAME` to apply the same mover to both branches:

```bash
python srcEarth/test/C14/run_C14.py --mover RK4 -np 4 -nt 16
python srcEarth/test/C14/run_C14.py --mover HC4 -np 4 -nt 16
python srcEarth/test/C14/run_C14.py --mover BORIS -np 4 -nt 16
```

Supported canonical names are:

```text
BORIS, HC4, RK2, RK4, RK6, GC2, GC4, GC6, HYBRID
```

Aliases accepted by the C++ parser are canonicalized by the runner. Omitting
`--mover` preserves the executable/input default.

C14 determines whether two solver architectures behave consistently with the
same mover. C12 remains the dedicated mover-versus-Størmer validation test.

## Mode3D mesh controls

The Mode3D branch supports:

```text
--mode3d-mesh-res-earth-re
--mode3d-mesh-res-boundary-re
--mode3d-mesh-coarsening
```

Example:

```bash
python srcEarth/test/C14/run_C14.py \
  --mover RK4 \
  --mode3d-field-eval MESH \
  --mode3d-mesh-res-earth-re 0.02 \
  --mode3d-mesh-res-boundary-re 3 \
  --mode3d-mesh-coarsening LINEAR \
  -np 4 -nt 16
```

The controls apply only to Mode3D. Gridless remains an independent direct-field
reference.

For a pure mover/cross-solver diagnosis, first remove mesh interpolation:

```bash
python srcEarth/test/C14/run_C14.py \
  --mover RK4 \
  --mode3d-field-eval ANALYTIC \
  -np 4 -nt 16
```

Then repeat with `MESH`. A discrepancy appearing only in the second run is a
mesh/interpolation sensitivity rather than a shared mover effect.

## Domain, scan, and step controls

The runner now exposes the numerical controls needed for convergence studies:

```text
--domain-half-size-re RE
--cutoff-scan-n N
--cutoff-trace-policy ACCURATE|LEGACY
--dt-trace SECONDS
--adaptive-dt T|F
--max-steps N
--max-trace-time SECONDS
--max-trace-distance RE
```

The domain half-size is written identically to all six Cartesian limits in both
input files. `ADAPTIVE_DT`, `DT_TRACE`, `MAX_STEPS`, time limit, and cumulative
distance limit are also identical between the branches.

`--cutoff-trace-policy` is also passed identically to both branches. Its default
for C14 is `ACCURATE`. This differs intentionally from the executable-wide
backward-compatible default `LEGACY`.

`CROSS_SOLVER` defaults:

```text
domain half-size = 35 Re
cutoff scan N     = 100
```

`STRICT_STORMER` defaults:

```text
domain half-size = 100 Re
cutoff scan N     = 200
```

## Relative-error definitions

Cross-solver and north/south comparisons use a symmetric relative difference:

```text
2 |a-b| / (|a|+|b|)
```

They are no longer normalized by the Størmer value. This prevents a small
analytical cutoff at high latitude from contaminating a comparison whose actual
question is agreement between two numerical values.

Longitude and 90-degree-group spreads use:

```text
(max(value)-min(value)) / mean(|value|)
```

The Størmer residual retains the conventional `(Rc_num-Rc_ref)/Rc_ref` form.

## Tolerances

Important defaults are:

```text
mid-latitude Mode3D/gridless                 0.01
high-latitude, Mode3D ANALYTIC               0.05
high-latitude, Mode3D MESH                   0.15
finite-box 90-degree rotational symmetry     0.03
Mode3D MESH high-latitude box symmetry       0.15
north/south                                  0.03
Mode3D MESH high-latitude north/south        0.10
Størmer, |lat| <= 30                         0.05
Størmer, |lat| > 30                          0.25
```

The Størmer tolerances are hard only in `STRICT_STORMER`. Full-longitude spread
is diagnostic in both profiles because the finite Cartesian box does not preserve
continuous rotational symmetry.

The 90-degree symmetry tolerance has an additional resolution floor derived from
one logarithmic `UPPER_SCAN` interval:

```text
2 (q-1) / (q+1),
q = (Rmax/Rmin)^(1/(Nscan-1)).
```

The effective tolerance is the larger of the configured tolerance and 1.10 times
this one-bin resolution. For the default 100-point scan over 1 MeV to 20 GeV,
one bin corresponds to approximately 6.24% in symmetric relative difference.
Therefore a 6.34% quarter-turn difference is consistent with a one-cell cutoff
classification shift and cannot be tested against a fixed 3% threshold. Increasing
`--cutoff-scan-n` lowers this resolution floor.

## Outputs

Run artifacts are written below `test_output/C14_cross_solver` unless another
`--workdir` is supplied:

```text
mode3d/AMPS_PARAM_C14.in
gridless/AMPS_PARAM_C14.in
mode3d/C14_3d_amps.log
gridless/C14_gridless_amps.log
C14_summary.csv
C14_result.json
C14_mode3d_vs_gridless.png
reference_C14_cross_solver_generated.csv
```

`C14_result.json` records the profile, mover, cutoff trace policy, finite-domain
size, adaptive-step setting, step limit, mesh controls, scan count, derived
one-bin rigidity resolution, and output file locations.

## Reanalyzing existing output

The profile and tolerance logic can be changed without rerunning AMPS:

```bash
python srcEarth/test/C14/run_C14.py \
  --skip-run \
  --workdir test_output/C14_cross_solver \
  --check-profile CROSS_SOLVER \
  --mode3d-field-eval MESH
```

When using `--skip-run`, specify the same field-evaluation mode and relevant
tolerance/profile options used to interpret the existing outputs.

The cutoff trace policy is not a post-processing option. If existing outputs were
generated with `LEGACY`, rerun both AMPS branches with `ACCURATE` before applying
the strict Størmer checks.
