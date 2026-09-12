# PyNNDescent bibliography: scope and verification

[`references.bib`](references.bib) contains four distinct works, ordered by
relevance: the upstream-cited NN-descent method, the pinned software itself,
random-projection-tree background, and the Numba implementation substrate.
This is bibliographic documentation, not a change to module approval or task
validation. The existing generated metadata reports are left unchanged.

## Coverage

Checked on 2026-09-12 against ScienceAccelBench base commit
`9b9ac0eea13712f2e40d25e28ac4c4b49a07c761`:

| Evidence surface | Finding and bibliography coverage |
| --- | --- |
| Existing `codebase-metadata.json` | Upstream `lmcinnes/pynndescent`, pin `969b03a753afa409b879287143b9bba62d096a53`, approved whole-codebase module `pynndescent-ann-engine`. The software citation covers the complete pinned source; the other entries distinguish its core algorithm, tree background, and implementation context. |
| Shipped `tasks/pynndescent/**` | No files in the base Git tree, independently of the sparse checkout. There are no shipped task leaves requiring additional task-specific references. |
| Supplied open/pending-review PR inventory | No PR maps to `pynndescent`. A live `gh pr list --repo aitofound/ScienceAccelBench --state open --search pynndescent` also returned no matches before this bibliography PR was created. |
| [Source PR #613](https://github.com/aitofound/ScienceAccelBench/pull/613) | Inspected its body, changed paths, and state with `gh pr view`. It is merged, vendors release 0.6.0, and explicitly contains no task leaf. Its revised single-module boundary includes distance kernels, NN-descent graph construction/prepared search, and RP/hub forests; the earlier three-way module split was withdrawn. |

The source PR and existing report also record ungraded optimal-transport and
graph-utility paths. This bibliography does not invent downstream checks, claim
coverage of those ungraded paths by a separate paper, or treat the old module
names as shipped tasks. The pinned software entry remains the primary source
record for implementation details not tied to a verified paper here.

## Entry verification

### `dong2011nndescent` — core method

The pinned [upstream README](https://github.com/lmcinnes/pynndescent/blob/969b03a753afa409b879287143b9bba62d096a53/README.rst)
and `doc/index.rst` explicitly cite *Efficient K-Nearest Neighbor Graph
Construction for Generic Similarity Measures* as the NN-descent algorithm
implemented by PyNNDescent.

[Crossref's DOI record](https://api.crossref.org/works/10.1145/1963405.1963487)
verifies the title, WWW 2011 proceedings, pages 577–586, publisher, and DOI.
The [original paper hosted by Princeton](https://www.cs.princeton.edu/cass/papers/www11.pdf)
verifies the author order **Wei Dong, Moses Charikar, Kai Li**. The BibTeX
corrects the given/family-name reversal for Moses Charikar in the DOI metadata
and upstream README rather than copying that error.

### `mcinnes2026pynndescent` — pinned software

The pinned [pyproject.toml](https://github.com/lmcinnes/pynndescent/blob/969b03a753afa409b879287143b9bba62d096a53/pyproject.toml)
lists Leland McInnes as author and version 0.6.0. The upstream
[tag API](https://api.github.com/repos/lmcinnes/pynndescent/git/ref/tags/release-0.6.0)
resolves to the exact report pin; the
[commit API](https://api.github.com/repos/lmcinnes/pynndescent/commits/969b03a753afa409b879287143b9bba62d096a53)
dates it to 2026-01-08. The entry's year refers to this snapshot, not to the
project's original creation. The pinned tree contains no CITATION/CFF/BibTeX
file; no software DOI or standalone PyNNDescent publication is asserted.

### `dasgupta2008randomprojectiontrees` — algorithmic background

[Crossref's DOI record](https://api.crossref.org/works/10.1145/1374376.1374452)
verifies Dasgupta and Freund, the title, STOC 2008 proceedings, pages 537–546,
and DOI. The pinned upstream
[how-it-works notebook](https://github.com/lmcinnes/pynndescent/blob/969b03a753afa409b879287143b9bba62d096a53/doc/how_pynndescent_works.ipynb)
explains random-projection forests as NN-descent initialization. This paper is
included as foundational background selected for that subsystem, not as an
upstream-mandated citation or a claim that PyNNDescent's particular RP/hub
splits implement every detail of the paper.

### `lam2015numba` — implementation context

[Crossref's DOI record](https://api.crossref.org/works/10.1145/2833157.2833162)
verifies Lam, Pitrou, and Seibert; the title and subtitle; the 2015 LLVM-HPC
workshop proceedings; pages 1–6; and DOI. Numba is a declared dependency in the
pinned `pyproject.toml`, and the source engine uses `numba.njit` and
`numba.prange`. This reference supports the JIT implementation context noted in
the codebase report; it is not presented as a PyNNDescent algorithm paper.

## Verification limits

Publication metadata was retrieved from the DOI registration records above;
the supplemental DOI content-negotiation request for the random-projection
paper returned HTTP 429, so no successful publisher-page fetch is claimed.
All three paper DOIs are distinct, and the software snapshot is a separate
work. No preprint/published duplicate or placeholder entry is included.
