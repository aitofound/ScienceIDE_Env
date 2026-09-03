# Web and GitHub Snapshot

Snapshot date: 2026-07-27 (Asia/Shanghai)

This is a curated, source-linked snapshot rather than a mirror. It records
facts that affect implementation, registration, reproducibility, or dependency
selection. Live pages remain authoritative.

## Upstream Challenge

Source: https://github.com/QuantumBFS/quantum.harness/issues/129

| Field | Snapshot |
|---|---|
| State | Open |
| Labels | `challenge`, `accepted` |
| Author | Guo P. Chen (`chenpeizhi`) |
| Created | 2026-07-24T14:40:29Z |
| Updated | 2026-07-26T01:42:00Z |
| Method | Exact Diagonalization |
| Contact | guochen@hkust-gz.edu.cn |
| Comments | None at snapshot time |

Implementation-critical points in the issue:

- ED and FCI are the same finite-basis eigenproblem in the intended molecular
  electronic-structure setting.
- FCIDUMP contains spatial-orbital integrals in Mulliken ordering and uses
  one-based orbital indices.
- The determinant basis factorizes into alpha/beta occupation strings.
- The mandatory direct-CI kernel is `sigma = H C`, driven by precomputed
  single-excitation lists and fermionic signs rather than a stored Hamiltonian.
- Davidson is the mandatory ground-state eigensolver.
- General-order CC(n) reuses determinant operators on full-FCI-length vectors,
  constructs `exp(T)|HF>` as a Taylor series, and solves projected residual
  equations with denominator updates and DIIS.
- CC(2) means CCSD here; it does not mean the approximate method CC2.
- Level 4 can use the Rust `libcint` crate to build a direct-integral RHF and
  AO-to-MO path.

## Original Registration PR Snapshot

Source: https://github.com/QuantumBFS/quantum.harness/pull/210

| Field | Snapshot |
|---|---|
| State | Open, non-draft |
| Mergeability | Mergeable |
| Checks | None reported |
| Base | `QuantumBFS/quantum.harness:main` |
| Head | `JunkaiWang-TheoPhy:challenge/ed-wangtheophys-rust-workbench` |
| Created | 2026-07-27T10:27:32Z |
| Updated | 2026-07-27T10:35:33Z |
| Registered team | Rewrite It In Rust! (RIIR 2607 Hefei) |
| Members | Chenxi Wan, Yedi Shen, Junkai Wang |
| Solution path | `tracks/ed/solutions/WangTheoPhys/README.md` |

The PR contains two commits at snapshot time:

- `56d93d0` — Register WangTheoPhys ED workbench challenge.
- `641aeef` — Update WangTheoPhys ED workbench repo name.

PR #210 was subsequently closed and superseded by active solution PR
[#217](https://github.com/QuantumBFS/quantum.harness/pull/217). As re-checked
on 2026-07-27, #217 is open, non-draft, mergeable, uses the same head branch,
and contains:

- `tracks/ed/solutions/WangTheoPhys/README.md`
- `tracks/ed/solutions/WangTheoPhys/reproduction-prompt.md`

The dedicated AGPL-3.0 workbench was also made public for anonymous
reproduction:
https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust

## Narrow Ecosystem Context

- [ED track](https://github.com/QuantumBFS/quantum.harness/tree/main/tracks/ed)
  is the official upstream placement for this solution.
- Issues [#114](https://github.com/QuantumBFS/quantum.harness/issues/114)
  and [#115](https://github.com/QuantumBFS/quantum.harness/issues/115) matter
  only when fulfilling #129's required tenferro-rs gap-list deliverable. They
  are not additional project scope.

## Working Repository

Source:
https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust

| Field | Snapshot |
|---|---|
| Visibility | Private |
| Default branch | `main` |
| Description | Private workspace for Quantum Harness challenge #129: Rust ED/FCI workbench |
| License | AGPL-3.0 |
| Created | 2026-07-27T10:25:48Z |
| Canonical remote | `https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust.git` |

## Reference Software Snapshot

Versions are the latest GitHub releases visible on the snapshot date. They are
not automatic dependency pins.

| Project | Role | Latest visible release | License |
|---|---|---|---|
| [PySCF](https://github.com/pyscf/pyscf) | RHF, AO-to-MO, FCIDUMP, FCI, CCSD oracle | `v2.14.0` (2026-07-18) | Apache-2.0 |
| [libcint](https://github.com/sunqm/libcint) | Gaussian integral engine | `v6.1.3` (2025-08-17) | Apache-2.0 |
| [libcint crate](https://crates.io/crates/libcint) | Rust bindings and molecule/integral API | docs.rs showed `0.3.2` | See crate metadata |
| [Psi4NumPy](https://github.com/psi4/psi4numpy) | Readable reference implementations | `v1.0` (2018-05-16) | BSD-3-Clause |
| [tenferro-rs](https://github.com/tensor4all/tenferro-rs) | Rust tensors, einsum, linalg, AD, CPU/GPU | published crates at `0.2.0`; pre-1.0 API | MIT OR Apache-2.0 |
| [Quantum Package 2](https://github.com/QuantumPackage/qp2) | Determinant-driven comparison | `2.1.2` | AGPL-3.0 |
| [faer](https://github.com/sarah-quinones/faer-rs) | Rust linear algebra fallback | `faer-v0.24.4` | MIT |
| [argmin](https://github.com/argmin-rs/argmin) | Optional unitary-CC optimization | `argmin-v0.11.0` | Apache-2.0 |

## Documentation Entry Points

- PySCF FCIDUMP API:
  https://pyscf.org/pyscf_api_docs/pyscf.tools.html#module-pyscf.tools.fcidump
- PySCF FCIDUMP implementation:
  https://pyscf.org/_modules/pyscf/tools/fcidump.html
- PySCF FCI:
  https://pyscf.org/pyscf_api_docs/pyscf.fci.html
- PySCF coupled cluster:
  https://pyscf.org/pyscf_api_docs/pyscf.cc.html
- Rust libcint bindings:
  https://docs.rs/libcint/latest/libcint/
- tenferro user guide:
  https://tensor4all.org/tenferro-rs/
- tenferro tensor 0.2.0 API:
  https://docs.rs/tenferro-tensor/0.2.0/tenferro_tensor/
- tenferro gather/scatter backend trait:
  https://docs.rs/tenferro-tensor/0.2.0/tenferro_tensor/backend/trait.TensorIndexing.html
- tenferro memory-order guide:
  https://tensor4all.org/tenferro-rs/guides/memory-order.html
- tenferro supported-operation inventory:
  https://tensor4all.org/tenferro-rs/design/supported-ops.html
- tenferro normative specifications:
  https://tensor4all.org/tenferro-rs/spec/

## Provenance Policy

- GitHub issue/PR state and release versions are dated observations and may
  change.
- Numerical anchors and method requirements trace to issue #129 and its cited
  papers.
- API behavior should be checked against linked official documentation and
  source at implementation time.
- Third-party text is summarized here; this repository does not mirror whole
  websites or vendor copyrighted pages.

## Level 4 Implementation Snapshot

- The production dependency is `libcint` crate 0.3.2 with
  `build_from_source` and `static`.
- The checked path computes overlap, kinetic, nuclear-attraction, and AO ERIs
  inside Rust, then runs Rust RHF/DIIS, AO-to-MO, and direct FCI/Davidson.
- H2 and H2O/STO-3G complete without Python at runtime and match PySCF FCI
  within `2e-10` hartree.
- The tenferro 0.2.0 audit confirmed documented gather/scatter and strided
  views, while identifying collision-reducing scatter-add and allocation-free
  BLAS-1 updates as the remaining direct-FCI API gaps.
