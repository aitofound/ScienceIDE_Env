# C9 - PAMELA public-data global-shell cutoff-latitude validation

## 1. Purpose

C9 is the routinely executable, public-data form of the PAMELA December 2006
storm validation. It tests whether the AMPS time-dependent IGRF+T05/TS05
backtracing calculation reproduces the observed equatorward motion and recovery
of the low-Earth-orbit proton cutoff boundary. The same observable is evaluated
through two explicit solver branches: GRIDLESS direct field evaluation and the
GRIDDED standalone Mode3D mesh/interpolation path.

The observational reference is **Supporting Information Table S1** from:

> Adriani, O., et al. (2016), *PAMELA's measurements of geomagnetic cutoff
> variations during the 14 December 2006 storm*, Space Weather, 14, 210-220,
> doi:10.1002/2016SW001364.

Table S1 states that its values are geomagnetic cutoff **AACGM latitudes**, with
statistical uncertainties, and that each listed date is the midpoint of a
94-minute interval. The checked-in reference CSV contains all 37 intervals and
all seven published rigidity bins; the single missing table cell remains
missing and is never interpolated.

C9 intentionally avoids a claim that public information is sufficient to
recreate PAMELA's exact event geometry. The public test does **not** need:

- definitive Resurs-DK1 ephemeris;
- spacecraft attitude quaternions;
- the PAMELA-to-spacecraft mounting matrix;
- individual PAMELA track directions.

Instead, it compares the published orbit-averaged product with an AMPS global
shell evaluated at the representative PAMELA altitude.

## 2. Fixed physical defaults

The agreed routine defaults are:

| Quantity | C9 default |
|---|---:|
| Shell altitude | 475 km |
| Incidence | vertical protons |
| Rigidity representation | geometric center of each Table-S1 bin |
| Field model | IGRF + T05/TS05 |
| Driver cadence | 5 minutes |
| Cutoff evaluation | `FULL_SCAN` by default; optional fast GRIDDED `DIRECT_ACCESS` |
| Backtrace convention | reversed velocity and charge |
| Trace-limit policy | forbidden |
| Geographic shell spacing | 30 deg longitude x 2 deg latitude |
| Routine temporal sampling | one field snapshot at each Table-S1 midpoint |
| Primary observable | longitude-averaged, hemisphere-resolved `PAMELA_T50` |
| Solver selection | `BOTH` for `FULL_SCAN`; `GRIDDED` for `DIRECT_ACCESS` |
| Mode3D near-Earth mesh target | 0.02 Re |
| Mode3D boundary mesh target | 2.0 Re |
| Mode3D coarsening | `LINEAR`, exponent 1.0 |

The seven geometric-center rigidities are approximately:

```text
0.423556372 GV
0.498397432 GV
0.587877538 GV
0.692820323 GV
0.817006732 GV
0.962081078 GV
1.131017241 GV
```

The runner calculates the exact centers from the reference CSV rather than
copying rounded values from this README.



## 3. Numerical products and solver branches

C9 separates the **scientific comparison observable** from the numerical way
that fixed-rigidity access states are obtained. Both products now use the same
primary observable, `PAMELA_T50`, so their PASS/FAIL results are directly
comparable.

### Common primary observable: `PAMELA_T50`

For each of the seven exact Table-S1 rigidity centers, AMPS writes one access
state at every retained shell node:

```text
0 = PHYSICAL_FORBIDDEN
1 = ALLOWED
2 = UNRESOLVED
```

Before AACGM conversion, the Python postprocessor restricts both products to
the same configured absolute-geodetic-latitude band. This is required because
`FULL_SCAN` contains the complete shell whereas `DIRECT_ACCESS` calculates only
the retained band; fitting different latitude domains can move an isotonic
crossing even when the common access states are identical. The retained nodes
are then converted to AACGM at the exact epoch, used to construct a
longitude-averaged transmission profile separately in the north and south
hemispheres, fitted by weighted nondecreasing isotonic regression, and
interpolated at transmission 0.5. Both hemispheres must explicitly bracket the
crossing, and each boundary must remain at least
`--t50-min-edge-margin-deg` inside the retained AACGM coverage. The final
modeled latitude is the median of the two hemisphere T50 values.

The same postprocessing function is used for `FULL_SCAN` and `DIRECT_ACCESS`.
This removes the previous definition mismatch in which the full scan used
`Rc_effective = R` while direct access used the first individual longitude
transition.

### `FULL_SCAN` - complete penumbra plus exact PAMELA access

`FULL_SCAN` runs the configured `PENUMBRA_SCAN` over the complete shell. In
addition to the regular scan, the solver independently classifies the seven
exact PAMELA rigidity centers. This produces both the traditional penumbra
quantities and the fixed-rigidity access states required by `PAMELA_T50`.

Raw products:

```text
GRIDLESS: cutoff_gridless_shells_penumbra.dat
          cutoff_gridless_shells_pamela_access.dat
GRIDDED : cutoff_3d_shells_penumbra.dat
          cutoff_3d_shells_pamela_access.dat
```

`Rc_lower`, `Rc_effective`, and `Rc_upper` remain available as diagnostic
observables through `--comparison-observable`, but `PAMELA_T50` is the default
and is used for the primary scientific pass/fail decision.

With 160 regular scan values, the nominal coarse work is up to 167 rigidity
classifications per shell location before any penumbra refinement:

```text
1092 locations x (160 scan values + 7 exact PAMELA values)
= 182,364 nominal classifications per snapshot
```

### `DIRECT_ACCESS` - efficient GRIDDED product

`DIRECT_ACCESS` launches `RIGIDITY_LIST` in standalone Mode3D. It traces only
the seven exact PAMELA rigidities and retains shell nodes satisfying

```text
access_abs_lat_min <= |geodetic latitude| <= access_abs_lat_max
```

while keeping both hemispheres and all configured longitudes. The default
35-75 degree band retains 480 of 1092 nodes for the 30-degree x 2-degree shell:

```text
480 locations x 7 rigidities = 3,360 trajectories per snapshot
```

This is approximately a 54-fold nominal reduction relative to the complete
160+7 full-shell calculation. `DIRECT_ACCESS` is intentionally GRIDDED-only.
Its raw product is:

```text
cutoff_3d_shells_access.dat
```

The direct file and both full-scan companion files use the same long-form schema
and redundant state flags; the parser cross-checks them before scientific
postprocessing. For the primary T50 reduction, `FULL_SCAN` is filtered to the
same geodetic band used by `DIRECT_ACCESS`; the complete full-shell files remain
unchanged and continue to support global maps and Rc diagnostics.

### Field-evaluation branches

`GRIDLESS` evaluates IGRF+T05 directly along trajectories. `GRIDDED` samples the
field onto the standalone Mode3D AMR mesh and uses mesh interpolation during
backtracing. `--solver BOTH` independently compares both branches with PAMELA
and writes GRIDDED-minus-GRIDLESS diagnostics. `DIRECT_ACCESS` requires
`--solver GRIDDED`.

## 4. Postprocessing and boundary extraction

### Common `PAMELA_T50` algorithm

For each rigidity and hemisphere, access rows are grouped into independent
longitude profiles and sampled on a common absolute-AACGM latitude grid. At
each latitude, the raw transmission is the mean of all resolved longitude
contributions:

```text
T_raw = sum(access_state for resolved longitudes) / N_resolved
```

A grid point is excluded when the resolved-longitude fraction is below
`--t50-min-resolved-longitude-fraction` (default 0.80). C9 never interpolates
through an unresolved trajectory.

Finite longitude sampling and real access islands can make the raw transmission
locally nonmonotonic. The runner therefore uses a weighted pool-adjacent-
violators algorithm to obtain the least-squares nondecreasing transmission
profile. The weight at each latitude is the number of resolved longitudes. The
T=0.5 crossing is linearly interpolated from the fitted profile; the center of
a finite T=0.5 plateau is used when one occurs.

North and south are processed independently. A primary boundary is valid only
when both hemispheres explicitly begin below and end above T=0.5, and when the
crossing is separated from both retained profile edges by at least the configured
AACGM margin. C9 records the raw and isotonic boundaries, isotonic adjustment
RMS, unresolved fraction, resolved-longitude coverage, bracket status, edge
margin, north-south difference, and the number of individual longitude profiles
with more than one resolved state change.

The default fitting grid is 0.25 degree AACGM and can be changed with
`--t50-grid-step-deg`. The default edge margin is 1 degree AACGM and can be
changed with `--t50-min-edge-margin-deg`. These controls affect only
postprocessing quality checks and interpolation; the underlying trajectory
latitude spacing remains `--shell-lat-res-deg`.

### Optional FULL_SCAN/DIRECT_ACCESS state-consistency gate

The strongest check of method consistency is performed before AACGM conversion
or T50 fitting. Run one product, then point the other product at the first output
root with `--access-consistency-root`. For every matching GRIDDED snapshot, C9 compares the raw three-state
classification at the exact same longitude, geodetic latitude, and PAMELA
rigidity. It also compares all common trajectory/mesh directives in the two
generated `AMPS_PARAM_C9.in` files, verifies that their copied driver files have
the same SHA-256 digest, and checks the common command-line controls recorded in
`C9_amps.log`. Product-specific controls such as `PENUMBRA_SCAN` versus
`RIGIDITY_LIST` are intentionally excluded.

Resolved rows must agree at or above `--min-access-state-agreement` (default
0.999), while the fraction unresolved in either product must not exceed
`--max-access-unresolved-fraction` (default 0.01). Missing keys or configuration differences are always a failure because they
indicate different shell settings, rigidity lists, selected epochs,
`--interval-samples`, drivers, movers, trace controls, or mesh settings. When
requested, this consistency check is a numerical acceptance gate in addition to
the independent PAMELA metrics.

The comparison deliberately uses a separately generated output root instead of
launching both expensive calculations automatically. This keeps runtime and
failure provenance clear while still requiring identical epoch/sample directory
names.

### Full-scan diagnostic contours

`FULL_SCAN` also reads `Rc_lower_GV`, `Rc_effective_GV`, `Rc_upper_GV`, and
penumbra topology by column name from the Tecplot `VARIABLES` record. These can
be selected with:

```text
--comparison-observable RC_LOWER
--comparison-observable RC_EFFECTIVE
--comparison-observable RC_UPPER
--comparison-observable ALL
```

`ALL` writes every diagnostic boundary product but retains `PAMELA_T50` for the
primary comparison. Rc contours are diagnostics and no longer substitute for
the PAMELA-like T50 observable.

### Geographic-to-AACGM conversion and temporal aggregation

For every retained shell point, `run_C9.py` calls `aacgmv2` at the exact
snapshot UTC and 475-km altitude. AACGM failures near the magnetic equator are
ignored because the validation boundary lies in the mid/high-latitude band.

Install the dependency with:

```bash
python3 -m pip install -r srcEarth/test/C9/requirements.txt
```

For `--interval-samples N > 1`, the modeled Table-S1 interval is the arithmetic
mean of the N independently evaluated snapshot boundaries. `N` applies to every
selected PAMELA interval and every selected solver branch. Before execution,
the runner prints the full launch arithmetic, for example:

```text
7 intervals x 5 samples/interval x 1 solver branch = 35 AMPS launches
```

Each command includes an explicit `--epoch`, and the generated input file stores
the same `EPOCH` directive.

## 5. Reference data

`reference_C9_pamela_table_s1.csv` contains 259 rows:

```text
37 interval midpoints x 7 rigidity bins = 259
```

There are 258 numerical observations and one missing value. Columns include:

```text
interval_midpoint_utc
interval_start_utc
interval_end_utc
rigidity_min_gv
rigidity_max_gv
rigidity_geometric_center_gv
pamela_cutoff_aacgm_deg
sigma_plus_deg
sigma_minus_deg
missing
source
source_pdf_sha256
notes
```

The 94-minute start/end fields are midpoint +/-47 minutes. They are metadata for
temporal averaging; the original table prints only the midpoint.

Validate the complete transcription without AMPS:

```bash
python3 srcEarth/test/C9/run_C9.py --validate-references
```

For audit/reproduction, `tools/extract_table_s1.py` recreates the CSV from
`swe20314-sup-0001-supinfo.pdf`. Runtime execution does not depend on Camelot or
on the PDF.

## 6. TS05 driver bundled with C9

C9 uses the checked-in five-minute event driver:

```text
srcEarth/test/C9/data/ts05_driving.txt
```

Its columns are:

```text
UTC Bx By Bz Vx Vy Vz Np Temp SYM-H IMFflag SWflag Tilt Pdyn W1 W2 W3 W4 W5 W6
```

The file covers `2006-12-14T00:00:00` through
`2006-12-17T00:00:00` at five-minute cadence and includes the abrupt solar-wind
and IMF change near 14 December 14:10 UT.  The exact file supplied for C9 is
protected by this SHA-256 digest:

```text
cb3f3f1959763660beb1e26e5a49489b132708944fb91c4e1ee37cfc3a6c4317
```

The runner uses this file automatically; `--driver` is not required.  It checks:

1. timestamp plus 19 numerical fields on every data row;
2. strictly increasing UTC times;
3. nominal five-minute median cadence;
4. no gap larger than ten minutes;
5. complete coverage of every requested field snapshot; and
6. agreement with the checked-in SHA-256 digest.

Validate the bundled driver without running AMPS:

```bash
python3 srcEarth/test/C9/run_C9.py --validate-driver
```

An alternate driver may be selected with either:

```bash
--driver /path/to/driver.txt
```

or:

```bash
export C9_TS05_DRIVER=/path/to/driver.txt
```

An alternate driver is scientifically eligible when it either matches the
bundled checksum or contains provenance produced by
`tools/prepare_official_ts05_driver.py`.  A structurally valid but unverified
alternate driver is accepted only with `--allow-unverified-driver`, and the
result JSON is marked `scientific_validation_eligible=false`.

## 7. Execution profiles

### `SMOKE`

Two Table-S1 intervals:

```text
2006-12-14T14:31:00Z  shock interval
2006-12-15T03:03:00Z  cutoff minimum
```

This verifies input generation, driver interpolation, shell output, AACGM
conversion, contour extraction, and comparison logic. It is not the default
scientific regression.

### `ROUTINE` - default

Seven representative intervals:

```text
2006-12-14T09:49:00Z  pre-shock reference
2006-12-14T14:31:00Z  shock interval
2006-12-14T16:05:00Z  early storm response
2006-12-14T23:55:00Z  main-phase development
2006-12-15T03:03:00Z  observed minimum cutoff latitude
2006-12-15T09:19:00Z  recovery
2006-12-16T04:08:00Z  late recovery
```

One TS05 field snapshot is used at each published midpoint unless
`--interval-samples` is increased.

### `FULL`

All 37 Table-S1 intervals. Use this for publication-quality reproduction or
sensitivity work rather than frequent source regression.

Example with five snapshots per 94-minute interval:

```bash
python3 srcEarth/test/C9/run_C9.py \
  --profile FULL \
  --interval-samples 5 \
  -np 32
```

Custom Table-S1 midpoints can be selected with a comma-separated list:

```bash
--timestamps 2006-12-14T14:31:00Z,2006-12-15T03:03:00Z
```

## 8. Commands

Run the efficient GRIDDED regression product at one midpoint per interval:

```bash
python3 srcEarth/test/C9/run_C9.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --comparison-observable PAMELA_T50 --profile ROUTINE --interval-samples 1 --access-abs-lat-min-deg 35 --access-abs-lat-max-deg 75 --output-root test_output/C9_direct --amps /home/vtenishe/T11/AMPS/amps --shell-lon-res-deg 30 --shell-lat-res-deg 2 --t50-grid-step-deg 0.25 --t50-min-resolved-longitude-fraction 0.8 --dynamic-chunk 64 -np 4 -nt 16
```

Run the complete GRIDDED penumbra scan with the same primary T50 observable:

```bash
python3 srcEarth/test/C9/run_C9.py --solver GRIDDED --cutoff-evaluation FULL_SCAN --comparison-observable PAMELA_T50 --profile ROUTINE --interval-samples 1 --output-root test_output/C9_full --amps /home/vtenishe/T11/AMPS/amps --cutoff-scan-n 160 --shell-lon-res-deg 30 --shell-lat-res-deg 2 --t50-grid-step-deg 0.25 --t50-min-resolved-longitude-fraction 0.8 --dynamic-chunk 64 -np 4 -nt 16
```

Verify that the complete scan and direct product make the same raw access
decisions. The DIRECT_ACCESS output must already exist with the same timestamps,
`--interval-samples`, shell geometry, exact rigidity list, mesh settings, mover,
and trace policy:

```bash
python3 srcEarth/test/C9/run_C9.py --solver GRIDDED --cutoff-evaluation FULL_SCAN --comparison-observable PAMELA_T50 --profile ROUTINE --interval-samples 1 --output-root test_output/C9_full_verified --access-consistency-root test_output/C9_direct --amps /home/vtenishe/T11/AMPS/amps --cutoff-scan-n 160 --shell-lon-res-deg 30 --shell-lat-res-deg 2 --access-abs-lat-min-deg 35 --access-abs-lat-max-deg 75 --t50-grid-step-deg 0.25 --t50-min-resolved-longitude-fraction 0.8 --t50-min-edge-margin-deg 1.0 --min-access-state-agreement 0.999 --max-access-unresolved-fraction 0.01 --dynamic-chunk 64 -np 4 -nt 16
```

The same check can be added while reprocessing an existing full-scan tree:

```bash
python3 srcEarth/test/C9/run_C9.py --solver GRIDDED --cutoff-evaluation FULL_SCAN --comparison-observable PAMELA_T50 --profile ROUTINE --interval-samples 1 --skip-run --keep --output-root test_output/C9_full --access-consistency-root test_output/C9_direct
```

Write T50 plus all full-scan Rc diagnostics without changing the primary
pass/fail observable:

```bash
python3 srcEarth/test/C9/run_C9.py --solver GRIDDED --cutoff-evaluation FULL_SCAN --comparison-observable ALL --profile ROUTINE --interval-samples 1 --output-root test_output/C9_all_observables --amps /home/vtenishe/T11/AMPS/amps --cutoff-scan-n 160 --dynamic-chunk 64 -np 4 -nt 16
```

Run a five-sample diagnostic of only the storm minimum interval:

```bash
python3 srcEarth/test/C9/run_C9.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --comparison-observable PAMELA_T50 --timestamps 2006-12-15T03:03:00Z --interval-samples 5 --output-root test_output/C9_0303_5samples --amps /home/vtenishe/T11/AMPS/amps --dynamic-chunk 64 -np 4 -nt 16
```

Preview commands and the launch count without running AMPS:

```bash
python3 srcEarth/test/C9/run_C9.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --comparison-observable PAMELA_T50 --profile SMOKE --interval-samples 5 --dry-run --output-root test_output/C9_direct_dry -np 4 -nt 16
```

Use `--skip-run --keep` with the same product, observable, sample count, and
output root to reprocess existing raw files.

## 9. Numerical controls

Useful options are:

```text
--solver GRIDLESS|GRIDDED|BOTH
--cutoff-evaluation FULL_SCAN|DIRECT_ACCESS
--comparison-observable PAMELA_T50|RC_LOWER|RC_EFFECTIVE|RC_UPPER|ALL
-np 4
-nt 16
--scheduler DYNAMIC
--dynamic-chunk 0
--mode3d-mesh-res-earth-re 0.02
--mode3d-mesh-res-boundary-re 2.0
--mode3d-mesh-coarsening LINEAR
--mode3d-mesh-exponent 1.0
--cutoff-scan-n 160                 # FULL_SCAN only
--access-abs-lat-min-deg 35          # common T50 domain; also DIRECT C++ work band
--access-abs-lat-max-deg 75          # common T50 domain; also DIRECT C++ work band
--t50-grid-step-deg 0.25
--t50-min-resolved-longitude-fraction 0.80
--t50-min-edge-margin-deg 1.0
--access-consistency-root test_output/C9_direct
--min-access-state-agreement 0.999
--max-access-unresolved-fraction 0.01
--rigidity-min-gv 0.30
--rigidity-max-gv 1.35
--shell-lon-res-deg 30
--shell-lat-res-deg 2
--max-trace-time 20
--mover BORIS
```

The rigidity bracket must cover every Table-S1 geometric center with margin.
The default 0.30-1.35 GV bracket gives a linear scan spacing of about 0.0066 GV
for 160 samples.

Recommended convergence checks are:

```bash
# Latitude-grid sensitivity
--shell-lat-res-deg 2
--shell-lat-res-deg 1

# Longitude sampling
--shell-lon-res-deg 30
--shell-lon-res-deg 15

# FULL_SCAN rigidity scan
--cutoff-scan-n 160
--cutoff-scan-n 320

# DIRECT_ACCESS latitude band and resolution
--access-abs-lat-min-deg 35 --access-abs-lat-max-deg 75
--shell-lat-res-deg 2
--shell-lat-res-deg 1

# Common-observable method comparison
--cutoff-evaluation DIRECT_ACCESS --comparison-observable PAMELA_T50
--cutoff-evaluation FULL_SCAN --comparison-observable PAMELA_T50

# Exact state-by-state method comparison (run second product with first root)
--access-consistency-root test_output/C9_direct
--min-access-state-agreement 0.999
--max-access-unresolved-fraction 0.01

# Full-scan definition diagnostics
--comparison-observable ALL

# Temporal averaging
--interval-samples 1
--interval-samples 5
--interval-samples 19
```

## 10. Acceptance metrics

The routine defaults are intentionally looser than PAMELA's statistical
uncertainties because C9 is a global-shell approximation, not an exact
spacecraft-acceptance reproduction.

| Metric | Default requirement |
|---|---:|
| Valid modeled/reference fraction | >= 0.85 |
| Overall latitude RMSE | <= 3.0 deg |
| Absolute mean bias | <= 2.0 deg |
| PAMELA/AMPS correlation | >= 0.80 |
| Lowest-bin storm suppression | 4-9 deg |
| Time of lowest-bin minimum | within 100 min |
| Resolved FULL_SCAN/DIRECT_ACCESS state agreement | >= 0.999 when consistency root is supplied |
| Unresolved in either compared access product | <= 0.01 when consistency root is supplied |

All thresholds are CLI-configurable. The runner also reports:

- mean absolute and maximum absolute error;
- uncertainty-normalized residuals;
- fraction inside PAMELA 1-sigma and 2-sigma intervals;
- observed and modeled low-rigidity suppression;
- observed and modeled time of minimum;
- longitude/hemisphere and temporal scatter.

The statistical uncertainties are diagnostics, not the sole pass criterion:
they do not include global-shell geometry, rigidity-bin, magnetic-field-model,
or temporal-sampling systematics.

## 11. Output products

Top-level output defaults to `test_output/C9`:

```text
C9_reference_used.csv
C9_driver_info.json
C9_commands.json
C9_result.json
driver/ts05_driver.txt
gridless/
gridded/
C9_cross_solver_comparison.csv   # written for --solver BOTH
C9_cross_solver_result.json      # written for --solver BOTH
```

Each selected branch contains its own `C9_model.csv`, `C9_comparison.csv`,
`C9_comparison.png`, `C9_result.json`, command inventory, interval directories,
and snapshot directories.  In `C9_comparison.png`, PAMELA and AMPS use the
same color for the same rigidity; dashed circles identify PAMELA and solid
x-marked curves identify AMPS.  PAMELA Table S1 statistical uncertainties are
shown as error bars.  A snapshot contains:

```text
AMPS_PARAM_C9.in
C9_amps.log
cutoff_gridless_shells_penumbra.dat       # GRIDLESS FULL_SCAN diagnostics
cutoff_gridless_shells_pamela_access.dat   # GRIDLESS FULL_SCAN T50 states
cutoff_3d_shells_penumbra.dat             # GRIDDED FULL_SCAN diagnostics
cutoff_3d_shells_pamela_access.dat         # GRIDDED FULL_SCAN T50 states
cutoff_3d_shells_access.dat                # GRIDDED DIRECT_ACCESS T50 states
C9_snapshot_boundaries.csv                 # selected primary observable
C9_snapshot_boundaries_pamela_t50.csv
C9_snapshot_boundaries_rc_lower.csv        # FULL_SCAN / ALL
C9_snapshot_boundaries_rc_effective.csv    # FULL_SCAN / ALL
C9_snapshot_boundaries_rc_upper.csv        # FULL_SCAN / ALL
C9_snapshot_t50_profiles.csv               # raw and isotonic T profiles
C9_access_consistency.json                    # optional per-snapshot raw-state check
C9_access_consistency_differences.csv         # optional mismatches/missing keys
```

When `--access-consistency-root` is used, the GRIDDED branch additionally writes
`C9_access_consistency.json`, `C9_access_consistency_snapshots.csv`, and
`C9_access_consistency_differences.csv` at the branch root. The final branch
PASS requires both the PAMELA comparison and the configured access-state gate.

The exact driver, generated input, command line, and comparison settings are
therefore archived with every run.

## 12. Interpretation and limitations

C9 answers this question:

> Do the GRIDLESS direct-field and GRIDDED Mode3D-mesh implementations of the
> time-dependent AMPS IGRF+TS05 global vertical-access boundary at 475 km reproduce the rigidity dependence, timing, magnitude, and recovery of
> the published PAMELA AACGM cutoff-latitude product?

It does not answer the stricter question:

> Can AMPS reproduce every individual PAMELA event using exact Resurs-DK1
> position, attitude, detector acceptance, and reconstructed track direction?

That stricter future test is C9C. A map-comparison extension using published
PAMELA global cutoff maps can be treated as C9B.

Other known approximations are:

- one geometric-center rigidity represents each finite PAMELA rigidity bin;
- `FULL_SCAN` and `DIRECT_ACCESS` use the same longitude-averaged `PAMELA_T50` observable and the same configured geodetic fitting band; Rc contours are diagnostic only;
- default temporal sampling uses the interval midpoint only;
- longitude and both hemispheres are aggregated instead of following the orbit;
- both T50 products require the selected geodetic latitude band to bracket T=0.5 in both hemispheres and to leave the requested AACGM edge margin; otherwise the boundary is invalid;
- the isotonic T50 estimate reduces longitude-sampling reversals but does not reproduce PAMELA detector acceptance or its exact orbit;
- geodetic vertical is used rather than the exact instrument boresight;
- the GRIDDED branch adds finite mesh resolution and interpolation error, which
  must be assessed with the supplied mesh controls.

These limitations are recorded in `C9_result.json` and should be retained in
reports that use the test.

## 13. Source files

```text
AMPS_PARAM_C9_gridless.in
AMPS_PARAM_C9_mode3d.in
run_C9.py
reference_C9_pamela_table_s1.csv
requirements.txt
tools/extract_table_s1.py
tools/prepare_official_ts05_driver.py
data/README.md
data/ts05_driving.txt
```
