# C17 — Dipole charge-sign and meridional-reflection symmetry

C17 verifies the charge sign passed to the particle mover and the directional
coordinate convention without using observational data or an external cutoff
code. The test uses a static aligned centered dipole with no electric field.

## Correct symmetry contract

Let

```text
M(x,y,z) = (x,-y,z).
```

For the aligned centered dipole, `B(Mx)=M B(x)`. Because `det(M)=-1`,

```text
(Mv) x (MB) = -M(v x B).
```

Changing the charge from `q` to `-q` consequently gives

```text
A_q(x0,v,R) = A_-q(x0,Mv,R)
```

for an observation point fixed by the reflection. C17 therefore restricts
observation longitude to 0 or 180 degrees. In the global spherical coordinates
written by the AMPS directional output, the sky mapping is

```text
(lon,lat) -> ((-lon) mod 360, lat).
```

The former mapping `(lon,lat) -> (lon+180,-lat)` was a full velocity
reversal. It follows the opposite time branch from the same start point and is
not a same-branch directional-access identity. A failure under that old mapping
must not be interpreted as a mover failure.

## Files

```text
srcEarth/test/C17/AMPS_PARAM_C17_gridless.in
srcEarth/test/C17/reference_C17_symmetry.csv
srcEarth/test/C17/run_C17.py
srcEarth/test/C17/README.md
```

The checked-in CSV is an immutable 300-row mapping contract for five observation
points and the default 30 by 30 degree non-polar sky grid. The runner writes a
generated copy into the output directory and verifies it against the checked-in
table for the default configuration. It never rewrites the source table.

## Default run: direct access

Run from the AMPS repository root:

```bash
python3 srcEarth/test/C17/run_C17.py --amps ./amps --movers BORIS -np 4 -nt 16
```

The runner performs two AMPS calculations:

```text
charge_plus:             SPECIES=PROTON,          CHARGE=+1
charge_minus_reflected:  SPECIES=NEGATIVE_PROTON, CHARGE=-1
mass in both runs:       MASS_AMU=1.0073
```

Using the same mass isolates charge-sign handling from energy/rigidity
conversion. The input explicitly sets `CUTOFF_BACKTRACE_CHARGE SAME`; AMPS does
not apply another charge reversal inside the cutoff calculation.

The default numerical contract is:

```text
FIELD_MODEL                    DIPOLE
DIPOLE_TILT                    0
E field                        off/default
OUTPUT_MODE                    POINTS
observation longitude          0 deg
observation latitudes          -60,-30,0,30,60 deg
observation altitude           9000 km
DIRMAP_LON_RES                 30 deg
DIRMAP_LAT_RES                 30 deg
DIRMAP_COVERAGE                FULL_SPHERE
CUTOFF_SAMPLING                VERTICAL
CUTOFF_SEARCH_ALGORITHM        DIRECT_ACCESS
CUTOFF_RIGIDITY_LIST_GV        0.1,0.2,0.5,1,2,5,10
CUTOFF_DIRECT_ACCESS_ADAPTIVE  F
CUTOFF_TRACE_LIMIT_POLICY      UNRESOLVED
DT_TRACE                       0.25 s
MAX_TRACE_TIME                 2400 s
CUTOFF_MAX_TRAJ_TIME           2400 s
MAX_TRACE_DISTANCE             0 (disabled)
mover                          BORIS
```

`CUTOFF_SAMPLING VERTICAL` is required by the common AMPS input parser whenever
`DIRECT_ACCESS`, `PENUMBRA_SCAN`, or `CUTOFF_RIGIDITY_LIST_GV` is selected. It
does not collapse C17 to one arrival direction: the actual direction-resolved
trajectory set is still defined by `DIRECTIONAL_MAP`, `DIRMAP_*`, and
`DIRMAP_COVERAGE FULL_SPHERE`.

The zero path-distance limit prevents a fixed path cap from becoming an
energy-dependent physical trace-time cutoff. The complete 2400-s budget is
applied directly because the baseline AMPS parser does not recognize the newer
C19-only `CUTOFF_UNRESOLVED_EXTENSION_*` or `TRAP_*` controls. Omitting these
keywords is intentional: unknown AMPS keywords are fatal, and silently assuming
that a newer parser is installed makes the test package unusable with the
baseline executable.

This compatible profile can be more expensive than unresolved-only retry. It
also cannot convert a validated closed drift orbit into a physical forbidden
state without the corresponding AMPS trap-classifier implementation. Remaining
unresolved trajectories are therefore reported honestly rather than relabeled.

Adaptive insertion is disabled so every sky cell contains the same ordered
rigidity nodes. A node is a **stable-interior sample** when both reflected
trajectories are resolved and every available immediately adjacent rigidity
node is also resolved and retains the current state independently on each
charge branch. This removes only nodes touching an observed access transition
or unresolved interval; it does not select samples according to whether the two
charges agree.

At each point the runner requires:

- the complete expected directional grid in both runs;
- no missing, unexpected, or duplicated sky cells;
- identical common rigidity grids in every reflected pair;
- a stable-interior mismatch fraction no larger than 1%;
- stable-interior coverage of at least 20% of all requested samples;
- at least five matching stable `ALLOWED` and five matching stable `FORBIDDEN`
  anchors; and
- a one-sided `UNRESOLVED` fraction no larger than 5%.

The gates are configurable with `--max-access-mismatch-fraction`,
`--max-one-sided-unresolved-fraction`, `--min-stable-sample-fraction`, and
`--min-stable-state-count`. They are applied separately at every observation
point, preventing one problematic latitude from being hidden by the others.

A pair with one unresolved side is a symmetry failure candidate and contributes
to the one-sided unresolved gate. A pair unresolved on both sides is symmetric
as a numerical state but scientifically incomplete; it is reported separately
and excluded from the stable set. The minimum coverage and two-state anchor
requirements prevent a mostly unresolved calculation from passing. Raw
resolved-state and all-state mismatch counts remain diagnostics rather than
acceptance predicates because they are dominated by nodes touching a cutoff
separatrix.

### Why the first direct-access configuration failed

The initial run had 724 of 2100 reflected samples with at least one unresolved
state. The parser-compatible long-trace run reduced this to 251, but converted
many former timeouts into classifications on opposite sides of the numerical
penumbra. Its 200 raw resolved disagreements collapse to one mismatch among
1124 stable-interior samples. Every point retains at least 102 stable samples,
including both allowed and forbidden anchors. This demonstrates why raw binary
equality at an observed transition is not a convergent symmetry observable.

## Legacy scalar-cutoff analysis

`UPPER_SCAN` remains available for compatibility with previous C17 output:

```bash
python3 srcEarth/test/C17/run_C17.py \
  --comparison-mode UPPER_SCAN \
  --skip-run \
  --workdir test_output/C17_gridless
```

The corrected runner recognizes both the new directory name
`charge_minus_reflected` and the former `charge_minus_reversed` name when
analyzing existing output.

A scalar upper cutoff is a lossy reduction of a potentially alternating
penumbra and is more sensitive to numerical separatrices than a direct state at
a declared rigidity. It is therefore a secondary distributional comparison,
not a bitwise equality test. The defaults require at every point:

```text
per-pair Rc relative tolerance                 0.20
minimum finite Rc-pair fraction within limit  0.90
maximum step-transmission mismatch fraction   0.03
```

The absolute fallback is `1e-8 GV`. These values can be changed with
`--rc-rel-tol`, `--min-rc-pass-fraction`,
`--max-transmission-mismatch-fraction`, and `--abs-tol-gv`.

The negative `UPPER_SCAN` value is a documented sentinel: the scan maximum was
forbidden and no allowed upper branch was found in the requested range. A
reflected pair containing the sentinel in both runs is a valid matching state.
A one-sided sentinel or a zero/non-finite cutoff is a hard failure.

## Grid completeness

At 30-degree longitude and latitude resolution the full grid has 84 cells. The
24 polar cells are excluded by default because longitude is degenerate at
`lat=+/-90 deg`, leaving 60 paired cells per observation point and 300 pairs in
total. Use `--include-poles` only when the producer's polar-coordinate contract
is itself under test.

The expected cell set is built independently from the requested resolutions.
Two identically truncated AMPS maps therefore cannot pass simply because their
surviving rows agree.

## Output

Both modes write:

```text
test_output/C17_gridless/C17_summary.csv
test_output/C17_gridless/C17_pairwise_comparison.csv
test_output/C17_gridless/C17_result.json
test_output/C17_gridless/reference_C17_symmetry.csv
```

Direct-access mode additionally writes `C17_pairwise_access_states.csv`. It
labels `stable_interior` and `stable_state_match` for every pair. When provided
by AMPS, it also retains termination code, trace time and distance, step count,
retry count, extension count, and final trace limit for both sides.
Legacy `UPPER_SCAN` mode additionally writes
`C17_pairwise_directional_residuals.csv` and
`C17_residual_histogram.png`.

The old `failed_rc` label has been removed. The scalar summary now distinguishes
coverage errors, invalid values, cutoff-state mismatches, matched sentinels,
finite-pair tolerance exceedances, and transmission-proxy mismatches.

## Useful commands

Confirm that the corrected runner is installed before reusing an existing work
directory:

```bash
python3 srcEarth/test/C17/run_C17.py --version
```

The expected response is `C17 runner schema 5 (2026-08-28)`. Results written by
this release contain `runner_schema_version`, `runner_release`, and
`trace_configuration` in `C17_result.json`. Older results lacking those fields
must not be interpreted using the revised acceptance contract.

Exercise all runner guards without AMPS:

```bash
python3 srcEarth/test/C17/run_C17.py --self-test
```

Prepare both default commands and rendered inputs:

```bash
python3 srcEarth/test/C17/run_C17.py --dry-run --amps ./amps -np 4 -nt 16
```

Run more than one mover:

```bash
python3 srcEarth/test/C17/run_C17.py \
  --movers BORIS,RK4,RK6 --amps ./amps -np 4 -nt 16
```

Use a faster 60-degree longitude smoke grid:

```bash
python3 srcEarth/test/C17/run_C17.py \
  --dir-lon-res 60 --dir-lat-res 30 --amps ./amps -np 2 -nt 8
```

## Interpretation

A persistent DIRECT_ACCESS failure can indicate:

1. the configured species charge is not reaching the mover correctly;
2. the directional longitude convention is inconsistent with the output label;
3. the centered-dipole field evaluation is not symmetric under `y -> -y`;
4. the trajectory classifier handles reflected terminations differently; or
5. too many trajectories remain unresolved under the selected trace budget.

C17 is an internal symmetry regression. It does not establish the absolute
physical correctness of the cutoff values; analytical Størmer tests such as C1
and C12 provide that independent validation.
