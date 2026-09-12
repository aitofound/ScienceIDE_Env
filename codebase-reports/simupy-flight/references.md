# simupy-flight bibliography: scope and verification

`references.bib` contains seven distinct publications, ordered with the toolkit
paper and the scientific check-case/model sources before supporting methods.
The existing metadata report was present and is unchanged; this is not a report
backfill or a new simulation-validation claim.

## Scope reviewed

- Upstream: [`nasa/simupy-flight`](https://github.com/nasa/simupy-flight), pinned
  at `70754e6916afc206e8c0abb386d1a9c98bf8f561` by the shipped task.
- Shipped task: `tasks/simupy-flight/nesc-6dof-flight-dynamics/`.
  Its `task.toml`, `comment/README.md`, model/provenance records, and check
  documentation identify all 13 checks: `nesc-case-01` through `nesc-case-11`,
  `f16-aero`, and `f16-prop`. The NESC volumes cover the scenario definitions,
  models, and comparison data; the toolkit paper covers their implementation.
  The F-16 model and interpolation/model-exchange sources provide more specific
  provenance for Case 11 and both standalone generated-model checks.
- Pending work: the fanout inventory (`work/scienceaccel_inventory.json`, an
  orchestration artifact outside this repository) has **no open PR mapped to
  simupy-flight**. A `gh pr list` query for open simupy PRs in
  `aitofound/ScienceAccelBench` also returned none at review time. Thus there
  were no additional mapped PR tasks to inspect. Reviewed on 2026-09-12 against
  benchmark base `9b9ac0eea13712f2e40d25e28ac4c4b49a07c761`.

## Bibliographic evidence

| BibTeX key | Verified source and relevance |
|---|---|
| `MargolisLyons2022SimuPyFlight` | The pinned [upstream README](https://github.com/nasa/simupy-flight/blob/70754e6916afc206e8c0abb386d1a9c98bf8f561/README.rst) links DOI `10.21105/joss.04299`. [Crossref DOI metadata](https://api.crossref.org/works/10.21105/joss.04299) confirms the authors, title, 2022 publication year, journal, volume 7, issue 75, and article 4299. The draft's 2020 front-matter date is not used as the publication year. |
| `MurriJacksonShelton2015CheckCases` | [NASA NTRS record and API](https://ntrs.nasa.gov/api/citations/20150001263) identify Volume I, January 2015, the report number, and DOI `10.64631/IGUD7952`. The linked official PDF title page supplies the full author names (the current API abbreviates Murri's name). |
| `MurriJacksonShelton2015CheckCasesAppendices` | The [original NASA 2015 assessment site](https://nescacademy.nasa.gov/flightsim/2015) explicitly distinguishes the two volumes. The [Volume II PDF](https://nescacademy.nasa.gov/src/flightsim/Reports/NASA-TM-2015-218675-EOM_checkcase_appendices.pdf) title page verifies the appendix title, all three authors, January 2015, and `NASA/TM-2015-218675/Volume II`. It is not a duplicate of Volume I. No unverified DOI is supplied for this volume. |
| `GarzaMorelli2003NonlinearAircraft` | [NASA NTRS metadata](https://ntrs.nasa.gov/api/citations/20030013626) verifies the title, authors, January 2003, and `NASA/TM-2003-212145`. The pinned `NESC_data/All_models/F16_package/F16_S119_source/F16_aero.dml` cites this report as reference `REF02`; NASA's spelling **Frederico** is used rather than the DAVE-ML header's typo. |
| `Margolis2017SimuPy` | The pinned [upstream paper bibliography](https://github.com/nasa/simupy-flight/blob/70754e6916afc206e8c0abb386d1a9c98bf8f561/paper/references.bib) cites the dynamical-system framework. [Crossref](https://api.crossref.org/works/10.21105/joss.00396) confirms the title, year, volume, issue, and article number. The author's family name is normalized to Margolis using upstream authorship, rather than copying Crossref's misplaced initials into the surname. |
| `MargolisLyons2019NDSplines` | The same upstream bibliography supplies the authors' full initials. [Crossref](https://api.crossref.org/works/10.21105/joss.01745) confirms the publication fields. The generated `nesc_test_cases/F16_aero.py` and `F16_prop.py` import `ndsplines` for their tables. |
| `JacksonHildreth2002FlightModelExchange` | The upstream paper cites this DAVE-ML background publication. [Crossref](https://api.crossref.org/works/10.2514/6.2002-4482) verifies the authors, title, conference, publisher, year, and DOI. `2002-4482` is a paper identifier, not a page range. |

The pinned `nesc_test_cases/process_NESC_DaveML.py` explicitly generates both
`F16_aero.py` and `F16_prop.py` from the shipped DAVE-ML sources. The propulsion
header separately attributes its model to Stevens and Lewis, *Aircraft Control
and Simulation*, second edition (2003, ISBN 0-471-37145-9); this source lineage is
not conflated with the Garza--Morelli aerodynamic-model reference. This is a
selected bibliography, not a claim to enumerate every transitive dependency or
every reference in the NESC reports.

## Preserved limitations

The shipped task discloses an unresolved Case 11 trim/model/reference discrepancy
against NASA SIM 05. Its coarse external sanity bounds and tighter
candidate-equivalence checks are different claims. These citations do not resolve
that discrepancy, certify NASA accuracy, or establish accelerator performance.
The six closed-loop scenarios excluded by the task remain outside its benchmark
scope. Existing metadata unknowns and warnings have not been overwritten.
