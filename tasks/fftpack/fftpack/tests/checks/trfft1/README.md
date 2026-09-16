# trfft1

Upstream test: `code/fftpack/test/trfft1.f`. Policy: `pointwise`.

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
max errors, one per line) and `transformed.txt` (every element of the array
the first forward transform of the first round trip produces, one signed value
per line; this is the field itself, not a magnitude or a reduction of it).
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

Human ruling 2026-09-16 on PR #780 ("yeah switch to pointwise i think"): the
copied driver already dumps the transformed array, and the prior `invariants`
policy graded only its max and mean magnitude, which a wrong sign, a swapped
index or a wrong twiddle factor can leave unchanged; grading the array
pointwise, element by element, catches those faults. `transformed` compares every
element of the transformed array (`real one-dimensional FFT`, routines above, N=1000)
under `|candidate - reference| <= 1e-09 + 1e-09*|reference|`, read from
`transformed.txt` printed at 17 significant digits (`1PE24.16`, scale factor
`1P`) so the stream itself never floors the bound
(`references/pitfalls/output-precision-floors-the-bound.md`). `rtol` is
traced to the round-off of a double-precision real one-dimensional FFT of N=1000: the
mechanism floor is on the order of N times machine epsilon, about 2.22e-13
here (eps=2.22e-16), and the bound sits roughly 4,504x above it, real
headroom for reordering across compilers, architectures and radix
decomposition while still separating a fault: a wrong sign, a transposed
axis or a wrong twiddle factor moves an element by an order-1 relative
amount, six to nine orders of magnitude over the bound. `atol` is derived
from the input's own scale, not from the measured spread: `ic/nominal` is
drawn from `numpy`'s `default_rng` uniform on `[0, 1)`, so `max|input| <= 1`,
and `atol = rtol * max|input| = 1e-09` ties the absolute floor for
near-zero transformed elements to the same relative scale `rtol` already
uses at the input's own magnitude. `roundtrip-max-error` is kept as a
secondary invariant at its unchanged bound (`atol` 1e-09, `rtol` 0): its
analytic value is exactly zero (see
`references/pitfalls/residual-below-one-ulp.md`: a residual whose true value
is zero carries no information about a port beyond its OK/FAILED verdict
against a wide bound), so it stays a ceiling on the round-off remainder of
the forward/inverse stages rather than a pointwise stream.

## Evidence

Selfcheck on the worker (x86_64 debian trixie, gfortran, 2026-09-16, run3): nominal vs. variant (every element of the fixed input perturbed by two ulps at binary64), graded pointwise per element. transformed.txt[col 0]: max |err| 1.110e-16, 1000 values, bound_fraction 8.525e-08 (headroom 11,729,595x) roundtrip-max-error (secondary invariant): reference 6.661e-16, candidate 6.661e-16, bound_fraction 0.000e+00. Worst check-level bound_fraction 8.525e-08 (headroom 11,729,595x); nominal and variant outputs are not byte-identical (`identical`: false). Altbuild (`run.sh altbuild`, gfortran `-O0`): bit-identical to nominal (`distance` 0.0, `floor` 0.0) -- uninformative on this host, per the family ruling in `comment/README.md`. Full record: `comment/pipeline/self-validation.json`.
