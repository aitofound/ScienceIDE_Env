# KCOR tolerance review

All five calibration checks pass (reward 1.0). The figures below are measured on the committed scientific source pin, before the curator finalises the new kcor bounds.

## A. Proposed physical comparison rules

| Observable | Numerical bound |
|---|---|
| Zero points; peak SNMAG and DM15 | Absolute difference <= 5e-4 mag |
| Full K-correction and observer-magnitude grids | Absolute difference <= 2e-5 mag |
| MW extinction slope | Absolute difference <= 5e-5 mag per mag E(B-V) |
| Passband response | 1e-8 + 3e-6 x absolute reference |
| SN SED and primary spectrum | 1e-10 x the reference array peak + 3e-6 x absolute reference |

All physical identities, complete coordinate sets and 999/666 validity masks must agree. Every associated array is aligned together; storage order is ignored. SN SED flux is per wavelength bin, primary flux is per Angstrom.

## B. Measured spread, margin and fault separation

| Check | Nominal/variant maximum fraction of bound | Remaining factor to the bound | Computed response | Smallest source-fault fraction of bound |
|---|---|---|---|---|
| example-duplicate-shift | 0.400543 | 2.497x | zeropoints: 4 cells, max delta 0.000200271606; snmag_summary: 2 cells, max delta 0.0002 | 20.0001x |
| example-hst-kgrid | 0.0953674 | 10.49x | zeropoints: 2 cells, max delta 5.01051545e-07; kcor_values: 20670 cells, max delta 7.15255737e-07; grid_magnitudes: 1282 cells, max delta 1.90734863e-06 | 500.059x |
| example-sdss-bessell | 0.4 | 2.5x | zeropoints: 2 cells, max delta 0.000199999995; snmag_summary: 1 cells, max delta 0.0002 | 20x |
| manual-filter-shift | 0.400543 | 2.497x | zeropoints: 2 cells, max delta 0.000200271606; snmag_summary: 1 cells, max delta 0.0002 | 20.0001x |
| official-sdss | 0.400543 | 2.497x | zeropoints: 2 cells, max delta 0.000200271606; snmag_summary: 1 cells, max delta 0.0002 | 20.0001x |

The non-grid checks perturb one reference magnitude by 2e-4 mag (two units of the four-decimal summary). HST uses a 5e-7 mag perturbation: it changes 20,670 K-correction cells and 1,282 observer magnitudes. Unrelated spectra, passbands and extinction slopes intentionally do not move under a zero-point perturbation; they are not counted as numerical-floor evidence.

## C. Compiler and architecture evidence

| Comparison | Checks | Largest graded difference | Interpretation |
|---|---|---|---|
| ARM64 Linux GCC versus Clang, strict IEEE -O2 | 5/5 | 0 at output precision | Blind: no nonzero arithmetic floor is resolved |
| ARM64 Linux container versus native Midway x86_64 GCC 10.2/GSL 1.16 | 5/5 | 0 at output precision | Confirms the baseline on a second architecture; still does not resolve an arithmetic floor |

Independent photon-weighted quadrature reproduces all zero points within 4.17e-7 mag and all printed peak/DM15 summaries within 4.96e-5 mag (half the last printed digit). The source stores the large grids as binary32. The proposed bounds therefore have a stated output-precision basis and measured fault separation; they are not estimates of observational error or a claim of zero floating-point error.

## Fault and validator evidence

| Deliberate fault | Check(s) | Fraction of the allowed bound |
|---|---|---|
| zero-point-bias | example-duplicate-shift | 20.0001x |
| zero-point-bias | example-hst-kgrid | 500.059x |
| zero-point-bias | example-sdss-bessell | 20x |
| zero-point-bias | manual-filter-shift | 20.0001x |
| zero-point-bias | official-sdss | 20.0001x |
| omit-energy-response | official-sdss | 2.47452e+06x |
| omit-mw-extinction | example-hst-kgrid | 105249x |
| sn-flux-scale | official-sdss | 3333.25x |

All 8 deliberate-source runs completed and were rejected. All 56 supplemental validator probes behaved as expected, including simultaneous permutation of every identity axis, missing identities, nonfinite outputs, corrupted physical arrays, duplicate K-grid coordinates, changed invalid masks and a truncated later block. An unshifted-output proxy also fails the wavelength-shift check.

The calibration bounds await the curator's decision. The compiler/architecture comparison is explicitly accepted only as agreement at the available output precision, not as a measured nonzero floor.
