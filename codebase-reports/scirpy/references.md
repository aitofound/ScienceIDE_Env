# Scirpy bibliography: coverage and verification

[`references.bib`](references.bib) contains eight distinct works, ordered with
the primary Scirpy paper first, followed by the acceleration kernel, its
substitution matrices, other shipped distance metrics, and the upstream-requested
scverse ecosystem citation. This is a bibliography-only supplement; the existing
canonical metadata report and its recorded unknowns are unchanged.

## Scope inspected

- Upstream: [scverse/scirpy](https://github.com/scverse/scirpy), at the shipped
  task's exact pin `79a83440e71a758b531a44b93565c0f2d379263b`.
- Shipped task: [`tasks/scirpy/sequence-distance-metrics`](../../tasks/scirpy/sequence-distance-metrics),
  including `task.toml`, `instruction.md`, `comment/README.md`, the module record,
  and all 16 check READMEs. The task explicitly cites the Scirpy and TCRdist papers
  and links the v0.25.1 software release; the exact commit, rather than an inferred
  release date, identifies the source considered here.
- The supplied open/pending-review inventory maps **no PRs** to Scirpy.
  A live `gh pr list --repo aitofound/ScienceAccelBench --state open --search scirpy`
  check on 2026-09-12 also returned no matches, before this bibliography PR was
  created. There were therefore no mapped pending PRs requiring extra citations.
- `clonotype-network` appears in the existing report as **proposed-only**. It is
  not a shipped task and is not represented as an approved benchmark here.

## Coverage of every shipped check

The Scirpy paper (`Sturm2020Scirpy`) provides software context for the entire task.
The following references add method-specific coverage; ordinary equality,
cutoff, dispatch, and matrix-shape checks do not imply separate research works.

| Check(s) | Additional bibliography keys and role |
| --- | --- |
| `tcrdist-reference`, `tcrdist-dense`, `tcrdist-parameter-matrix`, `tcrdist-scaled` | `Dash2017TCRdist`: TCRdist distance definition, including the acceleration workload; `Henikoff1992BLOSUM`: underlying BLOSUM62 substitution scores. |
| `tcrdist-blosum`, `tcrdist-distance-cap` | `Dash2017TCRdist`, `Postovskaya2024TCRBLOSUM`, `Henikoff1992BLOSUM`: TCRdist with TCRBLOSUM alpha/beta or BLOSUM62 matrices and per-position caps. |
| `hamming-reference`, `hamming-full-cutoff`, `hamming-long-sequence`, `hamming-normalized`, `hamming-scaled` | `Hamming1950Codes`: mathematical background for mismatch-count distance; normalization, cutoffs, and GPU implementation details are defined by Scirpy, not attributed to the 1950 article. |
| `metrics-dispatch-sweep`, `rectangular-two-sets` | `Dash2017TCRdist`, `Hamming1950Codes`: metric families reached by public dispatch, including two unequal-sized input sets. |
| `alignment-metrics` | `Daily2016Parasail`, `Henikoff1992BLOSUM`, `Dash2017TCRdist`: external alignment implementation, substitution scores, and Scirpy's documented inspiration for alignment-derived distances. |
| `levenshtein-metric` | `Levenshtein1965Codes`: edit-distance background. The actual implementation delegates to python-Levenshtein; the original mathematical article is not presented as a software-package citation. |
| `identity-metric` | `Sturm2020Scirpy`: Scirpy's exact-equality calculator and asymmetric public entry point; no separate identity-distance paper is asserted. |

## Authoritative verification

Verification performed on 2026-09-12. The pinned upstream sources inspected were:

- [README citation section](https://github.com/scverse/scirpy/blob/79a83440e71a758b531a44b93565c0f2d379263b/README.md#citation):
  explicitly requests the Scirpy paper and recommends the scverse paper.
- [Upstream bibliography](https://github.com/scverse/scirpy/blob/79a83440e71a758b531a44b93565c0f2d379263b/docs/references.bib):
  entries `TCRdist`, `TCRBLOSUM`, `Daily2016`, and `Virshup_2023`.
- [Pinned distance calculators](https://github.com/scverse/scirpy/blob/79a83440e71a758b531a44b93565c0f2d379263b/src/scirpy/ir_dist/metrics.py):
  `TCRdistDistanceCalculator` explicitly cites TCRdist and TCRBLOSUM;
  `AlignmentDistanceCalculator` and `FastAlignmentDistanceCalculator` explicitly
  cite Parasail and TCRdist. The Hamming and Levenshtein class documentation
  establishes the other metric definitions and their implementation provenance.

The following DOI-registration records were fetched successfully and checked for
authors, title, journal, publication year, volume/issue, pages or article number,
and DOI. These are publisher-deposited metadata, not search-result snippets.

| BibTeX key | Verification record |
| --- | --- |
| `Sturm2020Scirpy` | [Crossref: 10.1093/bioinformatics/btaa611](https://api.crossref.org/works/10.1093/bioinformatics/btaa611), also the pinned upstream README. |
| `Dash2017TCRdist` | [Crossref: 10.1038/nature22383](https://api.crossref.org/works/10.1038/nature22383), also upstream `TCRdist`. |
| `Postovskaya2024TCRBLOSUM` | [Crossref: 10.1093/bib/bbae602](https://api.crossref.org/works/10.1093/bib/bbae602), also upstream `TCRBLOSUM`. |
| `Henikoff1992BLOSUM` | [Crossref: 10.1073/pnas.89.22.10915](https://api.crossref.org/works/10.1073/pnas.89.22.10915). |
| `Daily2016Parasail` | [Crossref: 10.1186/s12859-016-0930-z](https://api.crossref.org/works/10.1186/s12859-016-0930-z), also upstream `Daily2016`. |
| `Hamming1950Codes` | [Crossref: 10.1002/j.1538-7305.1950.tb00463.x](https://api.crossref.org/works/10.1002/j.1538-7305.1950.tb00463.x). |
| `Levenshtein1965Codes` | [Math-Net.Ru journal archive, dan31411](https://www.mathnet.ru/eng/dan31411): author, English rendering of the title, Russian-language original, journal, 1965, volume 163, issue 4, pages 845–848. |
| `Virshup2023Scverse` | [Crossref: 10.1038/s41587-023-01733-8](https://api.crossref.org/works/10.1038/s41587-023-01733-8), with the collective-author form explicitly supplied by the pinned upstream README. |

### Metadata decisions and limits

- The TCRBLOSUM year is **2024**, matching both the pinned upstream bibliography
  and the DOI record's 2024-11-22 publication date. The volume, issue, and article
  identifier are retained as registered: 26(1), bbae602.
- Parasail's `81` and TCRBLOSUM's `bbae602` are article identifiers, recorded in
  the conventional BibTeX `pages` field rather than invented page ranges.
- Hamming is dated **1950**, its original print year, not the 2013 online
  digitization date also present in the DOI record.
- Levenshtein cites the verified **1965 Russian original**, using the archive's
  English title. Its 1966 English translation is the same underlying work and
  is not added as a duplicate. No DOI is asserted because the inspected journal
  record does not supply one.
- The scverse author list follows the upstream README's collective-author form
  (`Scverse Community`) instead of expanding consortium members from Crossref
  and counting them again. No malformed trailing `and` or encoding artifacts
  from the upstream BibTeX export were copied.
- The eight entries contain seven distinct DOIs and one distinct journal-archive
  URL. The software release is not counted as an additional publication. This
  bibliography is targeted to the codebase and shipped task, not an exhaustive
  list of every dependency, tutorial dataset, or proposed analysis module.
