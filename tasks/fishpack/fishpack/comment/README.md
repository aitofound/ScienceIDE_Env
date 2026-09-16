# fishpack: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, the Step 1.2 build-and-run record, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

FISHPACK 4.1 computes second- and fourth-order finite-difference solutions of
separable elliptic PDEs (Helmholtz, Poisson, general separable equations) in
Cartesian, polar, cylindrical and spherical coordinates by cyclic reduction; the
module owns the whole vendored tree under `code/fishpack/` (single-module cut,
`paths: ["."]`, recorded by `propose-modules`). The survey (`tests.json`) found
19 official test programs under `test/`, one per solver entry point, all
`suitable: true`; this leaf has exactly 19 checks, one per row, with no
omissions. Every check's `driver.f` is a byte-for-byte copy of the
corresponding `code/fishpack/test/t*.f` with one output section added (see
each check's README.md); nothing in `code/fishpack/` itself is edited. FISHPACK
ships no other example decks or benchmark suites beyond `test/`, so the 19
checks are exhaustive over the codebase's official tests.

## Build

Every check's `run.sh` builds `code/fishpack/src` (24 Fortran 77 files) into
`libfishpack.a` with `gfortran -fdefault-real-8 <-O2|-O0> -std=legacy`, then
links its own `driver.f` against that library. The compile-and-link is fast
(well under 1 s per check for the library, well under 0.1 s for a driver), so
each `run.sh` caches the compiled library in
`${TMPDIR:-/tmp}/sab-fishpack-buildcache<-O2|-O0>` and reuses it when an
earlier check of the same run already built that configuration (keyed by the
optimization flag only, since every check compiles the same, unmodified
`src/`); a check that finds nothing cached builds for itself, so it stays
self-contained. `SAB_BUILD_SECONDS` reports the actual seconds spent (the
library build on a cache miss, essentially zero on a hit, plus the driver
link). `-fdefault-real-8` is required to match the shipped `output/darwin.dp`
double-precision reference transcript; `-std=legacy` is required because
gfortran >= 10 refuses this Fortran 77 tree's argument-type and rank
mismatches otherwise (measured on the codebase's own Step 1.2 record,
`comment/pipeline/build-and-run.json`). The declared per-check resources are 1
cpu / 1 GB; the resource-aware `solve.sh` packs as many single-cpu containers
as the consented host allows, and since every check shares one build
configuration per initial condition, they land in the same shard and share
one cached library build per solve.

## Tolerances

Every check's rubric was authored with the same provisional bound,
`atol=1e-8, rtol=1e-8` on every raw f64 value of `solution.bin` and
`scalars.bin` (the stock pointwise `validate.py`). The bound was chosen to sit
three to six orders of magnitude below each routine's own printed
discretization error (1.6e-05 to 2.9e-02 across the 19 solvers) and many
orders above the ULP-level floor: on this leaf's own arm64 macOS scratch
calibration (gfortran 15.2, not the target host), a two-ULP perturbation of
one active grid or coefficient input moved every graded value by at most
4.6e-13 (worst case, tcmgnbn/tgenbun; most checks were under 1e-14). The
target-host floor is measured by `task selfcheck` on the chartered x86 Debian
trixie worker and recorded per check in `rubric.json`'s `evidence` block and
in `comment/pipeline/self-validation.json`; per-check detail and the warrant
paragraph live in each check's own `README.md` and `rubric.json`.

One policy choice changed after authoring, not after a selfcheck run:
`thstssp` and `thwsssp` (HSTSSP and HWSSSP) solve singular pure-Neumann
problems on a closed surface, unique only up to an additive constant that the
solver fixes by a least-squares projection (`PERTRB`). Scratch calibration
showed that the *raw* solution array's arbitrary constant moves by a uniform
~5e-3 under a two-ULP perturbation of the grid spacing -- a discrete-choice
sensitivity in that projection's near-singular pivot, comparable in size to
the routine's own discretization error -- while every other check's raw array
moved by ordinary ULP-level noise. Rather than loosen the bound to
accommodate a non-physical gauge constant, both drivers dump the gauge-fixed
field `F(I,J)-F(1,1)` (exactly the normalization the official test itself
applies before computing its own discretization error), which restored the
ULP-level spread. This is documented as the `extra_warrant` text in both
checks' `rubric.json` and `README.md`.

## Blind spots

FISHPACK 4.1's own `test/` has no example beyond the 19 official test
programs (no separate benchmark suite, no undocumented driver families), so
the 19 checks cover every official test with no `suitable: false` rows. What
they do not cover: (1) the single-precision build (`darwin.sp`), which the
codebase's own Step 1.2 record found reproduces the reference to fewer
digits and is not the shipped double-precision default this leaf builds; (2)
FISHPACK's FFT dependency (`src/fftpack.f`, an internal, unexported copy used
only by `POIS3D`/`GENBUN`/`CMGNBN`) is exercised only indirectly through
those three checks, never as its own check, since it ships no standalone
official test; (3) no check varies `IFLG` (BLKTRI/CBLKTRI's "already
initialized" fast path) beyond what the official test itself exercises,
since the official test always takes the same two-call sequence.
