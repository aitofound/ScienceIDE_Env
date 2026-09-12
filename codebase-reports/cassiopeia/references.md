# Cassiopeia bibliography: scope and verification

[`references.bib`](references.bib) lists the upstream-recommended paper first,
the exact software snapshot second, and a directly attributed reconstruction
method third. The paper and software are distinct works; the software entry
records the implementation pin rather than duplicating the paper. No preprint
of the published paper is included.

## Coverage

Reviewed on 2026-09-12 against ScienceAccelBench `main` commit
`9b9ac0eea13712f2e40d25e28ac4c4b49a07c761` and upstream
[`YosefLab/Cassiopeia` at `1ee5959eb9d3f8d4d26e2af5678234493bf54d6d`](https://github.com/YosefLab/Cassiopeia/tree/1ee5959eb9d3f8d4d26e2af5678234493bf54d6d).

| Evidence / scope | Bibliography coverage |
| --- | --- |
| Existing `codebase-metadata.json`: the single `cassiopeia-lineage-reconstruction` module covers preprocessing, the tree data model, reconstruction, and Bayesian branch-length estimation | `jones2020cassiopeia` is the package's requested scientific citation; `yoseflab2026cassiopeia` identifies the complete pinned implementation, including later components not individually attributed to that paper. |
| All shipped `tasks/cassiopeia/**` | None exist at the reviewed main commit: `git ls-tree -r --name-only HEAD tasks/cassiopeia` returns no paths, consistent with the task inventory. Proposed module/check families in the metadata report are not represented as shipped tasks. |
| [Open PR #630: preserve terminal leaf in vendored source](https://github.com/aitofound/ScienceAccelBench/pull/630), the only open/pending PR mapped to Cassiopeia in the supplied inventory | Inspected with `gh pr view` (body, files, comments, reviews) and `gh api repos/aitofound/ScienceAccelBench/pulls/630/files` (patches). The change preserves a source node's sole terminal child in `CassiopeiaTree.collapse_unifurcations`, adds a regression, and documents a local vendor divergence. It adds no task or separately published method. The existing package paper and upstream snapshot provide context, **not** evidence that the pending local fix is already upstream. The PR was open with no submitted review at inspection time. |
| `cassiopeia/solver/NeighborJoiningSolver.py` | Explicitly attributes the algorithm to Saitou and Nei (1987), covered by `saitou1987neighborjoining`. |

The existing generated report is preserved unchanged. This bibliography is not
an exhaustive list of every dependency or algorithm in the repository. In
particular, the inspected `IIDExponentialBayesian.py` and its C++ implementation
do not identify a separate publication for the Bayesian dynamic program; no
such publication or release version is inferred. The snapshot citation covers
that source without claiming that the 2020 paper introduced it.

## Authoritative verification

- **`jones2020cassiopeia`** — the pinned upstream
  [README, Reference section](https://github.com/YosefLab/Cassiopeia/blob/1ee5959eb9d3f8d4d26e2af5678234493bf54d6d/README.md#reference)
  and [documentation reference list](https://github.com/YosefLab/Cassiopeia/blob/1ee5959eb9d3f8d4d26e2af5678234493bf54d6d/docs/references.rst)
  identify this as the requested package citation. The publisher-deposited
  [Crossref DOI record](https://api.crossref.org/works/10.1186/s13059-020-02000-8)
  verifies the nine authors and their order, title, *Genome Biology* 21(1),
  article 92, and publication date 2020-04-14. `pages = {92}` is the article
  number, also made explicit in the entry's note. DOI:
  [10.1186/s13059-020-02000-8](https://doi.org/10.1186/s13059-020-02000-8).
- **`yoseflab2026cassiopeia`** — the upstream README supplies the software title
  and repository identity. The upstream
  [commit record](https://api.github.com/repos/YosefLab/Cassiopeia/commits/1ee5959eb9d3f8d4d26e2af5678234493bf54d6d)
  verifies the full SHA and date 2026-02-05. `YosefLab` is the repository-owning
  organization, used as the corporate software author; the year denotes this
  snapshot, not the initial software release. The report records the same pin.
- **`saitou1987neighborjoining`** — the pinned upstream
  [solver docstring](https://github.com/YosefLab/Cassiopeia/blob/1ee5959eb9d3f8d4d26e2af5678234493bf54d6d/cassiopeia/solver/NeighborJoiningSolver.py)
  names Saitou and Nei (1987). The publisher-deposited
  [Crossref DOI record](https://api.crossref.org/works/10.1093/oxfordjournals.molbev.a040454)
  verifies the title, journal, year, and DOI but omits authors and pagination.
  [Europe PMC's record for PMID 3447015](https://europepmc.org/article/MED/3447015)
  supplies the verified author initials, volume 4, issue 4, and pages 406–425;
  initials are retained rather than expanding names without that evidence.
  DOI: [10.1093/oxfordjournals.molbev.a040454](https://doi.org/10.1093/oxfordjournals.molbev.a040454).
