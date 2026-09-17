# osiris: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, the Step 1.2 build-and-run record, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

OSIRIS is packaged as one whole-codebase module because particle advance,
current deposition, field solve, MPI exchange, diagnostics and I/O share one
executable and simulation state. All 13 official text decks became checks:
the 1D/2D/3D base cases, three PWFA examples, exact-pusher and custom-solver
examples, the four-deck RAW producer/import workflow, and the 3D RAW reference
box. No official deck and no source path was excluded.

## Build

The pinned source is compiled at solve time with GNU Fortran, OpenMPI and
parallel HDF5. Each run script keeps a dimension-specific production build in
`/tmp`; checks with the same 1D, 2D or 3D configuration reuse it inside one
container, while an isolated check rebuilds for itself and reports
`SAB_BUILD_SECONDS`. The accepted fresh selfcheck measured 244.3 s for the
nominal solve and 249.0 s for the variant solve, with 227.0 s reported as
build time and 13.5 s as nominal suite run time excluding builds.

## Tolerances

The human accepted `atol=1e-12`, `rtol=1e-9` for the four pointwise checks and
`atol=1e-12`, `rtol=1e-6` for every agreement invariant in the other nine
checks. The accepted fresh nominal-versus-two-ULP calibration produced
byte-identical graded outputs in all 13 checks (spread 0). No alternative
build is declared, so these bounds are conservative cross-platform
portability envelopes rather than fits to a measured nonzero floor.

## Blind spots

The suite does not grade wall-clock timings, MPI layout, iteration counts,
random draws, particle storage order, restart files, collisions, ionization
or every commented alternative custom-solver coefficient. The first five are
non-physical or implementation-dependent bookkeeping. The remaining models
have no distinct checked-in official deck at this pin; this limitation is
kept visible rather than replaced with an unapproved custom check.
