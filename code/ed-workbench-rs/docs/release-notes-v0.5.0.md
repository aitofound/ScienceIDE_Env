# v0.5.0: Symmetry-resolved large-scale FCI and HPC evidence

v0.5.0 extends the validated v0.4.0 solver into larger exact sectors while
preserving every primary Quantum Harness #129 acceptance result.

## Release continuity

The post-submission sequence now has a clear role for every release:

| Release | Purpose |
|---|---|
| v0.1.1 | bounded all-electron H2O/cc-pVDZ benchmark without point-group reduction |
| v0.2.0 | general active spaces and checked numerical contracts |
| v0.3.0 | restartable disk-backed Davidson storage |
| v0.4.0 | deterministic bounded-memory parallel sigma |
| v0.5.0 | symmetry-resolved exact FCI, extended validation, and HPC evidence |

The original H2O/6-31G frozen-core result remains unchanged at
`-76.121174204141980 E_h`, and all 36 published CC, CI, and MBPT entries
remain accepted at the precision printed by Hirata and Bartlett.

## Converged all-electron H2O/cc-pVDZ FCI

The headline v0.5.0 result is a converged finite-basis full-CI calculation:

| Quantity | Value |
|---|---:|
| geometry | `R(O-H)=0.967 Å`, `angle(H-O-H)=107.6°` |
| basis | spherical cc-pVDZ |
| electrons | all 10 electrons, no frozen core |
| spatial orbitals | 24 |
| spin sector | `Nalpha=Nbeta=5`, `MS2=0` |
| point group and irrep | C₂ᵥ A1 |
| determinants without point-group reduction | 1,806,590,016 |
| determinants in the A1 block | 451,681,246 |
| Rust Davidson FCI | **`-76.243218589558566 E_h`** |
| residual norm | **`6.602e-8`** |
| iterations | 21 |
| wall time | 3:55:43 |
| Slurm step MaxRSS | 222.257 GiB |

Relative to the earlier reviewer-requested benchmark, the only intentional
feasibility change is exact reduction to the C₂ᵥ A1 block. The requested
geometry, basis, all-electron treatment, 24 spatial orbitals, ten electrons,
singlet sector, Hamiltonian convention, and residual threshold remain
unchanged.

Spatial symmetry block diagonalizes the same finite-basis Hamiltonian. It
does not truncate the A1 wave function. The water ground state is the lowest
singlet A1 state.

The exact production FCIDUMP, SHA-256, unedited Slurm output, Slurm error log,
machine-readable result, and submitted batch script are committed. PySCF
2.14.0 independently reproduces the same RHF input and provides MP2, CISD,
CCSD, and CCSD(T) cross-checks. The same-input CCSD(T) energy is
`-76.24257144581735 E_h`, which lies `0.647144 mE_h` above the Rust FCI
result. This gap is consistent with the scale reported in the cited
all-electron water literature while respecting its different geometry and
printed precision.

See
[`reports/h2o-ccpvdz-c2v-fci.md`](../reports/h2o-ccpvdz-c2v-fci.md)
and
[`reports/data-provenance.md`](../reports/data-provenance.md).

## Extended exact-method validation

Point-group metadata and compact symmetry-sector enumeration now propagate
through FCIDUMP parsing, problem construction, determinant addressing, dense
and direct FCI, CI, MBPT, CC, and UCC paths.

New accepted calculations include:

| System | Determinants | Rust result |
|---|---:|---:|
| H2O/DZ all electron | 1,002,708 | `-76.156699030930056 E_h` |
| H2O/DZP frozen core | 28,233,466 | `-76.256624441300147 E_h` |

The release also adds stretched-water CC and Davidson fixtures at
`1.5 R_e` and `2.0 R_e`, block Davidson excited-state roots, and a
dense-verified full-rank H4 UCC check.

## SCNet reproducibility and throughput evidence

The pinned v0.4.0 executable was independently rebuilt and tested on SCNet
with an offline Rust, Cargo, libcint, CMake, and compiler environment.

The committed evidence records:

* one successful 56-CPU build, test, and numerical smoke gate;
* 18 of 18 converged Davidson robustness cases;
* 216 of 216 converged repeated solves across ten observed nodes;
* a maximum absolute energy deviation of `8.10e-13 E_h`;
* 37 verified task-level SHA-256 manifests;
* 960 downloaded evidence files;
* an observed peak of 560 allocated CPUs.

The submitted 1,008-CPU gang job uses 72 independent processes with 14 Rayon
threads each. The committed evidence does not claim that 1,008 CPUs were
observed, and it does not describe this task-parallel ensemble as multi-node
strong scaling of one Davidson eigenproblem.

See
[`reports/scnet-hpc-benchmark.md`](../reports/scnet-hpc-benchmark.md)
and
[`fixtures/hpc/scnet-2026-07-30.json`](../fixtures/hpc/scnet-2026-07-30.json).

## Verification

Normal release verification remains:

```bash
uv sync --locked
scripts/verify-submission.sh
```

The committed cc-pVDZ evidence has a dedicated Rust integration test:

```bash
cargo test --locked --test ccpvdz_fci_result
cargo test --locked --test scnet_hpc_fixture
```

## Scope boundary

This release does not claim a completed all-electron cc-pVDZ solve without
point-group reduction. It does not add MPI-distributed CI vectors, GPU
execution, selected CI, or PT2. The SCNet ensemble measures portability,
determinism, robustness, and task throughput. The headline FCI energy is exact
only for the recorded finite orbital basis, electron sector, spin sector, and
C₂ᵥ A1 irrep.
