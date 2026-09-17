# C18 — deterministic POSIX-thread field initialization

C18 verifies that standalone Mode3D background-field initialization produces the
same initialized mesh and the same exact-rigidity access map when the temporary
POSIX-worker count is changed.

The routine profile tests:

```text
models:                    DIPOLE, IGRF, T96, T05
temporary pthread workers: 1, 2, 4, 8, 16
repeats:                   2
MPI ranks:                 4 by default
```

The calling MPI-rank thread also evaluates one equal share. Therefore
`-mode3d-threads 16 -mode3d-parallel-field-init` means 16 temporary workers plus
the caller, or 17 field-initialization participants per MPI rank. C18 parses the
AMPS log and fails if the reported worker/participant counts do not match.

## No checked-in reference solution

C18 intentionally has **no stored numerical reference**. For each magnetic model,
the runner first generates a serial baseline using the same executable, MPI rank
count, input deck, mesh, trajectory controls, and output grid. It then compares
every pthread run against that runtime baseline.

This makes C18 a parallel-equivalence regression, not a physical-accuracy test. A
deterministic error shared by both serial and threaded execution will not be found
by C18. Physical validation remains the responsibility of C6 (IGRF), C7 (T96), and
C9/C10 (storm-time T05/TS05 observations).

## What is compared

Every AMPS run uses:

```text
-mode3d-output-initialized
```

The runner canonicalizes all finite numeric records in the initialized mesh
output by converting each decimal value to its exact Python binary64 hexadecimal
representation. It then records a SHA-256 fingerprint. Different decimal text
formatting of the same binary64 value does not cause a failure, but any represented
numeric difference does.

The exact-rigidity file

```text
cutoff_3d_shells_access.dat
```

is fingerprinted the same way. Consequently C18 checks the complete written
access map, including coordinates, rigidity, access state, and all other numeric
diagnostics present in the file.

The default cutoff backend is `SERIAL`. Only field initialization is threaded, so
a cutoff-map mismatch is attributable to the initialized field, global field
assembly, stale model state, or output materialization—not to the cutoff thread
queue. `--cutoff-backend THREADS` is available as a separate end-to-end stress
mode.

## Run

From the directory containing the AMPS executable:

```bash
python3 srcEarth/test/C18/run_C18.py \
  --profile ROUTINE \
  -np 4 \
  --amps ./amps
```

Equivalent explicit command:

```bash
python3 srcEarth/test/C18/run_C18.py \
  --models DIPOLE,IGRF,T96,T05 \
  --thread-counts 1,2,4,8,16 \
  --repeats 2 \
  -np 4
```

Fast smoke test:

```bash
python3 srcEarth/test/C18/run_C18.py --profile SMOKE -np 2
```

End-to-end field-plus-cutoff pthread stress test:

```bash
python3 srcEarth/test/C18/run_C18.py \
  --profile ROUTINE \
  --cutoff-backend THREADS \
  -np 4
```

Preview all commands without launching AMPS:

```bash
python3 srcEarth/test/C18/run_C18.py --dry-run
```

Exercise the runner's parser and fingerprint logic without AMPS:

```bash
python3 srcEarth/test/C18/run_C18.py --self-test
```

Reprocess an existing output tree:

```bash
python3 srcEarth/test/C18/run_C18.py \
  --skip-run \
  --output-root test_output/C18_parallel_determinism
```

## Output

```text
test_output/C18_parallel_determinism/
  C18_configuration.json
  C18_commands.json
  C18_comparison.csv
  C18_result.json
  C18_summary.txt
  igrf/
    baseline_serial/
    pthreads_nt01_repeat01/
    ...
```

C18 passes only when every run exits successfully, the worker banner is correct,
and both the initialized-field and cutoff-map fingerprints exactly match the
serial runtime baseline.

## Scope

The default model list contains the standalone models currently evaluated by
`Earth::Mode3D::EvaluateBackgroundMagneticFieldSI`: DIPOLE, IGRF, T96, and T05.
T01 is not included because the current standalone Mode3D evaluator does not
dispatch `FIELD_MODEL T01`; adding it to the parser alone would produce a zero
field rather than a valid T01 test. SWMF is also excluded because the coupled
field is owned by the SWMF coupler and does not use
`-mode3d-parallel-field-init`.
