# rcwa-slab-resonances

## What it runs

Upstream test: `code/s4/examples/0d/fabry_perot/slab_resonances.lua`

0d/fabry_perot/slab_resonances.lua at upstream settings; all 9100 output rows graded.

The check builds S4 from the pinned source inside its own scratch copy, runs
the deck in `ic/<nominal|variant>/input.lua` through the Lua front end, and
grades every number the deck prints to stdout as `output.txt`.

**Observable.** the etalon resonance structure over a dense two-parameter sweep, 45000 graded values.

**Cost knob.** `SAB_NUMG` sets NumBasis. The layer eigenproblem is
`2*NumBasis` square and the eigendecomposition is `O(NumBasis^3)`, so this is
the single dial that scales the check. `run.sh --help` prints it.
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
Linux and Darwin, and it keeps the `O(N^3)` eigendecomposition inside
`S4/RNP/Eigensystems.cpp`, a path this module owns, instead of delegating it
to a system LAPACK that a port could satisfy by swapping a library call.

## Initial conditions

`ic/nominal/input.lua` is the graded deck. `ic/variant/input.lua` differs only
in the perturbation described below; it exists so self-validation can measure
how much numerical noise this check carries, and is never the graded input.

ic/variant perturbs the dielectric permittivity of the slab by two units of the fourteenth significant digit (relative 2e-14): 'AddMaterial("Dielectric", {4,0})' becomes 'AddMaterial("Dielectric", {4.0000000000000799,0})'. Two binary64 ulps (~1e-16 relative) was tried first and is NOT sufficient here: S4 prints through Lua's %.14g, so a perturbation below half a unit of the last printed digit rounds away and leaves the output byte-identical. At this size the inputs differ byte-wise and the graded output moves by 1.011e-05 absolute, which is the generic numerical-noise calibration evidence for the bound below.

Note the decks are stored with LF line endings; the upstream examples are
CRLF. Lua is indifferent, and it makes the one-line variant diff readable.

## Pass policy

`pointwise`: every graded value must satisfy
`|candidate - reference| <= 0.001` (rtol 0). S4 prints through Lua's
`%.14g` as whitespace-separated columns whose width is not constant, so
`validate.py` collects every float token in file order and compares
positionally (standard library only, no numpy); a candidate emitting a different count fails rather than being
truncated.

Measured floor: `2.998e-08`. Two legitimate builds of the pinned source were compared on this check's nominal deck: the default Linux/Darwin configuration, which uses the in-tree reference eigensolver S4/RNP/Eigensystems.cpp, and the same source rebuilt with -DHAVE_BLAS -DHAVE_LAPACK, which dispatches the same eigenproblem to LAPACK zgeev. Command: `make build/S4 CFLAGS="-O2 -fPIC -Wall -Wno-error=int-conversion" CXXFLAGS="-O2 -fPIC -Wall -std=c++11" CPPFLAGS="-IS4 -IS4/RNP -IS4/kiss_fft"` with and without `-DHAVE_BLAS -DHAVE_LAPACK` appended to CPPFLAGS, then `build/S4 input.lua` on ic/nominal/input.lua and a positional comparison of every float token. Measured difference on this check: 2.998e-08 absolute over 45000 graded values (largest |value| 3.837e+05).

## Warrant

Physical. The etalon resonance structure over a dense two-parameter sweep, 45000 graded values is the direct output of the per-layer eigendecomposition and the S-matrix recursion this module owns. A port that drops the evanescent branch, mishandles the sign of a decaying mode, substitutes a cheaper non-Hermitian eigensolver without restoring the eigenvector normalisation, or accumulates the layer product in the wrong order moves these values by order 1e-3 or more - a wrong diffraction efficiency is wrong in the third decimal, not the twelfth - so a bound of 0.001 separates a correct port from every one of those faults by many orders of magnitude. Achievable. The measured floor between two legitimate builds of this very source (in-tree RNP eigensolver versus LAPACK zgeev) is 2.998e-08 on this check, and a two-units-of-last-printed-digit perturbation of the input moves the output by 1.011e-05. The bound sits 99x above the larger of the two, which is headroom for a different BLAS, a different instruction set or a different summation order on another platform, while staying far below any physically wrong answer. The source mechanism setting the floor is the dense complex eigendecomposition in S4/RNP/Eigensystems.cpp, whose Householder reduction and QR iteration sum in an order that a vendor LAPACK does not reproduce exactly. The widest graded surface in the module and by far the most sensitive: a two-ulp-scale perturbation of the permittivity moves the output by 1e-5 absolute, and the two legitimate eigensolver backends differ by 3e-8, both far above every other check here. That is genuine ill-conditioning near sharp resonances, where a tiny shift in a pole position produces a large change in the sampled value, and it is why this check's bound is five orders of magnitude looser than its siblings. Graded values reach 3.8e5, so 1e-3 absolute is about 3e-9 relative at the top of the range.
