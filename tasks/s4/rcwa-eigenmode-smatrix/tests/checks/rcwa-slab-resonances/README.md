# rcwa-slab-resonances

## What it runs

Upstream test: `code/s4/examples/0d/fabry_perot/slab_resonances.lua`

0d/fabry_perot/slab_resonances.lua at upstream settings; all 9000 numeric rows graded, 45000 values; the 100 blank separator lines the deck prints between fi sweeps carry no value.

The check builds S4 from the pinned source inside its own scratch copy, runs
the deck in `ic/<nominal|variant>/input.lua` through the Lua front end, and
grades every number the deck prints to stdout as `output.txt`.

**Observable.** the etalon resonance structure over a dense two-parameter sweep, 45000 graded values.

**Cost knob.** `SAB_NUMG` sets NumBasis. The layer eigenproblem is
`2*NumBasis` square and the eigendecomposition is `O(NumBasis^3)`, so this is
the single dial that scales the check. `run.sh --help` prints it.
**Resource knob.** `SAB_THREADS` (default 1, the declared per-check cpus). S4 as built here is serial, so it changes nothing at the graded value; `run.sh --help` lists it.
Default vs upstream: upstream

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

ic/variant perturbs the dielectric permittivity of the slab by two units of the fourteenth significant digit (relative 2e-14): 'AddMaterial("Dielectric", {4,0})' becomes 'AddMaterial("Dielectric", {4.0000000000000799,0})'. Two binary64 ulps (~1e-16 relative) was tried first and is NOT sufficient here: S4 prints through Lua's %.14g, so a perturbation below half a unit of the last printed digit rounds away and leaves the output byte-identical. At this size the inputs differ byte-wise and the graded output moves by 9.920e-06 absolute, which is the generic numerical-noise calibration evidence for the bound below.

Note the decks are stored with LF line endings; the upstream examples are
CRLF. Lua is indifferent, and it makes the one-line variant diff readable.

## Pass policy

`pointwise`: every graded value must satisfy
`|candidate - reference| <= 0.001` (rtol 0). S4 prints through Lua's
`%.14g` as whitespace-separated columns whose width is not constant, so
`validate.py` collects every float token in file order and compares
positionally (standard library only, no numpy); a candidate emitting a different count fails rather than being
truncated.

Measured spread: `9.920e-06`, the nominal-versus-variant spread self-validation
recorded on the x86_64 worker; the bound is 101x above it.

Measured floor: `0.000e+00`. From skill 5.8.0 the CLI writes `evidence.floor` itself,
as the distance between `run.sh nominal` and `run.sh altbuild` (the same pinned source
compiled by clang instead of gcc); on this check the two builds produced **byte-identical**
graded output, so the floor is zero and the achievability argument rests on the spread
above and on the author's two native measurements, kept beside it as
`native_build_spread` (`2.998e-08`), `native_backend_spread`
(`2.998e-08`, the `-DHAVE_BLAS -DHAVE_LAPACK` rebuild) and
`native_cross_platform_spread` (`1.000e-10`,
linux/amd64 under emulation against native linux/arm64):

Two independent axes were measured, both on the pinned source, and the number kept here as native_build_spread is the larger.
(1) Two legitimate builds, same machine: the default configuration, which keeps the in-tree RNP linear algebra (the reference eigensolver S4/RNP/Eigensystems.cpp for an anisotropic layer, and RNP::LinearSolve/RNP::TBLAS for the S-matrix recursion in every case), against the identical source rebuilt with -DHAVE_BLAS -DHAVE_LAPACK, which sends the eigenproblem to LAPACK zgeev and the recursion's solves and BLAS calls to the system libraries. Command: `make build/S4 CFLAGS="-O2 -fPIC -Wall -Wno-error=int-conversion" CXXFLAGS="-O2 -fPIC -Wall -std=c++11" CPPFLAGS="-IS4 -IS4/RNP -IS4/kiss_fft"` with and without `-DHAVE_BLAS -DHAVE_LAPACK` appended to CPPFLAGS. Measured here: 2.998e-08.
(2) Two architectures, same Dockerfile: the oracle image built and run for linux/arm64 natively and for linux/amd64 under emulation, both from the same pinned tree with the same flags and the same pinned Debian base image, then a positional comparison of every float token of ic/nominal's output. This is the cross-implementation number, since a different instruction set brings a different BLAS kernel, different vectorisation and a different summation order. Measured here: 1.000e-10 over the graded values.
The bound of 0.001 sits 9995560x above the cross-platform difference.

## Warrant

Physical. The etalon resonance structure over a dense two-parameter sweep, 45000 graded values is the direct output of the uniform-layer mode construction and the S-matrix recursion this module owns (S4/rcwa.cpp). A port that drops the evanescent branch, mishandles the sign or the branch cut of a decaying mode (S4/rcwa.cpp:451-477), or accumulates the layer product in the wrong order moves these values enormously here, and unlike its siblings that claim was measured directly rather than argued: perturbing the permittivity by 1e-10 relative - far smaller than any real implementation fault, and four orders below single precision - already moves the output by 4.97e-2, fifty times this check's bound. At 1e-8 relative it moves by 4.97, at 1e-6 by 4.96e2. So although 1e-3 is five orders looser than the other checks here, this is the most discriminating check in the module, not the least: the same ill-conditioning that forces the loose bound also amplifies any genuine error far past it. Achievable. Two legitimate measurements bound the numerical noise here, both on the pinned source and both recorded under evidence. Self-validation's nominal-versus-variant spread is 9.920e-06 on this check, the same on the author's arm64 Mac and on the x86_64 worker this record comes from, and the same oracle image built and run for linux/amd64 under emulation differs from the native linux/arm64 reference by 1.000e-10. The bound sits 101x above the larger of them, which is headroom for a different BLAS, a different instruction set or a different summation order on another platform, while staying far below any physically wrong answer. The mechanism is the S-matrix recursion rather than the eigensolve: every layer of this deck is unpatterned with a scalar permittivity, so S4.cpp:1893 takes the closed-form SolveLayerEigensystem_uniform (S4/rcwa.cpp:422) and S4/RNP/Eigensystems.cpp is not reached at all. What two builds can differ in here is the linear algebra of the recursion itself - the RNP::LinearSolve calls at S4/rcwa.cpp:1089-1127 and the RNP::TBLAS operations around them - which is also what -DHAVE_BLAS -DHAVE_LAPACK replaces; rebuilding that way moves this check by 2.998e-08. The floor recorded under evidence.floor is a third, CLI-owned measurement: the distance to run.sh altbuild, the same source compiled by clang instead of gcc, and on this check the two builds produced byte-identical graded output, so it is 0. The widest graded surface in the module and by far the most sensitive: a two-ulp-scale perturbation of the permittivity moves the output by 1e-5 absolute, and the two legitimate eigensolver backends differ by 3e-8, both far above every other check here. That is genuine ill-conditioning near sharp resonances, where a tiny shift in a pole position produces a large change in the sampled value, and it is why this check's bound is five orders of magnitude looser than its siblings. Graded values reach 3.8e5, so 1e-3 absolute is about 3e-9 relative at the top of the range.
