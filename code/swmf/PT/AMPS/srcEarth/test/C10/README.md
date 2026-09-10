# C10 — POES/MetOp MEPED measured proton-access-boundary validation

## 1. Purpose

C10 is a real-measurement AMPS validation test for storm-time geomagnetic
proton access at low-Earth-orbit altitude. It is designed as a companion to C9:

- C9 compares AMPS with PAMELA cutoff latitudes.
- C10 compares AMPS with cutoff boundaries extracted from the NOAA POES/MetOp
  Space Environment Monitor-2 (SEM-2), Medium Energy Proton and Electron
  Detector (MEPED), omnidirectional proton channels.

The primary event is the December 2006 solar-particle event and geomagnetic
storm. NOAA-15, NOAA-16, NOAA-17, NOAA-18, and MetOp-02 sampled both polar caps
at approximately 800–850 km altitude. The constellation provides many more
local-time samples than a single spacecraft and permits validation of:

1. equatorward storm-time boundary motion;
2. recovery of the boundary;
3. north/south differences;
4. magnetic-local-time asymmetry; and
5. GRIDLESS versus GRIDDED field-evaluation sensitivity.

C10 does **not** use the synthetic reference table that was present in the
initial scaffold. A scientifically eligible run requires a reference rebuilt
from the historical NOAA/NCEI Level-2 archive.

## 2. Scientific observable

For each MEPED omnidirectional integral-proton channel, C10 now uses a
**background-normalized transmission boundary** on each pass leg:

```text
Tobs(lambda) = (F(lambda) - Fbackground) / (Fpolar - Fbackground)
```

The production boundary is the explicitly bracketed `Tobs = 0.5` crossing. The
implemented algorithm is:

1. convert each 16-second sub-satellite position to AACGM latitude and MLT;
2. split the series into individual single-hemisphere polar passes and inbound/
   outbound legs;
3. estimate `Fpolar` from the median flux at `|AACGM latitude| >= 75 deg`;
4. estimate `Fbackground` independently on each leg from its equatorward
   samples;
5. fit a nondecreasing isotonic profile to the normalized transmission;
6. interpolate `T25`, `T50`, and `T75`, retaining the transition width and fit
   residual as quality diagnostics;
7. reject legs with insufficient background/plateau samples, poor contrast, an
   unbracketed edge crossing, excessive transition width, or excessive
   nonmonotonicity; and
8. retain every accepted crossing in the pass-level product.

The old `0.5 * Fpolar` threshold is available only through
`--crossing-method HALF_POLAR_PLATEAU` for diagnostic sensitivity studies; it
does not produce acceptance-eligible cells. The aggregated reference keeps
one-hour-step cells for plotting, but the default PASS/FAIL gate uses only
nonoverlapping, background-corrected cells with at least two distinct pass legs.

For morphology diagnostics, the runner also fits a three-parameter first
harmonic to invariant colatitude versus MLT. This is a compact representation of
mean expansion, first-order asymmetry amplitude, and phase. It is **not** the
full five-parameter geometric ellipse used in the published Dmitriev model; the
primary C10 pass/fail comparison remains the individual MLT-bin boundary
latitudes.

### Common model observable: `ACCESS_T50`

C10 now follows the same two-product design as C9.  Both the complete
`FULL_SCAN` calculation and the accelerated `DIRECT_ACCESS` calculation write
exact access states at the four MEPED channel rigidities.  The primary model
boundary is built from those states in exactly the same way for both products:

1. retain the common 45°–85° absolute-geodetic-latitude band;
2. transform every shell node to AACGM latitude and MLT at the snapshot epoch;
3. group exact-rigidity longitude profiles by hemisphere and MLT sector;
4. evaluate the resolved allowed-state fraction on a common AACGM-latitude grid;
5. apply weighted nondecreasing isotonic regression; and
6. interpolate the explicitly bracketed transmission crossing at `T=0.5`.

This observable is selected with:

```text
--comparison-observable ACCESS_T50
```

It is the default and is directly comparable between the two numerical modes.
For `FULL_SCAN`, the traditional `Rc_lower`, `Rc_effective`, and `Rc_upper`
contours remain available as diagnostics through `--comparison-observable
RC_LOWER`, `RC_EFFECTIVE`, `RC_UPPER`, or `ALL`.  The older
`--boundary-cutoff effective` option remains as a compatibility alias when no
explicit comparison observable is supplied.

## 3. MEPED channels and nominal rigidities

C10 uses the four high-energy omnidirectional proton channels contained in the
historical 16-second Level-2 files:

| Channel | Nominal integral threshold | Assigned proton rigidity |
|---|---:|---:|
| P6 | >16 MeV | 0.174013525 GV |
| P7 | >36 MeV | 0.262395866 GV |
| P8 | >70 MeV | 0.369131538 GV |
| P9 | >=140 MeV | 0.531334344 GV |

The rigidity is calculated from the nominal lower energy threshold:

```text
R = sqrt(T * (T + 2 mp c^2)) / 1000    [GV for a proton]
```

This mapping is transparent and reproducible, but it is an approximation:
MEPED channels are integral response functions, not monochromatic rigidity
bins. A later high-fidelity extension can convolve the published detector
response with an assumed or independently measured SEP spectrum and assign an
effective rigidity. Every C10 row records the channel, threshold, mapping
method, and source-file checksum so this approximation cannot be mistaken for
an instrument calibration.


### Validation roles of the four channels

C10 deliberately separates code-validation channels from instrument-response
diagnostics:

- `P6` and `P7` are `PRIMARY` and control the default model/data PASS/FAIL
  metrics after background correction and independent-pass quality checks.
- `P8` and `P9` are `DIAGNOSTIC`. Every pass-level crossing is retained, but
  only cells supported by at least two quality-eligible crossings, two pass
  legs, and two spacecraft are used in connected diagnostic means. Sparse or
  cross-channel-outlier cells remain visible as open markers and are written to
  `C10_diagnostic_flags.csv`; they never control PASS/FAIL.

The comparison plots average POES and AMPS over the exact same paired
hemisphere/MLT cells. This prevents a sparse P9 observation from being compared
with an AMPS mean over many unobserved sectors. `C10_result.json` reports robust
and unfiltered per-channel metrics separately, so observational-quality
filtering cannot hide disagreement.

## 4. Can the reference solution be extracted from publications?

**No—not completely. The archive data are required.**

The publications and technical documents are sufficient to determine:

- the instrument design and channel definitions;
- the historical file columns and units;
- the cutoff extraction method;
- the spacecraft constellation used;
- the two-hour/one-hour-step boundary-aggregation methodology;
- known detector and processing limitations; and
- qualitative and fitted-model behavior.

They do **not** provide the complete numerical list of pass-level P6–P9 cutoff
crossings for every satellite, hemisphere, time, and MLT. Figures can be
digitized, and published regression coefficients can reproduce the published
empirical model, but neither is equivalent to an independent measurement
reference. The C10 observational reference must therefore be generated from the
NOAA/NCEI Level-2 archive.

The papers are used to define and justify the extraction algorithm. The archive
files supply the actual numerical measurements.

## 5. Official data source

Use the NOAA National Centers for Environmental Information POES/MetOp SEM
archive:

```text
https://www.ncei.noaa.gov/data/poes-metop-space-environment-monitor/
```

For December 2006 the appropriate product is the historical processed Level-2,
16-second data, version `v01r00`:

```text
access/l2/v01r00/txt/2006/
access/l2/v01r00/cdf/2006/
```

The ASCII product is recommended for C10 because:

- it is directly documented by `readme_16s_ascii.txt`;
- it requires no CDF library;
- it contains the needed time, sub-satellite latitude/longitude, L value, MLT,
  and `mepomp6`–`mepomp9` channels; and
- it is straightforward to inspect and checksum.

CDF input is also supported through `cdflib`.

### Satellites

Download the available daily files for:

```text
n15  NOAA-15
n16  NOAA-16
n17  NOAA-17
n18  NOAA-18
m02  MetOp-02 / MetOp-A
```

MetOp-02 began supplying SEM-2 data in December 2006. The downloader tolerates
missing daily files and records exactly what was obtained.

## 6. Detailed procedure for obtaining the reference data

### 6.1 Install the reference-pipeline dependencies

From the C10 directory:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install -r requirements.txt
```

`aacgmv2` is required to recompute AACGM latitude and MLT consistently.
`cdflib` is needed only when using CDF rather than ASCII.

### 6.2 Download the recommended event interval

The paper studied the longer 5–15 December SEP interval. For the C10 storm test,
the most important interval is 14–16 December. Downloading 5–16 December gives
extra quiet/event context and allows independent inspection:

```bash
python3 download_poes_sem2.py --start 2006-12-05 --end 2006-12-16 --satellites n15,n16,n17,n18,m02 --format txt --output-dir data/reference_source
```

The downloader:

1. begins at the official NCEI 2006 Level-2 directory;
2. recursively reads directory indexes;
3. selects requested spacecraft/date files;
4. downloads them without altering their contents; and
5. writes:

```text
data/reference_source/download_manifest.json
```

The manifest contains each original URL, local path, byte count, and SHA-256.
Do not delete it.

### 6.3 Manual download fallback

Some networks permit file downloads but block recursive directory-index access.
In that case:

1. open the official `txt/2006/` directory in a browser;
2. locate the daily files for `n15`, `n16`, `n17`, `n18`, and `m02`;
3. copy their exact HTTPS URLs into a text file, one URL per line; and
4. run:

```bash
python3 download_poes_sem2.py --url-list poes_urls.txt --format txt --output-dir data/reference_source
```

This path still creates the same checksum manifest.

### 6.4 Verify the archive files before extraction

List the files and inspect the manifest:

```bash
find data/reference_source -maxdepth 2 -type f -print | sort
python3 -m json.tool data/reference_source/download_manifest.json | less
```

For an ASCII file, verify that its header contains at least:

```text
year mo dy hr mi second sslat sslon lval mlt mepomp6 mepomp7 mepomp8 mepomp9
```

NCEI documents the 16-second timestamps as the center of each averaged interval.

### 6.5 Build the pass-level and binned references

```bash
python3 build_poes_reference.py --input-dir data/reference_source --event-start 2006-12-14T00:00:00Z --event-end 2006-12-16T12:00:00Z --crossings-output C10_poes_boundary_crossings.csv --reference-output reference_C10_poes_meped_boundary.csv.gz --manifest-output C10_reference_manifest.json
```

The default extraction settings are:

| Parameter | Default |
|---|---:|
| minimum pass latitude | 45° absolute AACGM |
| polar plateau latitude | 75° absolute AACGM |
| minimum polar samples | 4 |
| minimum background samples per leg | 3 |
| minimum plateau/background ratio | 2 |
| production boundary | background-normalized isotonic T50 |
| maximum T25–T75 transition width | 15° |
| maximum isotonic RMS | 0.35 |
| aggregation window | 2 hours |
| plotting-window step | 1 hour |
| acceptance-window stride | 2 hours (nonoverlapping) |
| MLT-bin width | 3 hours |
| minimum primary P6/P7 crossings per cell | 2 |
| minimum distinct primary pass legs per cell | 2 |
| minimum diagnostic P8/P9 quality-eligible crossings per cell | 2 |
| minimum diagnostic pass legs per cell | 2 |
| minimum diagnostic spacecraft per cell | 2 |
| minimum P8/P9 transition-support samples | 3 |
| minimum P8/P9 robust contrast/noise | 3 |
| P8-minus-P9 outlier rule | median + 4 robust sigma, with 6° fallback |

### 6.6 Products created by the builder

#### `C10_poes_boundary_crossings.csv`

One row per measured pass-leg crossing. Important columns include:

```text
satellite
channel
energy_threshold_mev
assigned_rigidity_gv
hemisphere
pass_id
leg
crossing_time_utc
geographic_lat_deg
geographic_lon_deg
altitude_km
aacgm_lat_deg
mlt_hour
polar_plateau_flux
half_plateau_flux (the actual T50 flux; background + 0.5*(plateau-background))
boundary_uncertainty_deg
quality_flags
source_file
source_sha256
```

This file is the authoritative C10 observational product.

#### `reference_C10_poes_meped_boundary.csv.gz`

The windowed table consumed by `run_C10.py`. It contains every configured
window/channel/hemisphere/MLT cell. Cells without a measurement are retained as:

```text
missing=TRUE
```

The runner excludes missing cells from numerical metrics rather than filling or
interpolating them.

This production reference is always written and kept as gzip-compressed CSV.
`run_C10.py` reads it directly; do not decompress it before a run.  The builder
uses a deterministic gzip header (`mtime=0` and no embedded source filename),
so identical source measurements and processing settings produce identical
compressed bytes and SHA-256 values.  To inspect it without modifying it:

```bash
gzip -cd reference_C10_poes_meped_boundary.csv.gz | head
```

For a previously generated uncompressed reference, either regenerate it with
the updated builder or migrate it once with:

```bash
gzip -n -9 reference_C10_poes_meped_boundary.csv
```

The `-n` option omits the original filename and timestamp from the gzip header.

#### `C10_reference_manifest.json`

Contains:

- extraction configuration;
- source file names, sizes, and SHA-256 values;
- channel threshold and rigidity mapping;
- number of pass-level crossings;
- total and nonmissing reference-cell counts; and
- generation time.

Archive this file with any published or regression result.

### 6.7 Validate the generated reference

```bash
python3 run_C10.py --validate-references --reference reference_C10_poes_meped_boundary.csv.gz
```

The validator checks the complete grid shape, coordinate ranges, and source
labels. It permits explicitly missing cells.

## 7. Quality-control details

### 7.1 P8/P9 subcommutation

Historical P8 and P9 readouts share a telemetry word and alternate. NCEI
published an important notice about older unpacking-software behavior. C10:

- treats zero, negative fill, and unphysical very large values as missing;
- never linearly fills absent P8/P9 samples before finding a crossing; and
- adds a `P8_P9_SUBCOMMUTATED_ARCHIVE_CHANNEL` flag to those crossings.

Use the processed Level-2 files or corrected unpacking software. Do not use the
obsolete buggy C unpacker.

### 7.2 P8/P9 cross-channel and coverage safeguards

P8 and P9 from the same pass leg are compared before aggregation. The builder
computes `delta = |latitude_P8| - |latitude_P9|` and flags an anomalously large
positive separation using a robust median/MAD threshold, never using AMPS to
select or reject observations. A flagged P9 crossing remains in
`C10_poes_boundary_crossings.csv` with `P9_CROSS_CHANNEL_OUTLIER`, but it is
excluded from robust diagnostic aggregation.

Each fitted leg also records the number of samples supporting the central
T20-T80 transition and a nonparametric plateau-minus-background contrast/noise
ratio. P8/P9 crossings that lack transition support or contrast remain available
for inspection but are not quality eligible. Reference cells carry
`diagnostic_eligible`, `quality_status`, eligible-crossing count, outlier count,
and support/noise summaries.

The runner writes `C10_diagnostic_flags.csv` for sparse cells, cross-channel
outliers, and diagnostic residuals larger than 5 degrees. Connected P8/P9 curves
require at least two robust paired cells; isolated estimates are shown as open
markers annotated with their paired-cell count.

### 7.2 Detector degradation

MEPED proton detectors degrade with accumulated radiation dose. This is crucial
for long-term quantitative flux studies. For C10, the production boundary is defined by background-normalized
transmission within each pass, with T50 located after monotonic isotonic fitting.
This reduces—but does not completely eliminate—sensitivity to absolute
calibration and detector degradation. NOAA-15 through
NOAA-17 were already several years old in 2006, while NOAA-18 and MetOp-02 were
newer. The pass-level output preserves spacecraft identity so sensitivity tests
can be run with and without older spacecraft.

### 7.3 Integral-channel interpretation

A half-flux boundary in an integral channel is spectrum dependent. C10's nominal lower-threshold rigidity mapping is adequate for a controlled
code-regression observable in P6/P7, but it is not a complete instrument response
model. P8/P9 therefore remain diagnostic until response folding is implemented. Recommended sensitivity studies are:

```text
P6-P9 nominal threshold mapping
spacecraft subsets
Rc_lower / Rc_effective / Rc_upper
1-hour versus 2-hour windows
3-hour versus 2-hour MLT bins
polar plateau threshold 74° / 75° / 76°
```

### 7.4 Orbit altitude

The historical ASCII documentation emphasizes geographic sub-satellite
position but an altitude field may not be present in every processed file. The
reader uses a file altitude when available; otherwise it uses the explicit
builder default of 850 km. The actual/default altitude is written into the
crossing and binned products.

## 8. AMPS configuration

C10 separates the magnetic-field solver from the cutoff-evaluation product.

### Field-evaluation branches

- `GRIDLESS` evaluates IGRF+T05 directly during trajectory integration.
- `GRIDDED` samples the same field on the Mode3D AMR mesh and uses interpolation.

### Cutoff-evaluation products

- `FULL_SCAN` uses `PENUMBRA_SCAN` on the complete configured shell.  It writes
  `Rc_lower`, `Rc_effective`, and `Rc_upper`, and also writes companion exact-
  rigidity access states at P6–P9.  It supports `GRIDLESS`, `GRIDDED`, or `BOTH`.
- `DIRECT_ACCESS` uses `RIGIDITY_LIST`, traces only P6–P9, and restricts work to
  the configured absolute-geodetic-latitude band.  It is available for
  `GRIDDED` only because the current gridless solver intentionally does not
  implement `RIGIDITY_LIST`.

The default comparison observable for both products is `ACCESS_T50`.  A typical
C10 shell has 91 latitude rows over the full globe, while the 45°–85° direct
band retains roughly 42 rows.  Combining that spatial reduction with four exact
rigidities instead of a 120-point scan reduces the nominal trajectory count by
about a factor of 60–70.  Mesh construction and field initialization are not
removed, so the wall-clock speedup is smaller when those costs dominate.

Recommended defaults:

| Setting | Value |
|---|---:|
| shell altitude | 850 km |
| shell longitude spacing | 15° |
| shell latitude spacing | 2° |
| direct/common latitude band | 45°–85° absolute geodetic |
| full-scan rigidity samples | 120 |
| primary observable | `ACCESS_T50` |
| T50 AACGM grid step | 0.25° |
| minimum resolved profile fraction | 0.66 |
| minimum T50 edge margin | 1° |
| trajectory policy | `ACCURATE` |
| field | IGRF + T05/TS05 |

The test reuses the checksum-verified December-2006 C9 TS05 driver. The runner
looks first for `data/ts05_driving.txt` and then for
`../C9/data/ts05_driving.txt`. The required SHA-256 is checked by the runner.

## 9. Running C10

All commands below are one-line commands run from the C10 directory.

### 9.1 Structural checks

```bash
python3 -m py_compile poes_sem2.py download_poes_sem2.py build_poes_reference.py run_C10.py && python3 -m unittest discover -s tests -v
```

```bash
python3 run_C10.py --validate-references --reference reference_C10_poes_meped_boundary.csv.gz
```

```bash
python3 run_C10.py --validate-driver
```

### 9.2 Fast routine test: `DIRECT_ACCESS`

This is the recommended first production run.  It evaluates the same four
rigidities used by the reference and avoids the complete penumbra scan:

```bash
python3 run_C10.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --comparison-observable ACCESS_T50 --profile ROUTINE --interval-samples 1 --reference reference_C10_poes_meped_boundary.csv.gz --output-root test_output/C10_direct --amps /home/vtenishe/T11/AMPS/amps --shell-lon-res-deg 15 --shell-lat-res-deg 2 --access-abs-lat-min-deg 45 --access-abs-lat-max-deg 85 --t50-grid-step-deg 0.25 --t50-min-resolved-profile-fraction 0.66 --t50-min-edge-margin-deg 1.0 --dynamic-chunk 32 -np 8 -nt 16
```

After a one-snapshot-per-window run is stable, repeat with
`--interval-samples 5` when interval averaging is scientifically needed.

### 9.3 Complete GRIDDED `FULL_SCAN`

Run the complete scan into a separate output tree.  The optional consistency
root compares every exact P6–P9 state against the prior direct run:

```bash
python3 run_C10.py --solver GRIDDED --cutoff-evaluation FULL_SCAN --comparison-observable ACCESS_T50 --profile ROUTINE --interval-samples 1 --reference reference_C10_poes_meped_boundary.csv.gz --output-root test_output/C10_full --access-consistency-root test_output/C10_direct --amps /home/vtenishe/T11/AMPS/amps --cutoff-scan-n 120 --shell-lon-res-deg 15 --shell-lat-res-deg 2 --access-abs-lat-min-deg 45 --access-abs-lat-max-deg 85 --t50-grid-step-deg 0.25 --t50-min-resolved-profile-fraction 0.66 --t50-min-edge-margin-deg 1.0 --min-access-state-agreement 0.999 --max-access-unresolved-fraction 0.01 --dynamic-chunk 32 -np 8 -nt 16
```

### 9.4 Full GRIDLESS/GRIDDED diagnostic run

This retains every `Rc_*` diagnostic and uses `ACCESS_T50` for pass/fail:

```bash
python3 run_C10.py --solver BOTH --cutoff-evaluation FULL_SCAN --comparison-observable ALL --profile ROUTINE --interval-samples 1 --reference reference_C10_poes_meped_boundary.csv.gz --output-root test_output/C10_full_both --amps /home/vtenishe/T11/AMPS/amps --cutoff-scan-n 120 --dynamic-chunk 32 -np 8 -nt 16
```

### 9.5 Smoke test

```bash
python3 run_C10.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --comparison-observable ACCESS_T50 --profile SMOKE --interval-samples 1 --reference reference_C10_poes_meped_boundary.csv.gz --output-root test_output/C10_direct_smoke --amps /home/vtenishe/T11/AMPS/amps --dynamic-chunk 32 -np 4 -nt 8
```

### 9.6 Reprocess existing outputs

Use the same mode and output root that produced the raw files:

```bash
python3 run_C10.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --comparison-observable ACCESS_T50 --profile ROUTINE --interval-samples 1 --reference reference_C10_poes_meped_boundary.csv.gz --skip-run --keep --output-root test_output/C10_direct --amps /home/vtenishe/T11/AMPS/amps -np 8 -nt 16
```

`--skip-run` can change postprocessing and plots, but it cannot add missing shell
nodes, epochs, rigidities, or a missing full-scan companion access product.

## 10. Runner outputs

For each solver branch the runner writes:

```text
C10_comparison.csv
C10_comparison.png
C10_scatter.png
C10_mlt_comparison.png
C10_result.json
C10_snapshot_t50_profiles.csv
C10_access_consistency.json                 # when a counterpart root is supplied
C10_access_consistency_snapshots.csv        # branch-level summary
C10_access_consistency_differences.csv      # only mismatches/missing keys
```

### `C10_comparison.png`

A C9-style two-panel figure:

- top: observed POES/MetOp and modeled AMPS cutoff latitude versus UTC, with one
  color per channel rigidity;
- bottom: `AMPS - POES` residual versus UTC.

Observed points include reference uncertainties when available.

### `C10_scatter.png`

Observed boundary versus modeled boundary with a 1:1 line, separated by
rigidity.

### `C10_mlt_comparison.png`

Mean observed and modeled boundary versus MLT, shown separately for northern
and southern hemispheres.

### `C10_comparison.csv`

One row per selected nonmissing or missing reference cell. It includes:

```text
interval_midpoint_utc
rigidity_gv
channel
hemisphere
mlt_hour
observed_boundary_aacgm_deg
modeled_boundary_aacgm_deg
residual_deg
sigma_deg
n_observed_crossings
observing_satellites
```

## 11. Acceptance metrics

The runner reports:

- valid modeled/reference fraction;
- mean bias;
- mean absolute error;
- RMSE;
- maximum absolute error;
- Pearson correlation;
- observed and modeled low-rigidity expansion;
- time of maximum expansion and timing error; and
- mean observed and modeled MLT-asymmetry amplitude.

Default numerical gates are intentionally initial validation-scale gates:

| Metric | Default requirement |
|---|---:|
| valid fraction | >=0.85 |
| RMSE | <=3° |
| absolute bias | <=2° |
| correlation | >=0.80 |
| modeled low-rigidity expansion | 4°–12° |
| expansion timing error | <=180 min |

These values should be tightened only after the archive-derived reference and
controlled convergence studies establish a defensible baseline.

## 12. Recommended convergence and sensitivity studies

```bash
# Latitude grid
--shell-lat-res-deg 2
--shell-lat-res-deg 1

# Longitude / MLT sampling
--shell-lon-res-deg 15
--shell-lon-res-deg 7.5

# Full-scan penumbra resolution
--cutoff-scan-n 120
--cutoff-scan-n 240

# Common ACCESS_T50 reconstruction
--t50-grid-step-deg 0.25
--t50-grid-step-deg 0.125
--t50-min-resolved-profile-fraction 0.66
--t50-min-resolved-profile-fraction 1.0

# Time averaging
--interval-samples 1
--interval-samples 3
--interval-samples 5

# Product and boundary definition
--cutoff-evaluation DIRECT_ACCESS --comparison-observable ACCESS_T50
--cutoff-evaluation FULL_SCAN --comparison-observable ACCESS_T50
--cutoff-evaluation FULL_SCAN --comparison-observable RC_LOWER
--cutoff-evaluation FULL_SCAN --comparison-observable RC_EFFECTIVE
--cutoff-evaluation FULL_SCAN --comparison-observable RC_UPPER
```

## 13. File inventory

```text
AMPS_PARAM_C10_gridless.in       GRIDLESS AMPS template
AMPS_PARAM_C10_mode3d.in         GRIDDED/Mode3D AMPS template
poes_sem2.py                     archive readers and scientific extraction
download_poes_sem2.py            official archive downloader + checksums
build_poes_reference.py          reference builder
run_C10.py                       AMPS runner, comparison, metrics, plots
requirements.txt                 Python dependencies
REFERENCES.md                    ten core dataset/method references
INSTALL.md                       one-line installation/data/run commands
CHANGES.md                       file-by-file implementation summary
VALIDATION.md                    completed checks and required archive validation
tests/test_reference_pipeline.py offline unit tests
tests/test_runner_plots.py       comparison-plot smoke tests
tests/test_cutoff_modes.py       FULL_SCAN/DIRECT_ACCESS unit tests
data/README.md                   source-data directory instructions
```

## 14. Reproducibility checklist

A scientifically reviewable C10 result should archive all of the following:

```text
original NCEI daily files
download_manifest.json
C10_poes_boundary_crossings.csv
reference_C10_poes_meped_boundary.csv.gz
C10_reference_manifest.json
C10_reference_summary.json
TS05 driver and SHA-256
all generated AMPS_PARAM_C10.in files
all C10_amps.log files
raw AMPS shell/penumbra outputs
C10_commands.json
comparison CSVs, plots, and result JSONs
AMPS git commit and local diff
Python package versions
```

## 15. Known limitations

1. Nominal threshold rigidity is not a full response-function convolution.
2. The omnidirectional detector response is not identical to a set of vertical
   trajectories. `ACCESS_T50` is a transparent access-fraction analogue, not a
   complete detector-response convolution.
3. Some historical products lack explicit altitude; the configured fallback is
   recorded.
4. Sparse MLT coverage produces explicit missing cells.
5. Detector aging can affect relative profiles differently among satellites.
6. A two-hour window assumes that the boundary evolves slowly enough for
   constellation fitting; strong main-phase changes should also be inspected at
   the pass level.
7. The observed half-flux definition depends on the polar-cap plateau being
   well measured during that pass.

These limitations are reasons to retain the crossing-level product and perform
sensitivity tests—not reasons to replace measurements with a synthetic curve.


## 16. References

See [`REFERENCES.md`](REFERENCES.md) for ten core dataset, instrument, calibration, and cutoff-method references, together with a source-by-source assessment of whether each can supply event-level numerical boundaries.

## Improved-reference migration

A reference generated by the older uncorrected half-polar-cap algorithm is not
accepted by the current runner. Rebuild the compressed file after installing
this update:

```tcsh
python build_poes_reference.py \
  --input-dir data/reference_source \
  --crossing-method BACKGROUND_NORMALIZED_ISOTONIC \
  --minimum-background-samples 3 \
  --minimum-crossings-per-cell 2 \
  --minimum-distinct-pass-legs-per-cell 2 \
  --acceptance-window-stride-hours 2 \
  --reference-output reference_C10_poes_meped_boundary.csv.gz

python run_C10.py --validate-references \
  --reference reference_C10_poes_meped_boundary.csv.gz
```

The runner obtains `sym_h_nt` directly from column 9 of the verified C9 TS05
driver at each midpoint; the observational CSV no longer needs to duplicate
that value.

## 17. Validation-integrity plots and model-only convergence

The connected curves in `C10_comparison.png` and `C10_mlt_comparison.png` now
represent the same cells that are allowed to support a validation statement:

- P6/P7 connected curves use only `acceptance_eligible=TRUE` cells;
- P6/P7 `PRIMARY_PLOT_ONLY` cells remain as open markers;
- P8/P9 connected curves use only robust `diagnostic_eligible=TRUE` cells; and
- every POES mean and its AMPS mean use the identical paired row set.

This presentation change does not remove rows and does not change the frozen
P6/P7 acceptance metrics.

Additional branch products are:

```text
C10_driver_audit.json
C10_driver_snapshot_inputs.csv
C10_epoch_bias.csv
C10_epoch_bias.png
C10_mlt_residual_epoch_balanced.csv
C10_mlt_residual_epoch_balanced.png
C10_mlt_decomposition_summary.json
C10_model_convergence.csv                 # with --model-convergence-root
C10_model_convergence.json                # with --model-convergence-root
```

The epoch/MLT decomposition fits

```text
residual(epoch, MLT) = epoch_offset(epoch) + zero_mean_MLT_shape(MLT) + error
```

separately for each channel and hemisphere, using only predeclared acceptance or
robust-diagnostic cells. It makes time-dependent bias and MLT-shape error visible
as separate diagnostics. The fitted terms are never subtracted from AMPS and do
not enter PASS/FAIL.

For temporal sampling, use at least five snapshots across each two-hour
observational window. A defensible convergence check compares five and nine
snapshots with `--model-convergence-root`. For spatial convergence, use the same
option for two runs that differ only in the predeclared mesh or shell resolution.
The comparison uses current-AMPS minus counterpart-AMPS boundaries; it does not
look at which run has a smaller observational residual.

Example convergence gate:

```tcsh
python run_C10.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --profile ROUTINE --interval-samples 9 --output-root test_output/C10_9sample --model-convergence-root test_output/C10_5sample --require-model-convergence --min-model-convergence-common-fraction 0.95 --max-model-convergence-rmse-deg 0.25 --max-model-convergence-abs-deg 0.75 -np 8 -nt 16
```

The runner verifies that both runs identify the same solver, cutoff-evaluation
product, comparison observable, reference SHA-256, and driver SHA-256 before a
model-convergence result can pass.

No event-fitted latitude offset, effective-rigidity adjustment, detector-channel
rescaling, or other empirical correction is implemented. Full detector-response
folding still requires an independently specified official response table and an
independently measured event spectrum. Until that forward operator is available,
the nominal lower-threshold mapping remains explicit and P8/P9 remain diagnostic.

This update modifies runner postprocessing only. It does not require a new
`reference_C10_poes_meped_boundary.csv.gz`.
