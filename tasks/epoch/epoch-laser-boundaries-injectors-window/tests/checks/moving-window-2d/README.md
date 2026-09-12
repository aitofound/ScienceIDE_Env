# moving-window-2d

Upstream test: `code/epoch/epoch2d/example_decks/window.deck`. Policy: `invariants`.

## The test

`run.sh` builds the pinned tree's epoch2d binary once and runs the shipped 2-D `window` example deck, unchanged apart from an explicit 2x2 rank layout, a quieter stdout, a dump cadence coarsened from 1e-10 s to 1e-9 s, an output block trimmed to the grid and the number density, and an end time halved to 5 ns. The halving is a budget decision and nothing else: the deck at its own end time and cadence spends 68 of its 69 seconds writing 2.6 GB of particle dumps, and by 5 ns the window has already advanced 256 cells, a full box length, so every field array has been shifted and refilled from the leading edge more than two hundred times and the transverse structure of the density slab has passed through the box. What two dimensions add over the 1-D check is that `insert_particles` now runs on every rank that owns the right-hand edge, each drawing from its own seeded stream over its own y range, and that the inserted weight comes from a triangle interpolation across three transverse cells. The knobs are `SAB_NX`, `SAB_NY`, `SAB_T_END`, `SAB_DT_SNAPSHOT`, `SAB_PPC` and `SAB_MAKE_JOBS`; the official end time is `SAB_T_END=10e-9`. The check takes 54 s.

## The two initial conditions

`ic/nominal` is the shipped deck with `nprocx = 2, nprocy = 2` written in, the coarsened grid and window where this check coarsens them, and the knob markers. `ic/variant` differs on exactly those rank-layout lines: `nprocx = 1, nprocy = 4`, a different valid decomposition of the same domain over the same number of ranks' worth of work. EPOCH's generator is a 32-bit integer KISS seeded 7842432 + rank (`src/housekeeping/random_generator.f90`, `src/housekeeping/setup.F90`), so a different decomposition draws this deck's per-cell random stream in a different order: a correct, equally valid realisation of the same physics, and exactly what a differently parallelised or vectorised port would also produce. That is the point of the variant after the round-2 redesign -- the earlier variant perturbed the deck's density constant by five ulps and deliberately stayed on the same random stream, which tested rounding but not robustness to reordering. A THIRD decomposition (`nprocx = 4, nprocy = 1`) was run by hand through this check's own `run.sh` and `extract.py` on 2026-09-06 on the x86_64 worker, so every graded statistic below has three correct realisations behind it rather than two; the largest pairwise spread among those three is what sets each bound. `run.sh` also accepts `altbuild`, which runs `ic/nominal` unchanged on the same shipped gfortran build one optimisation level down (`-O0` in place of `-O3`, `epoch2d/Makefile` line 72 in the scratch build copy only), the same pinned source and deck; EPOCH's own `MODE=debug` profile was tried first and rejected because it aborts with SIGFPE inside Open MPI's own `mpi_minimal_init` before any EPOCH arithmetic runs.

## The pass policy

The policy is `invariants` (round-2 steward review items 3 and 4, curator options A and B). The check splits into two kinds of graded quantity. The moving grid's origin and extent are deterministic: EPOCH shifts the window by whole cells on a criterion timed off `dt` and `window_v_x` and advances the origin by adding `dx` once per shift (`src/housekeeping/window.F90`), with no random draw anywhere in it, so they are graded as near-exact agreement at rtol 1e-12 -- round-off room for a port that computes the origin in closed form as `x_min + N*dx` instead of EPOCH's repeated-`dx` recurrence, and nothing more. All three rank layouts measured on 2026-09-06 returned bit-identical origins and extents at every graded dump, which is what the source predicts. The electron density is the other kind: the loader draws five macroparticles per cell from the seeded stream, so which draw lands in which cell is a realisation detail and the raw 65536-cell array is not graded. What is graded are moments of the density profile -- the mean density, the blob's excess-mass centroid along x and y, its excess-mass width along x and y and its total excess mass -- at three dumps. Bounds are absolute, in the deck's own units (density in units of its background constant, positions in metres of a domain one metre on a side): mean density and excess mass atol 3e-4, 2e-4 and 1.5e-4 at the three graded dumps; the x centroid atol 1.5e-3, 3e-4 and 7e-4 and the y centroid atol 2e-3, 2e-3 and 1.5e-3; the y width atol 1e-3, 3e-4 and 3e-4 and the x width atol 2e-4 and 7e-4 at the last two dumps. The x width at the first graded dump is the one exception: the blob has only partly entered the window there and the per-cell loading noise makes the excess second moment negative in every measured layout, so `extract.py`'s clamp reports zero; its bound is set to 0.2, the width this statistic reaches at the later graded dumps, and it carries no discriminating power at that dump. Every bound is the smallest 1/1.5/2/3/5/7 x 10^n value that is at least four and at most ten times that statistic's largest pairwise spread over the three rank layouts (the measured headroom per statistic is 4.0x to 6.5x, and each bound carries a one-line `_bound_note` in `rubric.json`). A deleted or corrupted shift leaves the grid origin wrong by a whole cell, 3.9e-3 m, nine orders of magnitude outside the rtol 1e-12 origin bound; a lost transverse seam in the shift would move the y centroid or the y width by a cell or more, more than an order of magnitude outside their bounds. Gap stated under round-2 review item 3 and the curator's option B: this deck carries no laser and no initialised field, so the field and CPML-memory half of `shift_fields` is exercised but not graded here.

## Evidence

The altbuild floor and the in-container nominal-versus-variant spread are measured by `sab.py task selfcheck` on the assigned x86_64 worker and written into `rubric.json` (`evidence.floor`, `evidence.floor_how`, `evidence.self_validation_spread`, `evidence.self_validation_bound_fraction`) and `comment/pipeline/self-validation.json`. The three-rank-layout calibration that set every bound above -- each statistic's value in the three layouts, the largest pairwise spread and the resulting headroom -- is recorded in `comment/probes/rank-layout-spread-20260906.json`, which is authoring material and not part of the solver's inputs. Nothing in this file states a reference output value.

## Redesign, 2026-09-06 (round-2 steward review, items 3 and 4; curator option A/B)

Moved from `pointwise` to `invariants`. The moving grid's shift
(src/housekeeping/window.F90) is a deterministic arithmetic recurrence with
no random draw in it, so the grid origin and extent stay graded
exact/near-exact -- unchanged in kind, only regrouped under the invariants
comparison. The electron load, by contrast, is a per-cell random draw from
EPOCH's seeded KISS stream (7842432 + rank), subject to the same
reordering argument as the injector checks, so `extract.py` now writes
density-profile moments (mean, excess-mass centroid and width along every
axis, total excess mass) instead of the raw per-cell density array.
`ic/variant` is now the deck's rank layout changed to a different valid
decomposition (was: a five-ulp density-constant perturbation on the same
random stream). Gap (steward item 3, curator option B, documented rather
than closed): this deck carries no laser and no nonzero field
initialisation, so the window's E/B/J field shift and CPML memory
translation are not exercised by this check; only the density-loading
translation and the grid bookkeeping are graded. No check was added or
removed. Bounds were set on 2026-09-06 from the measured realisation spread
(see below).

Extractor fix, 2026-09-06 (measured on the worker against real SDF dumps): `Grid/Grid` is EPOCH's node-centred coordinate array (nx+1 points along each axis); `Derived/Number_Density/electron` is cell-centred (nx values). `extract.py` was computing the density-profile moments against the node coordinates directly, which raised a numpy broadcast error (measured: (256,) vs (257,) in 1-D, (256,256) vs (257,257) in 2-D, (64,64,64) vs (65,65,65) in 3-D). It now takes cell centres as the midpoints of adjacent grid nodes for the density moments; the grid-origin/extent invariant above is unaffected (it reads the node array directly, as EPOCH reports it).

## Bounds, 2026-09-06

The provisional bounds this check carried between the redesign and the calibration are gone. Every bound above was set from the realisation spread measured across THREE valid MPI rank layouts of this deck on the x86_64 worker on 2026-09-06 (the two shipped in `ic/` plus a third run by hand through this check's own `run.sh` and `extract.py`), using the convention this project's physics-packages leaf uses: four to ten times the largest measured spread per statistic, relative where the quantity scales with the deck, absolute where it does not, and tight to round-off where the quantity is deterministic. The per-statistic numbers behind each bound are in `comment/probes/rank-layout-spread-20260906.json`.
