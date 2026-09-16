# fftpack: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, the Step 1.2 build-and-run record, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

FFTPACK 5.1 computes normalized fast Fourier transforms of real and complex
sequences, plus their cosine and sine variants and quarter-wave forms, in one
and two dimensions. The module owns the whole vendored tree under
`code/fftpack/` (113 Fortran 77 files, 6,961 lines). All 8 rows of
`tests.json` (every official test program in `test/`) became checks: the
complex and real 1D and 2D FFTs (`tcfft1`, `trfft1`, `tcfft2`, `trfft2`), the
cosine transform and its quarter-wave form (`tcost1`, `tcosq1`), and the sine
transform and its quarter-wave form (`tsint1`, `tsinq1`). Nothing was left
out (`tests.json` has no `suitable: false` rows). Every official test draws
its input vector from `RANDOM_SEED()` with no arguments followed by
`RANDOM_NUMBER(...)`, which is not reproducible run to run; each check's
`driver.f` is a copy of the corresponding `code/fftpack/test/<name>.f` with
that call replaced by a read of a fixed input vector under `ic/`, generated
once at authoring time with a seeded `numpy.random.default_rng` (not
FFTPACK's own RNG). Both round-trip directions of the original test (forward
routine first vs. backward routine first) are kept, run on the SAME fixed
input instead of the upstream test's two independent random draws (the
second of which its own unseeded `RANDOM_SEED()` cannot reproduce either).

## Build

`run.sh` builds `libfftpack.a` from the pinned source with
`gfortran -fdefault-real-8 -O2 -std=legacy` (the flags that produced the
shipped `output/darwin.dp` reference), the same command for every check since
the library does not depend on which test drives it. Within one run, a
flag-keyed build cache under `$TMPDIR/sab-fftpack-buildcache/<sha256(flags)>`
(nominal `-O2` and altbuild `-O0` get separate cache entries) means only the
first check of a solve actually compiles the library; every later check of
the same solve reuses it. Measured on the worker (run1, x86_64 debian
trixie): `tcfft1` (the first check `test.sh` runs alphabetically) built in
12.0 s, every other check 0.0 s (cache hit). `SAB_CPUS` controls the build's
`make -j` parallelism; the library build and the driver run together take
well under 1 s per check once cached. This is a plain per-check cache
(`mkdir`/`mv -T`/atomic publish inside one container), not the cross-container
`SAB_BUILD_CACHE_ROOT`/source-fingerprint machinery `swmf-batsrus` uses,
because the whole library takes 1-2 s to build: a cache miss on every check
of a sharded, multi-container solve would still cost under 16 s total,
inconsequential against the 900 s suite budget.

## Tolerances

Every check grades three `invariants`: `roundtrip-max-error` (the max, over
both round-trip call orders, of `|output - input|` on the fixed input; atol
1e-9, rtol 0) and `transformed-max-magnitude` / `transformed-mean-magnitude`
(the max and mean magnitude of the array the first forward transform of the
fixed input produces; atol 1e-12, rtol 1e-9 each). The round-trip error's
analytic value is exactly zero (it is a round-off residual of the forward and
inverse stages, references/pitfalls/residual-below-one-ulp.md), so its bound
is set as an OK/FAILED-style ceiling well above FFTPACK's own round-off scale
for these transform sizes (about N times machine epsilon, ~2.2e-13 for
N=1000) rather than tuned to the measured spread. The magnitude invariants
are set from the fact that two correct implementations of the SAME transform
on the SAME fixed input agree to double-precision round-off under any
legitimate reordering (compiler, architecture, radix decomposition), so a
1e-9 relative bound is generous headroom above that agreement while still
failing an implementation that computes the wrong transform (order-1
relative error) by six to seven orders of magnitude.

Calibration (worker, x86_64 debian trixie gfortran, selfcheck run1,
`comment/pipeline/self-validation.json`): reward 1.0 on all 8 checks, nominal
vs. variant (input perturbed by two ulps in every element). Worst
`bound_fraction` observed: `tcost1` roundtrip-max-error at 2.60e-05 (headroom
about 38,500x) and `tsint1` roundtrip-max-error at 9.00e-06 (headroom about
111,000x); every magnitude invariant's `bound_fraction` sits at 1e-7 to 1e-5
(headroom 10^5 to 10^7). `trfft1` and `tsinq1`'s roundtrip-max-error did not
move under the variant (candidate equalled reference exactly at this
precision), which is why the transformed-magnitude invariants are graded
alongside it: their magnitude invariants did move (abs_error 1.1e-16 to
2.2e-16), so every check has at least one invariant that the variant
perturbation demonstrably moves. No policy or tolerance changed after
calibration; the numbers above matched the pre-selfcheck estimate closely
enough that no bound needed loosening or tightening.

**Altbuild.** All 8 checks declare `run.sh altbuild` (gfortran `-O0` instead
of `-O2`, same `-fdefault-real-8 -std=legacy`): FFTPACK's default build is
already IEEE (no fast-math), so this is the same-compiler fallback per the
family ruling, not a strict-IEEE flip. Measured on the worker: **0 of 8
checks moved** (`distance` 0.0, `identical` true on every check) — the
altbuild is uninformative for this codebase family on x86_64, reported here
plainly rather than treated as a floor. This is itself a measured instance of
`references/pitfalls/altbuild-floors-are-host-specific.md`: the same rebuild
measured natively on macOS arm64 gfortran 15.2 during authoring was NOT
bit-identical (transformed-magnitude agreement 2e-17 to 8e-16 relative,
still far under every bound). The variant remains the only calibration
evidence behind these bounds; the graded record is the x86_64 worker's.

## Blind spots

- The multiple-sequence (`M > 1`) forms of these routines
  (`CFFTMF/CFFTMB`, `RFFTMF/RFFTMB`, `COSTMF/COSTMB`, `SINTMF/SINTMB`,
  `COSQMF/COSQMB`, `SINQMF/SINQMB`) are exercised internally by the 2D
  drivers (`CFFT2`/`RFFT2` call the M-form routines column-wise) but have no
  standalone official test in `code/fftpack/test/`, so there is no check that
  drives them directly at `LOT > 1` with a stride other than what the 2D
  tests impose; this was flagged as an open question in the source PR
  (#759) and is unchanged here since the survey found no additional
  official test or example to cover it.
- Every check's fixed input is drawn from a seeded `numpy.random.default_rng`
  standing in for FFTPACK's own unseeded generator; this input is not
  claimed to reproduce anything upstream ever printed (upstream's own
  transcripts are not reproducible either, per `runs.json`), only to make the
  check itself reproducible.
- The altbuild axis measured no floor on this host (see above); a second
  architecture or a genuinely different compiler was not tried, so the only
  independent-build evidence behind the magnitude bounds' headroom is the
  authoring-time macOS arm64 measurement noted above, not a second worker
  selfcheck.
