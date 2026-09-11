# dsm-1d-solver: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

The module is the Direct Solution Method engine of SEM_DSM_hybrid: dsmti
(`src/DSM/src/DSM_Solver`) integrates the radial equations of motion of a
spherically symmetric Earth frequency by frequency and writes complex
displacement spectra, and spectotime (`src/DSM/src/DSM_FreqToTimeSac`)
low-pass filters those spectra, transforms them back to time and writes SAC
seismograms. Both are graded. The repository's three other modules were
deliberately left out of this task: `src/SPECFEM3D/` is a large modified
upstream tree with its own provenance and licence, and `src/InjectedWaves/`
and `src/Coupling/` only make sense downstream of a full 3-D SEM run, which
needs hundreds of MPI ranks and hours of wall time -- far outside what a
reward suite can run per iteration. The DSM module is the one that is both
self-contained and on the critical path: it produces the incident wavefield
and the 1-D reference waveform the rest of the workflow consumes, and its
frequency loop is where the time goes.

## Tolerances

Every number below comes from native runs on the authoring host (4 ranks,
64 frequencies, ~380 s per run), not from estimates. The build floor was
measured by building the same sources twice -- nominal `-O` and altbuild
`-O2 -mfma` (SAC side keeping `-fallow-argument-mismatch -std=legacy`) -- and
comparing the two outputs: 4.44e-16 absolute on the SAC waveforms and
1.43e-16 absolute (8.29e-9 relative) on the frequency-domain spectra. Plain
`-O0` was tried first and rejected as a vacuous altbuild: without
`-ffast-math` gfortran may not reassociate, so `-O` and `-O0` produce
bit-identical output (max_abs_diff exactly 0) even though `-O0` runs 1.4x
slower.

The tolerances are tied to a physical equivalence threshold and then
deliberately tightened one decade inside it. Two seismograms are treated as
the same answer when they agree to 0.1 percent of peak amplitude; the bounds
actually shipped are one part in 1e4 of peak, which is 3e-12 absolute on the
SAC waveforms (peak 3.17e-8) and 3.7e-11 absolute on the frequency-domain
spectra (peak 3.70e-7). The extra decade is a second, separate choice: a bound
placed exactly at 0.1 percent leaves the implementation faults argued in the
warrants only a 1x-10x margin, whereas at one part in 1e4 they clear it by ten
to three hundred times while the measured legitimate input noise still
occupies about a tenth of the bound.
That criterion is a statement about what "the same answer" means for this
module: two implementations agree when no reader of the seismogram could tell
them apart. It is an authoring choice rather than a number taken from the
literature or from the module's own documentation -- the one quantity in this
task that was chosen rather than measured, and the one a reviewer should push
back on first.

The self-validation variant plays the opposite role from a defect. The
pipeline compares nominal against variant and requires them to agree *within*
the tolerance, so the variant has to represent legitimate input uncertainty
that any correct implementation is expected to absorb; detection of real
defects is argued in each rubric's warrant, not carried by the variant. Both
checks therefore perturb the two receiver distances by +1.0e-5 deg -- about
1.1 m of station location error, well inside real network uncertainty --
keeping the solid and fluid receiver lists consistent. That perturbation is
sized to land the spread about a tenth of the way to each tolerance, and the
spread was measured on bare metal rather than extrapolated: 3.268497e-13 on
the waveforms and 3.827653e-12 on the spectra, recorded in
work/reports/calibrate_1e5.log. Both sit far enough above the build floor to
be a real physical signal rather than compiler noise (about 7e2x the floor on
the waveforms, 2.7e4x on the spectra), and far enough below the tolerance that
a correct implementation passes. The scaffold
default of 2 ulp was rejected for the frequency-domain check for the mirror
reason: at 3.7e-13 relative it sits four orders of magnitude *below* the
compile floor, so it would measure compiler noise instead of input
sensitivity. The linear anchors for sizing the perturbation come from the
calibration run (`work/reports/calibrate_phys.log`): 3.27e-11 on the waveforms
for 1e-3 deg, 7.66e-11 on the spectra for 2e-4 deg. The spread actually
measured for the shipped variant is written back into each `rubric.json` under
`evidence.self_validation_spread` by the self-check. Per-check detail lives in
each check's README.md and rubric.json.

## Blind spots

Two checks is thin by the pipeline's own standard, and the gap was put to the
human explicitly. The reason is that this module ships one example that runs
inside a reward-suite budget; the repository's other examples submit 256-512
MPI ranks through Slurm. Within that example the two checks were chosen to be
complementary: the waveform check grades what a user reads (real*4 SAC, after
filtering and the inverse transform), the spectra check grades the binary64
stream before any post-processing, where detail the f32 export quantises away
is still visible. Not covered: the solver's other output directories
(velocity, fluid displacement, stress, pressure, potential, coefficients) are
written but not graded; the frequency count is reduced from 2048 to 64 to fit
the budget, so a defect appearing only above 0.027 Hz would pass; the
transverse component is identically zero for this explosion source and is
excluded from grading; and single-rank behaviour is untested, since every run
uses four ranks. All were accepted as the price of a suite that runs in about
thirteen minutes and rebuilds from source every iteration.
