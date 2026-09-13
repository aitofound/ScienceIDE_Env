# EFTCAMB bibliography and coverage

[`references.bib`](references.bib) contains eight distinct works, ordered with
the primary implementation and current extension first. Published papers and
their arXiv versions share one entry. This is an informational bibliography;
it does not change the source, task contract, tolerances, or existing generated
[codebase metadata](codebase-metadata.md).

## Upstream and verification

The shipped task pins [`EFTCAMB/EFTCAMB` at
`16d9c4e9f85751e30efd0a53b177941713078904`](https://github.com/EFTCAMB/EFTCAMB/tree/16d9c4e9f85751e30efd0a53b177941713078904).
Its [README, sections 2 and 4](https://github.com/EFTCAMB/EFTCAMB/blob/16d9c4e9f85751e30efd0a53b177941713078904/README.md)
identifies the numerical notes and H-EFTCAMB extension, and requests the
original EFTCAMB implementation, EFTCAMB/EFTCosmoMC constraints, H-EFTCAMB,
and original CAMB papers. The first bibliography entry follows the citation
section's order; the second describes the H-EFTCAMB line in the pinned tree.

Author lists, titles, arXiv identifiers and categories were checked against
the linked arXiv API records. Journal details come from publisher-deposited
Crossref metadata, as indicated below.

| BibTeX key | Authoritative verification | Relevance |
|---|---|---|
| `Hu2014EFTCAMB` | [arXiv:1312.5742](https://arxiv.org/abs/1312.5742), including *Physical Review D* **89**, 103530 (2014), DOI `10.1103/PhysRevD.89.103530` | Original EFTCAMB implementation of full linear perturbation dynamics; requested upstream citation. |
| `Ye2026HEFTCAMB` | [arXiv:2603.01662](https://arxiv.org/abs/2603.01662), including all nine authors | Current H-EFTCAMB extension; explicitly cited by the task and pertinent to its full-Horndeski acceleration workload. |
| `Pan2025EFTCAMBInitialConditions` | [arXiv API metadata](https://export.arxiv.org/oai2?verb=GetRecord&identifier=oai:arXiv.org:2506.17411&metadataPrefix=arXiv) and [Crossref publisher record](https://api.crossref.org/works/10.1103/zqkg-3kkh) | The task-owned `09_EFTCAMB_IC.f90` implements the paper's theory-consistent constant-Omega and constant-Gamma-plus-Omega initial-condition branches at lines 290–319. Its line-570 `D1`/`D2` inconsistency is in the separate legacy GR-style branch and is not this paper's correction. |
| `Hu2014EFTCAMBNumericalNotes` | [arXiv:1405.3590](https://arxiv.org/abs/1405.3590), including submission history | Numerical equations and implementation guide explicitly cited by the task. First issued in 2014; the title's **v3.0** corresponds to **arXiv version 4**, revised 27 September 2017. |
| `Lewis2000CAMB` | [arXiv:astro-ph/9911177](https://arxiv.org/abs/astro-ph/9911177), including *Astrophysical Journal* **538**, 473–476 (2000), DOI `10.1086/309179` | Original CAMB paper requested by EFTCAMB; GR baseline and shared CMB projection infrastructure. |
| `Raveri2014EFTCAMBConstraints` | [arXiv:1405.1022](https://arxiv.org/abs/1405.1022), including *Physical Review D* **90**, 043513 (2014), DOI `10.1103/PhysRevD.90.043513` | Second original package paper requested upstream; pure EFT and designer-f(R) context. This is not a claim that the task runs cosmological inference. |
| `Benevento2019Kmouflage` | [arXiv:1809.09958](https://arxiv.org/abs/1809.09958) and [Crossref publisher record](https://api.crossref.org/works/10.1088/1475-7516/2019/05/027) | EFTCAMB implementation of K-mouflage and K-mimic; cited in the task's background audit. Published in *JCAP* **2019**(05), 027; the preprint is from 2018. |
| `DeFelice2017Stability` | [arXiv:1609.03599](https://arxiv.org/abs/1609.03599) and [Crossref publisher record](https://api.crossref.org/works/10.1088/1475-7516/2017/03/027) | Modified-gravity stability analysis cited by the task's background-consistency investigation. The BibTeX uses the publisher's title, which includes “in the presence”. |

No journal or journal DOI is assigned to the two arXiv-only records: none was
established from the sources inspected. Article numbers are stored in `pages`
for conventional BibTeX compatibility.

## Shipped task coverage

The only shipped leaf under `tasks/eftcamb/` is
[`eftcamb-linear-perturbations`](../../tasks/eftcamb/eftcamb-linear-perturbations/task.toml).
Its manifest cites the numerical notes, H-EFTCAMB and upstream repository.
It describes coupled photon, baryon, dark-matter, neutrino, metric and EFT scalar
perturbations, source functions, transfers and initial conditions. Its 12 check
manifests contain 73 model decks in total. The core implementation and numerical
notes supply shared context across these families; the table records more
specific links without claiming each check has a separate paper.

| Shipped checks | Decks | Bibliographic coverage |
|---|---:|---|
| All 12 checks (initial-condition path) | 73 | `Pan2025EFTCAMBInitialConditions` documents the constant-Omega and constant-Gamma-plus-Omega branches in the task-owned initial-condition source. The shipped decks do not set `EFT_IC_type`, so the pinned default `1` selects the legacy GR-style branch; the paper-derived branches are present in scope but are not directly selected by these checks. |
| `gr-baseline` | 1 | `Lewis2000CAMB`; core EFTCAMB context. |
| `pure-eft-omega`, `pure-eft-gamma`, `pure-eft-wde` | 7 + 21 + 2 | `Hu2014EFTCAMB`, `Hu2014EFTCAMBNumericalNotes`, `Raveri2014EFTCAMBConstraints`. |
| `rph-alpha-basis` | 7 | Core implementation and numerical notes. |
| `designer-fr`, `designer-mc5e` | 4 + 8 | Core implementation and numerical notes; the constraints paper additionally discusses designer f(R). |
| `horava` | 7 | Numerical notes and shared EFTCAMB implementation. |
| `kmouflage`, `kmimic` | 3 + 3 | `Benevento2019Kmouflage`; core numerical notes; `DeFelice2017Stability` for the retained stability investigation. |
| `quintessence-galileon` | 6 | Numerical notes and shared EFTCAMB implementation. The five `5_quint_*` decks select `EFTflag=4`, `FullMappingEFTmodel=4` (Quintessence), and `5_scg.ini` selects `EFTflag=4`, `FullMappingEFTmodel=6` (Scaling Cubic Galileon); none selects the separate `EFTflag=5` covariant-Horndeski extension, so `Ye2026HEFTCAMB` is not assigned to this row. |
| `horndeski-full` (acceleration-labelled) | 4 | `Ye2026HEFTCAMB` and shared EFTCAMB implementation. |

The additional citations are traceable to the shipped
[background audit](../../tasks/eftcamb/eftcamb-linear-perturbations/comment/kmimic-background-audit/README.md)
and [background-consistency investigation](../../tasks/eftcamb/eftcamb-linear-perturbations/comment/kmimic-background-consistency/README.md).
These are scientific context, not new claims about oracle accuracy, physical
stability, or validation of the task's numerical bounds. In particular, the
stability citation does not imply that disabled mass-stability gates were tested.

## Pending-review coverage and validation boundary

The supplied `scienceaccel_inventory.json` snapshot had no open/pending-review
PR mapped to `eftcamb` before this bibliography PR was opened. There was
therefore no mapped PR adding another task or citation to inspect. The existing
report is present, so no metadata backfill or speculative changes to its
unknowns were needed.

Bibliography validation consists of a real BibTeX run with `plain.bst` and
`\\citation{*}`, checking that all eight entries render, plus duplicate-key,
duplicate-DOI/arXiv, required-field and placeholder checks. Repository validation
is limited to `git diff --check` and changed-path scope. No scientific workload,
GPU benchmark, or task selfcheck is claimed by this bibliography-only change.
