# shieldSim Test Suite

This directory contains automated and semi-automated test scripts for the
`shieldSim` Geant4 shielding application.

The tests are organized in layers. Lower-numbered layers test basic software
functionality, while higher-numbered layers test geometry, source sampling,
scoring, physics-output behavior, numerical controls, and regression stability.

## Test Philosophy

The purpose of the test suite is to separate several different questions:

1. Does the code build?
2. Does the command-line interface work?
3. Are invalid inputs rejected cleanly?
4. Are the geometry, source, and scoring algorithms behaving correctly?
5. Do the physics-facing outputs respond correctly to simple controlled cases?
6. Are Monte Carlo results statistically stable enough for the intended use?
7. Do future code changes preserve validated behavior?

Layer-1 tests should be quick and should be run frequently. Layer-2 tests are
also intended to be lightweight, but they run short Geant4 simulations and use
diagnostic output to check geometry/source/scoring behavior. Layer-3 tests run
more physics-facing simulations and are intended as sanity checks, not as a full
validation against external reference data. Layer-4 tests implement a baseline-driven regression workflow: generate an accepted baseline from a trusted version, then compare future runs against it with documented tolerances.

---

## Directory Layout

```text
tests/
├── README.md
├── run_layer1_tests.sh
├── run_layer2_tests.sh
├── run_layer3_tests.sh
├── run_layer4_tests.sh
├── regression_tools.py
├── data/
│   ├── mono_50MeV_proton.dat
│   ├── mono_100MeV_proton.dat
│   ├── mono_100MeV_proton_rate10.dat
│   ├── mono_100MeV_alpha.dat
│   ├── mono_100MeV_alpha_rate10.dat
│   └── mono_150MeV_proton.dat
├── expected/
│   ├── README.md
│   └── layer4/
│       ├── README.md
│       └── layer4_reference.json    # generated after accepting a baseline
└── logs/
    └── .gitignore
```

### `run_layer1_tests.sh`

Build and smoke tests.

This script verifies that the code builds, that basic CLI commands work, and
that invalid input is rejected cleanly.

### `run_layer2_tests.sh`

Geometry, source, and scoring tests.

This script verifies beam-source sampling, isotropic-source sampling,
near-vacuum transmission through the shield rear face, target ordering, and
basic material alias behavior.

### `run_layer3_tests.sh`

Physics-output and numerical-sanity tests.

This script verifies that computed-quantity outputs are written, that source
normalization scales rates correctly, that H100/10 behaves correctly for simple
below/above-threshold spectra, that alpha LET is larger than proton LET in a
controlled silicon case, that all supported physics lists run, and that sweep
output has monotonic areal density.


### `run_layer4_tests.sh`

End-to-end regression tests.

This script runs deterministic reference cases, collects compact signatures from
run-summary and output files, and compares the current results against an
accepted JSON baseline. It can also create or update that baseline after manual
review.

### `regression_tools.py`

Python helper used by `run_layer4_tests.sh` to collect run summaries into JSON
and compare actual results against the accepted baseline with configurable
numerical tolerances.

### `data/`

Small deterministic input spectra for tests. These files are intentionally
simple and should be version controlled. Rate-scaled spectra have the same shape
as their unit-rate counterpart, but a larger absolute source normalization. They
are used to test that per-primary Monte Carlo quantities are unchanged while
physical rates scale with the input spectrum intensity.

### `expected/`

Reference output summaries and accepted regression baselines. The Layer-4 baseline is stored under `tests/expected/layer4/layer4_reference.json` after it is generated with `tests/run_layer4_tests.sh --update-baseline`. Large raw
Geant4 output files should generally not be committed unless they are small and
essential.

### `logs/`

Generated test logs. This directory should not be version controlled except for
its `.gitignore` file.

---

## Diagnostic and Numerical CLI Options Used by Tests

The test suite uses several CLI options implemented in the main code:

```bash
--random-seed=<integer>
--output-prefix=<name>
--dump-source-samples=<file>
--dump-exit-particles=<file>
--dump-run-summary=<file>
--diagnostic-max-rows=<n>
--production-cut=<mm>
--max-step=<mm>
```

### `--random-seed=<integer>`

Sets the CLHEP/Geant4 random seed before the run. This makes source-sampling and
short Monte Carlo tests reproducible.

### `--output-prefix=<name>`

Changes standard output file names. With the default prefix `shieldSim`, files
keep their historical names, such as:

```text
shieldSim_spectra.dat
shieldSim_quantities.dat
shieldSim_let_spectrum.dat
shieldSim_dose_sweep.dat
shieldSimOutput*.csv
```

With `--output-prefix=caseA`, they become:

```text
caseA_spectra.dat
caseA_quantities.dat
caseA_let_spectrum.dat
caseA_dose_sweep.dat
caseAOutput*.csv
```

### `--dump-source-samples=<file>`

Writes one row per generated primary:

```text
row species E_MeV x_mm y_mm z_mm ux uy uz
```

Positions are global coordinates in millimeters. The vector `(ux,uy,uz)` is the
unit momentum direction requested by `PrimaryGeneratorAction`.

This file is used to verify that:

- beam mode generates `x = y = 0` and direction `+z`,
- isotropic mode keeps positions on the upstream plane,
- isotropic mode samples the inward-hemisphere crossing distribution
  `p(mu) = 2 mu`, where `mu = uz`.

### `--dump-exit-particles=<file>`

Writes one row for each particle accepted as crossing the downstream shield
face:

```text
row species E_MeV xg_mm yg_mm zg_mm xl_mm yl_mm zl_mm uxl uyl uzl
```

The `g` coordinates are global coordinates. The `l` coordinates and directions
are in the local coordinate frame of the shield. This file is used to verify
that shield rear-face scoring is based on the correct local-coordinate crossing
condition.

### `--dump-run-summary=<file>`

Writes a machine-readable scalar summary for Layer-3 tests. The normal Tecplot
files remain the science-facing outputs, but the run summary gives stable
keyword rows that are easier to parse in automated tests:

```text
scalar H100_10 <value>
count output_proton <value>
target index name thickness_mm TID_Gy_perPrimary TIDRate_Gy_s DDD_MeV_g_perPrimary DDDRate_MeV_g_s n_eq_cm2_perPrimary n_eq_rate_cm2_s
```

In sweep mode, the file contains one `begin_run` / `end_run` block per sweep
thickness.

### `--production-cut=<mm>`

Sets the Geant4 default production range cut in millimeters. The Layer-3 script
uses this as a smoke/numerical-control test. Production runs should document the
chosen value because low-energy secondaries, TID, DDD, `n_eq`, and LET tails can
be cut-sensitive.

### `--max-step=<mm>`

Applies a maximum step length to the shield and scoring slabs and registers
`G4StepLimiterPhysics`. This is mainly for numerical convergence checks in thin
targets and LET-spectrum tests. Omit it to use the normal physics-list stepping
behavior.

### `--diagnostic-max-rows=<n>`

Limits the number of rows written to each diagnostic file. This prevents
accidentally generating huge diagnostic dumps. Use a value less than or equal to
zero to remove the explicit cap.

---

## Layer-1 Tests

Layer-1 tests are software-level build and CLI smoke tests.

They check:

- clean CMake configuration,
- clean build,
- executable discovery,
- `--help`,
- `--list-materials`,
- `--list-target-materials`,
- `--list-detector-materials`,
- `--list-quantities`,
- rejection of invalid shielding material,
- rejection of invalid physics list,
- rejection of invalid source mode,
- rejection of negative target thickness,
- rejection of invalid computed quantity,
- rejection of malformed shield specification,
- rejection of negative event count.

Run from the top-level package directory:

```bash
chmod +x tests/run_layer1_tests.sh
tests/run_layer1_tests.sh
```

To see script options:

```bash
tests/run_layer1_tests.sh --help
```

Optional environment variables:

```bash
BUILD_DIR=build_test JOBS=8 tests/run_layer1_tests.sh
```

Available variables:

```text
BUILD_DIR   Build directory used by the test script.
            Default: build_layer1_tests

EXE_NAME    Executable name.
            Default: shieldSim

LOG_DIR     Directory where test logs are written.
            Default: layer1_test_logs

JOBS        Number of parallel build jobs.
            Default: nproc, or 2 if nproc is unavailable.
```

Expected result:

```text
All Layer-1 tests passed.
```

---

## Layer-2 Tests

Layer-2 tests verify geometry, source sampling, and scoring logic using short
Geant4 runs and diagnostic files.

The implemented Layer-2 script checks:

1. The diagnostic options are present in `--help`.
2. Beam mode generates 100 MeV protons at `x = y = 0` with direction `+z`.
3. Isotropic mode samples the source plane and direction distribution correctly:
   - all directions point inward, `0 < mu <= 1`,
   - `<mu> ≈ 2/3`,
   - `<mu^2> ≈ 1/2`,
   - source positions stay within the finite upstream plane.
4. A near-vacuum shield transmits most 100 MeV protons through the downstream
   shield face.
5. Exit diagnostics use shield-local coordinates and positive downstream local
   direction cosine.
6. Detector/target material order is preserved in output metadata.
7. `Al` and `G4_Al` both run successfully as shield material specifications.

Run from the top-level package directory:

```bash
chmod +x tests/run_layer2_tests.sh
tests/run_layer2_tests.sh
```

To see script options:

```bash
tests/run_layer2_tests.sh --help
```

Optional environment variables:

```bash
BUILD_DIR=build_l2 JOBS=8 tests/run_layer2_tests.sh
```

Available variables:

```text
BUILD_DIR   Build directory used by the test script.
            Default: build_layer2_tests

EXE_NAME    Executable name.
            Default: shieldSim

LOG_DIR     Directory where test logs are written.
            Default: layer2_test_logs

RUN_DIR     Directory where each Layer-2 test case writes its run outputs.
            Default: layer2_test_runs

JOBS        Number of parallel build jobs.
            Default: nproc, or 2 if nproc is unavailable.

SKIP_BUILD  Set to 1 to skip CMake configure/build.
            Default: 0

EXE_PATH    Explicit executable path when using SKIP_BUILD=1.
```

Example using an already-built executable:

```bash
SKIP_BUILD=1 EXE_PATH=build/shieldSim tests/run_layer2_tests.sh
```

Expected result:

```text
All Layer-2 tests passed.
```

Layer-2 test outputs are written under:

```text
layer2_test_runs/
```

Logs are written under:

```text
layer2_test_logs/
```

These generated directories should not be committed.

---

## Layer-3 Tests

Layer-3 tests verify physics-output behavior and numerical-control plumbing with
short, deterministic Geant4 runs. They are sanity tests, not a substitute for
full validation against NIST stopping powers, SR-NIEL/device-response tables, or
experimental data.

The implemented Layer-3 script checks:

1. `--help` lists `--dump-run-summary`, `--production-cut`, and `--max-step`.
2. A monoenergetic 100 MeV proton run writes spectra, scalar quantity, LET, and
   summary outputs.
3. Source normalization is applied correctly: scaling the input spectrum by 10
   leaves per-primary TID unchanged but scales the source-normalized TID rate by
   10.
4. H100/10 behaves correctly for simple monoenergetic spectra:
   - 50 MeV protons give `H100/10 ≈ 0`,
   - 150 MeV protons give `H100/10 ≈ 1`.
5. The folded mean LET for 100 MeV total-energy alpha particles in silicon is
   larger than the folded mean LET for 100 MeV protons.
6. All supported physics lists run in a short smoke test:
   - `FTFP_BERT`,
   - `FTFP_BERT_HP`,
   - `Shielding`,
   - `QGSP_BIC_HP`.
7. A three-point Al sweep writes scalar outputs and increasing areal density.

Run from the top-level package directory:

```bash
chmod +x tests/run_layer3_tests.sh
tests/run_layer3_tests.sh
```

To see script options:

```bash
tests/run_layer3_tests.sh --help
```

Optional environment variables:

```bash
BUILD_DIR=build_l3 JOBS=8 tests/run_layer3_tests.sh
EVENTS_SHORT=10000 EVENTS_SMOKE=1000 tests/run_layer3_tests.sh
```

Available variables:

```text
BUILD_DIR       Build directory used by the test script.
                Default: build_layer3_tests

EXE_NAME        Executable name.
                Default: shieldSim

LOG_DIR         Directory where test logs are written.
                Default: layer3_test_logs

RUN_DIR         Directory where each Layer-3 test case writes its run outputs.
                Default: layer3_test_runs

JOBS            Number of parallel build jobs.
                Default: nproc, or 2 if nproc is unavailable.

SKIP_BUILD      Set to 1 to skip CMake configure/build.
                Default: 0

EXE_PATH        Explicit executable path when using SKIP_BUILD=1.

EVENTS_SHORT    Events for ordinary Layer-3 cases.
                Default: 2000

EVENTS_SMOKE    Events for quick physics-list/sweep smoke cases.
                Default: 300
```

Example using an already-built executable:

```bash
SKIP_BUILD=1 EXE_PATH=build/shieldSim tests/run_layer3_tests.sh
```

Expected result:

```text
All Layer-3 tests passed.
```

Layer-3 test outputs are written under:

```text
layer3_test_runs/
```

Logs are written under:

```text
layer3_test_logs/
```

These generated directories should not be committed.

---

## Layer-4 Regression Tests

Layer-4 tests verify regression stability of complete end-to-end cases. They are
intended to answer: did a code change alter previously accepted results beyond
the configured tolerance? They do **not** independently validate Geant4 physics.

The implemented Layer-4 workflow uses three modes:

```bash
tests/run_layer4_tests.sh --check            # default: compare against baseline
tests/run_layer4_tests.sh --update-baseline  # replace accepted baseline
tests/run_layer4_tests.sh --actual-only      # run and collect actual JSON only
```

The first trusted Layer-4 run should be reviewed manually, then accepted with:

```bash
chmod +x tests/run_layer4_tests.sh
tests/run_layer4_tests.sh --update-baseline
```

After a baseline exists, normal regression checks are run with:

```bash
tests/run_layer4_tests.sh
```

The accepted baseline is written to:

```text
tests/expected/layer4/layer4_reference.json
```

The current-run result is written by default to:

```text
layer4_test_runs/layer4_actual.json
```

The implemented Layer-4 script runs these deterministic cases:

```text
regression_001_beam_100MeV_p_Al2mm_Si1mm
regression_002_beam_100MeV_alpha_Al2mm_Si1mm
regression_003_isotropic_SEP_Al2mm_BFO50mm_Si1mm
regression_004_builtin_HDPE10mm_BFO50mm_Si1mm
regression_005_highZ_Shielding_W5mm_Si1mm
regression_006_sweep_Al_0p5_to_2mm_Si1mm
```

For each case, the script compares compact signatures derived from:

```text
run-summary metadata
input/output particle counts
source normalization
incident particle rate
shield thickness and areal density
H100/10
target TID, TID rate, DDD, DDD rate, n_eq, and n_eq rate
LET-spectrum integral and mean LET
compact numeric signatures of spectra and quantities output files
```

The comparison is performed by:

```text
tests/regression_tools.py
```

Default tolerances are intentionally moderate because these are short Monte
Carlo regression cases. The defaults can be overridden:

```bash
REL_TOL=0.05 ABS_TOL=1e-30 tests/run_layer4_tests.sh --check
EVENTS_REGRESSION=20000 EVENTS_SMOKE=5000 tests/run_layer4_tests.sh --check
```

Available variables:

```text
BUILD_DIR          Build directory used by the test script.
                   Default: build_layer4_tests

EXE_NAME           Executable name.
                   Default: shieldSim

LOG_DIR            Directory where test logs are written.
                   Default: layer4_test_logs

RUN_DIR            Directory where each Layer-4 test case writes run outputs.
                   Default: layer4_test_runs

JOBS               Number of parallel build jobs.
                   Default: nproc, or 2 if nproc is unavailable.

SKIP_BUILD         Set to 1 to skip CMake configure/build.
                   Default: 0

EXE_PATH           Explicit executable path when using SKIP_BUILD=1.

EVENTS_REGRESSION  Events for ordinary regression cases.
                   Default: 4000

EVENTS_SMOKE       Events for high-Z and sweep smoke cases.
                   Default: 1000

REL_TOL            Relative tolerance for numerical comparison.
                   Default: 0.15

ABS_TOL            Absolute tolerance for numerical comparison.
                   Default: 1e-30

BASELINE_FILE      Accepted baseline JSON.
                   Default: tests/expected/layer4/layer4_reference.json

ACTUAL_FILE        Actual JSON produced by the current run.
                   Default: layer4_test_runs/layer4_actual.json
```

Example using an already-built executable:

```bash
SKIP_BUILD=1 EXE_PATH=build/shieldSim tests/run_layer4_tests.sh --check
```

Layer-4 generated outputs are written under:

```text
layer4_test_runs/
```

Logs are written under:

```text
layer4_test_logs/
```

Do not update the Layer-4 baseline just to make tests pass. Update it only when
the changed behavior is expected and has been reviewed.

---

## What Should Be Committed

Commit:

```text
tests/*.sh
tests/README.md
tests/data/*.dat
tests/expected/*.md
tests/expected/*.json
tests/expected/layer4/*.md
tests/expected/layer4/*.json
tests/expected/*.txt
tests/logs/.gitignore
```

Do not commit generated build directories, logs, test-run directories, or large
raw output files.

Recommended `.gitignore` entries:

```text
build_layer1_tests/
build_layer2_tests/
build_layer3_tests/
build_layer4_tests/
layer1_test_logs/
layer2_test_logs/
layer3_test_logs/
layer4_test_logs/
layer2_test_runs/
layer3_test_runs/
layer4_test_runs/
tests/logs/
```

---

## Notes on Monte Carlo Testing

Monte Carlo tests are not exactly reproducible unless the random seed, Geant4
version, physics list, production cuts, and geometry are fixed. For reliable
regression testing, use:

```bash
--random-seed=<integer>
--output-prefix=<name>
--dump-run-summary=<file>
```

Useful future diagnostic or convenience options include:

```bash
--mono-proton=<MeV>
--mono-alpha=<MeV>
--disable-protons
--disable-alphas
```

The current package already implements source, exit, and run-summary diagnostic
dumps, which are enough for the implemented Layer-2, Layer-3, and Layer-4 tests.
