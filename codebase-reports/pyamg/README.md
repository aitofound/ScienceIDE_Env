# PyAMG report and bibliography

[references.bib](references.bib) contains **15 distinct works**, ordered with the
upstream software citation first, then shared foundations and specialized task
methods. The report was absent and has been backfilled as
[JSON](codebase-metadata.json), [Markdown](codebase-metadata.md), and
[self-contained HTML](codebase-metadata.html), following the neighboring
`codebase-reports/edkit/` informational/non-blocking conventions. This is an
explicitly evidence-based backfill, not a new source-size audit or benchmark run.

## Scope and task coverage

Reviewed against the shipped `tasks/pyamg/*/task.toml`, each task's
`comment/pipeline/module.json`, and its checked-in `tests/checks/*/check.json`.
All four tasks cite the same JOSS paper and pin
[`pyamg/pyamg@0c021343e7dce3274f0d587fd60be2bd0ae53495`](https://github.com/pyamg/pyamg/tree/0c021343e7dce3274f0d587fd60be2bd0ae53495).
The primary citation `pyamg2023` covers the upstream codebase and every task;
the following references supply method-specific context.

| Shipped task | Checked-in check directories | Scientific coverage and BibTeX keys |
|---|---:|---|
| [`aggregation-amg`](../../tasks/pyamg/aggregation-amg/task.toml) | 23 | Smoothed aggregation and candidate fitting: `vanek1996smoothed`; adaptive candidates: `brezina2005adaptive`; root-node hierarchy: `manteuffel2017rootnode`; pairwise aggregation: `notay2010aggregation`; energy-minimizing prolongation: `olson2011energy`. Includes the gallery demo and the upstream paper's smoothed-aggregation example. |
| [`classical-amg`](../../tasks/pyamg/classical-amg/task.toml) | 17 | Ruge–Stuben splitting/interpolation and README example: `ruge1987amg`; AIR: `manteuffel2018lair`, `manteuffel2019reduction`; compatible relaxation: `brannick2010compatible`; binormalization: `livne2004binormalization`; the published Figure 4.1 MIS oracle: `desterck2006complexity`; CLJP/CLJPc: `alber2007coarsegrid`. |
| [`krylov-solvers`](../../tasks/pyamg/krylov-solvers/task.toml) | 18 | CG/CGNE/CGNR, CR, GMRES/FGMRES, BiCGStab, simple iterations, stopping criteria, preconditioner interfaces, and shipped sparse problems: `saad2003iterative`, directly cited by upstream solver docstrings. |
| [`relaxation-smoothing`](../../tasks/pyamg/relaxation-smoothing/task.toml) | 16 | Point/block/indexed/normal-equation/Schwarz relaxations, Chebyshev smoothing, and smoother composition/rebinding: `saad2003iterative` and `briggs2000multigrid` as shared iterative-method and multigrid foundations, together with `pyamg2023` for the implemented interfaces. |

These **74 check directories** are a static task inventory, not 74 independent
papers, official test definitions, collected pytest items, or newly passing
checks. Closely related kernel variants share the relevant method reference;
this is not a claim to catalog every historical algorithm mentioned upstream.

### Open/pending-review PR review

On 2026-09-12, the supplied `scienceaccel_inventory.json` contained **no**
`open_prs` record whose `codebases` array included `pyamg`. The live command
`gh pr list --repo aitofound/ScienceAccelBench --state open --search pyamg`
returned [PR #647](https://github.com/aitofound/ScienceAccelBench/pull/647).
`gh pr view 647 --repo aitofound/ScienceAccelBench --json number,title,url,state,files`
confirmed that this was an unrelated MFEM task PR and changed no
`tasks/pyamg/` or `codebase-reports/pyamg/` paths. There is therefore no additional
mapped pending-review PyAMG task to cite in this snapshot. This review is a
point-in-time observation, not a promise about future PRs.

## Validation

- BibTeX 0.99d (TeX Live 2024), using `plain.bst` and `\\citation{*}`, parsed all
  15 entries and emitted 15 bibliography items with no warnings or errors.
- All keys and the 14 DOI values are unique; every entry has nonempty author,
  title, year and URL fields. No placeholder markers or example-domain URLs
  occur in the bibliography.
- The report's four module mappings and 74 check-directory count match the
  shipped tree; all cited keys exist, JSON and embedded HTML data agree, and
  local Markdown links resolve.
- `git diff --check` and `git diff --cached --check` pass. The five changed
  files are confined to `codebase-reports/pyamg/`.
- Numerical task tests are intentionally not rerun for this documentation-only
  change; no benchmark or accelerator validation is claimed.

## Citation verification

Verified on 2026-09-12. All upstream paths below are relative to the pinned
[upstream tree](https://github.com/pyamg/pyamg/tree/0c021343e7dce3274f0d587fd60be2bd0ae53495).
The upstream `CITATION.bib`, `CITATION.cff`, and `docs/paper/paper.bib` were read
with `gh api` at that exact commit. Method docstrings and the MIS oracle were
inspected in the corresponding vendored `code/pyamg/` snapshot, without edits.

For DOI-bearing method works, the linked DOI's publisher-deposited Crossref
record was checked for title, author order, year, venue, volume/issue and pages
where applicable. For example, the chapter-specific registration is
[`api.crossref.org/works/10.1137/1.9781611971057.ch4`](https://api.crossref.org/works/10.1137/1.9781611971057.ch4).
A DOI identifies a work rather than asserting that any particular benchmark
result follows from it.

| BibTeX key | Authoritative verification and upstream relevance |
|---|---|
| `pyamg2023` | [JOSS publisher page](https://joss.theoj.org/papers/10.21105/joss.05495), including citation meta tags and embedded BibTeX, agrees with pinned [`CITATION.cff`](https://github.com/pyamg/pyamg/blob/0c021343e7dce3274f0d587fd60be2bd0ae53495/CITATION.cff) and `CITATION.bib`: Bell, Olson, Schroder, Southworth; 2023; 8(87), 5495. |
| `ruge1987amg` | [DOI](https://doi.org/10.1137/1.9781611971057.ch4) verifies chapter pages 73–130 and 1987; `docs/paper/paper.bib` provides editor/series/volume and `pyamg/classical/split.py` cites the work. |
| `vanek1996smoothed` | [DOI](https://doi.org/10.1007/BF02238511); `docs/paper/paper.bib` and `pyamg/aggregation/aggregation.py`. |
| `saad2003iterative` | [DOI](https://doi.org/10.1137/1.9780898718003), plus the [author's book page](https://www-users.cse.umn.edu/~saad/books.html) for the second edition; cited by `pyamg/krylov/_cg.py`, `_cr.py`, `_gmres_mgs.py`, `_fgmres.py`, `_bicgstab.py`, and `pyamg/relaxation/relaxation.py`. |
| `briggs2000multigrid` | [DOI](https://doi.org/10.1137/1.9780898719505) verifies the second edition and three authors; `docs/paper/paper.bib` supplies its upstream connection as multigrid background. |
| `manteuffel2017rootnode` | [DOI](https://doi.org/10.1137/16M1082706); `docs/paper/paper.bib`, with the shipped task owning `rootnode_solver`. |
| `brezina2005adaptive` | [DOI](https://doi.org/10.1137/050626272); `docs/paper/paper.bib` and `pyamg/aggregation/adaptive.py`. |
| `manteuffel2018lair` | [DOI](https://doi.org/10.1137/17M1144350); `docs/paper/paper.bib` and `pyamg/classical/air.py`. |
| `manteuffel2019reduction` | [DOI](https://doi.org/10.1137/18M1193761); `docs/paper/paper.bib` and `pyamg/classical/air.py`. |
| `notay2010aggregation` | [ETNA volume 37 index](https://etna.ricam.oeaw.ac.at/volumes/2001-2010/vol37/) verifies Yvan Notay, title, 2010 and pages 123–146, and links the [publisher PDF](https://etna.ricam.oeaw.ac.at/vol.37.2010/pp123-146.dir/pp123-146.pdf); cited by `pyamg/aggregation/pairwise.py`. |
| `olson2011energy` | [DOI](https://doi.org/10.1137/100803031); `pyamg/aggregation/smooth.py` and `pyamg/aggregation/rootnode.py`. |
| `brannick2010compatible` | [DOI](https://doi.org/10.1137/090772216); `pyamg/classical/cr.py`. |
| `livne2004binormalization` | [DOI](https://doi.org/10.1023/B:NUMA.0000016606.32820.69); `pyamg/classical/cr.py`. |
| `desterck2006complexity` | [DOI](https://doi.org/10.1137/040615729); `pyamg/classical/split.py` and the Figure 4.1 comment in `pyamg/classical/tests/test_split.py::TestMIS::test_paper_result`, preserved in the shipped `mis-paper-splitting` check. |
| `alber2007coarsegrid` | [DOI](https://doi.org/10.1002/nla.541); `pyamg/classical/split.py` explicitly cites it for `CLJP` and `CLJPc`. |

### Normalization and limitations

- The preferred PyAMG paper is included once, rather than duplicating the four
  identical task citations or adding the same work as a software entry.
- Pinned `CITATION.bib` omits a comma after its `journal` field. The new entry
  fixes the syntax and agrees with the publisher; the upstream file is untouched.
- The Ruge–Stuben entry uses the verified **chapter DOI**, not the whole-book
  DOI in the upstream bibliography. The chapter heading number is not part of
  the stored title.
- The Notay entry omits the unsupported issue `6` found upstream; ETNA's volume
  index does not list an issue. No DOI was established from the inspected
  upstream/publisher records, so the authoritative article URL is used instead.
- BibTeX accents, protected acronyms and double-hyphen page ranges preserve
  portable formatting. Initials are retained where that is what the verified
  metadata supplies; they are not guessed expansions.
- Initial Crossref requests were partly rate-limited and retried at lower rate.
  JOSS's `.bib` endpoint returned HTTP 406, so its working publisher landing
  page and pinned CFF were used. The old ETNA HTML link was stale; the working
  official volume index and article PDF are used above.
- Source size/fingerprint, fresh official-test collection, execution results,
  accelerator performance and scientific accuracy have not been remeasured.
  The report preserves those unknowns instead of inferring success from the
  presence of shipped task artifacts.
