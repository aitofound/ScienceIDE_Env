> Do not go gentle into that good night,<br>
> Old age should burn and rave at close of day;<br>
> Rage, rage against the dying of the light.<br>
> [Dylan Thomas](https://www.poetryfoundation.org/poets/dylan-thomas), 「**Do Not Go Gentle into That Good Night**」

> 不要温和地走进那良夜，<br>
> 老年应当在日暮时燃烧咆哮；<br>
> 怒斥，怒斥光明的消逝。<br>

![Ranger: determinant states around a gravitationally lensed accretion disk](https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/ranger-pr-banners/assets/ranger/pr-217-ed-fci-accretion-states.png)

# Ranger: completed #129 submission

## Team

| Field | Value |
|---|---|
| Team | Ranger |
| Members | Chenxi Wan, Yedi Shen, Junkai Wang |
| Challenge | [#129: Exact diagonalization workbench in Rust for electronic structure method development](https://github.com/QuantumBFS/quantum.harness/issues/129) |
| Public workbench | [`JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust`](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust) |
| Current release | [`v0.5.0`](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/releases/tag/v0.5.0) |
| License | AGPL-3.0 |

This PR is the public submission and review index. The linked workbench
repository is the authoritative source for Rust code, tests, fixtures, raw
Slurm records, reports, and releases. Python and PySCF construct and audit
independent oracle fixtures. The checked FCI, CC, CI, MBPT, UCC, RHF, and
direct-integral production paths are Rust.

## Primary challenge acceptance

The primary H2O/6-31G Hamiltonian freezes the oxygen 1s orbital and uses
`R(O-H)=0.967 Å`, `angle(H-O-H)=107.6°`, 12 active spatial orbitals,
8 active electrons, and 245,025 determinants.

* Matrix-free FCI: `-76.121174204141980 E_h`, residual `5.044e-8`.
* CC(1) through CC(8): all 8 equilibrium differences match Hirata and
  Bartlett 2000 Table 2 at its six printed decimal places.
* CC(2), meaning CCSD here: `-76.119629519205702 E_h`, only
  `3.025e-10 E_h` from the independent PySCF CCSD oracle.
* CC(8): `-76.121174196144139 E_h`, within `7.998e-9 E_h` of FCI.
* CI(1) through CI(8): all 8 Table 2 entries match; CI(8) is
  `-76.121174204143969 E_h`, within `2.004e-12 E_h` of FCI.
* MBPT(1) through MBPT(20): all 20 Table 2 partial sums match.

Together, the submission matches all 36 equilibrium CI, MBPT, and CC entries
printed in Hirata 2000 Table 2. Comparison respects the paper's six-decimal
precision rather than inventing unprinted digits.

## Design delivered

* Level 0: PySCF oracle generation, FCIDUMP parsing, determinant bases,
  fermionic signs, tiny dense Hamiltonians, and dense FCI.
* Level 1: signed string links, matrix-free spin-free sigma contraction,
  independent diagonal construction, restarted Davidson, disk checkpoints,
  deterministic parallel reduction, and block Davidson roots.
* Level 2: arbitrary-order determinant CC(n), exact ranked subset convolution,
  Taylor-oracle coefficient checks, denominator updates, DIIS, and rank warm
  starts.
* Level 3: warm-started CI(n), recursive MBPT(n), and variational UCC(n).
* Level 4: direct libcint AO integrals, Rust RHF and DIIS, staged AO-to-MO
  transformation, and shared direct FCI.
* Extended exact sectors: compact Abelian point-group enumeration propagated
  through FCI, CC, CI, MBPT, and UCC paths.

## Release progression

| Release | Purpose |
|---|---|
| v0.1.1 | bounded all-electron H2O/cc-pVDZ benchmark without point-group reduction |
| v0.2.0 | general active spaces and checked numerical contracts |
| v0.3.0 | restartable disk-backed Davidson storage |
| v0.4.0 | deterministic bounded-memory parallel sigma |
| v0.5.0 | symmetry-resolved exact FCI, extended validation, and HPC evidence |

Every release preserves the original primary Hamiltonian and accepted
numerical results.

## v0.5.0 all-electron H2O/cc-pVDZ result

The reviewer-requested v0.1.1 benchmark kept point-group symmetry disabled.
It established the 1,806,590,016-determinant memory boundary and intentionally
did not claim a converged FCI energy.

The v0.5.0 production calculation changes only one feasibility condition:
it uses the exact C₂ᵥ A1 block. The requested geometry, spherical cc-pVDZ
basis, all-electron treatment, 24 spatial orbitals, ten electrons, singlet
`Nalpha=Nbeta=5` sector, Hamiltonian convention, and residual threshold remain
unchanged.

| Quantity | Value |
|---|---:|
| determinants without point-group reduction | 1,806,590,016 |
| determinants in C₂ᵥ A1 | 451,681,246 |
| Rust Davidson FCI | **`-76.243218589558566 E_h`** |
| residual norm | **`6.602e-8`** |
| Davidson iterations | 21 |
| Slurm state | `COMPLETED`, exit `0:0` |
| wall time | 3:55:43 |
| Slurm step MaxRSS | 222.257 GiB |

This is exact diagonalization within the recorded finite orbital basis,
electron sector, spin sector, and A1 irrep. Symmetry block diagonalization is
not a selected-determinant truncation of the A1 wave function.

PySCF 2.14.0 independently reproduces the same RHF input and supplies MP2,
CISD, CCSD, and CCSD(T) comparisons. The same-input CCSD(T) energy is
`-76.24257144581735 E_h`, which is `0.647144 mE_h` above FCI. The exact
FCIDUMP, checksum, unedited Slurm logs, resource accounting, and
machine-readable result are public.

## Additional exact-method evidence

| System | Determinants | Rust result |
|---|---:|---:|
| H2O/DZ all electron | 1,002,708 | `-76.156699030930056 E_h` |
| H2O/DZP frozen core | 28,233,466 | `-76.256624441300147 E_h` |

The workbench also validates stretched-water Davidson and CC series at
`1.5 R_e` and `2.0 R_e`, multiple orthogonal Davidson roots for H4, and a
dense-verified full-rank H4 UCC calculation.

## SCNet HPC evidence

The pinned v0.4.0 source was rebuilt, tested, and benchmarked on SCNet with a
fully offline dependency and toolchain path.

* The 56-CPU build, all-target test, and numerical smoke gate passed.
* All 18 Davidson robustness cases converged.
* All 216 repeated solves converged across ten observed nodes.
* The maximum absolute energy deviation was `8.10e-13 E_h`.
* The evidence contains 37 verified task-level SHA-256 manifests and
  960 downloaded files.
* The observed peak was 560 allocated CPUs.

A 1,008-CPU gang experiment was submitted with 72 independent solver
processes. The committed evidence does not claim that 1,008 CPUs were
observed, and it does not present task-parallel ensemble throughput as
multi-node strong scaling of one Davidson eigenproblem.

## Evidence and reproduction

* [v0.5.0 release](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/releases/tag/v0.5.0)
* [standalone reproduction prompt](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.5.0/docs/reproduction-prompt.md)
* [C₂ᵥ FCI report](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.5.0/reports/h2o-ccpvdz-c2v-fci.md)
* [machine-readable C₂ᵥ result](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.5.0/fixtures/h2o-ccpvdz-ae/fci-c2v-xh5-result.json)
* [same-input PySCF cross-check](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.5.0/fixtures/h2o-ccpvdz-ae/pyscf-crosscheck.json)
* [SCNet report](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.5.0/reports/scnet-hpc-benchmark.md)
* [scientific data provenance](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/blob/v0.5.0/reports/data-provenance.md)
* [continuous verification](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/actions/workflows/ci.yml)

Normal acceptance remains:

```bash
uv sync --locked
scripts/verify-submission.sh
```

## Scope boundary

v0.5.0 does not claim a completed all-electron cc-pVDZ solve without
point-group reduction. It does not add MPI-distributed CI vectors, GPU
execution, selected CI, or PT2. The SCNet ensemble establishes portability,
determinism, robustness, and task throughput. The 1,008-CPU submitted job is
not counted as observed thousand-CPU evidence.

## Reviewer checklist

* [ ] Confirm the public v0.5.0 release and checksums resolve.
* [ ] Confirm the normal CI workflow is green.
* [ ] Review the primary 36 published-table matches.
* [ ] Review the symmetry-only feasibility change for the cc-pVDZ result.
* [ ] Review the unedited Slurm records and same-input PySCF comparison.
* [ ] Review the SCNet requested-versus-observed concurrency boundary.
