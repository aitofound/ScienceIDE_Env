# rcwa-gyrotropic-halfspace

## What it runs

Upstream test: `code/s4/examples/magneto/halfspace.lua`

magneto/halfspace.lua with NumBasis raised from upstream's 1 to 801, so the layer eigenproblem is 1602x1602 dense complex. All 1001 output rows are graded.

The check builds S4 from the pinned source inside its own scratch copy, runs
the deck in `ic/<nominal|variant>/input.lua` through the Lua front end, and
grades every number the deck prints to stdout as `output.txt`.

**Observable.** the reflected and transmitted Poynting flux and the Kerr/Faraday rotation angles of a gyrotropic half-space, swept over 1001 incidence conditions (5005 graded values).

**Cost knob.** `SAB_NUMG` sets NumBasis. The layer eigenproblem is
`2*NumBasis` square and the eigendecomposition is `O(NumBasis^3)`, so this is
the single dial that scales the check. `run.sh --help` prints it.
Default vs upstream: NumBasis raised from upstream's 1 to 801; SAB_NUMG=1 restores the upstream setting. Every other setting is upstream's.

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

ic/variant perturbs eps, the diagonal permittivity of the gyrotropic medium by two units of the fourteenth significant digit (relative 2e-14): 'eps  = 10' becomes 'eps  = 10.000000000000199'. Two binary64 ulps (~1e-16 relative) was tried first and is NOT sufficient here: S4 prints through Lua's %.14g, so a perturbation below half a unit of the last printed digit rounds away and leaves the output byte-identical. At this size the inputs differ byte-wise and the graded output moves by 3.670e-13 absolute, which is the generic numerical-noise calibration evidence for the bound below.

Note the decks are stored with LF line endings; the upstream examples are
CRLF. Lua is indifferent, and it makes the one-line variant diff readable.

## Pass policy

`pointwise`: every graded value must satisfy
`|candidate - reference| <= 1e-10` (rtol 0). S4 prints through Lua's
`%.14g` as whitespace-separated columns whose width is not constant, so
`validate.py` collects every float token in file order and compares
positionally (standard library only, no numpy); a candidate emitting a different count fails rather than being
truncated.

Measured floor: `0.000e+00`. Two legitimate builds of the pinned source were compared on this check's nominal deck: the default Linux/Darwin configuration, which uses the in-tree reference eigensolver S4/RNP/Eigensystems.cpp, and the same source rebuilt with -DHAVE_BLAS -DHAVE_LAPACK, which dispatches the same eigenproblem to LAPACK zgeev. Command: `make build/S4 CFLAGS="-O2 -fPIC -Wall -Wno-error=int-conversion" CXXFLAGS="-O2 -fPIC -Wall -std=c++11" CPPFLAGS="-IS4 -IS4/RNP -IS4/kiss_fft"` with and without `-DHAVE_BLAS -DHAVE_LAPACK` appended to CPPFLAGS, then `build/S4 input.lua` on ic/nominal/input.lua and a positional comparison of every float token. Measured difference on this check: 0.000e+00 absolute over 5005 graded values (largest |value| 10). The two backends agree bit-for-bit here, so the floor is set by the variant spread rather than by the backend difference.

## Warrant

Physical. The reflected and transmitted poynting flux and the kerr/faraday rotation angles of a gyrotropic half-space, swept over 1001 incidence conditions (5005 graded values) is the direct output of the per-layer eigendecomposition and the S-matrix recursion this module owns. A port that drops the evanescent branch, mishandles the sign of a decaying mode, substitutes a cheaper non-Hermitian eigensolver without restoring the eigenvector normalisation, or accumulates the layer product in the wrong order moves these values by order 1e-3 or more - a wrong diffraction efficiency is wrong in the third decimal, not the twelfth - so a bound of 1e-10 separates a correct port from every one of those faults by many orders of magnitude. Achievable. The measured floor between two legitimate builds of this very source (in-tree RNP eigensolver versus LAPACK zgeev) is 0.000e+00 on this check, and a two-units-of-last-printed-digit perturbation of the input moves the output by 3.670e-13. The bound sits 272x above the larger of the two, which is headroom for a different BLAS, a different instruction set or a different summation order on another platform, while staying far below any physically wrong answer. The source mechanism setting the floor is the dense complex eigendecomposition in S4/RNP/Eigensystems.cpp, whose Householder reduction and QR iteration sum in an order that a vendor LAPACK does not reproduce exactly. This is the module's expensive check. Because the layer is uniform, the physics cannot depend on the basis size, and the graded output at NumBasis 801 is BYTE-IDENTICAL to the output at upstream's NumBasis 1 (verified at 1, 121, 401 and 801). Raising the knob therefore multiplies the arithmetic by roughly 200x - 0.10 s to about 20-30 s - without changing a single expected digit. The correct answer is fixed by the cheap analytic configuration, not by this pinned build, which is the only anchor of that kind in this codebase.
