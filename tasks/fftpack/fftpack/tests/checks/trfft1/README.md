# trfft1

Upstream test: `code/fftpack/test/trfft1.f`. Policy: `invariants`.

## The test

`run.sh` builds the FFTPACK 5.1 library from the pinned source at `/workspace/code`
(`gfortran -fdefault-real-8 -O2 -std=legacy`, the flags that produced the shipped
`output/darwin.dp` reference transcript), compiles `driver.f` against it, and runs
the driver. `driver.f` is a copy of `code/fftpack/test/trfft1.f` (real one-dimensional FFT, routines
`RFFT1I/RFFT1F/RFFT1B`) with one change: the upstream test draws its test vector from
`CALL RANDOM_SEED()` followed by `CALL RANDOM_NUMBER(...)`, an unseeded call that is
not reproducible between runs (see `~/.sciaccel_pipeline/fftpack/runs.json`); the
driver instead reads a length-1000 real vector (ic/*/input.txt) from the current directory, which `run.sh` copies
in from `ic/nominal/` or `ic/variant/`. Both round-trip directions the upstream
test exercises (call the forward routine first, then the backward routine; and the
reverse order) are kept, run on the SAME fixed input (the upstream test draws a
second random vector for its second round trip, which its own unseeded
`RANDOM_SEED()` cannot reproduce either; one fixed input exercises both call
orders identically). The driver writes `roundtrip_errors.txt` (the two round-trip
max errors, one per line) and `transformed.txt` (the magnitude of the array the
first forward transform of the first round trip produces, one value per line).
Knobs: `SAB_N` (or `SAB_L`/`SAB_M` for the 2D checks) is the transform size, graded
default the upstream value; `SAB_CPUS` is the make parallelism for the library
build. The whole run (library build plus driver) takes under 1 s on the declared
core; well under 300 s. frames: not applicable, one solve (a one-shot transform,
not a time-stepping code).

## The two initial conditions

`ic/nominal` is a fixed input vector (or, for the complex checks, a fixed pair of
real and imaginary vectors) drawn once at authoring time with a seeded
`numpy.random.default_rng`, standing in for the upstream test's unseeded random
draw. `ic/variant` perturbs every element of `ic/nominal` by two ulps at binary64
(`numpy.nextafter` applied twice); this is the check's only active
initial-condition input, so perturbing all of it is the minimal sufficient
perturbation. `run.sh altbuild` runs `ic/nominal` on the same pinned source built
with `gfortran -O0` instead of `-O2` (same `-fdefault-real-8 -std=legacy`); FFTPACK's
default build is already IEEE (no fast-math), so this is the same-compiler
fallback per the family ruling, not a strict-IEEE flip.

## The pass policy

Two invariants. `roundtrip-max-error` is the max, over both round-trip call
orders, of `|output - input|` on the fixed input; its analytic value is exactly
zero, so what is measured is the round-off remainder of the transform's forward
and inverse stages (see `references/pitfalls/residual-below-one-ulp.md`: a
residual whose true value is zero carries no information about a port beyond its
OK/FAILED verdict against a wide bound). The bound, 1e-09
absolute, sits about 4545x above FFTPACK's own
round-off scale for a double-precision length-1000 transform (roughly N times
machine epsilon, 2.2e-13), clearing legitimate reordering across compilers or
architectures while failing an implementation that does not actually invert the
transform (round-trip error of order the input itself, about 1, ten orders of
magnitude over the bound). `transformed-max-magnitude` and
`transformed-mean-magnitude` compare the aggregate size of the array the first
forward transform produces on the fixed input (`agreement`, `rtol`
1e-09, `atol`
1e-12): because the input is fixed, two
correct implementations of the same transform on the same input agree to
double-precision round-off under any legitimate reordering, while a wrong
normalization, a wrong axis, or a transform that never ran moves the magnitude by
an order-1 relative amount, far over the bound.

## Evidence

one nominal run and one variant run (input perturbed by two ulps in every element) compared on macOS arm64 native gfortran 15.2 during authoring: round-trip max error nominal 6.661e-16, transformed max magnitude changed by 1.110e-16 absolute (2.252e-16 relative) out of 4.929e-01. Reconfirmed on the worker's x86_64 debian trixie gfortran at selfcheck.
Self-validation (nominal vs. `altbuild`, the -O0 same-compiler fallback):
round-trip max error 6.661e-16 nominal vs 6.661e-16 altbuild; transformed-magnitude relative spread
2.816e-17, using 6.661e-07 of the
1e-09 bound. Both measurements were taken natively (no Docker) on
macOS arm64 gfortran 15.2 during authoring; the worker's x86_64 debian trixie
gfortran selfcheck reconfirms them under `comment/pipeline/self-validation.json`.
