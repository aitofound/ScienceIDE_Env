# pyXSIM bibliography provenance

[`references.bib`](references.bib) is ordered by relevance: the software record
and implementation paper first, followed by the PHOX foundation, the atomic
physics used by the shipped thermal checks, and supporting literature. It is
not a claim that every feature discussed in these papers is benchmarked.

## Sources checked

Verified on 2026-09-12 against upstream pyXSIM commit
`e21e7fb782174e2e88253c6fb1de24f90f4f45dc`, the pin in the existing
[`codebase-metadata.json`](codebase-metadata.json).

- The upstream tree was inspected with `gh api`; it has no `CITATION` or CFF
  file at this pin. The official
  [README](https://github.com/jzuhone/pyxsim/blob/e21e7fb782174e2e88253c6fb1de24f90f4f45dc/README.md)
  and [photon-list overview](https://github.com/jzuhone/pyxsim/blob/e21e7fb782174e2e88253c6fb1de24f90f4f45dc/doc/source/photon_lists/overview.rst)
  identify both PHOX papers and the SciPy 2014 paper as its algorithmic heritage.
- The official [thermal-source documentation](https://github.com/jzuhone/pyxsim/blob/e21e7fb782174e2e88253c6fb1de24f90f4f45dc/doc/source/source_models/thermal_sources.rst)
  describes collisional-ionization-equilibrium (CIE) models, interpolation of
  tabulated emissivities, variable elemental abundances, and thermal broadening.
- The existing metadata report was already present and was not backfilled or
  changed. No task, source, registry, or generated metadata file is changed.

| BibTeX key | Verification and relevance |
|---|---|
| `zuhone2016pyxsim` | [ASCL record 1608.002](https://ascl.net/1608.002) supplies the software title, authors, year (also encoded in bibcode `2016ascl.soft08002Z`), and upstream links. This is a software record, not a journal article or an invented version DOI. |
| `zuhone2014xray` | [SciPy publisher page](https://proceedings.scipy.org/articles/Majora-14bd3278-010) and its downloadable BibTeX verify all six authors, title, proceedings, editors, pages, year, and DOI. The upstream README explicitly links this description of the original yt implementation. |
| `biffi2012phox` | Upstream README gives MNRAS 420, 3545 (2012); [Crossref DOI metadata](https://api.crossref.org/works/10.1111/j.1365-2966.2011.20278.x) and [arXiv:1112.0314](https://arxiv.org/abs/1112.0314) verify title, authors, and DOI. Crossref's publisher PDF link identifies issue 4. The entry retains only the verified first page, rather than inventing an end page from Crossref's incomplete `no-no` pagination. |
| `smith2001apec` | [Crossref DOI metadata](https://api.crossref.org/works/10.1086/322992) verifies the APEC/APED article, authors, year, journal, volume, issue, and pages. It provides the atomic-emission-model foundation for the APEC/VAPEC spectra used in all four checks. |
| `foster2012atomdb` | [Crossref DOI metadata](https://api.crossref.org/works/10.1088/0004-637X/756/2/128) verifies the atomic-data paper and article number 128. This is methodological background for the emissivity database, not a citation claiming to publish the particular v3.1.3 files shipped by the task. |
| `biffi2013velocity` | [Crossref DOI metadata](https://api.crossref.org/works/10.1093/mnras/sts120), [arXiv:1210.4158](https://arxiv.org/abs/1210.4158), and the upstream README agree on the PHOX velocity/X-ray-properties study. The entry uses the 2013 print year rather than the 2012 online/preprint year. It is upstream context for velocity-sensitive synthetic observations, not the specification of the benchmark's relativistic kernel. |
| `turk2011yt` | The [official yt CITATION](https://github.com/yt-project/yt/blob/main/CITATION) and [Crossref DOI metadata](https://api.crossref.org/works/10.1088/0067-0049/192/1/9) verify the recommended toolkit paper. The 2011 print year is used, not its 2010 online date. yt provides the uniform-grid datasets, physical fields, and cosmology used by the beta-model checks. |

## Shipped task coverage

The sole shipped module is
[`tasks/pyxsim/thermal-spectral-synthesis`](../../tasks/pyxsim/thermal-spectral-synthesis).
Its check READMEs and `probe.py` implementations were inspected directly.

| Check | Relevant references and evidence |
|---|---|
| `apec-table-spectrum` | `smith2001apec`, `foster2012atomdb`: broadened `TableCIEModel("apec", ...)`, temperature-table interpolation, continuum/metal composition, and photon/energy band integrals. |
| `beta-model-band-fields` | APEC/AtomDB plus `turk2011yt`: CIE source/intensity fields on a yt grid, with eight integrated energy/photon luminosity and flux observables, including Doppler and no-Doppler paths. |
| `beta-model-doppler-spectrum` | APEC/AtomDB plus yt underpin the deterministic thermal spectra. `zuhone2014xray` and `biffi2013velocity` supply broader synthetic-observation/velocity context. The actual rest-frame, cosmological, line-of-sight, and transverse relativistic outputs are specified by the task and pinned implementation, not inferred from these papers. |
| `vapec-variable-element-spectrum` | APEC/AtomDB plus yt: a `CIESourceModel("apec", ...)` combines explicit O/Ca abundance fields and produces a deterministic spectrum and band integrals. This does not grade the upstream test's random photon sampling. |

The [task environment](../../tasks/pyxsim/thermal-spectral-synthesis/environment/Dockerfile)
pins SOXS 5.3.0, yt 4.4.2, and the APEC v3.1.3 continuum and line tables with
checksums. The [authoring notes](../../tasks/pyxsim/thermal-spectral-synthesis/comment/README.md)
explicitly exclude NEI, Cloudy/PION tables, absorption, random photon/event
outputs, and other external datasets from this check set. No extra bibliography
entries were added on the assumption that these untested paths are covered.

## Pending work and deduplication

The bibliography inventory used for this review (`scienceaccel_inventory.json`)
contains **no open/pending PR mapped to pyxsim** by codebase or task-file path.
A live `gh pr list --repo aitofound/ScienceAccelBench --state open --search pyxsim`
also returned no matches before this bibliography PR was created. Thus there
was no mapped pending module to add to the coverage table; this does not imply
coverage of unrelated open PRs.

There are seven distinct works: one ASCL software record and six publications.
Keys and normalized DOIs are unique. arXiv identifiers are attached to their
journal entries rather than duplicated as separate preprint entries.
