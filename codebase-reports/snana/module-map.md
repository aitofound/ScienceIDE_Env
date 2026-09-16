# SNANA module map: first task is wfit

The first task PR covers **`wfit` only**. The other four rows below are candidates for later work, with no delivery schedule or completed task validation in this contribution. This source PR preserves the complete SNANA tree at `f7ad9ad6a1f58d7c550b646d9aeb86a9e8c34eb1`; the repository-wide size is separate from the first task's scope.

## Functional boundaries

| Module | Entry points | Inputs → outputs | Owned files / physical lines | Delivery status |
|---|---|---|---:|---|
| `wfit` | `wfit.exe` | Hubble-diagram distances, covariance and fixed priors → cosmological parameters, uncertainties and likelihood diagnostics | 1 / 5,626 | **First task**; three official regression decks reproduced natively |
| `light-curve-analysis` | `snana.exe`, `snlc_fit.exe`, `psnid.exe` | Light curves, fixed models and calibration → selected data, fitted parameters or photometric classifications | 7 / 60,123 | Later candidate; not built or run in this contribution |
| `supernova-simulation` | `snlc_sim.exe` | Populations, transient models and survey conditions → simulated light curves, spectra and selection statistics | 21 / 58,803 | Later candidate; not built or run in this contribution |
| `bias-correction` | `SALT2mu.exe` | Light-curve fit summaries and correction simulations → standardization parameters and bias-corrected distances | 1 / 27,399 | Later candidate; not built or run in this contribution |
| `photometric-calibration` | `kcor.exe` | Passbands, reference spectra and transient templates → calibration and K-correction tables | 2 / 6,465 | Later candidate; not built or run in this contribution |

These are functional boundaries within a shared `src/` directory and build system. The proposal does not claim independently packaged subprojects. In particular, [`snlc_fit.F90`](../../code/snana/src/snlc_fit.F90#L5) and [`psnid.F90`](../../code/snana/src/psnid.F90#L5) both include `snana.F90`; the three analysis programs therefore remain together. Shared model evaluation also couples simulation and fitting. Acceptance of this functional split is part of source review.

The first `wfit` task consumes fixed distance/covariance inputs. It does not need to run the light-curve fitter, simulator, calibration generator or BBC stage. Those upstream workflow products are input fixtures, not additional task implementations.

## Shared and supporting files

| Component | Files | Physical text lines | Role |
|---|---:|---:|---|
| Common runtime used by `wfit` | 22 | 42,730 | Conservative local-include closure of the six-object target, excluding `wfit.c`; includes numerical helpers, cosmology, data declarations, I/O, NPZ and bundled notices |
| Model evaluation | 34 | 23,656 | Light-curve and spectral model family used by later candidate programs |
| Build system | 8 | 3,621 | Common build/configuration files and helpers |
| Other numerical/data infrastructure | 40 | 63,410 | Remaining numerical, data-format, calibration and model-grid routines for later candidates |
| Repository tools and orchestration | 88 | 66,361 | Batch/test drivers, converters, table/model preparation and retained legacy helpers; outside the first task |
| Documentation and repository metadata | 15 | 43,812 | Upstream documentation, generated manual artifacts and repository metadata; outside scientific task code |

Every file is assigned once: **32 module-owned files + 207 shared/support files = 239 files**, with no overlapping ownership or unclassified files. Exact path lists and computed counts are in [`codebase-metadata.json`](codebase-metadata.json). A component's size describes its source inventory; it is not a claim that every associated executable links every file or that all files are acceleration targets.

General cosmology helpers are now counted in common runtime rather than exclusively under `wfit`. This changes the reported ownership count from 6,509 to 5,626 lines without changing source content. The `wfit` dependency view contains its one owned file, 22 conservative runtime dependencies and 8 build-support files. Optional preprocessor branches remain in that source inventory, so it is not a minimal linker footprint.

No source file was moved or modified. The canonical source fingerprint remains `39af311ecaa368643d807ea2ae62396e2823d50db781e901a0a1ae29b2b622cb`.

## External test landscape and execution status

The external `SNANA_TESTS` task definitions were inspected on Midway on 2026-09-16. `SNANA_tasks_all.LIST` contains 101 entries; `SNANA_tasks_salt2mu.LIST` adds 15. Three of the resulting 116 distinct definitions invoke external SCONE code, leaving 113 definitions for programs in the pinned SNANA tree. `SNANA_tasks_wfit.LIST` repeats the same three WFIT entries and is not counted again.

| Area | Inspected definitions | Execution in this contribution |
|---|---:|---|
| `wfit` | 3 | All three ran on the clean native Midway build and existing Mac binary; all four COSPAR values matched the stored upstream reference to five decimal places |
| Light-curve analysis | 49: 43 `snlc_fit`, 6 `snana` | Not run; no PSNID-specific definition was located in these selected lists |
| Simulation | 44 | Not run |
| Bias correction | 15 | Not run |
| Calibration | 1 | Not run |
| SIMSED extraction support | 1 | Not run; supporting utility rather than an additional task module |

These counts describe inspected definitions, not executed checks. A complete suite collection was not performed. Several fitting definitions depend on generated simulation outputs, so later task work would need separate fixture preparation and validation. References for the later candidates were not inspected; a `none` reference marker in the source-stage landscape means no reference evidence was recorded for those candidates.

List-file SHA-256 values:

- `SNANA_tasks_all.LIST`: `7303037a8ce6e285c7d659ae336a43012ccb7a6c07c9066f9bce76b380862a7f`
- `SNANA_tasks_salt2mu.LIST`: `2beab1ac33464380bc1497a509fc919e1833a4736689d280edea908990c58ef1`
- `SNANA_tasks_wfit.LIST`: `d678411f8c9e0f7b10076cd8451ac3028ebafb6b8b874f95594e9ed5d1dbb29e`

Only the existing 12-file WFIT data subset, 12,156,883 bytes, was downloaded. The broader module inspection read task definitions without downloading model or survey datasets and without executing non-wfit programs.

The native WFIT comparisons and supplemental prototype probes are source-stage evidence. The formal task survey, checks and ScienceAccelBench self-validation follow source merge. Licensing evidence and unresolved repository-wide redistribution terms remain documented in [`license-audit.md`](license-audit.md).

## Source evidence

- [`src/Makefile.am`](../../code/snana/src/Makefile.am#L100) defines common object groups and separate executable targets.
- [`src/snlc_fit.F90`](../../code/snana/src/snlc_fit.F90#L5) and [`src/psnid.F90`](../../code/snana/src/psnid.F90#L5) establish the shared light-curve analysis substrate.
- [`src/snlc_sim.c`](../../code/snana/src/snlc_sim.c#L17) describes the survey simulation contract.
- [`src/SALT2mu.c`](../../code/snana/src/SALT2mu.c#L1) describes standardization and corrected-distance outputs.
- [`src/kcor.c`](../../code/snana/src/kcor.c#L1) describes filter/spectrum calibration inputs and tables.
