# SCNet HPC Davidson Benchmark

Date: 2026-07-30

Energy unit: hartree (`Eh`)

Memory units: `KiB` and `GiB` (binary)

## Outcome

The pinned v0.4.0 Rust source was built, tested, and benchmarked on the
authorized SCNet `giggleliu` allocation.  The completed evidence contains:

- one 56-CPU offline build, test, and numerical smoke job;
- one 18-case H2O/6-31G frozen-core Davidson robustness matrix;
- 216 repeated Davidson solves across ten observed nodes;
- 37 independently rechecked task-level SHA-256 manifests;
- 960 downloaded evidence files and no non-empty program stderr files.

All 18 robustness cases and all 216 repeated solves converged.  The maximum
absolute deviation from the fixed reference energy
`-76.121174204141980 Eh` was `8.10e-13 Eh`.

The first two ensemble jobs requested a maximum of 1,008 CPUs but did not
observe that concurrency.  The short first job peaked at 280 CPUs.  The
longer replicate job peaked at 560 CPUs on ten nodes.  During a live snapshot,
eight 56-CPU elements were running while ten were pending with
`AssocGrpCpuLimit`; additional capacity became available later in the run.
No unrelated jobs were cancelled.

An initial 1,152-CPU gang request, SCNet job `23015348`, was cancelled while
still pending and before consuming CPU time because long-running work under
the same association left only about 1,096 CPUs available.  It was replaced
by the 1,008-CPU gang submission documented below.  Therefore the completed
evidence still claims an observed 560-CPU peak until the replacement job
passes its fail-closed result gate.

## Reproducibility Gate

SCNet job `23015273` completed in 71 seconds on node `a01r05n04` with 56
allocated CPUs.  It used a fully offline dependency set on the compute node.

| Gate | Wall time | Peak RSS | Exit |
|---|---:|---:|---:|
| release build | `32.47 s` | `827,684 KiB` | 0 |
| all-target test suite | `29.26 s` | `657,944 KiB` | 0 |
| primary Davidson smoke | `3.00 s` | `187,456 KiB` | 0 |

The executable and every input were pinned:

| Item | Identity |
|---|---|
| source commit | `48f1964a1b3b88090497e1ffce285fde09c98541` |
| Cargo.lock SHA-256 | `3e47c3256ebc4bb6503c447f124c2050d8a8c718f567ff1e53efe727d196533b` |
| FCIDUMP SHA-256 | `826dd373a8b6047dff8136168431a803b59d9ef029a074da3b8f74f22603db3e` |
| release binary SHA-256 | `25a0617d029542bdcf21556cf4303627347535387ce10a13a19c99d28f11c81b` |
| Rust | `rustc 1.89.0 (29483883e 2025-08-04)` |
| CPU model | AMD EPYC 7742 |

The smoke calculation returned:

```text
energy:        -76.121174204142079 Eh
residual norm: 5.044e-8
iterations:    16
converged:     true
```

## Independent Small-System Accuracy

The compute-node binary reproduced the existing PySCF references:

| System | Rust dense FCI (`Eh`) | PySCF FCI (`Eh`) | Absolute error (`Eh`) |
|---|---:|---:|---:|
| H2/STO-3G | `-1.137270174660904` | `-1.137270174660904` | `2.220e-16` |
| linear H4/STO-3G | `-2.166387448634769` | `-2.166387448634763` | `6.217e-15` |
| H2O/STO-3G | `-75.012918738044235` | `-75.012918738044462` | `2.274e-13` |

All three checks used a `1e-10 Eh` acceptance tolerance and passed.

## 18-Case Numerical Robustness Matrix

SCNet job `23015277` evaluated:

```text
residual tolerances = {1e-6, 1e-7, 1e-8}
maximum subspaces   = {12, 16, 20, 24, 32, 48}
```

Each case used 56 Rayon workers and 56 fixed sigma source blocks.  All cases
exited zero and reported convergence.

| Requested tolerance | Cases | Observed residual range | Iterations | Energy range (`Eh`) |
|---|---:|---:|---:|---:|
| `1e-6` | 6 | `4.279e-7`–`7.727e-7` | 14 | `7.390e-13` |
| `1e-7` | 6 | `3.891e-8`–`5.044e-8` | 16–17 | `8.384e-13` |
| `1e-8` | 6 | `4.027e-9`–`5.674e-9` | 18–19 | `7.248e-13` |

Across all 18 cases, the energy range was `9.237e-13 Eh`.  The complete Slurm
accounting records show five simultaneous tasks at the peak:

```text
5 tasks × 56 allocated CPUs = 280 CPUs
5 distinct nodes
```

The individual solves lasted only about 2.4–3.8 seconds, shorter than the time
needed for Slurm to expand the entire array.

## 216-Solve Repeatability Run

SCNet job `23015308` repeated every matrix case 12 times.  It produced:

- 216/216 converged solves;
- 216/216 zero command exits;
- 18/18 cases with exactly identical stored energy across their 12 repeats;
- no non-empty program stderr;
- a total cross-parameter energy range of `9.237e-13 Eh`.

Per-solve resource statistics were:

| Quantity | Minimum | Median | Mean | Maximum |
|---|---:|---:|---:|---:|
| wall time (`s`) | 2.15 | 2.845 | 2.852 | 4.27 |
| CPU utilization (`%`) | 1,704 | 2,068 | 2,062.6 | 2,406 |
| effective busy CPU cores | 17.04 | 20.68 | 20.63 | 24.06 |
| peak RSS (`KiB`) | 173,680 | 187,654 | 187,940 | 214,920 |

Slurm accounting reconstructs the true peak as:

```text
10 tasks × 56 allocated CPUs = 560 CPUs
10 distinct nodes
```

The CPU-time measurements show why allocation alone is not a sufficient
parallel-efficiency claim: a single 56-CPU solve kept only about 21 CPU cores
busy on average for this small determinant space.

## Utilization-Oriented 1,008-CPU Design

The replacement gang job, SCNet job `23015354`, addresses the per-solve
saturation directly:

```text
18 nodes
× 4 independent solver processes per node
× 14 Rayon threads per process
= 72 processes and 1,008 concurrent CPUs
```

The 72 process ranks map four ways onto each of the 18 parameter cases.  Each
rank runs three sequential repeats, preserving the same 216-sample scientific
matrix.  This is task-parallel ensemble throughput, not MPI strong scaling of
one Davidson eigenproblem.  Packing moderately threaded solves is expected to
use the allocation more effectively than assigning 56 CPUs to a single
245,025-determinant solve.

This job remains fail-closed: it must return all 72 worker statuses, all 216
convergence records, and all worker SHA-256 manifests before the fixture may be
updated to claim a successful thousand-core observation.

## H2O/cc-pVDZ All-Electron Boundary

The preflight also reran the requested no-point-group-symmetry,
all-electron H2O/cc-pVDZ bounded benchmark with 56 Rayon threads:

| Quantity | Value |
|---|---:|
| spatial orbitals | 24 |
| electrons | 10 |
| determinants | 1,806,590,016 |
| one dense CI vector | `13.460145 GiB` |
| minimum current Davidson storage | `67.300723 GiB` |
| 24-vector-pair subspace | `646.086937 GiB` |
| bounded measured estimate | `1.030791 GiB` |
| Rust RHF energy | `-76.025792594842571 Eh` |
| PySCF RHF absolute error | `6.220e-11 Eh` |
| sparse-kernel throughput | `2.302e7 contributions/s` |

The JSON explicitly records `full_fci_executed: false`.  For the pinned
v0.4.0 no-point-group-symmetry path, a full all-electron cc-pVDZ Davidson
solve was not claimed because its required vectors could not fit within one
approved node.  The bounded kernel is evidence for the runnable stages and
the memory boundary, not a fabricated full-FCI result.

A later, separate calculation reduced the same Hamiltonian to its C₂ᵥ A1
sector and converged the resulting 451,681,246-determinant FCI problem.  That
result and its distinct Slurm provenance are documented in
[`h2o-ccpvdz-c2v-fci.md`](h2o-ccpvdz-c2v-fci.md); it does not retroactively
turn this no-symmetry bounded benchmark into a full-FCI run.

## Evidence and Regeneration

The committed machine-readable artifact is
[`fixtures/hpc/scnet-2026-07-30.json`](../fixtures/hpc/scnet-2026-07-30.json).
It includes every completed Slurm task row, all 18 one-shot case records, and
all 216 replicate measurements.

After downloading the three immutable remote run directories and the `sacct`
rows, regenerate it with:

```bash
python3 scripts/hpc/summarize_scnet.py \
  --evidence-root /path/to/downloaded/evidence \
  --output fixtures/hpc/scnet-2026-07-30.json
```

The summarizer rechecks every `SHA256SUMS` file before writing output.  The Rust
integration test `tests/scnet_hpc_fixture.rs` independently enforces the
completed job counts, bounded cc-pVDZ scope, residual tolerances, convergence,
energy stability, and the distinction between requested and observed
thousand-core concurrency.

## Environment Bring-Up Record

No failed environment attempt was promoted into scientific evidence.  The
retained SCNet job history documents the fail-closed bring-up:

| Job | Failure isolated |
|---|---|
| `23015008` | Slurm spool path invalidated a relative orchestration import |
| `23015011` | compute node had no external DNS |
| `23015045` | shared Cargo target/vendor metadata bottleneck |
| `23015082` | recursive shared vendor copy bottleneck |
| `23015179` | system CMake 2.8 was too old |
| `23015262` | shallow local libcint snapshot could not be recloned |
| `23015273` | final offline build/test/numerical gate passed |

The final path uses a pinned standalone Rust toolchain, a versioned Cargo
vendor archive expanded on node-local storage, CMake 3.25, GCC 11.4, and a
non-shallow local libcint v6.1.2 Git snapshot.

## Claim Boundary

The completed evidence proves:

- the pinned Rust executable builds and passes its tests on SCNet;
- its small-system numerical references remain correct on the HPC platform;
- H2O/6-31G frozen-core Davidson is stable across the tested tolerance and
  subspace matrix;
- 216 repeated solves are deterministic per case and numerically consistent;
- the completed runs reached an observed peak of 560 allocated CPUs;
- a utilization-oriented 1,008-CPU gang job has been submitted as `23015354`.

It does not prove:

- that one Davidson solve scales across nodes;
- that 1,008 CPUs have already been observed simultaneously;
- that allocated CPUs equal busy CPUs;
- that the pinned v0.4.0 no-point-group-symmetry path can complete
  all-electron H2O/cc-pVDZ FCI.
