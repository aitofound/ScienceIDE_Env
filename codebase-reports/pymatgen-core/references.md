# Bibliography coverage and verification

[`references.bib`](references.bib) contains four distinct works, ordered with
the upstream software citation first, then the task's phase-diagram foundations
and phase-separation method. Metadata was checked on 2026-09-12. The existing
source report and its unknown values are unchanged.

## Scope inspected

- Upstream: [`materialsproject/pymatgen-core` at
  `73af4e53f5f24e1dcf11e0d94ca13be10ea956ad`](https://github.com/materialsproject/pymatgen-core/tree/73af4e53f5f24e1dcf11e0d94ca13be10ea956ad),
  matching this report's `source-manifest.json` and the shipped task's pin.
- The sole shipped task is
  [`pymatgen-phase-diagram-thermodynamics`](../../tasks/pymatgen-core/pymatgen-phase-diagram-thermodynamics/task.toml).
  Its [module record](../../tasks/pymatgen-core/pymatgen-phase-diagram-thermodynamics/comment/pipeline/module.json)
  and [check overview](../../tasks/pymatgen-core/pymatgen-phase-diagram-thermodynamics/comment/README.md)
  describe 56 checks over `src/pymatgen/analysis/phase_diagram.py`.
- The supplied `work/scienceaccel_inventory.json` snapshot mapped none of its
  22 open PRs to `pymatgen-core` (checked both `codebases` and file paths).
  A live `gh pr list --repo aitofound/ScienceAccelBench --state open --search pymatgen`
  also returned no matches on the verification date, before this bibliography
  PR was opened. Thus there were no mapped pending-review PRs to inspect.

## Sources and relevance

| BibTeX key | Authoritative verification | Coverage |
|---|---|---|
| `Ong2013Pymatgen` | Pinned upstream [`CITATION.cff`](https://github.com/materialsproject/pymatgen-core/blob/73af4e53f5f24e1dcf11e0d94ca13be10ea956ad/CITATION.cff) and [DOI metadata](https://doi.org/10.1016/j.commatsci.2012.10.028) agree on the ten authors, title, journal, year, volume and pages. | Primary software citation for the upstream codebase, also explicitly named in the shipped task's `references` field. |
| `Ong2008PhaseDiagram` | The pinned [`PhaseDiagram` docstring, lines 309–320](https://github.com/materialsproject/pymatgen-core/blob/73af4e53f5f24e1dcf11e0d94ca13be10ea956ad/src/pymatgen/analysis/phase_diagram.py#L309-L320) cites this algorithmic foundation; [Crossref's DOI record](https://api.crossref.org/works/10.1021/cm702327g) verifies the full authors, title, journal, year, volume, issue and pages. | Composition–energy hulls, phase stability, decomposition, formation/hull energies and chemical potentials. Also explicitly cited by `GrandPotentialPhaseDiagram`. |
| `Ong2010ThermalStabilities` | The same `PhaseDiagram` docstring and [`GrandPotentialPhaseDiagram`, lines 1506–1524](https://github.com/materialsproject/pymatgen-core/blob/73af4e53f5f24e1dcf11e0d94ca13be10ea956ad/src/pymatgen/analysis/phase_diagram.py#L1506-L1524) cite this work; [DOI metadata](https://doi.org/10.1016/j.elecom.2010.01.010) verifies full bibliographic fields. | Phase-diagram thermodynamics and open-element/grand-potential calculations. |
| `Bartel2020CompoundStability` | The pinned [`get_decomp_and_phase_separation_energy` docstring, lines 925–942](https://github.com/materialsproject/pymatgen-core/blob/73af4e53f5f24e1dcf11e0d94ca13be10ea956ad/src/pymatgen/analysis/phase_diagram.py#L925-L942) and [`_get_slsqp_decomp`, lines 2605–2627](https://github.com/materialsproject/pymatgen-core/blob/73af4e53f5f24e1dcf11e0d94ca13be10ea956ad/src/pymatgen/analysis/phase_diagram.py#L2605-L2627) explicitly name this work; [Crossref's DOI record](https://api.crossref.org/works/10.1038/s41524-020-00362-y) verifies the six authors, title, journal, year, volume, issue and article number. | Constrained decomposition/phase-separation energies, including the shipped separation, equilibrium-reaction and inner-hull-reduction checks and the patched-hull routines that reuse them. This is method attribution, not a claim that the task trains a machine-learning model. |

The task's entry projection/normalization, compound diagrams, patched-hull
updates and reaction-path utilities are covered as implementations in the pinned
module and the primary software paper, with the hull/decomposition foundations
above. No separate publication for each wrapper or update routine is asserted.
Pourbaix thermodynamics, DFT execution and unrelated core modules are explicitly
outside this task's module scope; their specialized references are not imported
from the separately reported `pymatgen` codebase.

## Metadata and provenance notes

- The upstream CFF was fetched at the exact task pin with `gh api`; it recommends
  the 2013 pymatgen paper for this core repository. Its illustrative software
  version is **not** used as a release identifier for the benchmark snapshot.
- For the two Elsevier works, DOI content negotiation with
  `Accept: application/vnd.citationstyles.csl+json` supplied full metadata after
  direct Crossref requests were rate-limited. The ACS and Nature works were
  verified using the linked Crossref records, in addition to upstream citations.
- `97` in `Bartel2020CompoundStability` is the article number, represented in the
  BibTeX `pages` field for compatibility with traditional bibliography styles.
  Formula subscripts and chemical-symbol capitalization are protected in titles.
- Each DOI appears once. Repeated citations across docstrings and checks have
  been consolidated; there are no placeholder entries or invented releases.
- Fixture attribution and the unknown Materials Project database release remain
  as documented in the existing [README](README.md#scope-and-attribution).
  These papers do not establish a new dataset version or imply fresh API access.
