# Bibliography coverage and verification

[`references.bib`](references.bib) contains six distinct works, with the primary
pymatgen software paper first, followed by the task methods and fixture-project
attribution. This supplements, rather than replaces, the source and licensing
provenance in [README.md](README.md).

## Scope audit

The audit used benchmark revision
`9b9ac0eea13712f2e40d25e28ac4c4b49a07c761` and the existing upstream pymatgen pin
`0428f232a569ffe6b16fa030d38ea35a56d70fd6`.

| Shipped scope | Evidence inspected | References and limits |
| --- | --- | --- |
| Upstream pymatgen and both tasks | Pinned `code/pymatgen/CITATION.cff`, upstream README, existing source report | `Ong2013Pymatgen` is the upstream-requested software citation. The historical version example in the CFF is not treated as the benchmark's version. |
| `tasks/pymatgen/pymatgen-interface-matching/` (8 checks) | `comment/pipeline/module.json`, `comment/README.md`, and every check README; upstream `analysis/interfaces/zsl.py` and `substrate_analyzer.py` | `Zur1984LatticeMatch` covers ZSL matching, which feeds substrate search, strain evaluation and coherent-interface construction; `Ong2013Pymatgen` credits the implementation. The custom `controlled-elastic-energy` check uses an explicitly specified isotropic material and formula, not measured silicon stiffness or a separately identified publication. No experimental-data citation is invented for it. |
| `tasks/pymatgen/pymatgen-pourbaix-thermodynamics/` (13 checks) | `comment/pipeline/module.json`, `comment/README.md`, and every check README; upstream `analysis/pourbaix_diagram.py` | `Persson2012SolidAqueousEquilibria` covers solid/ion reference states and pH/potential thermodynamics; `Singh2017ElectrochemicalStability` covers electrochemical metastability/decomposition energies; `Patel2019EfficientPourbaix` supplies many-element diagram methodology. Together with the software paper these cover entry coefficients, normalization, mixtures, stable phases, filtering, domains and decomposition-energy checks. |
| Offline Pourbaix fixtures | Existing source-report attribution and task check READMEs | `Jain2013MaterialsProject` credits the Materials Project. The fixtures do not identify a database release, and the tasks do not query the API; neither a release-specific dataset nor an API-method citation is asserted. |

`git ls-tree HEAD:tasks/pymatgen` confirmed that these are the only two shipped
task directories. The parent's inventory mapped no open/pending-review PR to
this exact codebase. A live `gh pr list --repo aitofound/ScienceAccelBench
--state open --search pymatgen` also returned no PRs during the audit, before
this bibliography PR was created. There were therefore no mapped review PRs
requiring additional task coverage. The separately assigned `pymatgen-core`
codebase and its solid-state phase-diagram task are not modified here.

## Authoritative verification

Verification performed on 2026-09-12. Crossref records are publisher-deposited DOI
metadata; the links below identify the records used to check author order,
title, journal, year, volume, issue and page/article identifiers where supplied.

- **`Ong2013Pymatgen`** — verified against the pinned upstream
  [CITATION.cff](https://github.com/materialsproject/pymatgen/blob/0428f232a569ffe6b16fa030d38ea35a56d70fd6/CITATION.cff),
  the upstream README's "How to cite pymatgen" section, and the BibTeX returned
  by [DOI content negotiation](https://doi.org/10.1016/j.commatsci.2012.10.028)
  (`Accept: application/x-bibtex`).
- **`Zur1984LatticeMatch`** — the DOI is explicitly attached to `ZSLGenerator`
  in the pinned upstream
  [zsl.py](https://github.com/materialsproject/pymatgen/blob/0428f232a569ffe6b16fa030d38ea35a56d70fd6/src/pymatgen/analysis/interfaces/zsl.py).
  Bibliographic metadata, including pages 378–386, was verified against the
  [DOI record](https://api.crossref.org/works/10.1063/1.333084).
- **`Persson2012SolidAqueousEquilibria`** — explicitly cited in the pinned
  [pourbaix_diagram.py](https://github.com/materialsproject/pymatgen/blob/0428f232a569ffe6b16fa030d38ea35a56d70fd6/src/pymatgen/analysis/pourbaix_diagram.py).
  The [DOI record](https://api.crossref.org/works/10.1103/PhysRevB.85.235438)
  confirms article 235438 and the other bibliographic fields. The entry retains
  the author spelling supplied by that record.
- **`Singh2017ElectrochemicalStability`** — explicitly cited in the same upstream
  Pourbaix module. Verified against the
  [DOI record](https://api.crossref.org/works/10.1021%2Facs.chemmater.7b03980),
  which identifies the eight authors and pages 10159–10167.
- **`Patel2019EfficientPourbaix`** — verified using the
  [DOI record](https://api.crossref.org/works/10.1039/c9cp04799a), including the
  published title and abstract describing efficient many-element Pourbaix
  construction. The upstream module's third citation hook describes "Fast
  computation of many-element Pourbaix diagrams" but repeats the Singh DOI
  `10.1021/acs.chemmater.7b03980`. That DOI is not a second paper and is included
  only once. The separately verified Patel paper is cited under its actual
  published title and DOI `10.1039/C9CP04799A`, not under the mismatched hook.
- **`Jain2013MaterialsProject`** — the pinned upstream
  [reference guidance](https://github.com/materialsproject/pymatgen/blob/0428f232a569ffe6b16fa030d38ea35a56d70fd6/docs/references.md)
  recommends this paper for Materials Project data. Full author names and other
  included fields were verified from the
  [DOI BibTeX export](https://api.crossref.org/works/10.1063/1.4812323/transform/application/x-bibtex).
  That export omits a page/article-number field, so this entry omits it rather
  than reconstructing it from the shortened identifier in upstream guidance.

Direct ACS and RSC publisher pages rejected automated access; their metadata was
verified through the successful DOI registration records instead. No citation
is based solely on a search snippet. Duplicate DOI and title checks ensure that
each exact work occurs once. These citations provide scientific attribution,
not new claims about accelerator performance, task tolerances or validation of
the underlying models.
