# Quantum ESPRESSO bibliography: scope and verification

[`references.bib`](references.bib) contains ten distinct publications, ordered
with the three upstream-recommended software papers first, followed by methods
relevant to the ground-state and phonon/response tasks. It supplements the
existing codebase metadata; it does not regenerate or change that metadata.

## Scope inspected

Inspection date: **2026-09-12**. Repository baseline:
`9b9ac0eea13712f2e40d25e28ac4c4b49a07c761`.
The source report pins Quantum ESPRESSO to
`9f93ddec427d2b9a45bb72d828c6d324f62fcabd` (QE 7.6), upstream
<https://gitlab.com/QEF/q-e>.

There are **no shipped `tasks/quantum-espresso/**` leaves at this baseline**
(checked in the Git tree, not inferred from the sparse working tree).
Both task PRs mapped to this codebase in the bibliography inventory were
inspected using `gh pr view` and the GitHub contents API, including their
`task.toml`, `instruction.md`, module/test-survey records, and check-directory
listings. They were open at inspection time:

| Task path | PR and inspected head | Coverage |
| --- | --- | --- |
| `tasks/quantum-espresso/pw-ground-state` | [#635](https://github.com/aitofound/ScienceAccelBench/pull/635), `f0be0714d9cddc797a8c4a0833d5f0b11bcbab61` | All 20 `pw.x` checks share the three Giannozzi software references and `KohnSham1965SelfConsistent`; `Vanderbilt1990Ultrasoft` and `Blochl1994PAW` add the USPP/PAW method foundations. |
| `tasks/quantum-espresso/ph-linear-response` | [#636](https://github.com/aitofound/ScienceAccelBench/pull/636), `50ee4be31477fe0acb7ebbbbbd768ab5351cc35e` | All 12 SCF-to-response chains share the three Giannozzi references and `Baroni2001DFPT`. Additional upstream-documented methods cover DFPT+U, two-dimensional response, and Raman coefficients. |

The ground-state task's manifest cites the 2009, 2017, and 2020 software
papers individually. The response task combines the same three works in one
reference string with only the 2009 DOI; the bibliography separates those
real publications and deduplicates them across tasks.

### Check inventory and method links

- Ground-state checks: `al-metal-smearing`, `al-metal-tetrahedra`,
  `as-vc-relax`, `ch4-metagga-tpss`, `co-beef-vdw`, `co-relax`,
  `cu-uspp-smearing`, `fe-noncolin`, `feo-lda-u`, `h2o-cluster-isolated`,
  `h2o-uspp-gamma`, `ni-lsda`, `nico-dipole`, `o-atom-occ`, `o-paw-atom`,
  `pt-spinorbit`, `si-electric-string`, `si-md-verlet`, `si-ncpp-gamma`,
  and `si-ncpp-scf`. These are the semi-local PWscf module, not the separate
  hybrid/exact-exchange module. The software papers cover that implementation
  breadth; this is not an exhaustive bibliography of every functional or
  individual pseudopotential dataset used by the decks.
- Response checks: `ph-1d-ch4`, `ph-2d-bn`, `ph-base-c-gamma`,
  `ph-base-si-gamma`, `ph-base-si-x`, `ph-diag-direct`,
  `ph-insulator-paw-magn-o2`, `ph-metal-al-elph`,
  `ph-ni-nc-spinorbit-mag`, `ph-raman-h2o`, `ph-u-insulator-us-bn`,
  and `ph-u-metal-paw-ni`.
  - `Floris2020HubbardDFPT`: the Hubbard-corrected response method relevant
    to the two `ph-u-*` checks; identified by the upstream PHonon guide.
  - `Sohier2017TwoDimensionalDFPT`: the 2-D DFPT extension relevant to
    `ph-2d-bn`; the paper's graphene application is not a claim that the task
    uses graphene.
  - `LazzeriMauri2003Raman`: the second-order-response Raman method used by
    `lraman`, relevant to `ph-raman-h2o`; the paper's silica application is
    not a claim that the task uses silica.
  - `Baroni2001DFPT` supplies the general phonon, dielectric, and
    electron-phonon theory background for the remaining response chains.

These are scientific provenance links, not new claims about which observables
are graded. In particular, #636 reports dropped phonon/lambda groups for some
checks, including under-convergence in the Al electron-phonon deck. A reference
to that method does not reinstate those groups or certify task accuracy.
Other approved modules in the metadata have no shipped or inventory-mapped
pending task in this inspection; none is represented here as an active task.

## Authoritative verification

The pinned upstream documentation was read from the vendored Git objects:

- [`Doc/quote.tex`](https://gitlab.com/QEF/q-e/-/blob/9f93ddec427d2b9a45bb72d828c6d324f62fcabd/Doc/quote.tex)
  requests the 2009 and 2017 papers and additionally the 2020 paper for the
  GPU-enabled version. The latter is relevant to acceleration context, not
  evidence that either task has already achieved GPU acceleration.
- [`PHonon/Doc/user_guide.tex`](https://gitlab.com/QEF/q-e/-/blob/9f93ddec427d2b9a45bb72d828c6d324f62fcabd/PHonon/Doc/user_guide.tex)
  names the 2020 Floris paper under “Phonons from DFPT+U” and the 2017 Sohier
  paper under “Phonons for two-dimensional crystals”.
- [`PHonon/Doc/INPUT_PH.def`](https://gitlab.com/QEF/q-e/-/blob/9f93ddec427d2b9a45bb72d828c6d324f62fcabd/PHonon/Doc/INPUT_PH.def)
  explicitly cites Lazzeri and Mauri, PRL 90, 036401 (2003), for `lraman`.

Titles, author lists, publication years, journals, volumes, issues, and page
ranges/article identifiers were checked against the following DOI records
(Crossref publisher-deposited metadata, via `/works/<DOI>` or DOI content
negotiation with `Accept: application/x-bibtex`), supplemented by the pinned
upstream citations above:

| BibTeX key | Verified DOI |
| --- | --- |
| `Giannozzi2009QuantumEspresso` | [10.1088/0953-8984/21/39/395502](https://doi.org/10.1088/0953-8984/21/39/395502) |
| `Giannozzi2017AdvancedCapabilities` | [10.1088/1361-648X/aa8f79](https://doi.org/10.1088/1361-648X/aa8f79) |
| `Giannozzi2020Exascale` | [10.1063/5.0005082](https://doi.org/10.1063/5.0005082) |
| `Baroni2001DFPT` | [10.1103/RevModPhys.73.515](https://doi.org/10.1103/RevModPhys.73.515) |
| `KohnSham1965SelfConsistent` | [10.1103/PhysRev.140.A1133](https://doi.org/10.1103/PhysRev.140.A1133) |
| `Vanderbilt1990Ultrasoft` | [10.1103/PhysRevB.41.7892](https://doi.org/10.1103/PhysRevB.41.7892) |
| `Blochl1994PAW` | [10.1103/PhysRevB.50.17953](https://doi.org/10.1103/PhysRevB.50.17953) |
| `Floris2020HubbardDFPT` | [10.1103/PhysRevB.101.064305](https://doi.org/10.1103/PhysRevB.101.064305) |
| `Sohier2017TwoDimensionalDFPT` | [10.1103/PhysRevB.96.075448](https://doi.org/10.1103/PhysRevB.96.075448) |
| `LazzeriMauri2003Raman` | [10.1103/PhysRevLett.90.036401](https://doi.org/10.1103/PhysRevLett.90.036401) |

Publisher small-cap/MathML markup was converted to plain titles with BibTeX
case protection and a TeX chemical formula; accented names use TeX escapes.
The 2020 exascale, Floris, and Sohier article numbers missing from the returned
BibTeX exports were supplied from the explicit upstream volume/article
citations. Author ordering follows publisher-deposited metadata; the 2017
DiStasio suffix is retained from upstream `quote.tex`. Article numbers are
stored in `pages` for conventional BibTeX compatibility. Exact works are
identified by case-insensitive DOI, not repeated for each task or preprint.
