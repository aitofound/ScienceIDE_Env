# Squidpy bibliography: scope and verification

[`references.bib`](references.bib) contains six distinct publications, with the
upstream-requested Squidpy paper first, followed by methods and supporting
software/resources relevant to the existing report. This is a focused
bibliography, not a copy of every dataset or optional-method reference in the
upstream documentation.

## Scope audited on 2026-09-12

- The existing `codebase-metadata.{json,md,html}` report is present and remains
  unchanged; no report backfill was needed. Its approved module is the complete
  `squidpy-spatial-analysis` environment, not the withdrawn three-way split.
- The report pins [scverse/squidpy v1.8.3 at
  `005c9056fea7c5432fb220abc9b48a384fb8c090`](https://github.com/scverse/squidpy/tree/005c9056fea7c5432fb220abc9b48a384fb8c090).
  The pinned tree has no CITATION/CFF file; its
  [README citation section](https://github.com/scverse/squidpy/blob/005c9056fea7c5432fb220abc9b48a384fb8c090/README.md#citation)
  and [documentation bibliography](https://github.com/scverse/squidpy/blob/005c9056fea7c5432fb220abc9b48a384fb8c090/docs/references.bib)
  are the primary upstream citation sources.
- No `tasks/squidpy/**` files exist at the audited ScienceAccelBench base commit
  `9b9ac0eea13712f2e40d25e28ac4c4b49a07c761`. A live GitHub API listing of the
  `main` task tree also contains no `squidpy` directory.
- The supplied `scienceaccel_inventory.json` lists Squidpy as a report codebase,
  but not a shipped task codebase, and maps **zero open/pending PRs** to it.
  A live `gh pr list --state open` inspection of PR titles, branches and changed
  files found no Squidpy PR before this bibliography PR was opened.
- The report's referenced [source PR #616](https://github.com/aitofound/ScienceAccelBench/pull/616)
  was additionally inspected with `gh pr view`; it is merged and describes the
  same pin and whole-codebase module. Proposed scale-up checks in that report
  are not treated here as shipped benchmark tasks.

These findings describe the audited snapshot, not a claim that future task or
PR additions are covered. Existing report unknowns, test caveats and source
omissions are preserved rather than inferred away.

## Source-to-reference mapping

All Squidpy source links below use the report's immutable upstream pin.

| BibTeX key | Relevance and upstream evidence | Metadata verification |
| --- | --- | --- |
| `palla2022squidpy` | Primary software paper explicitly requested by the README; umbrella reference for the complete environment, including spatial graphs, neighborhood enrichment and co-occurrence. | Pinned README and [Crossref DOI record](https://api.crossref.org/works/10.1038/s41592-021-01358-2): 2022, *Nature Methods* 19(2), 171–178, all 13 authors. |
| `andersson2021sepal` | Diffusion-based spatial transcript scoring; [`gr/_sepal.py`](https://github.com/scverse/squidpy/blob/005c9056fea7c5432fb220abc9b48a384fb8c090/src/squidpy/gr/_sepal.py#L53-L54) explicitly cites `andersson2021`. | [Crossref DOI record](https://api.crossref.org/works/10.1093/bioinformatics/btab164): 2021, *Bioinformatics* 37(17), 2644–2650. The DOI record spells the first author **Andersson**, correcting the upstream bibliography's `Anderson`. |
| `efremova2020cellphonedb` | Ligand–receptor permutation testing; [`gr/_ligrec.py`](https://github.com/scverse/squidpy/blob/005c9056fea7c5432fb220abc9b48a384fb8c090/src/squidpy/gr/_ligrec.py#L694-L697) explicitly identifies the CellPhoneDB analysis. | Pinned bibliography and [Nature Protocols publisher metadata](https://www.nature.com/articles/s41596-020-0292-x): 2020, 15(4), 1484–1506, four authors. |
| `rey2010pysal` | Spatial autocorrelation (Moran's I / Geary's C); [`gr/_ppatterns.py`](https://github.com/scverse/squidpy/blob/005c9056fea7c5432fb220abc9b48a384fb8c090/src/squidpy/gr/_ppatterns.py#L72) explicitly cites `pysal`. | [Springer chapter metadata](https://link.springer.com/chapter/10.1007/978-3-642-03647-7_11): *Handbook of Applied Spatial Analysis*, 2010, 175–193. Use the publisher's book publication year, consistent with upstream, rather than Crossref's 2009 online-first chapter date. |
| `vanderwalt2014scikitimage` | Image-analysis dependency, **not owned Squidpy arithmetic**: [`im/_feature_mixin.py`](https://github.com/scverse/squidpy/blob/005c9056fea7c5432fb220abc9b48a384fb8c090/src/squidpy/im/_feature_mixin.py) imports and calls texture and region-property functions; [`im/_segment.py`](https://github.com/scverse/squidpy/blob/005c9056fea7c5432fb220abc9b48a384fb8c090/src/squidpy/im/_segment.py) delegates watershed segmentation. | [Official scikit-image CFF preferred citation](https://github.com/scikit-image/scikit-image/blob/main/CITATION.cff) and [Crossref DOI record](https://api.crossref.org/works/10.7717/peerj.453): 2014, *PeerJ* 2, e453. The CFF supplies the collective author and the first author's middle initial. |
| `turei2016omnipath` | Interaction-resource credit: [`gr/_ligrec.py`](https://github.com/scverse/squidpy/blob/005c9056fea7c5432fb220abc9b48a384fb8c090/src/squidpy/gr/_ligrec.py#L587-L590) cites OmniPath for default interaction datasets. | Pinned bibliography and [Nature Methods publisher metadata](https://www.nature.com/articles/nmeth.4077): 2016, 13(12), 966–967, three authors. |

Each publication appears once, identified by a unique DOI; no duplicate
preprint/software alias entry is added. BibTeX uses standard `article` and
`incollection` types, braced software names, TeX-escaped author accents, and DOI
resolver URLs. The references document scholarly provenance; they do not assert
benchmark correctness, measured acceleration, or ownership of delegated kernels.
