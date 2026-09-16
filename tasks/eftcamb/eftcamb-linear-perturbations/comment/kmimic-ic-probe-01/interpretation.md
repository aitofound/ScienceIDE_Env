# STOP 4: the D2 correction is not an accepted calibration fix

All twelve approved model executions completed with exit code zero. Two scratch
builds took 313.450 s in total and the model executions took 27.607 s, using the
existing arm64 oracle image under verified 8-CPU, 4-GiB, zero-swap limits and no
network. The retained raw diagnostic directory is 38 MiB. This was not a full
task selfcheck and does not refresh the CLI fingerprint.

## Outcome

| Deck | Values per pair | Stock failing | Corrected failing | Corrected bulk margin | Corrected near-zero margin | Corrected combined margin |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| K-mimic 1 | 213,524 | 604 | 6,673 | 0.00538218 | 40.3210 | 0.347894 |
| K-mimic 2 | 213,511 | 3,363 | 2,093 | 0.0136094 | 106.895 | 0.470225 |
| K-mimic 3 | 213,511 | 0 | 0 | 0.109735 | 1930.50 | 4.60915 |

The corrected arm has 8,766 failing entries versus 3,967 in the stock arm. These
counts include repeated TT observables across several official files; they do
not count independent physical failures. The third deck still passes, but its
combined margin falls from 9.10492 to 4.60915. No claim of scientific convergence
or fault-free behavior follows from that pass. Both component margins are
diagnostics; the actual policy remains the additive pointwise bound, not a
piecewise relative/absolute test. Per-file values, failures, bulk-relative
maxima, near-zero absolute maxima and all margins are in `per-file.csv`.

The stock instrumented build reproduced all 54 graded files of the previous
uninstrumented full calibration byte-for-byte (three decks, nine files, two
ICs). No paired printed-coordinate row changed. Thus this experiment controls
for output changes caused by the added tracing on these inputs.

## What the source inconsistency did, and did not establish

The D1-to-D2 inconsistency documented in `../kmimic-initialization-investigation.md`
is real in the pinned source. The single-line correction changes the initialized
pi velocity; it leaves the logged C, D1 and pi values identical between the two
nominal arms. For K-mimic 1 at k=33.22974864190532, the scaled pi velocity changes
from 0.2580092445960526 to 0.00023545447469095675. This confirms that the line is
executed and consequential, not that the corrected run is converged or meets the
benchmark's sensitivity bound. It did not fix this calibration and has not been
adopted into either the task or `code/eftcamb/`.

The hypothesis of a near-cancelled initial denominator is not supported at the
sampled startup points: minimum `abs(C+k^2 D1+k^4 D2) / (abs(C)+abs(k^2 D1)+abs(k^4 D2))`
for nominal K-mimic 1/2/3 is 0.877169, 0.984141 and 0.985323. D2 is zero at every
logged initialization, as expected from this model's Gamma3/4/5 settings. This
does not rule out cancellations *inside* the calculation of C or later evolution.

## Stronger numerical lead: background interpolation and C derivatives

The original derivative helper's own estimated absolute error in dC/da reaches
16.9961, 17.0848 and 21.7439 times the absolute returned derivative for nominal
K-mimic 1/2/3. The helper computes and discards that error estimate
(`09_EFTCAMB_IC.f90:589,616`; `02_utilities.f90:120–159`). These are internal
estimates, not independently known true errors, and the passing control also
shows the problem. They motivate inspection; they do not prove the cause of
the final failing values.

The K-mimic implementation separately interpolates background functions and
their stored derivatives (`08f_full_models/008p3_Kmouflage.f90:342–375`) on 1000
uniform log(a) points from 1e-9 to 1, as set at lines 120–122. The interpolation
class returns separate linear blends of the value and derivative arrays
(`02_equispaced_interpolation_linear_1D.f90:151–178,235,295`). C then
combines these interpolated quantities through the cancellation-prone formula
in `06_abstract_EFTCAMB_model.f90:470–483` before numerical differentiation.

Saved stock traces show a repeatable association with position within that grid.
For K-mimic 1, C is -0.712584 at a=9.999999919769415e-9, almost exactly a grid node;
at a=1.032675384295327e-8, 0.549985 of the way through a grid interval, C is
-578665.117452. To avoid attributing the changing physical scale to the grid,
an exploratory analysis normalizes C by `A1*adotoa^2` and fits a straight line
against `theta*(1-theta)`, where theta is the fractional grid position. It uses
one sample per distinct logged time with a<1e-6, not a new solver evaluation:

| Deck | Distinct times | Slope | R squared |
| --- | ---: | ---: | ---: |
| K-mimic 1 | 67 | -0.0100233 | 0.998798 |
| K-mimic 2 | 67 | -0.0100691 | 0.998912 |
| K-mimic 3 | 68 | -0.0100352 | 0.998590 |

This is strong evidence of grid-associated structure in the initialized
coefficient and is consistent with interpolation error. It is exploratory, not
a causal test, not evidence of physical instability, and not a new scoring rule.
No subtraction of this fitted pattern or fitted output correction is proposed.
A controlled coefficient/interpolation convergence test is the next useful
investigation; blindly increasing accuracy or changing IC type is not supported.
Changing the scientific source would require a separately reviewed source
revision, followed by a fresh full task selfcheck.

No checks were removed. No input, tolerance, policy, source or CLI record was
changed. No push, message, PR action or merge was performed. Further model runs
need a new exact preview and explicit approval.
