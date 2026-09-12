# S4 bibliography: provenance and coverage

## Scope and ordering

Verified on **2026-09-12** against ScienceAccelBench baseline
`9b9ac0eea` and the S4 source pin
[`7fd00a231610bff51f5c7de5f723e3956eab7453`](https://github.com/victorliu/S4/tree/7fd00a231610bff51f5c7de5f723e3956eab7453).
[references.bib](references.bib) contains **10 distinct journal articles**.
The upstream preferred software paper comes first, followed by numerical
methods and publications underlying the shipped task examples. A work cited
by both tasks is included only once; the repository itself is linked here
rather than counted as a second version of the software paper.

The supplied inventory maps **no open/pending-review PRs to `s4`**. A live
`gh pr list --repo aitofound/ScienceAccelBench --state open --search 's4 in:title'`
check also returned no task PRs at the time of this review. Thus there were no
mapped PR diffs or review threads to inspect. The complete shipped scope is
`tasks/s4/fmm-fourier-factorization/` and `tasks/s4/rcwa-eigenmode-smatrix/`.

## Authoritative verification

The pinned upstream
[`doc/source/citing.rst`](https://github.com/victorliu/S4/blob/7fd00a231610bff51f5c7de5f723e3956eab7453/doc/source/citing.rst)
explicitly identifies `liu2012s4` as the preferred citation and supplies
BibTeX, including authors, title, journal, year, volume, issue, pages and DOI.
It was checked both in the vendored Git tree and through GitHub's raw upstream
file at this exact pin. The report's software description and license are
supported by the adjacent upstream `index.rst` and `license.rst`.

For the other nine works, title, complete author list, year, journal, volume,
issue and available page/article identifiers were checked against the
publisher-deposited Crossref records below. These are verification sources,
not search-result guesses. Title-search matches were disambiguated against
upstream example headers, authors, year and journal.

| BibTeX key | Authoritative DOI metadata | Local relevance/evidence |
|---|---|---|
| `liu2012s4` | Upstream preferred citation above; [DOI](https://doi.org/10.1016/j.cpc.2012.04.026) | Both task manifests' `references`; S4 package and RCWA/FMM/S-matrix algorithm |
| `li1997fourier` | [Crossref](https://api.crossref.org/works/10.1364/JOSAA.14.002758) | Both manifests' `references`; FMM crossed-grating checks and upstream `examples/2d/Li_JOSA_14_2758_1997/` |
| `kottke2008subpixel` | [Crossref](https://api.crossref.org/works/10.1103/PhysRevE.77.036611) | FMM manifest's explicit smoothing reference; owned `S4/fmm/fmm_kottke.cpp` |
| `sakaguchi1999faraday` | [Crossref record](https://api.crossref.org/works/10.1016/S0030-4018(99)00061-9), resolved by [exact-title query](https://api.crossref.org/works?query.title=Multilayer%20films%20composed%20of%20periodic%20magneto-optical%20and%20dielectric%20layers%20for%20use%20as%20Faraday%20rotators&rows=1) | RCWA `rcwa-magneto-optic-table`; upstream `examples/0d/Sakaguchi_OptComm_162_64_1999/table1m2g2.lua` header names authors, title, pages and Table 1 |
| `antonoyiannakis1999forces` | [Crossref record](https://api.crossref.org/works/10.1103/PhysRevB.60.2363), verified via [author/title query](https://api.crossref.org/works?query.bibliographic=Antonoyiannakis%20Pendry%201999%20Electromagnetic%20forces&rows=2) | RCWA `rcwa-stress-tensor-force-2`; upstream `examples/0d/Antonoyiannakis_PRB_60_1999/fig6.lua` header |
| `liu2009opticalforces` | [Crossref](https://api.crossref.org/works/10.1364/OE.17.021897) | Five FMM lamellar checks; upstream `examples/1d/Liu_OE_17_21897_2009/fig2a.lua` header includes all three authors |
| `fan2002guidedresonances` | [Crossref](https://api.crossref.org/works/10.1103/PhysRevB.65.235112) | FMM `fmm-guided-resonance-fano`; upstream `examples/2d/Fan_PRB_65_2002/fig12.lua` header |
| `suh2003displacement` | [Crossref record](https://api.crossref.org/works/10.1063/1.1563739), verified via [author/title query](https://api.crossref.org/works?query.bibliographic=Suh%20Yanik%20Solgaard%20Fan%202003%20Displacement-sensitive&rows=2) | Both FMM photonic-crystal transmission checks; upstream `examples/2d/Suh_APL_82_1999_2003/fig2a.lua` header |
| `bi2010beamsplitters` | [Crossref](https://api.crossref.org/works/10.1364/OE.18.011969) | FMM `fmm-metallic-grating`; check README names Bi et al., Optics Express 18, 11969 (2010), and upstream `examples/1d/Bi_OE_18_11969_2010/fig3a.lua` |
| `tikhodeev2002quasiguided` | [Crossref record](https://api.crossref.org/works/10.1103/PhysRevB.66.045102), verified via [author/title query](https://api.crossref.org/works?query.bibliographic=Tikhodeev%20Yablonskii%20Muljarov%202002%20Quasi-guided&rows=2) | FMM `fmm-quasiguided-modes`; upstream `examples/2d/Tikhodeev_PRB_66_45102_2002/fig4.lua` header |

Direct DOI content negotiation and some direct Crossref requests returned
HTTP 429. Successful Crossref records/queries and the explicit upstream
citation were used instead. Publisher landing pages did not supply usable
page-range metadata in the retrieved responses. Consequently, Li 1997,
Liu 2009 and Bi 2010 retain **only the verified starting page** supplied by
Crossref; no ending pages were inferred. Physical Review's six-digit article
identifiers are stored in `pages` for traditional BibTeX compatibility.
Initials in author records are preserved rather than expanded speculatively.

## Complete shipped-check mapping

All checks use S4 and are covered by `liu2012s4`. The additional keys below
identify particular example publications, not independent experimental
validation of benchmark tolerances. Method-level `li1997fourier` and
`kottke2008subpixel` explain the FMM task; no claim is made that every check
executes every factorization rule. The RCWA manifest also cites Li 1997 as
background, but its shipped checks deliberately avoid patterned-layer FMM.

### `fmm-fourier-factorization` — 21 checks

Evidence: [task manifest](../../tasks/s4/fmm-fourier-factorization/task.toml),
[module card](../../tasks/s4/fmm-fourier-factorization/comment/pipeline/module.json),
and the corresponding `tests/checks/<check>/README.md` upstream-test/provenance
fields.

| Check(s) | Additional publication / coverage |
|---|---|
| `fmm-crossed-grating-convergence`, `fmm-crossed-grating-convergence-new`, `fmm-crossed-grating-convergence-normal`, `fmm-crossed-grating-orders` | `li1997fourier` |
| `fmm-lamellar-fig2a`, `fmm-lamellar-fig3a`, `fmm-lamellar-fig3c`, `fmm-lamellar-grating-2`, `fmm-lamellar-grating-sweep` | `liu2009opticalforces` |
| `fmm-guided-resonance-fano` | `fan2002guidedresonances` |
| `fmm-pc-slab-transmission`, `fmm-pc-slab-transmission-2` | `suh2003displacement` |
| `fmm-metallic-grating` | `bi2010beamsplitters` (see naming caveat below) |
| `fmm-quasiguided-modes` | `tikhodeev2002quasiguided` |
| `fmm-circle-rasterization`, `fmm-composite-shape-rasterization`, `fmm-ellipse-rasterization`, `fmm-polygon-rasterization`, `fmm-rectangle-rasterization` | Upstream geometry examples; `liu2012s4`, no separate paper identified |
| `fmm-polarization-basis-square`, `fmm-polarization-basis-triangular` | Upstream basis-field examples; `liu2012s4`, no separate example paper identified |

### `rcwa-eigenmode-smatrix` — 7 checks

Evidence: [task manifest](../../tasks/s4/rcwa-eigenmode-smatrix/task.toml),
[module card](../../tasks/s4/rcwa-eigenmode-smatrix/comment/pipeline/module.json),
and the corresponding check READMEs' upstream-test fields.

| Check(s) | Additional publication / coverage |
|---|---|
| `rcwa-magneto-optic-table` | `sakaguchi1999faraday` |
| `rcwa-stress-tensor-force-2` | `antonoyiannakis1999forces` |
| `rcwa-evanescent-field-profile`, `rcwa-fabry-perot-spectrum`, `rcwa-slab-resonances` | Upstream `examples/0d/fabry_perot/` examples; `liu2012s4`, no separate paper identified |
| `rcwa-gyrotropic-halfspace` | Upstream `examples/magneto/halfspace.lua`; `liu2012s4`, no separate paper identified |
| `rcwa-simple-smoke` | Upstream `examples/simple/simple.lua`; `liu2012s4`, no separate paper identified |

## Inherited discrepancies and limits

- The lamellar-check README shorthand says “Liu & Fan” for the 2009 paper.
  Upstream's header and the DOI record also credit **Michelle Povinelli**;
  the BibTeX includes her.
- `fmm-metallic-grating` is the existing check slug. Its cited article is
  about **fused-silica polarizing gratings**, and the upstream deck explicitly
  creates `FusedSilica` with real permittivity `{n2^2,0}`. This bibliography
  preserves the actual paper title instead of repeating the metallic/lossy
  characterization in task prose. No task files are changed.
- Seven of the 21 FMM checks exercise geometry or polarization-basis output,
  not `FMMGetEpsilon_*`. The authoring notes contain one sentence saying
  “the other fifteen”; the actual enumeration and later floor table imply
  **14** remaining checks. Report counts are taken from shipped directories.
- Only `rcwa-gyrotropic-halfspace` and `rcwa-magneto-optic-table` exercise the
  dense anisotropic eigensolve; the five isotropic checks cover the uniform
  closed-form path and S-matrix/field machinery, per the task evidence.
- This change does not run numerical checks, reproduce paper figures, remeasure
  source sizes or certify task tolerances. Report backfill is explicitly
  informational, and unmeasured values remain `null`/“unknown”.
- Rejected/unshipped examples in the historical task surveys are not new
  benchmark scope and do not receive speculative citations here.
