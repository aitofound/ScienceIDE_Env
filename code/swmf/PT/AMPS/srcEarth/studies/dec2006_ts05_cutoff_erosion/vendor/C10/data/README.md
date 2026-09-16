# C10 observational data and reference-solution construction

## 1. Purpose

This directory contains the external observational inputs used by the C10
validation test and the provenance records needed to reproduce its numerical
reference solution.

C10 compares AMPS proton-access boundaries with boundaries extracted from the
historical NOAA/NCEI POES/MetOp Space Environment Monitor (SEM-2) Level-2
archive. The publications listed in [`../REFERENCES.md`](../REFERENCES.md)
describe the instruments, channels, calibration issues, and cutoff-boundary
methods, but they do not provide the complete pass-level numerical boundary
list used by C10. The actual C10 reference must therefore be generated from the
archived measurements.

The file used directly by `run_C10.py` is:

```text
srcEarth/test/C10/reference_C10_poes_meped_boundary.csv.gz
```

It must remain gzip-compressed. The builder writes it directly as deterministic
gzip text, and the runner reads it without creating an uncompressed copy.

---

## 2. Quick start

Run all commands from:

```text
<AMPS>/srcEarth/test/C10
```

For the checkout used in the examples below:

```tcsh
cd ~/T11/AMPS/srcEarth/test/C10
```

### 2.1 Activate the C10 Python environment

```tcsh
source .venv/bin/activate.csh
```

If the environment has not been created yet, see Section 5.

### 2.2 Download the source measurements

```tcsh
python download_poes_sem2.py \
  --start 2006-12-05 \
  --end 2006-12-16 \
  --satellites n15,n16,n17,n18,m02 \
  --format txt \
  --output-dir data/reference_source
```

### 2.3 Check the downloaded files

```tcsh
find data/reference_source -type f -size 0 -print
python -m json.tool data/reference_source/download_manifest.json | less
```

The first command should normally print nothing. Section 8 explains how to
recover from empty, truncated, or non-data files.

### 2.4 Build the reference solution
### 2.4a Production processing and validation roles

The default builder does **not** use an uncorrected `0.5 * polar_flux`
threshold. For each pass leg it estimates an equatorward background, normalizes
the flux between that background and the polar plateau, fits a monotonic
isotonic profile, and locates `T=0.25`, `0.50`, and `0.75`. P6/P7 form the
primary code-validation gate. P8/P9 are preserved as diagnostics because the
current model still assigns their nominal lower-threshold rigidities rather than
folding the full detector response.


```tcsh
python build_poes_reference.py \
  --input-dir data/reference_source \
  --event-start 2006-12-14T00:00:00Z \
  --event-end 2006-12-16T12:00:00Z \
  --crossings-output C10_poes_boundary_crossings.csv \
  --reference-output reference_C10_poes_meped_boundary.csv.gz \
  --manifest-output C10_reference_manifest.json \
  --summary-output C10_reference_summary.json
```

### 2.5 Verify the generated reference

```tcsh
gzip -t reference_C10_poes_meped_boundary.csv.gz
gzip -cd reference_C10_poes_meped_boundary.csv.gz | head -10
python -m json.tool C10_reference_summary.json
python run_C10.py --validate-references \
  --reference reference_C10_poes_meped_boundary.csv.gz
python run_C10.py --validate-driver
```

A successful reference contains numerical rows after the comment block and CSV
header. A compressed five-line placeholder is not a completed reference.

---

## 3. Files and directory layout

The expected layout after downloading and building is:

```text
srcEarth/test/C10/
├── data/
│   ├── README.md
│   ├── reference_source/
│   │   ├── download_manifest.json
│   │   └── NOAA/NCEI daily Level-2 files
│   └── ts05_driving.txt                  optional local copy
├── C10_poes_boundary_crossings.csv       generated pass-level crossings
├── reference_C10_poes_meped_boundary.csv.gz
│                                           generated comparison reference
├── C10_reference_manifest.json           generation provenance
├── C10_reference_summary.json            generation summary
└── C10_build_reference.log               optional captured build log
```

The roles are different:

- `data/reference_source/` contains the original downloaded daily files.
- `C10_poes_boundary_crossings.csv` contains individual observed boundary
  crossings extracted from satellite passes.
- `reference_C10_poes_meped_boundary.csv.gz` contains the time/rigidity/
  hemisphere/MLT cells consumed by `run_C10.py`.
- `download_manifest.json` describes network acquisition.
- `C10_reference_manifest.json` describes the exact sources and scientific
  settings used to construct the reference.
- `C10_reference_summary.json` provides row counts, coverage, checksums, and a
  compact integrity check.

Do not treat the aggregated `.csv.gz` file as a raw measurement product. It is a
reproducible reduction of the pass-level crossing table.

---

## 4. Official measurement source

Use the NOAA National Centers for Environmental Information POES/MetOp Space
Environment Monitor archive:

```text
https://www.ncei.noaa.gov/products/poes-metop-space-environment-monitor
```

The C10 downloader uses the historical Level-2 version `v01r00` archive. For
2006, the direct year directories are:

```text
https://www.ncei.noaa.gov/data/poes-metop-space-environment-monitor/access/l2/v01r00/txt/2006/
https://www.ncei.noaa.gov/data/poes-metop-space-environment-monitor/access/l2/v01r00/cdf/2006/
```

The recommended C10 source is the 16-second ASCII Level-2 product because it is
human-readable and does not require a CDF library. The CDF path can be used when
an existing local CDF mirror is available.

### 4.1 Spacecraft used

The standard download requests the historical archive tokens:

| Token | Spacecraft |
|---|---|
| `n15` | NOAA-15 |
| `n16` | NOAA-16 |
| `n17` | NOAA-17 |
| `n18` | NOAA-18 |
| `m02` | MetOp-02 / MetOp-A |

### 4.2 Time ranges

Two intervals are intentionally different:

```text
Download interval: 2006-12-05 through 2006-12-16, inclusive
Reference interval: 2006-12-14 00:00 UTC through 2006-12-16 12:00 UTC
```

The wider download interval preserves event context and permits independent
inspection. The builder filters the observations to the narrower scientific
comparison interval.

A request for 12 days and five spacecraft can produce as many as 60 daily files.
A smaller number is not automatically a failure; archive availability can vary
by spacecraft and day. The scientific checks are the resulting observation,
crossing, and populated-reference-cell counts.

---

## 5. Python setup without administrator privileges

### 5.1 Downloader requirements

`download_poes_sem2.py` uses only the Python standard library. It can download
and checksum the source files before the scientific Python dependencies are
installed.

### 5.2 Recommended setup for `/bin/tcsh`

```tcsh
cd ~/T11/AMPS/srcEarth/test/C10
python3 -m pip install --user virtualenv
setenv PATH "${HOME}/.local/bin:${PATH}"
python3 -m virtualenv .venv
source .venv/bin/activate.csh
python -m pip install "pip<25.1"
python -m pip install -r requirements.txt
```

The `pip<25.1` constraint is useful on machines that still use Python 3.8.
Verify the required scientific packages:

```tcsh
python -c "import numpy, matplotlib, aacgmv2; print('C10 dependencies available')"
```

CDF input additionally requires `cdflib`:

```tcsh
python -c "import cdflib; print('CDF support available')"
```

For the recommended ASCII input path, `cdflib` is not needed during reference
construction.

### 5.3 Standalone virtualenv fallback

When the system Python cannot install `virtualenv` through pip:

```tcsh
curl -L https://bootstrap.pypa.io/virtualenv.pyz -o "$HOME/virtualenv.pyz"
python3 "$HOME/virtualenv.pyz" .venv
source .venv/bin/activate.csh
python -m pip install "pip<25.1"
python -m pip install -r requirements.txt
```

No `sudo`, `apt`, or write access to the system Python installation is required.

---

## 6. Automatic download procedure

### 6.1 Standard command

```tcsh
cd ~/T11/AMPS/srcEarth/test/C10
python download_poes_sem2.py \
  --start 2006-12-05 \
  --end 2006-12-16 \
  --satellites n15,n16,n17,n18,m02 \
  --format txt \
  --output-dir data/reference_source
```

The downloader:

1. opens the official NCEI `v01r00` year directory;
2. recursively follows ordinary directory-index links;
3. selects files containing a requested date and spacecraft token;
4. downloads the matching files;
5. rejects empty payloads and obvious HTML responses;
6. reuses existing nonempty files unless `--overwrite` is given; and
7. writes `data/reference_source/download_manifest.json`.

The manifest records, for every source file:

```text
original URL
local path
file name
size in bytes
SHA-256 digest
download/reuse status
```

Keep the manifest with the raw files. It is part of the data provenance.

### 6.2 Capture a download log

```tcsh
python download_poes_sem2.py \
  --start 2006-12-05 \
  --end 2006-12-16 \
  --satellites n15,n16,n17,n18,m02 \
  --format txt \
  --output-dir data/reference_source |& tee C10_download.log
```

### 6.3 Re-download files

Use `--overwrite` when the source directory contains suspect or incomplete
files:

```tcsh
python download_poes_sem2.py \
  --start 2006-12-05 \
  --end 2006-12-16 \
  --satellites n15,n16,n17,n18,m02 \
  --format txt \
  --output-dir data/reference_source \
  --overwrite
```

A zero-byte destination is considered incomplete and is re-downloaded by the
updated downloader even without `--overwrite`. The explicit option is useful
when a nonempty but damaged file needs to be replaced.

### 6.4 Downloader controls

Display the installed options with:

```tcsh
python download_poes_sem2.py --help
```

Important controls include:

| Option | Default | Meaning |
|---|---:|---|
| `--start` | `2006-12-05` | first requested UTC date |
| `--end` | `2006-12-16` | last requested UTC date, inclusive |
| `--satellites` | all five C10 tokens | comma-separated spacecraft list |
| `--format` | `txt` | `txt` or `cdf` |
| `--output-dir` | `data/reference_source` | local source directory |
| `--maximum-depth` | `3` | recursive archive-index depth |
| `--timeout` | `60` s | network timeout per request |
| `--retries` | `3` | retries after a failed request |
| `--overwrite` | off | replace existing files |
| `--url-list` | none | use exact URLs instead of archive crawling |

---

## 7. Manual and restricted-network download methods

### 7.1 Exact URL-list method

Some institutional networks permit direct HTTPS downloads but block recursive
index pages. In that case:

1. open the official NCEI ASCII year directory in a browser;
2. identify the exact daily files for `n15`, `n16`, `n17`, `n18`, and `m02` for
   5–16 December 2006;
3. save one exact HTTPS URL per line in `poes_urls.txt`; and
4. run the downloader with `--url-list`.

Example file structure:

```text
# One exact NCEI HTTPS URL per line.
# Comments and blank lines are ignored.
https://www.ncei.noaa.gov/data/.../exact-first-file.txt
https://www.ncei.noaa.gov/data/.../exact-second-file.txt
```

Run:

```tcsh
python download_poes_sem2.py \
  --url-list poes_urls.txt \
  --format txt \
  --output-dir data/reference_source
```

This route creates the same SHA-256 download manifest as automatic discovery.
Do not invent filenames from a guessed template; copy the exact links shown by
NCEI.

### 7.2 Existing local or institutional mirror

When the archive files have already been downloaded elsewhere, copy them below:

```text
srcEarth/test/C10/data/reference_source/
```

Supported names include files ending in:

```text
.txt  .txt.gz  .asc  .asc.gz  .dat  .dat.gz  .cdf
```

The builder searches the directory recursively, so subdirectories by spacecraft
or date are allowed.

A manually copied data set may not have `download_manifest.json`. For a final
scientific archive, preserve the original source URLs and generate an external
inventory containing file sizes and SHA-256 values.

### 7.3 Direct `curl` fallback for one known file

For an exact URL obtained from the official archive page:

```tcsh
curl -L --fail --retry 3 \
  "EXACT_NCEI_FILE_URL" \
  -o data/reference_source/EXACT_ARCHIVE_FILENAME
```

Then verify that the downloaded file is nonempty and is not HTML before running
the builder. The project downloader is preferred because it also creates the
manifest and rejects obvious invalid responses.

---

## 8. Mandatory checks before building

### 8.1 Inventory the files

```tcsh
find data/reference_source -type f | sort
```

Count only supported data files:

```tcsh
find data/reference_source -type f \
  \( -name '*.txt' -o -name '*.txt.gz' -o -name '*.asc' -o \
     -name '*.asc.gz' -o -name '*.dat' -o -name '*.dat.gz' -o \
     -name '*.cdf' \) | wc -l
```

### 8.2 Inspect the download manifest

```tcsh
python -m json.tool data/reference_source/download_manifest.json | less
```

Check that the requested dates and spacecraft tokens are represented and that
sizes are nonzero.

### 8.3 Find zero-byte files

```tcsh
find data/reference_source -type f -size 0 -print
```

Any output identifies a file that cannot contribute measurements. Remove and
redownload it:

```tcsh
find data/reference_source -type f -size 0 -delete
python download_poes_sem2.py \
  --start 2006-12-05 \
  --end 2006-12-16 \
  --satellites n15,n16,n17,n18,m02 \
  --format txt \
  --output-dir data/reference_source
```

If a particular spacecraft/day remains unavailable, continue with the remaining
files only after documenting the gap and checking the resulting crossing and MLT
coverage. Never create a fabricated replacement file.

### 8.4 Detect HTML or error responses saved as data

For uncompressed ASCII files:

```tcsh
foreach f (`find data/reference_source -type f -name '*.txt'`)
    grep -qi -E '<html|<!doctype|access denied|not found|forbidden' "$f"
    if ($status == 0) echo "possible non-data response: $f"
end
```

Delete and redownload every identified response page.

### 8.5 Inspect a representative source file

```tcsh
set f = `find data/reference_source -type f -name '*.txt' -size +0c | head -1`
echo "$f"
head -5 "$f"
file "$f"
```

Historical files can use either:

- an explicit column-name header; or
- the documented fixed-order numerical layout without a header.

The parser supports both. For a headerless file, inspect the first nonblank row:

```tcsh
awk 'NF && $1 !~ /^#/ {print "fields in first data row:", NF; exit}' "$f"
```

The expected headerless 16-second record contains at least 41 numerical fields.
A nonempty file matching neither supported form is treated as malformed.

### 8.6 Verify an individual checksum

```tcsh
sha256sum "$f"
```

Compare the result with the matching entry in
`data/reference_source/download_manifest.json`.

### 8.7 Check the date coverage in filenames

```tcsh
find data/reference_source -type f | \
  grep -E '200612(05|06|07|08|09|10|11|12|13|14|15|16)' | \
  sort
```

This is only an inventory check. The builder validates actual parsed timestamps
inside the files.

---

## 9. Building the reference solution

### 9.1 Optional extraction-only check

Before writing outputs, test whether the source data can be parsed and whether
crossings can be extracted:

```tcsh
python build_poes_reference.py \
  --input-dir data/reference_source \
  --event-start 2006-12-14T00:00:00Z \
  --event-end 2006-12-16T12:00:00Z \
  --reference-output reference_C10_poes_meped_boundary.csv.gz \
  --validate-only
```

A successful validation-only run prints nonzero observation, crossing, and
reference-cell counts but does not overwrite the output files.

### 9.2 Production build

```tcsh
python build_poes_reference.py \
  --input-dir data/reference_source \
  --event-start 2006-12-14T00:00:00Z \
  --event-end 2006-12-16T12:00:00Z \
  --default-altitude-km 850 \
  --minimum-pass-abs-lat-deg 45 \
  --polar-plateau-abs-lat-deg 75 \
  --minimum-polar-samples 4 \
  --minimum-background-samples 3 \
  --minimum-plateau-to-low-ratio 2 \
  --minimum-leg-samples 4 \
  --crossing-method BACKGROUND_NORMALIZED_ISOTONIC \
  --maximum-transition-width-deg 15 \
  --maximum-isotonic-rms 0.35 \
  --minimum-edge-margin-deg 0.5 \
  --window-hours 2 \
  --step-hours 1 \
  --mlt-bin-hours 3 \
  --minimum-crossings-per-cell 2 \
  --minimum-diagnostic-crossings-per-cell 2 \
  --minimum-diagnostic-distinct-pass-legs-per-cell 2 \
  --minimum-diagnostic-distinct-satellites-per-cell 2 \
  --minimum-primary-transition-samples 2 \
  --minimum-diagnostic-transition-samples 3 \
  --minimum-diagnostic-contrast-to-noise 3 \
  --p8-p9-outlier-sigma 4 \
  --p8-p9-minimum-pairs 6 \
  --p8-p9-fallback-max-separation-deg 6 \
  --minimum-distinct-pass-legs-per-cell 2 \
  --acceptance-window-stride-hours 2 \
  --crossings-output C10_poes_boundary_crossings.csv \
  --reference-output reference_C10_poes_meped_boundary.csv.gz \
  --manifest-output C10_reference_manifest.json \
  --summary-output C10_reference_summary.json
```

The explicit values above are the current defaults. Including them makes a
captured command self-documenting.

### 9.3 Capture the build log

```tcsh
python build_poes_reference.py \
  --input-dir data/reference_source \
  --event-start 2006-12-14T00:00:00Z \
  --event-end 2006-12-16T12:00:00Z \
  --crossings-output C10_poes_boundary_crossings.csv \
  --reference-output reference_C10_poes_meped_boundary.csv.gz \
  --manifest-output C10_reference_manifest.json \
  --summary-output C10_reference_summary.json |& tee C10_build_reference.log
```

Because a shell pipeline can report the status of `tee` rather than Python,
verify the generated outputs and summary instead of assuming success from the
returned prompt.

To see the Python status directly, run without `tee`:

```tcsh
python build_poes_reference.py \
  --input-dir data/reference_source \
  --reference-output reference_C10_poes_meped_boundary.csv.gz
echo "build exit status = $status"
```

A successful build returns zero.

### 9.4 Builder controls

```tcsh
python build_poes_reference.py --help
```

Important scientific settings are:

| Option | Default | Purpose |
|---|---:|---|
| `--default-altitude-km` | 850 | fallback when altitude is absent |
| `--minimum-pass-abs-lat-deg` | 45 | lowest absolute latitude kept in a polar pass |
| `--polar-plateau-abs-lat-deg` | 75 | minimum latitude used for polar-cap flux |
| `--minimum-polar-samples` | 4 | samples required for plateau estimation |
| `--minimum-background-samples` | 3 | equatorward samples required on each leg |
| `--minimum-plateau-to-low-ratio` | 2 | required polar-to-lower-latitude contrast |
| `--minimum-leg-samples` | 4 | samples required on a pass leg |
| `--crossing-method` | `BACKGROUND_NORMALIZED_ISOTONIC` | production T25/T50/T75 fit; legacy method is diagnostic only |
| `--maximum-transition-width-deg` | 15 | reject excessively broad transitions |
| `--maximum-isotonic-rms` | 0.35 | reject strongly nonmonotonic normalized profiles |
| `--minimum-edge-margin-deg` | 0.5 | require the fitted T50 away from the retained latitude edge |
| `--window-hours` | 2 | aggregation-window width |
| `--step-hours` | 1 | separation between adjacent window centers |
| `--mlt-bin-hours` | 3 | MLT-bin width |
| `--minimum-crossings-per-cell` | 2 | crossings required for PRIMARY P6/P7 cells |
| `--minimum-diagnostic-crossings-per-cell` | 2 | quality-eligible crossings required for a robust P8/P9 cell |
| `--minimum-diagnostic-distinct-pass-legs-per-cell` | 2 | independent diagnostic pass legs |
| `--minimum-diagnostic-distinct-satellites-per-cell` | 2 | independent diagnostic spacecraft |
| `--minimum-primary-transition-samples` | 2 | central-profile support for P6/P7 |
| `--minimum-diagnostic-transition-samples` | 3 | central-profile support for P8/P9 |
| `--minimum-diagnostic-contrast-to-noise` | 3 | robust plateau/background contrast-noise requirement |
| `--p8-p9-outlier-sigma` | 4 | robust upper-separation threshold multiplier |
| `--p8-p9-fallback-max-separation-deg` | 6 | conservative limit when too few pass pairs exist |
| `--minimum-distinct-pass-legs-per-cell` | 2 | independent pass legs required by a primary cell |
| `--acceptance-window-stride-hours` | 2 | nonoverlapping midpoint stride used for PASS/FAIL |

Do not relax the quality thresholds merely to force a nonempty result. First
inspect the source format, event coverage, individual flux profiles, and pass-
level crossing table.

---

## 10. Scientific construction performed by the builder

### 10.1 Read and normalize Level-2 observations

For each daily file, the reader:

1. identifies the spacecraft from the filename;
2. parses the 16-second record time and spacecraft position;
3. reads the MEPED omnidirectional proton channels P6–P9;
4. rejects fill values, negative values, nonfinite values, and invalid large
   archive values;
5. uses the measured altitude when available or the configured fallback;
6. converts the position to AACGM latitude and magnetic local time with
   `aacgmv2`; and
7. retains source-file identity for provenance.

### 10.2 Map channel thresholds to nominal proton rigidity

| Channel | Nominal lower threshold | Assigned rigidity |
|---|---:|---:|
| P6 | 16 MeV | 0.174013525 GV |
| P7 | 36 MeV | 0.262395866 GV |
| P8 | 70 MeV | 0.369131538 GV |
| P9 | 140 MeV | 0.531334344 GV |

This is a nominal lower-threshold mapping for the integral channels. It is not a
full detector-response convolution with an event spectrum.

### 10.3 Identify polar passes and legs

Measurements are grouped by spacecraft and hemisphere into polar passes. Each
pass is divided near its maximum absolute AACGM latitude into:

```text
inbound/equator-to-pole leg
outbound/pole-to-equator leg
```

The legs are processed separately because they occur at different times and
usually at different MLT.

### 10.4 Estimate the two normalization levels

For each pass and channel, the polar plateau is the median valid flux at
`|AACGM latitude| >= 75 deg`. For each inbound or outbound leg independently,
the equatorward background is the median valid flux in the lowest retained
latitude band (by default 45–50 degrees absolute AACGM latitude). A leg is
rejected when either estimate lacks enough samples, the plateau is not above the
background, or the plateau/background contrast is below the configured ratio.

### 10.5 Fit the observed transmission and locate T50

The normalized profile is

```text
Tobs = (F - Fbackground) / (Fpolar - Fbackground).
```

Samples are ordered from equator to pole and fitted with a nondecreasing
pool-adjacent-violators isotonic regression. The builder requires explicitly
bracketed T25, T50, and T75 crossings. It records the T25–T75 transition width
and isotonic RMS and rejects unbracketed, edge-clipped, excessively broad, or
strongly nonmonotonic transitions. Time, geographic position, altitude, AACGM
latitude, and MLT are interpolated at the fitted T50.

The stored uncertainty is the maximum of a 0.25-degree floor, half the local
sampling interval, one quarter of the transition width, and an isotonic-residual
term. This is still a regression-test uncertainty rather than a full instrument
calibration covariance.

### 10.6 Assign validation roles and aggregate

Each crossing is tagged `PRIMARY` (P6/P7) or `DIAGNOSTIC` (P8/P9). All channels
are aggregated into two-hour windows stepped by one hour and three-hour MLT
bins. P6/P7 acceptance still requires two crossings, two pass legs, and the
nonoverlapping two-hour stride. P8/P9 receive an independent robust-diagnostic
gate requiring two quality-eligible crossings, two pass legs, and two
spacecraft. Sparse and cross-channel-outlier diagnostic cells retain a numerical
estimate and explicit `quality_status`, but `diagnostic_eligible=FALSE` keeps
them out of connected means and robust metrics.

### 10.7 P8/P9 observational consistency and noise diagnostics

For each pass leg containing both P8 and P9, the builder records the P8-minus-P9
boundary separation. A P9 point above the robust median-plus-four-MAD limit (or
above the conservative 6-degree fallback when the sample is small) is tagged
`P9_CROSS_CHANNEL_OUTLIER` and excluded from robust aggregation. This test is
measurement-only and never compares with AMPS while deciding eligibility.

The pass-level CSV also records `transition_support_samples`,
`contrast_to_noise_ratio`, `cross_channel_delta_p8_p9_deg`,
`cross_channel_outlier`, and `aggregate_eligible`. The compressed reference adds
`diagnostic_eligible`, `quality_status`, `n_aggregate_eligible_crossings`,
`n_cross_channel_outliers`, and median support/noise fields. Rebuild the
reference after installing this version; older schemas are intentionally
rejected.

### 10.7 Legacy threshold sensitivity

`--crossing-method HALF_POLAR_PLATEAU` reproduces the older direct
`0.5 * Fpolar` crossing for sensitivity studies. The resulting cells are never
marked acceptance eligible and cannot be used for the default code-validation
gate.

## 11. Generated outputs

### 11.1 Pass-level observations

```text
C10_poes_boundary_crossings.csv
```

This is the primary observational extraction product. Typical fields include:

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
half_plateau_flux  # legacy column name; stores the actual fitted T50 flux
boundary_uncertainty_deg
quality_flags
source_file
source_sha256
```

Use it to inspect individual passes, satellite subsets, hemispheric differences,
and MLT coverage.

### 11.2 Aggregated comparison reference

```text
reference_C10_poes_meped_boundary.csv.gz
```

This is the file consumed directly by `run_C10.py`. It includes:

```text
event_id
interval_midpoint_utc
interval_start_utc
interval_end_utc
rigidity_gv
energy_threshold_mev
channel
validation_role
acceptance_eligible
hemisphere
mlt_hour
boundary_aacgm_lat_deg
sigma_deg
altitude_km
n_crossings
n_distinct_pass_legs
n_distinct_satellites
median_transition_width_deg
background_corrected
satellites
missing
source
source_ref
notes
```

Inspect it without decompressing it on disk:

```tcsh
gzip -cd reference_C10_poes_meped_boundary.csv.gz | less
```

Count its lines:

```tcsh
gzip -cd reference_C10_poes_meped_boundary.csv.gz | wc -l
```

### 11.3 Scientific provenance manifest

```text
C10_reference_manifest.json
```

It records source paths, sizes, SHA-256 values, extraction settings,
channel-to-rigidity mapping, and crossing/reference counts. Archive it with the
compressed reference.

### 11.4 Build summary

```text
C10_reference_summary.json
```

It reports at least:

```text
number of source files
number of observations in the event interval
number of pass-level crossings
number of total reference cells
number of nonmissing reference cells
spacecraft represented
channels represented
first and last observation times
reference compression
output paths
manifest SHA-256
compressed reference SHA-256
```

---

## 12. Verifying the completed reference

Run all checks below:

```tcsh
ls -lh \
  C10_poes_boundary_crossings.csv \
  reference_C10_poes_meped_boundary.csv.gz \
  C10_reference_manifest.json \
  C10_reference_summary.json

gzip -t reference_C10_poes_meped_boundary.csv.gz

gzip -cd reference_C10_poes_meped_boundary.csv.gz | head -10

gzip -cd reference_C10_poes_meped_boundary.csv.gz | wc -l

wc -l C10_poes_boundary_crossings.csv

python -m json.tool C10_reference_summary.json
```

The summary must report nonzero values for:

```text
n_observations
n_crossings
n_nonmissing_reference_cells
```

Then run the C10 validators:

```tcsh
python run_C10.py --validate-references \
  --reference reference_C10_poes_meped_boundary.csv.gz

python run_C10.py --validate-driver
```

The runner defaults to the compressed reference, so the explicit `--reference`
argument can be omitted after installation.

### 12.1 Check that the reference is not the placeholder

```tcsh
gzip -cd reference_C10_poes_meped_boundary.csv.gz | head -5
```

A real reference begins with comments describing a POES/MetOp SEM-2 reference,
then a CSV header, then numerical rows. A file containing only explanatory
placeholder comments and the header must be rebuilt.

### 12.2 Confirm the recorded checksum

```tcsh
sha256sum reference_C10_poes_meped_boundary.csv.gz
grep -n "reference_sha256" C10_reference_summary.json
```

The values must match.

---

## 13. Using the reference in C10

A quick GRIDDED direct-access run can use the default compressed reference:

```tcsh
cd ~/T11/AMPS
./srcEarth/test/C10/run_C10.py \
  --solver GRIDDED \
  --cutoff-evaluation DIRECT_ACCESS \
  --comparison-observable ACCESS_T50 \
  --profile ROUTINE \
  --interval-samples 1 \
  --amps "${cwd}/amps" \
  -np 8 -nt 16
```

For an explicit reference path:

```tcsh
--reference ~/T11/AMPS/srcEarth/test/C10/reference_C10_poes_meped_boundary.csv.gz
```

See [`../README.md`](../README.md) for FULL_SCAN versus DIRECT_ACCESS,
comparison observables, acceptance criteria, plots, and consistency checks.

---

## 14. Troubleshooting

### 14.1 `File preview: <empty file>`

Example:

```text
could not recognize a headerless NCEI data row ... File preview: <empty file>
```

Cause: at least one downloaded source file has zero bytes.

Locate it:

```tcsh
find data/reference_source -type f -size 0 -print
```

Remove and redownload it:

```tcsh
find data/reference_source -type f -size 0 -delete
python download_poes_sem2.py \
  --start 2006-12-05 \
  --end 2006-12-16 \
  --satellites n15,n16,n17,n18,m02 \
  --format txt \
  --output-dir data/reference_source
```

### 14.2 `no reference rows parsed`

Example:

```text
C10 reference could not be loaded: no reference rows parsed
```

Cause: the builder did not complete, or the runner is reading the compressed
placeholder.

Check:

```tcsh
gzip -cd reference_C10_poes_meped_boundary.csv.gz | wc -l
ls -l C10_reference_summary.json
```

Correction: fix the source-data problem and successfully rerun
`build_poes_reference.py`.

### 14.3 Explicit header and headerless row are both unrecognized

Example:

```text
could not locate an explicit 16-second Level-2 header and could not recognize a headerless NCEI data row
```

Inspect the failing file:

```tcsh
head -10 path/to/file.txt
awk 'NF && $1 !~ /^#/ {print NF; exit}' path/to/file.txt
file path/to/file.txt
```

Common causes are:

- empty or truncated content;
- an HTML error page;
- an unrelated NCEI product;
- compressed data stored under the wrong suffix; or
- a format different from the supported Level-2 16-second ASCII/CDF layouts.

### 14.4 No matching files were discovered

The archive index may be blocked or the directory layout may have changed.
Use the exact URL-list workflow in Section 7.1. Also increase crawl depth only
when inspection shows that the files are below deeper subdirectories:

```tcsh
python download_poes_sem2.py \
  --maximum-depth 5 \
  --start 2006-12-05 \
  --end 2006-12-16 \
  --satellites n15,n16,n17,n18,m02 \
  --format txt \
  --output-dir data/reference_source
```

### 14.5 No valid observations in the event interval

Check:

- source filenames and internal timestamps;
- `--event-start` and `--event-end`;
- the downloaded year and product level;
- whether all channel values were interpreted as fill values; and
- whether the parser is using the expected column layout.

### 14.6 No boundary crossings were extracted

Do not immediately loosen the thresholds. First inspect:

- whether the SEP enhancement is present in P6–P9;
- whether the pass reaches the configured polar-plateau latitude;
- the number of valid plateau samples;
- the polar-to-lower-latitude flux ratio;
- inbound and outbound flux-versus-latitude profiles; and
- spacecraft/channel coverage.

Only after those checks should sensitivity runs alter the plateau latitude,
minimum sample counts, or contrast threshold.

### 14.7 `aacgmv2` cannot be imported

```tcsh
source .venv/bin/activate.csh
python -m pip install -r requirements.txt
python -c "import aacgmv2; print(aacgmv2.__version__)"
```

### 14.8 Gzip integrity failure

```tcsh
gzip -t reference_C10_poes_meped_boundary.csv.gz
```

If this fails, delete the compressed output and rebuild it. Do not edit a gzip
file with a text editor.

### 14.9 Old uncompressed reference remains

The production reference is now `.csv.gz`. When a previously generated real
CSV exists and must be migrated without rebuilding:

```tcsh
gzip -n -9 reference_C10_poes_meped_boundary.csv
```

This creates `reference_C10_poes_meped_boundary.csv.gz` and removes the
uncompressed input. Rebuilding from the original source files is preferable
because it regenerates the summary and records the compressed-file checksum.

---

## 15. TS05 driver required by C10

C10 uses the checksum-verified December 2006 TS05 driver shared with C9. The
runner searches for:

```text
srcEarth/test/C10/data/ts05_driving.txt
```

and then the sibling C9 location:

```text
srcEarth/test/C9/data/ts05_driving.txt
```

A separate C10 copy is optional when the C9 file is present. Validate it with:

```tcsh
python run_C10.py --validate-driver
```

The TS05 driver is not part of the POES/MetOp observational reference build, but
both are needed for the final model/data comparison.

---

## 16. Reproducibility checklist

Archive the following for a scientifically reviewable C10 result:

```text
all original NOAA/NCEI daily files
data/reference_source/download_manifest.json
C10_download.log, when captured
C10_poes_boundary_crossings.csv
reference_C10_poes_meped_boundary.csv.gz
C10_reference_manifest.json
C10_reference_summary.json
C10_build_reference.log
the TS05 driver and its checksum
all generated AMPS input files
all C10 AMPS logs
raw shell/access/penumbra products
comparison CSV files
comparison plots
result JSON files
AMPS git commit and local diff
Python version and package inventory
```

Record the Python environment:

```tcsh
python --version
python -m pip freeze > C10_python_environment.txt
```

Record the important checksums:

```tcsh
sha256sum \
  reference_C10_poes_meped_boundary.csv.gz \
  C10_poes_boundary_crossings.csv \
  C10_reference_manifest.json \
  C10_reference_summary.json \
  > C10_reference_products_SHA256SUMS.txt
```

Do not overwrite a frozen baseline without preserving the previous raw files,
manifests, settings, output products, and checksums.

---

## 17. Related documentation

- [`../README.md`](../README.md): C10 scientific definition, FULL_SCAN and
  DIRECT_ACCESS execution, metrics, and plots.
- [`../INSTALL.md`](../INSTALL.md): compact installation and run commands.
- [`../REFERENCES.md`](../REFERENCES.md): instrument, archive, calibration, and
  cutoff-method references.
- [`../VALIDATION.md`](../VALIDATION.md): software and archive-validation record.
- [`../CHANGES.md`](../CHANGES.md): implementation history.
- [`../../validation.docx`](../../validation.docx): complete Earth-model
  validation campaign description.


## 18. Rebuild required after the improved-processing update

The current runner rejects an older reference that lacks `validation_role`,
`acceptance_eligible`, and `background_corrected`. Re-run the builder; do not
simply add columns to the old CSV because the actual T50 values must be
re-extracted from the daily measurements. A successful summary must report a
nonzero `n_acceptance_eligible_cells`.

Use these checks after rebuilding:

```tcsh
python -m json.tool C10_reference_summary.json | grep -E \
  'n_(primary|diagnostic|acceptance|crossings)'
gzip -cd reference_C10_poes_meped_boundary.csv.gz | head -8
python run_C10.py --validate-references
```
