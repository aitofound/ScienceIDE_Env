# MFEM bibliography: provenance and coverage

The seven entries in [`references.bib`](references.bib) are ordered with the
upstream preferred citation first, then the software and the methods relevant to
the NURBS task. The existing generated metadata report is preserved unchanged.
This bibliography does not update module approvals or benchmark results.

## Scope inspected

- Benchmark base: `9b9ac0eea13712f2e40d25e28ac4c4b49a07c761`.
  `git ls-tree -r --name-only HEAD -- tasks/mfem` returned no files at that base:
  there are **no shipped MFEM task leaves** in this snapshot.
- The codebase inventory maps one open task PR to MFEM:
  [#647, `nurbs-isogeometric`](https://github.com/aitofound/ScienceAccelBench/pull/647).
  Inspected with `gh pr view` and `gh api` on 2026-09-12, while it was open, at
  head `bfd7be6bd7ab3013ff859c36681e3b50e3c5b9f9`. Its
  [task catalogue](https://github.com/aitofound/ScienceAccelBench/blob/bfd7be6bd7ab3013ff859c36681e3b50e3c5b9f9/tasks/mfem/nurbs-isogeometric/task.toml),
  instruction, PR body, and `comment/pipeline/module.json` establish 20 checks
  over 45 registered invocations. This is pending work, not a shipped task.
- Both the existing codebase report and that task identify the upstream pin as
  [`mfem/mfem@d964264cdb9a13e94a201b6c236c7721e0c8765f`](https://github.com/mfem/mfem/tree/d964264cdb9a13e94a201b6c236c7721e0c8765f).
  The task itself cites the MFEM article and pinned software. The additional
  references below document its numerical methods, not additional task leaves.

## Citation verification

Authoritative records were accessed on **2026-09-12**. Crossref links below are
publisher-deposited DOI metadata; DataCite supplies the software record's year.
Titles, authors, publication years, venues, volumes and pages were checked where
applicable. All seven works have distinct DOIs; the article and software are
separate objects, and no preprint duplicate is included.

| BibTeX key | Authoritative source and verification | Relevance |
|---|---|---|
| `anderson2021mfem` | Upstream [`CITATION.cff`](https://github.com/mfem/mfem/blob/d964264cdb9a13e94a201b6c236c7721e0c8765f/CITATION.cff), retrieved directly with `gh api`; [DOI metadata](https://api.crossref.org/works/10.1016/j.camwa.2020.06.009) agrees on all 17 authors, 2021, volume 81, pages 42–74. | Upstream preferred citation and the task's explicit paper reference; framework and partial-assembly context. |
| `mfemSoftware` | The same CFF specifies the project title, MFEM Team author, URL and [software DOI](https://doi.org/10.11578/dc.20171025.1248). [DataCite](https://api.datacite.org/dois/10.11578/dc.20171025.1248) gives publication year 2010 and links the [DOE software record](https://www.osti.gov/doecode/biblio/35738). | Software identity for every check. The CFF's team credit is retained; the DOI is project-level, **not** a DOI for the task's particular revision. |
| `piegl1997nurbs` | [Springer book page](https://link.springer.com/book/10.1007/978-3-642-59223-2) verifies Les Piegl, Wayne Tiller, edition 2, publisher, print ISBN and copyright 1997. It separately lists print publication in 1996 and electronic publication in 2012; the entry uses the edition's copyright year. | Explicitly cited by pinned `mesh/nurbs.cpp` (including algorithms A5.5 and A5.8) and `mesh/nurbs.hpp`; rational basis, derivatives and geometry-preserving knot operations. |
| `hughes2005isogeometric` | [Publisher DOI metadata](https://api.crossref.org/works/10.1016/j.cma.2004.10.008) verifies the three authors, title, 2005, volume 194, issue 39–41, pages 4135–4195. | Foundational isogeometric-analysis background selected for the task's stated use of the same rational spline basis for geometry and solution; not asserted to be an explicit source-code citation. |
| `buffa2010electromagnetics` | Pinned [`fem/fe/fe_nurbs.hpp`](https://github.com/mfem/mfem/blob/d964264cdb9a13e94a201b6c236c7721e0c8765f/fem/fe/fe_nurbs.hpp) explicitly names the paper and full authors for the H(curl) elements; [DOI metadata](https://api.crossref.org/works/10.1016/j.cma.2009.12.002) and DOI-negotiated BibTeX verify 2010, volume 199, issue 17–20, pages 1143–1152. | Curl-conforming spline spaces in the Maxwell and de Rham checks. |
| `buffa2011stokes` | The same upstream header explicitly names the paper and full authors for the H(div) elements; [publisher DOI metadata](https://api.crossref.org/works/10.1002/fld.2337) verifies 2011, volume 65, issue 11–12, pages 1407–1422. | Stable compatible spline spaces used by the mixed Darcy, solenoidal and de Rham checks. |
| `evans2013divergence` | The same upstream header explicitly names this paper for the H(div) elements; [publisher DOI metadata](https://api.crossref.org/works/10.1016/j.jcp.2013.01.006) verifies both authors, 2013, volume 241, pages 141–167. | Divergence-conforming B-spline construction; this does **not** mean the task solves the unsteady Navier–Stokes equations. |

## Coverage of pending task `nurbs-isogeometric`

Every row also inherits `anderson2021mfem` and `mfemSoftware`. Check names below
are the complete 20-check catalogue at the inspected PR head; method references
explain the algorithmic context, not benchmark tolerances or expected outputs.

| Checks (all names have prefix `nurbs-`) | Additional references and boundary |
|---|---|
| `basis-function-output`, `curve-interpolation`, `interpolation-point-families`, `knot-operations-invariance`, `mesh-io-and-patch-loading`, `mesh-topology-report`, `naca-cmesh-generation` | `piegl1997nurbs` covers the common spline/basis/knot geometry machinery. The exact point families, file formats and NACA construction are defined by the pinned miniapps/unit tests, not claimed as individually derived from this book. |
| `poisson-single-patch`, `poisson-multipatch`, `poisson-no-integration-by-parts`, `poisson-periodic`, `poisson-weak-boundary`, `two-patch-interface-matching` | `hughes2005isogeometric`, `piegl1997nurbs`: isogeometric discretization, rational derivatives and patch geometry. Exact boundary and patch-interface variants remain defined by `miniapps/nurbs/nurbs_ex1.cpp` and the task checks. |
| `patch-full-integration`, `patch-partial-assembly` | `hughes2005isogeometric`, `piegl1997nurbs`, plus the MFEM article's assembly context. Pinned `miniapps/nurbs/nurbs_patch_ex1.cpp` explicitly describes patch-wise matrix/partial assembly and full/reduced integration. No separate publication for its particular reduced rule was established; no specific quadrature paper is attributed to this implementation. |
| `maxwell-definite` | `buffa2010electromagnetics`: H(curl)-conforming NURBS elements. |
| `derham-interpolators` | `buffa2010electromagnetics`, `buffa2011stokes`, `evans2013divergence`: compatible grad/curl/div spline spaces. |
| `darcy-mixed`, `solenoidal-fields` | `buffa2011stokes`, `evans2013divergence`: H(div)-conforming and divergence-free constructions. The pinned `nurbs_solenoidal.cpp` explicitly describes the mixed Darcy projection. |
| `hyperelasticity` | `hughes2005isogeometric`, `piegl1997nurbs` for the spline discretization, with the MFEM article for framework context. The nonlinear/transient problem is specified by pinned `nurbs_ex10.cpp`; no more specific constitutive-model publication is claimed. |

The task is calibrated on a serial build. Parallel miniapps and other proposed
modules listed in the generated report are not silently promoted to shipped or
pending tasks here. Benchmark execution was not repeated for this bibliography-only
change; citation validation is distinct from numerical self-validation.
