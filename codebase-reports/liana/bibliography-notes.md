# LIANA bibliography: coverage and verification

`references.bib` lists the primary LIANA+ publication first, the original LIANA
methods/resource comparison second, and the MISTy method publication third.
These are three distinct published works, not duplicate preprint/publication
versions. Verification date: **2026-09-12**.

## Repository and task coverage

- Upstream: [scverse/liana](https://github.com/scverse/liana), pinned in the
  existing report to `f45f7efeb89fdb652dd13f6b303514348dadbc8b` (`v1.10.0`).
  The upstream README blob retrieved through the GitHub API matches the vendored
  `code/liana/README.md` blob: `5e1bcdf31358d561672ce8cd02a7257c9344cc6a`.
- The existing report describes one whole-codebase module,
  `liana-cell-communication`; it is preserved unchanged. The two LIANA papers
  are the upstream's recommended codebase citations. MISTy has an explicit
  additional citation request in its shipped upstream tutorial.
- At the inspected ScienceAccelBench base, `9b9ac0eea`, the complete Git tree
  contains **no `tasks/liana/**` files**. This was checked against the Git tree,
  not inferred from the sparse working directory. The supplied inventory also
  lists no shipped liana task codebase and maps **no open PRs** to liana.
- A live `gh pr list --state open --search liana` also returned no results.
  The report's source PR [#615](https://github.com/aitofound/ScienceAccelBench/pull/615)
  was inspected with `gh pr view`: it is merged and establishes the upstream
  source pin and revised whole-codebase module. It is not a pending task PR.
  These PR observations precede this bibliography PR.

## Citation evidence

| BibTeX key | Upstream relevance evidence | Authoritative publication metadata |
|---|---|---|
| `Dimitrov2024LIANAPlus` | [Pinned README, “Cite LIANA+”](https://github.com/scverse/liana/blob/f45f7efeb89fdb652dd13f6b303514348dadbc8b/README.md#cite-liana) | [Nature Cell Biology](https://www.nature.com/articles/s41556-024-01469-w): 2024, 26(9), 1613–1622; DOI `10.1038/s41556-024-01469-w` |
| `Dimitrov2022LIANA` | Same pinned README; original LIANA methods/resource and consensus framework | [Nature Communications](https://www.nature.com/articles/s41467-022-30755-0): 2022, 13(1), article 3224; DOI `10.1038/s41467-022-30755-0` |
| `Tanevski2022MISTy` | [Pinned MISTy tutorial](https://github.com/scverse/liana/blob/f45f7efeb89fdb652dd13f6b303514348dadbc8b/docs/notebooks/misty.ipynb) explicitly asks users of MISTy via LIANA+ to cite this work; implementation under `src/liana/method/sp/_misty/` | [Genome Biology](https://link.springer.com/article/10.1186/s13059-022-02663-5): 2022, 23(1), article 97; DOI `10.1186/s13059-022-02663-5` |

All three publisher pages returned HTTP 200. Titles, full author lists, journal,
year, volume, issue, page range or article number, and DOI were checked against
the publisher's embedded citation metadata. Article numbers are represented in
BibTeX's `pages` field. Author spellings and name segmentation follow each
publication's metadata; for example, the MISTy publisher records its second
author as `Flores, Ricardo Omar Ramirez`.

## Limits

This is a focused codebase bibliography, not an exhaustive bibliography of every
bundled dependency, scoring method, or tutorial dataset. No separate publication
for the newer LRIC implementation was established from the inspected pinned
source/tutorial evidence; its existence and bibliographic metadata remain
unknown. The 2024 LIANA+ paper is a framework citation, not a claim that it
introduced the later LRIC API. Task-specific references should be added if and
when liana tasks are shipped or proposed in mapped PRs.
