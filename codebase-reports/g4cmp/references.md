# G4CMP bibliography: provenance and coverage

`references.bib` is ordered by relevance: the primary G4CMP paper, the exact
software release used by the benchmark, the material-extension paper needed for
sapphire, the original transport description, the anharmonic-decay method, and
the Geant4 simulation framework. Verification date: **2026-09-12**.

## Scope audited

- The existing `codebase-metadata.{json,md,html}` report is retained unchanged;
  no report backfill was necessary.
- The only shipped task is
  [`g4cmp-phonon-transport`](../../tasks/g4cmp/g4cmp-phonon-transport/task.toml).
  Its task metadata, authoring notes, environment Dockerfile, and all eight
  check READMEs were inspected. The task pins
  `558c3ce54b1c2fcc4023cf40c3c67eea93d459d5` / `g4cmp-V10-01-01`.
- The supplied `work/scienceaccel_inventory.json` maps **no open/pending-review
  PRs** to `g4cmp` (checked both `codebases` and changed-path mappings).
  `gh pr list --repo aitofound/ScienceAccelBench --state open --search g4cmp`
  also returned no results before this bibliography PR was created. There were
  consequently no mapped PRs requiring an additional task review.
- Charge-carrier and quasiparticle modules are proposed-only in the existing
  report, not shipped tasks. The general G4CMP references cover the upstream
  codebase; this bibliography does not imply those modules were benchmarked.

## Verification sources and decisions

| BibTeX key | Authoritative evidence | Reason for inclusion |
|---|---|---|
| `kelsey2023g4cmp` | [Pinned upstream README](https://github.com/G4CMP/G4CMP/blob/558c3ce54b1c2fcc4023cf40c3c67eea93d459d5/README.md), [arXiv:2302.05998](https://arxiv.org/abs/2302.05998), [publisher-deposited Crossref record](https://api.crossref.org/works/10.1016/j.nima.2023.168473) | Upstream-recommended primary description of G4CMP, anisotropic propagation, transport processes and validation. Crossref confirms the full 47-author list, NIM A **1055**, article **168473**, **2023**. |
| `g4cmp2026v100101` | [Pinned source tree](https://github.com/G4CMP/G4CMP/tree/558c3ce54b1c2fcc4023cf40c3c67eea93d459d5), [annotated tag record](https://api.github.com/repos/G4CMP/G4CMP/git/tags/e909d6c214378b689ff884978b81286dd9485fca) | Reproducible software identity, including the exact boundary-transmission implementation and all example/tool programs. The annotated tag resolves to the task's commit and is dated **2026-02-24**. Authors are the five credited in that README, not a guessed exhaustive contributor list. |
| `hernandez2025athermal` | Pinned README's paper list, [arXiv:2408.04732](https://arxiv.org/abs/2408.04732), [publisher-deposited Crossref record](https://api.crossref.org/works/10.1016/j.nima.2024.170172) | Extends G4CMP phonon transport to sapphire and other substrate materials. Crossref confirms NIM A **1073**, article **170172**, **April 2025**. The BibTeX year is the journal year, not the 2024 preprint year or the year embedded in the DOI. |
| `brandt2014transport` | Pinned README's paper list and [arXiv:1403.4984](https://arxiv.org/abs/1403.4984) | Foundational Geant4 phonon/charge-transport simulation, including anisotropic propagation, caustics and heat-pulse validation. Title, all 12 authors and 2014 submission verified on arXiv. No unverified journal or DOI is supplied. |
| `tamura1985decay` | Pinned README's phonon-parameter table; [GetTTDecayProb source](https://github.com/G4CMP/G4CMP/blob/558c3ce54b1c2fcc4023cf40c3c67eea93d459d5/library/src/G4CMPAnharmonicDecay.cc); [DOI record](https://doi.org/10.1103/PhysRevB.31.2574) | Anharmonic downconversion used in the full-physics check. DOI BibTeX content negotiation confirms Shin-ichiro Tamura, *Physical Review B* **31**(4), **2574–2577**, **1985**. It resolves the source's abbreviated/mistyped `PRL31, 1985` comment; it is not presented as the isotope-scattering paper. |
| `agostinelli2003geant4` | [DOI record](https://doi.org/10.1016/S0168-9002(03)01368-8), pinned G4CMP README, task environment Dockerfile | Geant4 event/track/process framework used by the transport checks. DOI BibTeX content negotiation confirms title, full author list, NIM A **506**(3), **250–303**, **2003**. This is a framework citation, not a claim that the paper describes the task's exact Geant4 **11.3.0** build. |

The legacy `kelseymh/G4CMP` task URL currently redirects to `G4CMP/G4CMP`;
GitHub's API was used to confirm the same commit and tag at the canonical owner.
The 2023 paper's author list in the task is not consistent with arXiv and
publisher-deposited metadata. The bibliography uses the agreeing authoritative
47-author list; the out-of-scope task file was not changed.

DOI metadata was checked using Crossref's publisher-deposited record or DOI
content negotiation with `Accept: application/x-bibtex`. Some additional
Crossref/CSL requests were rate-limited; all included works have successful
verification by the sources above. The repository's old
`G4CMP_paper/literature.bib` was inspected but contains unrelated template
references and was not used as bibliographic authority.

## Shipped task coverage

All check paths below are under
`tasks/g4cmp/g4cmp-phonon-transport/tests/checks/`. The pinned software reference
applies to every row. These are scientific-background references, not claims
that the cited papers specify the benchmark's exact inputs or tolerance values.

| Checks | Scientific coverage | Additional reference keys |
|---|---|---|
| `phonon-kinematics-ge`, `phonon-kinematics-si` | Elastic-tensor-based anisotropic phase/group velocities and slowness for the three modes in Ge/Si | `kelsey2023g4cmp`, `brandt2014transport` |
| `kv-lookup-table-ge`, `kv-lookup-table-si` | Wavevector-direction to group-velocity/slowness lookup tables used by phonon stepping | `kelsey2023g4cmp`, `brandt2014transport` |
| `phonon-example-full-physics` | Ge transport with isotope scattering, mode mixing, anharmonic downconversion, and surface absorption | `kelsey2023g4cmp`, `brandt2014transport`, `tamura1985decay`, `agostinelli2003geant4` |
| `phonon-example-ballistic` | Anisotropic propagation and surface interactions with scattering/downconversion disabled | `kelsey2023g4cmp`, `brandt2014transport`, `agostinelli2003geant4` |
| `caustics-sapphire` | Sapphire phonon focusing; the task's acceleration workload | `hernandez2025athermal`, `kelsey2023g4cmp`, `agostinelli2003geant4` |
| `boundary-transmission` | Si/Ge interface transmission using the pinned `G4CMPPhononBoundaryProcess` and `Validation_BoundaryTransmission.mac` | `g4cmp2026v100101` is the exact implementation reference; `kelsey2023g4cmp` and `agostinelli2003geant4` provide framework background |

## Bibliographic conventions

- Six distinct works: four journal articles, one arXiv preprint, and one pinned
  software release. Article/preprint pairs share a single entry with `eprint`;
  no duplicate DOI or arXiv identifier is introduced.
- Full paper author lists are retained. Braces protect software names and
  scientific acronyms; page ranges use BibTeX `--`.
- The source release and its descriptive paper are different citable objects.
  No software DOI is invented, and missing publication details remain absent.

## Validation performed

- BibTeX **0.99d** (TeX Live 2024), `unsrt.bst`, and `\citation{*}` parsed and
  rendered all **six** entries in file order with **no warnings or errors**.
- Required citation fields are nonempty; years are numeric; citation keys,
  DOIs and arXiv identifiers are unique; no placeholder citation text remains.
- All three arXiv records were rechecked for author count/order and primary
  category: **47 / hep-ex**, **10 / physics.ins-det**, and
  **12 / physics.ins-det**, respectively. The task's domain/category labels
  are not substituted for a paper's primary arXiv category.
- `git diff --check`, the staged equivalent, and a changed-file allowlist
  restricted to `codebase-reports/g4cmp/` passed. Only this provenance document
  and `references.bib` are changed. No source/task/runtime tests were needed
  for this bibliography-only change.
