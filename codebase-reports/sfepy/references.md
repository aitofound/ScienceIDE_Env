# SfePy bibliography: sources and task coverage

[`references.bib`](references.bib) contains 12 distinct publications, ordered with
the upstream software and implementation papers first, then directly cited task
methods. Verification date: 2026-09-12. This is bibliographic work, not a new
scientific run or evidence of accelerator performance.

## Scope and provenance

- Benchmark snapshot inspected: `9b9ac0eea13712f2e40d25e28ac4c4b49a07c761`.
- Upstream: <https://github.com/sfepy/sfepy>, `release_2026.2`, commit
  [`3f01a19fad86d14c1d54706372fe591f8f7bf46c`](https://github.com/sfepy/sfepy/commit/3f01a19fad86d14c1d54706372fe591f8f7bf46c).
- The existing `codebase-metadata.{json,md,html}` and `module-plan.md` are retained
  unchanged; the report was already present, so no backfill was needed.
- All shipped `tasks/sfepy/**` belong to the single
  [`finite-element-multiphysics`](../../tasks/sfepy/finite-element-multiphysics/task.toml)
  task. The review included its task metadata, instructions, authoring notes,
  all 158 `check.json` files, copied upstream example/test citation text, and
  [`physics-family-coverage.json`](../../tasks/sfepy/finite-element-multiphysics/comment/physics-family-coverage.json).
- `work/scienceaccel_inventory.json` mapped **no open/pending PRs** to `sfepy`.
  A live `gh pr list --repo aitofound/ScienceAccelBench --state open --search sfepy
  --limit 100 --json number,title,headRefName,url` also returned `[]` before this
  bibliography PR was opened. There was no additional pending task to incorporate.

## Verification sources

The pinned upstream [`doc/index.rst`, Citing section](https://github.com/sfepy/sfepy/blob/3f01a19fad86d14c1d54706372fe591f8f7bf46c/doc/index.rst#L46-L107)
was retrieved from upstream and compared with the vendored git object. It
recommends all four software entries below. DOI metadata were checked against
publisher-deposited Crossref records (`https://api.crossref.org/works/<DOI>`),
including authors, title, venue, year, volume and pages. The two proceedings
records were also checked on arXiv. The task-specific source locations below
are under `tasks/sfepy/finite-element-multiphysics/tests/checks/`.

| BibTeX key | Verified publication / identifier | Relevance and source evidence |
| --- | --- | --- |
| `CimrmanLukesRohan2019` | [10.1007/s10444-019-09666-0](https://doi.org/10.1007/s10444-019-09666-0) | Primary SfePy paper; upstream Citing section and the task's explicit `references` list. |
| `Cimrman2021WeakForms` | [10.1016/j.advengsoft.2021.103033](https://doi.org/10.1016/j.advengsoft.2021.103033) | Upstream-recommended tensor-contraction / multi-linear weak-form implementation and performance paper. Its measurements concern version 2021.1, not the current benchmark. |
| `Cimrman2014SfePy` | [arXiv:1404.6391](https://arxiv.org/abs/1404.6391) | Framework and custom finite-element applications; proceedings title, editors and pages from the upstream Citing section. The meeting was in 2013; the cited proceedings year is 2014. |
| `Cimrman2014IGA` | [arXiv:1412.6407](https://arxiv.org/abs/1412.6407) | SfePy isogeometric discretization; directly relevant to `deck-diffusion-poisson-iga` and `deck-linear-elasticity-linear-elastic-iga`. Proceedings details from upstream. |
| `PinhoDaCruz2009HomogenisationI` | [10.1016/j.commatsci.2009.02.025](https://doi.org/10.1016/j.commatsci.2009.02.025) | Reference [2] in both `deck-homogenization-linear-homogenization/upstream.py` and `deck-homogenization-linear-homogenization-up/upstream.py`. |
| `Oliveira2009HomogenisationII` | [10.1016/j.commatsci.2009.01.027](https://doi.org/10.1016/j.commatsci.2009.01.027) | Reference [3] in the same two homogenization decks; finite-element procedures and multiscale applications. |
| `CioranescuPaulin1979` | [10.1016/0022-247X(79)90211-7](https://doi.org/10.1016/0022-247X(79)90211-7) | Reference [1] in the same two homogenization decks; perforated-domain homogenization. |
| `RohanLukes2012Perfusion` | [10.3182/20120215-3-AT-3016.00182](https://doi.org/10.3182/20120215-3-AT-3016.00182) | Explicit reference in `deck-homogenization-perfusion-micro/upstream.py`. |
| `RohanLukes2018Piezoelectric` | [10.1016/j.ijsolstr.2018.05.017](https://doi.org/10.1016/j.ijsolstr.2018.05.017) | Explicit reference in `deck-multi-physics-piezo-elasticity-macro/upstream.py` and `deck-multi-physics-piezo-elasticity-micro/upstream.py`. |
| `Gonzalez2018InverseMass` | [10.1002/nme.5613](https://doi.org/10.1002/nme.5613) | Explicit reference in `deck-linear-elasticity-elastodynamic/upstream.py` and `deck-linear-elasticity-seismic-load/upstream.py`; related `elastodynamic-reciprocal-mass` regression. |
| `Hohenberger2019Elastomers` | [10.5254/rct.19.80387](https://doi.org/10.5254/rct.19.80387) | Reference [1] in `driver-gen-yeoh-tl-up-interactive/upstream.py`; generalized Yeoh material model. |
| `MatthiesLube2007Oseen` | Preprint Series 2007-02, 2007 | Exact citation in `deck-navier-stokes-stabilized-navier-stokes/upstream.py`, independently retrieved from the [pinned upstream example](https://github.com/sfepy/sfepy/blob/3f01a19fad86d14c1d54706372fe591f8f7bf46c/sfepy/examples/navier_stokes/stabilized_navier_stokes.py#L1-L10). Verification is at the upstream-documentation level, not an independently retrieved copy of the preprint. |

## Complete shipped-task coverage

`CimrmanLukesRohan2019` and `Cimrman2014SfePy` provide software provenance for the
entire task, including shared assembly, quadrature, interpolation, projection,
constraints, material transformations, solvers and test infrastructure. The
following table accounts for **all 19 families** in the shipped coverage file.
Additional references are listed only where the method or example gives a
specific connection; a general SfePy citation does not imply that a paper
individually derives every example or validates all 158 checks.

| Shipped physics family | Additional method reference(s), beyond general SfePy provenance |
| --- | --- |
| `static-linear-elasticity` | `Cimrman2014IGA` for the IGA deck. |
| `linear-elastodynamics` | `Gonzalez2018InverseMass` for reciprocal-mass formulation. |
| `linear-viscoelasticity` | General software references; no additional paper asserted here. |
| `elastic-spectra-band-gaps` | General software references; no additional paper asserted here. |
| `elastic-contact` | General software references; no additional paper asserted here. |
| `shells-membranes` | General software references; no additional paper asserted here. |
| `trusses-springs` | General software references; no additional paper asserted here. |
| `nonlinear-solid-mechanics` | `Hohenberger2019Elastomers` for the generalized Yeoh driver. |
| `elliptic-diffusion` | `Cimrman2014IGA` for the IGA Poisson deck. |
| `transient-scalar-transport` | General software references; no additional paper asserted here. |
| `incompressible-flow` | `MatthiesLube2007Oseen` for the stabilized Navier–Stokes deck. |
| `acoustic-helmholtz` | General software references; no additional paper asserted here. |
| `single-particle-quantum` | General software references; no additional paper asserted here. |
| `porous-media-flow` | `RohanLukes2012Perfusion` for perfusion homogenization. |
| `piezoelectricity` | `RohanLukes2018Piezoelectric` for the micro/macro homogenization decks. |
| `thermoelasticity` | General software references; no additional paper asserted here. |
| `joule-heating` | General software references; no additional paper asserted here. |
| `elastic-homogenization` | `PinhoDaCruz2009HomogenisationI`, `Oliveira2009HomogenisationII`, `CioranescuPaulin1979`. |
| `flexoelectricity` | `Cimrman2021WeakForms` for the multi-linear term implementation, not a flexoelectric application paper. |

## Metadata decisions and limits

- Nine journal articles, two proceedings papers and one technical report; unique
  citation keys and identifiers. The 2019 and 2021 arXiv versions are included
  within their journal records rather than duplicated as separate entries.
- The publisher-deposited author order for homogenisation **Part II** starts with
  **J. A. Oliveira**, unlike the copied example's Pinho-da-Cruz-first list. The
  bibliography follows the publisher. Part I remains Pinho-da-Cruz first.
- For the inverse-mass article, the online date is 2017, but volume 113(2),
  pages 277–295 is the **2018** print issue, consistent with the upstream citation.
- Initials are retained where that is the verified source metadata. No missing
  proceedings publisher, technical-report DOI, or expanded Oseen-author given
  names has been guessed. The attempted institutional Oseen-preprint URL returned
  HTTP 500; only the upstream citation is claimed as verification for that entry.
  A differently titled Oseen journal paper was not substituted.
- The contact example links to optional IPC Toolkit rather than a specific
  paper. That link alone is not treated as evidence of an IPC benchmark or a
  reason to add an unrelated application paper. Web tutorials and general
  dependency links are not promoted into invented journal references.
- Existing coverage gaps and distinctions remain intact: flexoelectricity is an
  operator-level regression, not a coupled boundary-value solve. Bibliographic
  coverage does not convert unmeasured or excluded cases into tested cases.

## Validation

- BibTeX 0.99d (TeX Live 2024), `plain` style, rendered all **12** entries with
  no warnings or errors.
- Required fields, year syntax, non-placeholder content, 12 unique keys,
  nine unique DOIs and four unique arXiv identifiers checked.
- All 158 shipped check JSON files parsed; all 19 coverage-family rows and their
  check paths verified. This is metadata validation, not scientific execution.
- `git diff --check` and `git diff --cached --check` passed; the staged file
  scope is only `codebase-reports/sfepy/references.bib` and `references.md`.
