# Bibliography evidence and coverage

Verified on **2026-09-12**. `references.bib` is ordered with the primary
codebase first, followed by emission physics and its implementation, the
model-coordinate conventions used in sampling, and the optional GPU backend.
These are five distinct works; the GRFF paper and archived software are not
duplicate citations. Concept/version DOI aliases are not listed separately.

## Scope reviewed

- Existing report: `codebase-metadata.json` and `codebase-metadata.md` already
  describe `raytracing-grff` / GRFFradioSun, its approved `ray-transport` module,
  and the external dependencies. No report backfill or metadata change was needed.
- Benchmark baseline: ScienceAccelBench commit
  `9b9ac0eea13712f2e40d25e28ac4c4b49a07c761`.
- **Shipped tasks:** `git ls-tree -r --name-only HEAD -- tasks/raytracing-grff`
  returned no paths. This also avoids treating sparse-checkout absence as
  evidence of absence. The supplied `work/scienceaccel_inventory.json` does
  not include this codebase in `main_task_codebases`.
- **Open/pending-review PRs:** filtering that inventory's `open_prs` mapping
  for `codebases` containing `raytracing-grff` returned zero PRs, so there
  were no mapped PR bodies or task files to inspect. A live
  `gh pr list --repo aitofound/ScienceAccelBench --state open --search raytracing`
  also returned none before this bibliography PR was opened.
- **Upstream:** reviewed the vendored README, package metadata, citation/link
  search across the source tree, and matching upstream GitHub content at
  [`3d0306f0cf2484475ee9c7b0a33230eeae1c6330`](https://github.com/peijin94/raytracingGRFF/tree/3d0306f0cf2484475ee9c7b0a33230eeae1c6330).
  The snapshot has no CITATION/CFF file or preferred paper citation. The
  README describes CPU/CuPy ray tracing, LOS sampling, GRFF voxel preparation,
  MAS inputs and optional external fastGRFF emission synthesis.

## Entry-by-entry verification

| BibTeX key | Role and authoritative evidence |
| --- | --- |
| `grffradiosun2026raytracing` | Primary software reference covering the ray-transport module, including ray integration and sampling. The pinned [README](https://github.com/peijin94/raytracingGRFF/blob/3d0306f0cf2484475ee9c7b0a33230eeae1c6330/README.md) supplies GRFFradioSun and the workflow; [pyproject.toml](https://github.com/peijin94/raytracingGRFF/blob/3d0306f0cf2484475ee9c7b0a33230eeae1c6330/pyproject.toml) supplies `raytracingGRFF`, the description, version `0.1.0`, and the collective author `GRFFradioSun contributors`. The [commit](https://github.com/peijin94/raytracingGRFF/commit/3d0306f0cf2484475ee9c7b0a33230eeae1c6330) is dated 2026-09-07. The year is the snapshot year, not an inferred publication/release date. |
| `fleishman2021gyroresonance` | Scientific background for the external GRFF gyroresonance/free-free emission engine. [Publisher DOI](https://doi.org/10.3847/1538-4357/abf92c) and [publisher-deposited Crossref metadata](https://api.crossref.org/works/10.3847/1538-4357/abf92c) verify all three authors, title, June 2021, *The Astrophysical Journal* 914(1), article 52. The abstract describes the multithermal emission theory and available code; its reference `apjabf92cbib22` cites the GRFF software DOI below. This is not presented as a paper about this repository's RK4/CuPy implementation. |
| `kuznetsov2021grff` | Original archived GRFF implementation. [DataCite metadata](https://api.datacite.org/dois/10.5281/zenodo.4625572) verifies author order, title, 2021-03-21, version `v1.0.0`, and [Zenodo DOI](https://doi.org/10.5281/zenodo.4625572); it links the [upstream release tree](https://github.com/kuznetsov-radio/GRFF/tree/v1.0.0). The [GRFF README](https://github.com/kuznetsov-radio/GRFF/blob/7b60a227146df4505dc92f139f65919e33432aed/README.md) identifies the engine and compiled libraries. The archived release is cited for attribution, not as an asserted runtime pin. |
| `predictivesciencePsiIoOverview` | Official model-data conventions supporting MAS coordinate conversion and sampling. The raytracingGRFF README directly links the [PSI overview](https://predsci.com/doc/psi-io/guide/overview.html). This page identifies Predictive Science Inc., the spherical scales, Python `(phi, theta, r)` array order, HDF conventions, and MAS physical quantities. Publication year is omitted because it is not stated; access date is retained. This is a documentation citation, not a claim that psi-io or a particular MAS dataset is shipped. |
| `zhang2026fastgrff` | Optional CUDA/C + CuPy emission backend named in the raytracingGRFF README. The [fastGRFF README's Cite as section](https://github.com/peijin94/fastGRFF/blob/5c2c00cd6132d078cb234c95dfc6fc548b84071b/README.md) gives the citation. [DataCite metadata](https://api.datacite.org/dois/10.5281/zenodo.18436419) independently verifies the title, Peijin Zhang, 2026-01-30, version `v0.1.0`, and [Zenodo DOI](https://doi.org/10.5281/zenodo.18436419). The author's Romanized name is normalized to BibTeX `Zhang, Peijin`; the source also includes the Chinese-script name. This does not imply that this backend version is vendored or used by a shipped task. |

## Preserved unknowns and boundaries

No dedicated publication or DOI for the pinned raytracingGRFF snapshot was
identified in the reviewed upstream sources. Its repository citation is not
substituted with an unrelated ray-tracing paper. The existing report's
unknown license, unspecified MAS dataset, absent external GRFF library, and
unknown GRFF/fastGRFF runtime pins remain unchanged. No emission benchmark,
scientific acceptance policy, performance result, or passing CPU/CUDA parity
claim is inferred from these references.

Only this report directory is changed. BibTeX software records deliberately
use standard `@misc` rather than a style-specific `@software` type. Validation
uses the installed BibTeX parser with `plain.bst`, checks all five rendered
entries, rejects placeholders/duplicate keys or DOIs, checks changed-file
scope, and runs `git diff --check`; no source or task tests are changed.
