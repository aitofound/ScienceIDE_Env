# C7 — IZMIRAN T96 vertical-cutoff benchmark

C7 compares AMPS vertical proton cutoff rigidities with a complete 28-location
reference solution generated with the IZMIRAN Cutoff2050 online calculator.
The reference table is bundled in this directory:

```text
reference_C7_izmiran_t96.csv
```

No external reference download or `--reference` option is required for the
standard IZMIRAN subtest. C7 can still accept another strict reference CSV when
`--reference FILE` is supplied explicitly.

The test provides two Python entry points:

```text
run_C7_izmiran.py  standard IZMIRAN/T96 runner
run_C7.py          full generic C7 runner for alternate strict references
```

## Reference configuration

The bundled table contains seven geographic latitudes and four longitudes at a
500-km geodetic altitude:

```text
latitude  = -60, -45, -30, 0, 30, 45, 60 deg
longitude =   0,  90, 180, -90 deg in the IZMIRAN calculator
```

The signed calculator longitude `-90 deg` is equivalent to `270 deg east` in
AMPS. The C7 loader normalizes both representations to the same location.

Every online calculation used the settings visible in the archived screenshots:

```text
Mode                 advanced
Location             custom
Field model          IGRF+T96
Date                  2012-03-08
Altitude              500 km
SWDP                  2 nPa
Dst                  -50 nT
IMF By                0 nT
IMF Bz               -5 nT
Vertical angle        0 deg
Azimuth               0 deg
Rigidity range        0.01 to 20 GV
Rigidity step         0.1 GV
Maximum flight time   20 s
```

The calculator form displayed the date without an explicit time. The strict
reference table records the epoch as `2012-03-08T00:00:00`; this assumption is
also recorded in the notes column. The online calculator reported lower, upper,
and effective cutoff rigidities. The effective value is marked as
`PENUMBRA_INTEGRAL` in the reference table.

Reference screenshots are retained under:

```text
reference_evidence/IZMIRAN/
```

## Quick start

Run these commands from the AMPS repository root.

Validate the bundled 28-point reference without running AMPS:

```bash
python3 srcEarth/test/C7/run_C7_izmiran.py --validate-references
```

Preview the generated input and MPI command:

```bash
python3 srcEarth/test/C7/run_C7_izmiran.py --dry-run
```

Run the default gridless IZMIRAN comparison:

```bash
python3 srcEarth/test/C7/run_C7_izmiran.py -np 4
```

Use the generic runner only when selecting another strict reference source or
model:

```bash
python3 srcEarth/test/C7/run_C7.py \
  --reference /path/to/reference.csv \
  --source GEOMAGSPHERE \
  --models T96 \
  --validate-references
```

Run the Mode3D gridded comparison:

```bash
python3 srcEarth/test/C7/run_C7_izmiran.py --solver GRIDDED -np 4 -nt 16
```

Run both field-evaluation paths:

```bash
python3 srcEarth/test/C7/run_C7_izmiran.py --solver BOTH -np 4 -nt 16
```

Use another AMPS executable when necessary:

```bash
python3 srcEarth/test/C7/run_C7_izmiran.py --amps /path/to/amps -np 4
```

## Bundled defaults

The standard invocation selects:

```text
reference             reference_C7_izmiran_t96.csv
source                IZMIRAN
model                 T96
solver                GRIDLESS
mover                 RK4
rigidity range        0.01 to 20 GV
upper scan points     400
backtrace charge      REVERSED
trace-limit policy    FORBIDDEN
maximum trace time    20 s
```

The mover is passed explicitly so a future change to the executable's internal
default does not silently alter the regression. A mover sensitivity run can be
requested, for example, with `--mover RK6`.

For gridless cutoff tracing, the automatic MPI dynamic chunk is one location.
For Mode3D, an automatic chunk uses the requested `-nt` value. Override either
with a positive `--dynamic-chunk N`.

## Output layout

The default output root is:

```text
test_output/C7/
```

Each solver case contains:

```text
AMPS_PARAM_C7.in
C7_amps.log
reference_C7_selected.csv
C7_comparison.csv
C7_result.json
```

The output root also contains:

```text
C7_reference_inventory.json
C7_summary.json
```

## Acceptance logic

At each location C7 compares:

- lower cutoff rigidity;
- upper cutoff rigidity;
- the IZMIRAN effective cutoff against the AMPS penumbra-integral cutoff.

The point tolerance is the largest of:

```text
0.20 GV
1.5 times the reference rigidity step
10 percent of the reference cutoff
```

The default group requirements are:

```text
matched fraction             >= 0.95
valid fraction               >= 0.95
pass fraction of valid rows  >= 0.85
maximum judged RMSE          <= 0.50 GV
```

AMPS rows with unresolved cutoff brackets or reported cutoff limits outside the
configured rigidity scan are invalid and cannot pass.

## Supplying another reference

A different strict C7 table can be selected explicitly:

```bash
python3 srcEarth/test/C7/run_C7.py \
  --reference /path/to/reference.csv \
  --source IZMIRAN \
  --models T96 \
  --validate-references
```

The templates and importer remain available for creating other reference sets:

```text
reference_C7_izmiran_template.csv
reference_C7_geomagsphere_template.csv
reference_C7_request_manifest.csv
tools/import_reference.py
```

Generate a self-contained calculator request package with:

```bash
python3 srcEarth/test/C7/run_C7.py --prepare-reference-requests
```

## Interpretation limits

This is an external model-to-model benchmark. Differences may reflect not only
the particle mover but also differences in the IGRF implementation, coordinate
transforms, dipole tilt, magnetopause or outer-boundary treatment, inner
boundary, and trajectory termination rules. The bundled reference is therefore
best used as an independent numerical benchmark rather than as proof that all
physical conventions are identical between AMPS and Cutoff2050.
