# mhd-shwave

Upstream test: `code/athena/tst/regression/scripts/tests/shearingbox/mhd_shwave.py`. Policy: `pointwise`.

## The test

`run.sh` builds the pinned Athena++ once with the upstream test's configuration (`configure.py -b -fft --prob=jgg --coord=cartesian --eos=isothermal --flux=hlld --nghost=4`) and runs the Johnson-Guan-Gammie MHD shearing wave on a 32 x 16 x 16 shearing box twice, once with orbital advection switched off and once with the second-order orbital advection operator, both with the upstream settings (CFL 0.3, third-order reconstruction, t = 3.0, amplitude 1e-6, beta 20). This is the shearing-periodic boundary path (`src/bvals/`), the FFT remap and the orbital advection operator in `src/orbital_advection/`; the two runs must reproduce the same physical wave through very different code paths.
The knobs are `SAB_RES_SCALE` (every mesh dimension), `SAB_TLIM_SCALE` (end time), `SAB_SERIES`
(which decks run) and `SAB_MAKE_JOBS` (build parallelism); the defaults are the graded values,
50 s declared on 8 cores.

## The two initial conditions

`ic/nominal` holds complete decks with the upstream test's settings written in (problem id, mesh,
end time, one full-precision tab output at the end time, and a header comment naming the Riemann
solver the deck is built with). `ic/variant` is the same set with the background density `d0` of both decks multiplied by
(1 + 1e-15), a few ulps in double precision: the physics is unchanged, but the round-off path of the
whole run differs, so the variant produces a different file whose distance from the nominal one
measures what two legitimate runs of the same physics can differ by.

`run.sh altbuild` runs `ic/nominal` on the same pinned source built with `configure.py -debug`, Athena++'s own `-O0 -g` build, with the same compiler and every other configure switch unchanged; grading never uses it, and self-validation measures the check's floor between two legitimate builds from it.

## The pass policy

The graded observable is the conserved state of every cell of the two shearing-wave runs, one without and one with second-order orbital advection, at the upstream end time t = 3.0, compared value by value under an absolute bound of 1e-08 with no relative term. Physical: the background is density 1, azimuthal momentum 0.36 from the shear and a field of (0.1, 0.25, 0), and the shearing wave itself appears in the radial momentum, whose largest final value is 5.3e-06, so the bound is 1.9e-03 of the wave and catches any fault that changes it by more than about two tenths of a per cent; upstream accepts one per cent on the amplitude history. A wrong shearing-periodic offset or a wrong orbital-advection remap does worse still, because it damages the transport of the sheared background as well, which is four orders larger. Running the same physical wave with the orbital advection operator on and off means a fault in either path is caught. Achievable: the orbital advection remap applies a van Leer limiter with a hard sign test, if (du2 <= 0.0) dum = 0.0 (src/orbital_advection/orbital_remapping.cpp, line 49), and splits the shift into an integer cell count by truncation, int offset = static_cast<int>(olen/dx) (src/orbital_advection/set_orbital_advection.cpp, line 212), so a round-off difference near an extremum or near an integer shift changes the remap discretely, which is why this check has the second widest spread of the suite; the -O3 and -O2 builds of the pinned source are bit-identical on every graded file (floor 0), while the 1e-15 perturbation of the variant grows to a largest absolute difference of 4.60e-11 at the end time, and the bound of 1e-08 sits 218 times above the largest legitimate spread measured. Absolute rather than relative because the perturbation quantities oscillate through zero.

## Evidence

The two-build floor and the variant preview were measured on the x86 worker in the survey image by
building the pinned source twice, once with the check's own configure line and once with the
optimisation level lowered to -O2, running every deck of `ic/nominal` with both and every deck of
`ic/variant` with the first, and taking the largest absolute difference over all graded values
(`~/.sciaccel_pipeline/athena/survey/floor/floor_mhd.sh`). The numbers are in `rubric.json`
(`evidence`). The in-container nominal-versus-variant spread and the runtime on the declared cores
are written by `sab.py task selfcheck` into `rubric.json` (`evidence.self_validation_spread`) and
`comment/pipeline/self-validation.json`. Nothing here describes the reference outputs.
