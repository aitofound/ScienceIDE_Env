# rcwa-magneto-optic-table

## What it runs

Upstream test: `code/s4/examples/0d/Sakaguchi_OptComm_162_64_1999/table1m2g2.lua`

Sakaguchi table1m2g2.lua with NumBasis raised from upstream's 1 to 121. Each of the six rows builds a stack of 8n+4 layers (44 at n = 5, 84 at n = 10), so this grades the S-matrix cascade across up to 83 internal interfaces rather than a single one. All six rows graded.

The check builds S4 from the pinned source inside its own scratch copy, runs
the deck in `ic/<nominal|variant>/input.lua` through the Lua front end, and
grades every number the deck prints to stdout as `output.txt`.

**Observable.** the transmittance and Faraday rotation angle of six magneto-optical multilayer stacks (n = 5..10 periods), 18 graded values.

**Cost knob.** `SAB_NUMG` sets NumBasis. The layer eigenproblem is
`2*NumBasis` square and the eigendecomposition is `O(NumBasis^3)`, so this is
the single dial that scales the check. `run.sh --help` prints it.
**Resource knob.** `SAB_THREADS` (default 1, the declared per-check cpus). S4 as built here is serial, so it changes nothing at the graded value; `run.sh --help` lists it.
Default vs upstream: NumBasis raised from upstream's 1 to 121; SAB_NUMG=1 restores the upstream setting. Every other setting is upstream's.

## Build settings, and why they are pinned

`run.sh` passes `CFLAGS`/`CXXFLAGS` explicitly rather than letting
`Makefile.Linux` apply its own. Upstream's Linux flags include
`-march=native`, which is not reproducible across machines, and
`-fcx-limited-range`, which removes the NaN-rescue branch from complex
multiply and divide in a solver written entirely in `std::complex<double>`.
`-Wno-error=int-conversion` is required because `S4/main_lua.c:2207` passes an
`S4_LayerID` (an `int`) where `Simulation_GetAmplitudes` expects an
`S4_Layer *`; modern clang and gcc reject it outright. No check calls
`GetAmplitudes`.

`HAVE_LAPACK` is deliberately left undefined. That is upstream's default on
Linux and Darwin, and it keeps the `O(N^3)` eigendecomposition and the
S-matrix recursion's linear solves inside `S4/RNP/`, a path this module owns,
instead of delegating them to a system LAPACK that a port could satisfy by
swapping a library call.

**Alternative build.** `run.sh altbuild` (skill 5.8.0+) rebuilds exactly this
source, with exactly these flags and `HAVE_LAPACK` still undefined, using
`clang`/`clang++` instead of `gcc`/`g++` — the only difference between the two
builds is the compiler. Both images install `clang`. `run.sh --help` prints
the `altbuild:` line describing it.

## Initial conditions

`ic/nominal/input.lua` is the graded deck. `ic/variant/input.lua` differs only
in the perturbation described below; it exists so self-validation can measure
how much numerical noise this check carries, and is never the graded input.

`run.sh altbuild` is a third run, not a third initial condition: it runs
`ic/nominal` on the alternative build (clang instead of gcc, see above), and
self-validation grades it against the nominal run with this same `validate.py`
to measure the check's floor. It is not part of grading a candidate.

ic/variant perturbs the diagonal permittivity of the magneto-optic layer by two units of the fourteenth significant digit (relative 2e-14): '{4.75,0}, {0,0.00269}' becomes '{4.750000000000095,0}, {0,0.00269}'. Two binary64 ulps (~1e-16 relative) was tried first and is NOT sufficient here: S4 prints through Lua's %.14g, so a perturbation below half a unit of the last printed digit rounds away and leaves the output byte-identical. At this size the inputs differ byte-wise and the graded output moves by 5.100e-11 absolute on the x86_64 worker this record comes from (4.199e-11 on the author's arm64 Mac), which is the generic numerical-noise calibration evidence for the bound below.

Note the decks are stored with LF line endings; the upstream examples are
CRLF. Lua is indifferent, and it makes the one-line variant diff readable.

## Pass policy

`pointwise`: every graded value must satisfy
`|candidate - reference| <= 1e-08` (rtol 0). S4 prints through Lua's
`%.14g` as whitespace-separated columns whose width is not constant, so
`validate.py` collects every float token in file order and compares
positionally (standard library only, no numpy); a candidate emitting a different count fails rather than being
truncated.

Measured spread: `5.100e-11`, the nominal-versus-variant spread self-validation
recorded on the x86_64 worker (`4.199e-11` on the author's arm64 Mac); the bound is 196x above it.

Measured floor: `0.000e+00`. From skill 5.8.0 the CLI writes `evidence.floor` itself,
as the distance between `run.sh nominal` and `run.sh altbuild` (the same pinned source
compiled by clang instead of gcc); on this check the two builds produced **byte-identical**
graded output, so the floor is zero and the achievability argument rests on the spread
above and on the author's two native measurements, kept beside it as
`native_build_spread` (`7.006e-12`), `native_backend_spread`
(`5.002e-12`, the `-DHAVE_BLAS -DHAVE_LAPACK` rebuild) and
`native_cross_platform_spread` (`7.006e-12`,
linux/amd64 under emulation against native linux/arm64):

Two independent axes were measured, both on the pinned source, and the number kept here as native_build_spread is the larger.
(1) Two legitimate builds, same machine: the default configuration, which keeps the in-tree RNP linear algebra (the reference eigensolver S4/RNP/Eigensystems.cpp for an anisotropic layer, and RNP::LinearSolve/RNP::TBLAS for the S-matrix recursion in every case), against the identical source rebuilt with -DHAVE_BLAS -DHAVE_LAPACK, which sends the eigenproblem to LAPACK zgeev and the recursion's solves and BLAS calls to the system libraries. Command: `make build/S4 CFLAGS="-O2 -fPIC -Wall -Wno-error=int-conversion" CXXFLAGS="-O2 -fPIC -Wall -std=c++11" CPPFLAGS="-IS4 -IS4/RNP -IS4/kiss_fft"` with and without `-DHAVE_BLAS -DHAVE_LAPACK` appended to CPPFLAGS. Measured here: 5.002e-12.
(2) Two architectures, same Dockerfile: the oracle image built and run for linux/arm64 natively and for linux/amd64 under emulation, both from the same pinned tree with the same flags and the same pinned Debian base image, then a positional comparison of every float token of ic/nominal's output. This is the cross-implementation number, since a different instruction set brings a different BLAS kernel, different vectorisation and a different summation order. Measured here: 7.006e-12 over the graded values.
The bound of 1e-08 sits 1427x above the cross-platform difference.

## Warrant

Physical. The transmittance and faraday rotation angle of six magneto-optical multilayer stacks (n = 5..10 periods), 18 graded values is the direct output of the per-layer eigendecomposition and the S-matrix recursion this module owns. A port that drops the evanescent branch, mishandles the sign of a decaying mode, substitutes a cheaper non-Hermitian eigensolver without restoring the eigenvector normalisation, or accumulates the layer product in the wrong order moves these values by order 1e-3 or more - a wrong diffraction efficiency is wrong in the third decimal, not the eighth - so a bound of 1e-08 separates a correct port from every one of those faults by many orders of magnitude. Achievable. Two legitimate measurements bound the numerical noise here, both on the pinned source and both recorded under evidence. Self-validation's nominal-versus-variant spread is 5.100e-11 on this check on the x86_64 worker this record comes from (4.199e-11 on the author's arm64 Mac), and the same oracle image built and run for linux/amd64 under emulation differs from the native linux/arm64 reference by 7.006e-12. The bound sits 196x above the larger of them, which is headroom for a different BLAS, a different instruction set or a different summation order on another platform, while staying far below any physically wrong answer. The mechanism is the dense complex eigendecomposition in S4/RNP/Eigensystems.cpp: this deck's medium is a 3x3 permittivity tensor, so S4.cpp:1904 takes the full SolveLayerEigensystem path (S4/rcwa.cpp:684), whose Householder reduction and QR iteration sum in an order that a vendor LAPACK does not reproduce exactly; rebuilding with -DHAVE_BLAS -DHAVE_LAPACK, which sends the same eigenproblem to zgeev, moves this check by 5.002e-12. The floor recorded under evidence.floor is a third, CLI-owned measurement: the distance to run.sh altbuild, the same source compiled by clang instead of gcc, and on this check the two builds produced byte-identical graded output, so it is 0. The second expensive check, and the one that exercises S-matrix propagation at depth rather than the eigensolve alone: 18 s at NumBasis 121. As with the half-space the layers are uniform, so the result is basis-independent in exact arithmetic; here it is not byte-identical across basis sizes but agrees to 1e-12 absolute on values up to 77, which is round-off accumulated over ~90 layers and is itself the floor evidence below.
