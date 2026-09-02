# epoch-particle-kinetic-core: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

This is the particle half of EPOCH's PIC cycle, cut from the field solve because
the two are separable schemes and from the physics packages because those are
per-particle stochastic kernels with their own tables. It owns
`epoch{1,2,3}d/src/particles.F90` (the field gather through the compile-time
shape function, the relativistic Boris momentum update at lines 341-387, the
position advance, and the charge-conserving current deposition at 440-510, whose
running prefix sum `jxh = jxh - fjx*wx` at line 500 is what makes the scheme
exactly charge conserving), `src/housekeeping/shape_functions.F90` and the weight
kernels under `src/include/{triangle,tophat,bspline3}/`,
`src/housekeeping/current_smooth.F90`, `src/deck/deck_species_block.F90`, and
`epoch1d/src/user_interaction/deltaf_loader.F90`. Seven of the nine survey rows
became checks, one per row, with no merging: the two official pytest decks
(`landau-1d`, `twostream-1d`), the one-, two- and three-dimensional
current-smoothing example decks, the delta-f two-stream deck, and the power-law
loader deck. The two rows the survey already marked unsuitable, `power-law-2d`
and `power-law-3d`, are duplicates of the one-dimensional loader in a
dimension-independent code path and were dropped; `comment/pipeline/test-survey.json`
keeps the record of what was considered. Seven checks means seven builds of the
source, about 50 seconds each, which is most of the declared 640 s suite time;
the alternative, merging the four decks that share the plain `epoch1d` binary
into one check, was rejected because it would have coupled four independent
observables into one reward bit.

## Tolerances

Every bound in this leaf is a hypothesis measured natively on the authoring
machine (arm64 macOS, gfortran 15.2, OpenMPI 5) and is written for the human to
finalize after the calibration selfcheck. The floor was measured by building the
pinned source twice with legitimate flags -- the stock `FFLAGS = -O3 -g -std=f2003`
of each dimension's Makefile line 72, and a second copy with that line changed to
`-O2` -- and running `ic/nominal` of every check with both binaries at the graded
window and rank layout. The two builds produce bit-identical output for every
graded array of every check, with one exception: `Derived/Average_Particle_Energy`
in the delta-f check differs by 7.889e-31, 3.4e-16 relative, because that
diagnostic is a sum over particles that the two optimisation levels group
differently. So the floor is essentially zero and the bounds are set instead from
the variant preview, the -O3 binary run on `ic/variant` against `ic/nominal`. The
mechanism that will lift a real port off that zero, and the reason a bound well
above the measured spread is right, is the order of the floating-point sums in
the deposition: `particles.F90:504-506` accumulate the three current components
cell by cell over each species' linked list in traversal order, the list order
being fixed by the append-at-tail of `housekeeping/partlist.F90:378-381` and by
the order in which MPI migration delivers particles; the ghost-cell currents are
then summed pairwise between neighbouring ranks at `boundary.F90:423` and `:431`
rather than by a global reduction; and `io/calc_df.F90:712` deposits the number
density the same way. Any port that reorders those sums differs in the last bits
of each cell sum and then amplifies exactly as the variant does.

The bound is expressed uniformly as `rtol` 1e-08 with a per-array `atol` of 1e-08
of that array's own largest magnitude over the graded frames, because the graded
arrays of one check span thirty orders of magnitude -- a charge density of
1e-18 C/m^3 beside a number density of 1e1 m^-3 -- and one absolute number cannot
serve them all. The current density gets 1e-07 instead of 1e-08 of its own scale:
the binomial filter removes the grid-scale part of Jx from its dumped maximum
while the deposition noise the bound has to cover is set by the unfiltered
per-particle contributions, so Jx's own maximum understates the noise by about a
decade. The result is a margin between 1.5e4 (Jx in `current-filter-1d`) and 9e7
(the loader's distribution function) above the largest legitimate spread seen,
and about five orders of magnitude below the smallest plausible fault: swapping
the three-point triangle kernel for the two-point top-hat kernel moves smooth Ex
and Jx by (k dx)^2/24, about 2e-3 at thirty cells per wavelength, and the per-cell
number density by tens of per cent; dropping the time-centring term at
`particles.F90:497` moves Jy and Jz by half the particle beta times the Courant
number; losing the prefix sum at `particles.F90:500` inflates Jx by dx/(v dt), a
factor of hundreds; getting the unnormalised-weight compensation `fac` at
`particles.F90:123-131` wrong is an O(1) error.

For the two-stream family the graded window, not the bound, is the design
variable, and it was measured. A fixed (1 + 1e-15) perturbation gives Ex spreads
of 1.2e-15, 1.7e-15, 2.4e-14, 1.3e-13 and 2.9e-13 relative at 100, 400, 1600,
3200 and 6400 steps: an e-folding of about 1200 steps, so the graded 3200 steps
leave four to five orders of margin while 20000 steps would reach the bound. The
Landau deck grows only linearly with the step count (4.2e-15, 2.1e-14, 5.7e-14 at
2000, 8000 and 20000 steps), which is why it is not flagged chaotic and is graded
over a quarter of its upstream window. The three-dimensional check's window is set
by runtime alone: 400 steps of 1.05 million particles is about 315 core-seconds
and its spread is still 5e-15 relative. Nothing here has been through a selfcheck
yet; the in-container spreads and timings will be written by the CLI. The two -O2 source copies, the
window-scan run directories and the end-to-end run.sh outputs are under
`/private/tmp/claude-501/-Users-huangzesen-work-projects-sciaccelbench/899ee21d-3f16-45da-9c42-988ab390a398/scratchpad/`
(`o2/`, `meas/`, `pkc/e2e/`) on the authoring machine; they are scratch and are not part of the leaf.
Note that the check READMEs quote, for each graded array, the largest magnitude it reaches over the
graded frames, because that single scalar is what each per-array `atol` is derived from and the warrant
is not checkable without it. It is one number per array, not a reference result, and the bound in
rubric.json already encodes it; if the curator would rather not publish it, the alternative is to state
the atol alone and move the derivation into this file.

The variant is a (1 + 1e-15) perturbation of one deck constant, chosen per check
from what the loader actually does with that constant. For the five thermal decks
it is `temperature`, which enters only as the Gaussian width of the drawn momenta
at `user_interaction/particle_temperature.F90:386`, so it perturbs the initial
phase-space state without changing the order in which the KISS stream is consumed.
For the delta-f deck it is `frac_beam`, which sets both the beam density and the
drift momentum, chosen over `background_number_density` because that constant
feeds the Debye length and hence the cell size and the time step. For the loader
deck it is `dens`: the power-law species is drawn by rejection sampling against
the deck expression (`particle_temperature.F90:495-517`), so perturbing `v0`, `p0`
or the momentum range could flip an accept/reject decision and desynchronise the
whole stream, whereas `helper.F90:650` hands the loader only a boolean density
map, so density reaches the output through the particle weights alone and leaves
positions and momenta bit-identical. Perturbing `number_density` was measured on
the thermal decks too and gives a spread of the same order; it was not chosen
because it moves only the weights.

The calibration selfcheck on the x86 worker (8 cpus, 8 GB, 2026-09-02) passed with reward 1.0 and no identical check: the in-container nominal-versus-variant spreads were 6.9e-14 to 9.1e-13 in absolute terms on the six field and density checks (per-array bounds at 1e-8 of each array's scale, four to seven orders above) and 1.5e7 on the largest delta-f array against its 2e5-scaled bound family, matching the native previews, and the suite took 446 s of the 900 s budget (51 to 103 s per check). No check changed policy or tolerance after calibration, so the calibration run is the final record; the curator consented in advance and finalizes these numbers at review, in particular the decision that pointwise grading requires a port to keep the seeded particle loading.

## Blind spots

The one that the human has to rule on is stated in the check READMEs and repeated
here: **the pointwise policy requires a port to reproduce EPOCH's seeded particle
loading exactly.** `housekeeping/setup.F90:501-503` seeds one KISS generator per
rank with `7842432 + rank`, and `helper.F90:515-583` and
`particle_temperature.F90:30-77` consume that single stream in strict linked-list
order -- one uniform per particle for the position, then a complete sweep for px,
then py, then pz. Changing the rank count, the particle count, the species order
or the decomposition changes the stream and therefore the initial state, so every
deck here fixes `nprocx` (and `nprocy`, `nprocz`) explicitly and every rank-layout
knob says in its help text that changing it produces a different, equally valid
realisation whose output is not comparable with the graded default. A port that
loads the same physical distribution by a different random stream is scientifically
correct and will still score zero. That is defensible -- loading is initial-condition
generation, not the kinetic algorithm under test -- but it is a real restriction on
what a port may change, and if the curator rejects it the fallback is to move these
checks to `invariants` on moments, field energy and growth rate, which is a much
weaker gate on the deposition.

Beyond that: nothing here exercises the compile-time variants, because no upstream
deck selects them -- `PARTICLE_SHAPE_TOPHAT`, `PARTICLE_SHAPE_BSPLINE3`, `HC_PUSH`,
`PER_SPECIES_WEIGHT` and `HIGH_ORDER_SMOOTHING` are all commented out in the
pinned Makefiles, so the checks grade only the default triangle-shape,
Boris-pusher, per-particle-weight build. The decks are cold: the two-stream beams
sit at u = p/mc = 0.009 and the thermal spread at 2e-4, so a port that replaced
the relativistic gamma with its first-order Taylor expansion would change the
answer by u^4/8, below 1e-9, and pass; only a port that dropped gamma entirely
(u^2/2, about 4e-5) would be caught. Nothing in this leaf is relativistic, and a
relativistic pusher check would need a deck upstream does not provide. Per-particle
point variables (particle positions, momenta, weights) are never graded, because
their order in a dump follows the history of MPI migration between ranks; this is
why the delta-f deck's second output block was removed. The `dist_fn` histograms
are graded only in the two checks where they carry the observable (the loader and
the delta-f weights) and not in the dynamic decks: they are discretely binned, so
a trajectory perturbation that moves one particle across a bin edge changes a bin
by a whole particle weight, and in the dynamic decks the bins hold single-digit
particle counts. Finally, the graded windows are short by design -- 3200 steps of
37900 for the two-stream decks, 400 of 10500 for the three-dimensional one -- so
none of the collective physics these decks exist to show upstream (the damping
rate, the instability growth rate) actually develops inside the graded window.
What is graded is that every particle is gathered, pushed and deposited exactly
as the pinned code does it, at every step, which is what a port of this module has
to get right.
