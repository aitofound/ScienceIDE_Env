# EDKit bibliography: sources and coverage

Verified on 2026-09-12. `references.bib` contains four distinct works, ordered
by relevance: the upstream software, the task's explicit physical reference,
and two supplementary method papers. Existing metadata reports are unchanged.

## Codebase and shipped-task coverage

- **Upstream:** [EDKit.jl](https://github.com/jayren3996/EDKit.jl), version
  `0.5.0`, pinned to `538fce882ab73e3af447f4bc6a1704d290c88aba`.
  `edkit2026` credits the software itself, including its exact-diagonalization,
  symmetry-basis, matrix-free operator and time-evolution capabilities.
- **All shipped task leaves:**
  [`adaptive-krylov-time-evolution`](../../tasks/edkit/adaptive-krylov-time-evolution/task.toml)
  is the only `tasks/edkit/**/task.toml` in the inspected tree. It packages
  Hermitian real-time state-vector propagation, adaptive Lanczos basis
  reuse/extension/restart and reconstruction. Its 24 checks comprise 22 small
  numerical checks, one API check, and the full-basis L20 XXZ-quench workload.
  `edkit2026` and `sandvik2010quantumspin` cover both references explicitly
  listed in the task. The check
  [physical-reference explanation](../../tasks/edkit/adaptive-krylov-time-evolution/tests/checks/operator-dense-reference/README.md#physical-reference)
  identifies Sandvik sections 4.1–4.2 as spin-model construction and
  finite-precision Lanczos reliability background, not a real-time error-bound
  prescription. The supplementary papers below explain the task's underlying
  Lanczos propagator and matrix-exponential approximation, not new tasks.
- **Pending-review coverage:** the supplied `work/scienceaccel_inventory.json`
  has no open PR mapped to `edkit`. A live
  `gh pr list --repo aitofound/ScienceAccelBench --state open --search edkit`
  likewise returned no PRs at verification time. The shipped evidence's
  [source PR #495](https://github.com/aitofound/ScienceAccelBench/pull/495) and
  [task PR #499](https://github.com/aitofound/ScienceAccelBench/pull/499) were
  inspected with `gh pr view`; both were already **merged**. No additional
  pending task needed bibliography coverage.

## Entry verification

### `edkit2026` — upstream software

- The upstream
  [Project.toml at the pin](https://github.com/jayren3996/EDKit.jl/blob/538fce882ab73e3af447f4bc6a1704d290c88aba/Project.toml)
  declares `name = "EDKit"`, `authors = ["JayRen <…>"]`, and
  `version = "0.5.0"`. The bibliography retains the declared handle rather
  than inferring a personal name from GitHub account or commit metadata.
- The [commit record](https://github.com/jayren3996/EDKit.jl/commit/538fce882ab73e3af447f4bc6a1704d290c88aba)
  is dated 2026-05-08; **2026 is the year of this software snapshot**, not an
  asserted publication year for a software paper. The
  [license](https://github.com/jayren3996/EDKit.jl/blob/538fce882ab73e3af447f4bc6a1704d290c88aba/LICENSE)
  is MIT.
- The upstream Git tree at this pin contains no dedicated CITATION/CFF or
  bibliography file. No software-paper DOI or preferred scholarly citation
  was established from the inspected README, package metadata and method
  documentation, so this is a repository citation without an invented DOI.

### `sandvik2010quantumspin` — explicit task reference

- [arXiv:1101.3281](https://arxiv.org/abs/1101.3281) verifies the title, sole
  author **Anders W. Sandvik**, journal reference
  `AIP Conf.Proc.1297:135,2010`, and DOI `10.1063/1.3518900`.
- [Publisher-deposited Crossref metadata](https://api.crossref.org/works/10.1063/1.3518900)
  verifies the proceedings container, year and pages **135–338**. Its author
  list also includes Adolfo Avella and Ferdinando Mancini; the bibliography
  uses the paper authorship on arXiv instead of copying that conflicting list.
- The publication is from **2010**, despite the arXiv deposit being in 2011.
  The DOI and arXiv identifier are recorded in **one entry**, not duplicated
  as separate works.

### `park1986lanczos` — unitary Lanczos propagation

- [Publisher-deposited Crossref metadata](https://api.crossref.org/works/10.1063/1.451548)
  verifies Tae Jun Park and J. C. Light, the title, *The Journal of Chemical
  Physics* **85**(10), **5870–5876** (1986), and DOI `10.1063/1.451548`.
- This is background for propagating a quantum state using iterative Lanczos
  reduction. It is not asserted to be a citation requested by EDKit or a
  description of EDKit's particular adaptive controller.

### `saad1992matrixexponential` — Krylov approximation analysis

- [Publisher-deposited Crossref metadata](https://api.crossref.org/works/10.1137/0729014)
  verifies Y. Saad, the title, *SIAM Journal on Numerical Analysis* **29**(1),
  **209–228** (1992), and DOI `10.1137/0729014`.
- This is mathematical background for Krylov approximations to the action of
  a matrix exponential, not an assertion of a task-specific error guarantee.

The supplementary method choices are grounded in the pinned upstream
[time-evolution manual](https://github.com/jayren3996/EDKit.jl/blob/538fce882ab73e3af447f4bc6a1704d290c88aba/docs/src/manual/time-evolution.md),
which explicitly describes the Lanczos relation, reduced tridiagonal
exponential, basis reuse, extension/restart, and boundary-term defect monitor.
The manual's upstream Git blob `de982007b9b99d4b752c2b6663e5b34bde62be61`
matches the vendored file. Neither supplementary paper is represented as an
upstream-mandated citation. Publisher landing-page access for Park–Light
returned HTTP 403; verification used the publisher's Crossref deposit instead.
