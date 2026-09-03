# H2O/cc-pVDZ All-Electron Bounded Benchmark

Date: 2026-07-28

Reviewer request: [Quantum Harness PR #217](https://github.com/QuantumBFS/quantum.harness/pull/217#issuecomment-5100990552)

Coordinate input unit: angstrom

Energy unit: hartree

## Result

The requested H2O/cc-pVDZ all-electron input was executed in Rust without
point-group symmetry. The bounded command completed in a median wall time of
`1.42 s` across five independent release-process runs. Maximum observed
resident memory was `468,975,616 bytes` (`447.25 MiB`, `0.437 GiB`), below the
2 GiB command budget.

The calculation used all 10 electrons and 24 spatial orbitals in the fixed
`Nalpha = Nbeta = 5` sector. Fixing electron number and `MS2 = 0` is not a
point-group symmetry reduction.

Rust RHF converged in 13 iterations:

| Quantity | Value |
|---|---:|
| Rust RHF | `-76.025792594842471 Eh` |
| PySCF 2.14.0 RHF, `symmetry=False` | `-76.025792594904772 Eh` |
| absolute error | `6.230e-11 Eh` |
| final density RMS | `2.915e-9` |

## Exact Full-Space Size

The no-point-group-symmetry determinant product space is

```text
alpha strings = C(24, 5) = 42,504
beta strings  = C(24, 5) = 42,504
determinants  = 42,504^2 = 1,806,590,016
```

This is about `7,373.08x` the 245,025-determinant H2O/6-31G frozen-core
challenge calculation.

| Full-space object | Storage |
|---|---:|
| one dense `f64` CI vector | `13.460145 GiB` |
| current diagonal plus four initial/work vectors | `67.300723 GiB` |
| 24 Davidson basis plus 24 sigma vectors | `646.086937 GiB` |

No complete CI vector, Hamiltonian diagonal, Davidson subspace, or converged
full-FCI energy was allocated or claimed. The benchmark stops at a
diagonal-free sparse Hamiltonian source-column kernel.

## Executed Pipeline

```text
H2O geometry
  -> embedded PySCF 2.14.0 cc-pVDZ H/O basis in NWChem form
  -> libcint AO integrals
  -> all-electron Rust RHF/DIIS
  -> staged AO-to-MO transformation
  -> 42,504 alpha + 42,504 beta strings and 8,500,800 one-body links
  -> 16 evenly distributed sparse Hamiltonian source columns
```

The sparse-column path shares the same absorbed one-body integrals,
creation/annihilation links, fermionic signs, and spin-free link-pair algebra
as the full direct-FCI operator. A small-system regression test compares every
sparse source column against the original full-vector operator.

For the 16 measured columns:

| Kernel quantity | Value |
|---|---:|
| raw Hamiltonian contributions | `640,016` |
| accumulated nonzero destinations | `202,176` |
| median sparse-column time | `0.038472 s` |
| median throughput | `1.664e7 contributions/s` |
| deterministic checksum | `-503.655818952010236` |

The kernel is currently serial. `RAYON_NUM_THREADS=10` was present in the
environment and is recorded by the executable, but no parallel speedup is
claimed for these stages.

## Five-Process Timing

The executable was built once with optimization. Each row below came from a
fresh process using the same command and input.

| Stage | Run 1 | Run 2 | Run 3 | Run 4 | Run 5 | Median |
|---|---:|---:|---:|---:|---:|---:|
| AO integrals (s) | 0.162204 | 0.109001 | 0.298459 | 0.201854 | 0.143974 | **0.162204** |
| RHF (s) | 0.011512 | 0.024259 | 0.031870 | 0.022878 | 0.016376 | **0.022878** |
| AO-to-MO (s) | 0.019185 | 0.025779 | 0.044555 | 0.036823 | 0.024314 | **0.025779** |
| strings/links (s) | 0.603671 | 1.141302 | 1.115168 | 1.255049 | 0.861238 | **1.115168** |
| 16 sparse columns (s) | 0.035952 | 0.075601 | 0.045886 | 0.038463 | 0.038472 | **0.038472** |
| process wall (s) | 0.88 | 1.42 | 1.59 | 1.60 | 1.12 | **1.42** |
| peak RSS (MiB) | 444.08 | 444.61 | 447.25 | 444.86 | 446.88 | **444.86** |

The committed
[`benchmark-m4.json`](../fixtures/h2o-ccpvdz-ae/benchmark-m4.json) is one
complete machine-readable run. The table reports all five independent
measurements instead of selecting the fastest run. The
[`benchmark-m4-summary.json`](../fixtures/h2o-ccpvdz-ae/benchmark-m4-summary.json)
artifact records all five raw observations and aggregates in machine-readable
form; integration tests recompute every stored median and maximum.

## Environment

| Item | Value |
|---|---|
| processor | Apple M4 |
| logical CPUs | 10 |
| physical memory | 16 GiB |
| operating system | macOS 15.6, Darwin 24.6.0, arm64 |
| Rust | `rustc 1.95.0 (59807616e 2026-04-14)` |
| build | Cargo `--release` |
| peak-RSS tool | macOS `/usr/bin/time -l` |
| command budget | 2 GiB |
| conservative preflight estimate | `1.030791 GiB` |
| maximum observed RSS | `0.436768 GiB` |

## Reproduction

Build and run the bounded benchmark:

```bash
cargo build --release

/usr/bin/time -l target/release/ed_workbench_rs benchmark h2o-cc-pvdz \
  --sources 16 \
  --memory-budget-gib 2 \
  --json-output fixtures/h2o-ccpvdz-ae/benchmark-m4.json
```

`--memory-budget-gib` rejects the run when the conservative preflight estimate
exceeds the selected budget. It is not an operating-system hard memory limit.
The v0.1.1 option name `--max-memory-gib` remains a compatibility alias.

Regenerate only the independent PySCF RHF/space reference:

```bash
.venv/bin/python scripts/oracle/generate.py h2o-ccpvdz-ae
```

That oracle command explicitly skips FCIDUMP, FCI, CCSD, and MP2 for this
1.806-billion-determinant target.

The 0.5 GiB guard can be checked without constructing AO integrals or link
tables:

```bash
target/release/ed_workbench_rs benchmark h2o-cc-pvdz \
  --sources 1 \
  --memory-budget-gib 0.5
```

It exits with an estimate-exceeds-budget error.

## Scope Boundary

This benchmark answers the performance request for every runnable stage that
fits the approved few-GiB budget and measures a representative determinant
Hamiltonian kernel. A converged no-symmetry full-FCI calculation would require
a blocked, out-of-core, or distributed Davidson/vector design. Implementing
that solver is separate work; the present report does not extrapolate the
sparse-column timing into a false full-FCI wall time.

## Symmetry-adapted follow-up

On 2026-07-30, a separate C₂ᵥ A1 calculation converged the all-electron
cc-pVDZ FCI energy in a 451,681,246-determinant block. That result does not
retroactively turn the bounded benchmark above into a no-symmetry full solve:
the 1,806,590,016-determinant no-point-group space was not allocated.

The follow-up preserves all ten electrons and all 24 spatial orbitals. It uses
the exact block diagonalization allowed by point-group symmetry and is
documented in
[`h2o-ccpvdz-c2v-fci.md`](h2o-ccpvdz-c2v-fci.md), with its FCIDUMP,
same-input PySCF cross-check, machine-readable result, and unedited Slurm logs
under `fixtures/h2o-ccpvdz-ae`.
