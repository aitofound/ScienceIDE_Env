# NEST bibliography: provenance and task coverage

The five works in [references.bib](references.bib) describe **NEST: Noble Element
Simulation Technique**, upstream
[NESTCollaboration/nest](https://github.com/NESTCollaboration/nest), not the
similarly named neural-network simulator. The upstream-required original paper
and software archive come first, followed by cross-element methods, argon model
updates, and historical xenon yield refinements. Journal and arXiv versions of
an exact work share one entry; no duplicate preprints or software releases are
included.

## Scope audited

- Repository/task snapshot: `9b9ac0eea` on ScienceAccelBench `main`.
- Upstream source pin, from the existing report and shipped `task.toml`:
  [`307fb27d4c2f9b48dba27529bb57bc243e0ce2dc`](https://github.com/NESTCollaboration/nest/tree/307fb27d4c2f9b48dba27529bb57bc243e0ce2dc).
- The complete shipped `tasks/nest/**` task inventory has one leaf:
  [`noble-element-microphysics`](../../tasks/nest/noble-element-microphysics/).
  Its `task.toml`, `comment/pipeline/module.json`, and all six check READMEs were
  inspected. The existing codebase metadata report is present and unchanged;
  no backfill was needed.
- The supplied `scienceaccel_inventory.json` maps **zero** open/pending-review
  PRs to `nest`. A `gh pr list --repo aitofound/ScienceAccelBench --state open
  --limit 1000` title/branch audit on **2026-09-12** also found no NEST PR before
  this bibliography PR was opened. A broad keyword search only returned
  unrelated Pace/EPOCH PRs; these do not add NEST task coverage. There was no
  mapped NEST PR requiring a content inspection.

## Authoritative verification (2026-09-12)

| BibTeX key | Sources checked | Why retained |
| --- | --- | --- |
| `szydagis2011nest` | Pinned upstream [CITATION.md](https://github.com/NESTCollaboration/nest/blob/307fb27d4c2f9b48dba27529bb57bc243e0ce2dc/CITATION.md); [DOI registry metadata](https://api.crossref.org/works/10.1088/1748-0221/6/10/P10002); [arXiv:1106.1613](https://arxiv.org/abs/1106.1613) | Original NEST paper explicitly requested by upstream; liquid-xenon scintillation/recoil model foundation. |
| `nest2025software` | The upstream [latest-DOI badge](https://zenodo.org/badge/latestdoi/96344242) resolved to [Zenodo record 17851406](https://zenodo.org/records/17851406); [record metadata](https://zenodo.org/api/records/17851406); [upstream release](https://github.com/NESTCollaboration/nest/releases/tag/v2.4.5beta) | Real, version-specific software archive, also required by upstream. Zenodo verifies the title, all 19 creators, version `v2.4.5beta`, and date 2025-12-08. |
| `szydagis2021energy` | [Publisher DOI](https://doi.org/10.3390/instruments5010013); [publisher-deposited metadata](https://api.crossref.org/works/10.3390/instruments5010013); [arXiv:2102.10209](https://arxiv.org/abs/2102.10209) | Review explicitly covering both xenon and argon, scintillation/ionization, energy means and widths, and keV through MeV–GeV reconstruction. |
| `westerdale2024argon` | [Publisher DOI](https://doi.org/10.1088/1748-0221/19/02/C02008); [publisher-deposited metadata](https://api.crossref.org/works/10.1088/1748-0221/19/02/C02008); [arXiv:2312.07712](https://arxiv.org/abs/2312.07712), especially [section 4](https://arxiv.org/html/2312.07712v2#S4) | NEST-specific section reports liquid-argon light/charge-yield model updates. The published year is 2024; 2023 is the preprint year. The collaboration author follows the publisher metadata. |
| `szydagis2013enhancement` | [DOI registry metadata](https://api.crossref.org/works/10.1088/1748-0221/8/10/C10003); [arXiv:1307.6601](https://arxiv.org/abs/1307.6601) | Historical xenon ER/NR light-and-charge-yield and Monte Carlo model refinements; full author names verified on arXiv. |

**Version distinction:** The benchmark pin is a development commit dated
2026-08-27, later than the archived `v2.4.5beta` release. The release DOI is not
claimed to identify that exact commit. No exact-commit DOI was verified. The
pinned source link above records the benchmark identity separately. The upstream
README also contains an older fixed Zenodo link; it is not silently treated as
the current release. The concept DOI `10.5281/zenodo.1314499` identifies the
software series but is not a second bibliography entry.

## Complete shipped check coverage

All check paths below are under
`tasks/nest/noble-element-microphysics/tests/checks/`. The software citation and
pinned source apply to all six; the papers are scientific context, not claims
that they define the benchmark inputs or pass tolerances.

| Check | Evidence and relevant works |
| --- | --- |
| `execnest-detector-response` | Official `execNEST` monoenergetic LXe electron-recoil events: generated photons/electrons, drift, extraction, S1 and S2. `szydagis2011nest`, `szydagis2021energy`, and `szydagis2013enhancement` cover the yield and response context. |
| `barenest-single-event` | Official minimal `NESTcalc` API with the LUX detector, repeated over a seed ensemble. Same foundational and detector-response works as `execNEST`; this is not a separate scientific model or publication. |
| `lar-mean-yields` | Official modern LAr NR, ER and alpha energy/field grids. `szydagis2021energy` supplies cross-element light/charge context; `westerdale2024argon` section 4 documents LAr NEST yield-model progress. |
| `lar-fluctuations` | Official NR, ER and alpha sampled-yield/width surfaces. `szydagis2021energy` discusses means, widths, and fluctuations; the exact modern sampler is identified by the pinned software, not assumed to be fully specified by that review. |
| `lar-neutron-capture` | Upstream `examples/LArNEST/LArNESTNeutronCapture.cpp` evaluates **ER** yields at 38 hard-coded cascade energies over 11 fields. `szydagis2021energy` supplies high-energy light/charge reconstruction context; software is the direct provenance of the driver. No separate publication for that exact energy list was verified. |
| `legacy-larnest-yields` | The official `LegacyGetYields` stochastic electron configuration and eight-field grid. `szydagis2021energy` provides LAr yield/quanta context; the exact legacy implementation is pinned-source evidence, not an asserted match to the newer argon-update paper. |

The shipped module excludes optional Geant4/Garfield++ integrations and
ROOT-only downstream analysis. They do not warrant extra dependency citations
for these checks. Likewise, DEAP-3600 optical transport is not claimed as a
shipped check: only the paper's NEST update section motivates its inclusion.
No scientific reference is presented as justification for the task's numerical
tolerances or acceleration results.

## Bibliography validation

Validation uses the installed classic BibTeX engine with `unsrt.bst` and
`\citation{*}`; all five entries must produce bibliography items without errors
or warnings. Additional checks require nonempty authors, titles, years and DOIs,
unique citation keys/DOIs/arXiv identifiers, no placeholder records, a clean
`git diff --check`, and changed paths confined to `codebase-reports/nest/`.
No task execution or acceleration claim is involved in this documentation-only
change.
