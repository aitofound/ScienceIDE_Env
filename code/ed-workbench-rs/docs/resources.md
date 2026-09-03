# Resources

This file collects the useful upstream GitHub and web information currently
known for Challenge #129.

## Quantum Harness

- Challenge issue: https://github.com/QuantumBFS/quantum.harness/issues/129
- Active solution PR: https://github.com/QuantumBFS/quantum.harness/pull/217
- Superseded registration PR: https://github.com/QuantumBFS/quantum.harness/pull/210
- Official repository: https://github.com/QuantumBFS/quantum.harness
- Project website: http://yaoquantum.org/quantum.harness/
- ED track: https://github.com/QuantumBFS/quantum.harness/tree/main/tracks/ed
- Public workbench: https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust

The complete dated state and provenance are recorded in
[`web-and-github-snapshot.md`](web-and-github-snapshot.md).

## Software Repositories

| Resource | URL | Relevance |
|---|---|---|
| PySCF | https://github.com/pyscf/pyscf | Oracle generation: RHF, MO integrals, FCIDUMP export, FCI and CCSD references. |
| libcint | https://github.com/sunqm/libcint | Gaussian integral engine underneath PySCF; direct Rust integration is the Level 4 stretch target. |
| Psi4NumPy | https://github.com/psi4/psi4numpy | Readable reference implementations for quantum chemistry methods and verification logic. |
| tenferro-rs | https://github.com/tensor4all/tenferro-rs | Rust-native tensor/autodiff stack to evaluate for dense contractions and gap-list reporting. |
| faer-rs | https://github.com/sarah-quinones/faer-rs | Candidate Rust linear algebra backend when operations do not map to tenferro-rs. |
| argmin | https://github.com/argmin-rs/argmin | Candidate Rust optimizer for unitary CC(n) stretch work. |
| pounce | https://github.com/jkitchin/pounce | Candidate Rust optimization package mentioned by the challenge. |
| Quantum Package | https://github.com/QuantumPackage/qp2 | Modern determinant-driven electronic-structure package for conceptual comparison. |
| MRCC | https://www.mrcc.hu | Production general-order coupled-cluster reference named in the challenge. |
| libcint Rust crate | https://crates.io/crates/libcint | Existing Rust bindings for the independent Level-4 integral path. |
| libcint Rust API | https://docs.rs/libcint | `CInt`, molecule builders, integral evaluation, features, and linking instructions. |
| REST | https://github.com/RESTGroup | Rust electronic-structure toolkit from which the libcint wrapper work grew. |
| tenferro benchmark | https://github.com/tensor4all/tenferro-benchmark | Optional destination when a #129 performance gap needs a durable reproducer. |
| tensor-ad-oracles | https://github.com/tensor4all/tensor-ad-oracles | Optional destination when #129 exposes a tensor/autodiff correctness gap. |

## Implementation Documentation

| Topic | URL | Why it matters |
|---|---|---|
| PySCF FCIDUMP API | https://pyscf.org/pyscf_api_docs/pyscf.tools.html#module-pyscf.tools.fcidump | Documents `from_scf`, `from_integrals`, `read`, and the real-Hamiltonian FCIDUMP interface. |
| PySCF FCIDUMP source | https://pyscf.org/_modules/pyscf/tools/fcidump.html | Ground truth for header parsing, packed integral indexing, thresholds, and symmetry behavior. |
| PySCF FCI API | https://pyscf.org/pyscf_api_docs/pyscf.fci.html | Oracle FCI solvers and contraction routines. |
| PySCF CC API | https://pyscf.org/pyscf_api_docs/pyscf.cc.html | CCSD reference energies and amplitudes for Level 0/2 checks. |
| PySCF AO-to-MO API | https://pyscf.org/pyscf_api_docs/pyscf.ao2mo.html | Reference transformation used before FCIDUMP export. |
| tenferro guide | https://tensor4all.org/tenferro-rs/ | Architecture, first CPU example, tensor APIs, devices, and execution models. |
| tenferro API index | https://tensor4all.org/tenferro-rs/api/ | Public crate boundaries for runtime, CPU, einsum, linalg, AD, FFT, and GPU. |
| tenferro tensor 0.2 API | https://docs.rs/tenferro-tensor/0.2.0/tenferro_tensor/ | Owned tensors, strided views, backend contracts, and memory-layout rules checked during the Level 4 gap audit. |
| tenferro indexing trait | https://docs.rs/tenferro-tensor/0.2.0/tenferro_tensor/backend/trait.TensorIndexing.html | Current gather/scatter/slice surface; the documented scatter configuration has no reduction combiner. |
| tenferro memory order | https://tensor4all.org/tenferro-rs/guides/memory-order.html | Column-major ownership and strided-view interoperability with external row-major data. |
| tenferro supported ops | https://tensor4all.org/tenferro-rs/design/supported-ops.html | Operational inventory to check before recording a missing-operation gap. |
| tenferro specification | https://tensor4all.org/tenferro-rs/spec/ | Normative tensor, backend, AD, and operation contracts. |
| libcint crate docs | https://docs.rs/libcint/latest/libcint/ | PySCF-style integral calls, row/column-major behavior, build features, and an RHF example. |

## Primary Literature Cited by the Challenge

| Short name | DOI / URL | Use |
|---|---|---|
| Hirata 2000 | https://doi.org/10.1016/S0009-2614(00)00387-0 | Main CC(n), CI(n), MBPT(n) grading reference; Sec. 2 contains the determinant-based CC recipe. |
| Kallay 2000 | https://doi.org/10.1063/1.481925 | Independent determinant-based general-order CC. |
| Olsen 2000 | https://doi.org/10.1063/1.1290005 | General active-space coupled-cluster implementation. |
| Kallay 2001 | https://doi.org/10.1063/1.1383290 | Extended DZ/DZP grading tables. |
| Knowles 1984 | https://doi.org/10.1016/0009-2614(84)85513-X | Determinant-based FCI and lexical string addressing. |
| Olsen 1988 | https://doi.org/10.1063/1.455063 | Alpha/beta string factorization and determinant CI algorithms. |
| Handy 1980 | https://doi.org/10.1016/0009-2614(80)85158-X | Origin of alpha/beta string factorization. |
| Knowles 1989 | https://doi.org/10.1016/0010-4655(89)90033-7 | Detailed determinant-based FCI program reference. |
| Walter 1963 | https://doi.org/10.1145/366246.366260 | Combination ranking for string-to-index addressing. |
| Buckles 1977 | https://doi.org/10.1145/355732.355739 | Combination unranking for index-to-string addressing. |
| Davidson 1975 | https://doi.org/10.1016/0021-9991(75)90065-0 | Davidson eigensolver. |
| Crouzeix 1994 | https://doi.org/10.1137/0915004 | Davidson method numerical analysis. |
| Pulay 1980 | https://doi.org/10.1016/0009-2614(80)80396-4 | DIIS convergence acceleration. |
| Sherrill 1999 | https://doi.org/10.1016/S0065-3276(08)60532-8 | CI/FCI entry-point review. |
| Sun 2015 | https://doi.org/10.1002/jcc.23981 | libcint reference. |
| Sun 2018 | https://doi.org/10.1002/wcms.1340 | PySCF reference. |
| Smith 2018 | https://doi.org/10.1021/acs.jctc.8b00286 | Psi4NumPy reference-implementation philosophy. |
| Hirata 2003 | https://doi.org/10.1021/jp034596z | Tensor Contraction Engine and symbolic code-generation route. |
| Garniron 2019 | https://doi.org/10.1021/acs.jctc.9b00176 | Quantum Package 2.0 and modern determinant-driven electronic structure. |
| Olsen 1990 | https://doi.org/10.1016/0009-2614(90)85633-N | Historic billion-determinant FCI milestone. |
| Shayit 2025 | https://doi.org/10.1038/s41467-025-65967-7 | Quadrillion-determinant exact-CI scaling frontier cited upstream. |
| Bauschlicher 1986 | https://doi.org/10.1063/1.451034 | Geometry and DZ/DZP basis settings for the extended targets. |
| Bartlett 2007 | https://doi.org/10.1103/RevModPhys.79.291 | Modern coupled-cluster review. |
| Crawford 2000 | https://doi.org/10.1002/9780470125915.ch2 | Pedagogical coupled-cluster introduction. |
| Li 2025 | https://doi.org/10.1063/1674-0068/cjcp2510156 | REST and Rust-native electronic-structure context. |

## Notes From Current Web/GitHub Inspection

- As checked on 2026-07-27, the upstream issue is open and accepted.
- Registration PR #210 was closed and superseded by solution PR
  [#217](https://github.com/QuantumBFS/quantum.harness/pull/217). As checked
  on 2026-07-27, #217 is open, non-draft, mergeable, and targets
  `QuantumBFS/quantum.harness:main`.
- The AGPL-3.0 workbench repository is public so reviewers can clone the exact
  revision referenced by #217 and run the standalone reproduction prompt.
- Repository/release versions in the dated snapshot are discovery aids, not
  dependency pins. Actual code must pin versions in `Cargo.lock` and the Python
  oracle environment.
- The Level 4 implementation pins the Rust `libcint` crate at the Cargo
  resolver-selected 0.3.2 release, builds its C dependency from source, and
  links it statically into the final executable.
- The final tenferro audit checked the published 0.2.0 tensor API. Gather,
  scatter, views, and element-wise division exist; collision-reducing
  scatter-add and an ergonomic in-place BLAS-1 surface remain the relevant
  #129 gaps. See `reports/tenferro-gap-list.md`.
