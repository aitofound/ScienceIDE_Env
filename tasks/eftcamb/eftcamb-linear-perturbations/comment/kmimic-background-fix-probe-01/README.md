# K-mimic background repair: controlled diagnostic, STOP 4

The scratch repair passes all 1,280,752 nominal-versus-variant comparisons in
the six-deck `kmouflage-kmimic` check under the unchanged additive bounds.
The stock arm reproduces the previous 3,967 failures. This is evidence for
the proposed background repair, not a fresh full-suite selfcheck or an
approved replacement of the pinned scientific source.

The source change is scientifically substantial: some nominal spectra change
by tens of percent or more. Convergence and independent physical validation
of the repaired solution are still required before adopting a new oracle.

## Execution and scope

The user approved the exact frozen plan with `APPROVE RUN`. Two clean source
builds and all 24 model executions completed with exit code zero. Measured
build time was 331.886 seconds; model execution time was 114.037 seconds
(445.923 seconds combined, excluding orchestration and analysis). Resources
were verified as 8 CPUs, 4 GiB memory, zero swap, and no container network.
The existing arm64 image was used without a pull or image build. Retained raw
evidence occupies approximately 76 MiB.

Only the scratch repair arm changes the K-mimic background/EFT evaluation in
`fortran/eftcamb/08f_full_models/008p3_Kmouflage.f90`. Both arms receive identical
read-only initialization tracing. The earlier D1-to-D2 initialization helper
correction is NOT included. All physical parameters, accuracy settings,
stability flags and initial-condition formulas are unchanged. Saved effective
parameters match across arms except for `output_root` in all 12 deck/IC pairs.

The fixed positive-a formulas also replace the old low-a table-floor behavior
in this arm. The experiment therefore tests the complete approved continuous
background repair; it does not separately isolate that boundary extension
from the removal of independent interpolation in the interior.

## Calibration results

Every row below contains nine separately bounded files. Counts include all
graded numeric entries, including printed coordinates. No paired coordinate
rows changed between nominal and variant. The two component margins are the
minimum across each deck's files; the combined margin is the reciprocal of
the largest fraction of the additive allowance used by any value.

| Deck | Values | Stock failures | Repaired failures | Repaired bulk margin | Repaired near-zero margin | Repaired combined margin |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| K-mimic 1 | 213,524 | 604 | 0 | 10.3429 | 1,000,000 | 28.6968 |
| K-mimic 2 | 213,511 | 3,363 | 0 | 0.205072 | 5,995.20 | 20.3107 |
| K-mimic 3 | 213,511 | 0 | 0 | 10.6343 | 1,000,000 | 21.2606 |
| K-mouflage 1 | 213,446 | 0 | 0 | 10.3863 | 500,000 | 20.9468 |
| K-mouflage 2 | 213,262 | 0 | 0 | 10.2677 | 500,000 | 38.9578 |
| K-mouflage 3 | 213,498 | 0 | 0 | 10.2320 | 1,000,000 | 112.069 |
| Check total / minimum margin | 1,280,752 | 3,967 | 0 | 0.205072 | 5,995.20 | 20.3107 |

The repaired check's worst value uses 0.0492351 of its allowed additive error.
No value fails. This is NOT a measured 11/11 task reward: the other ten checks
were not rerun in this diagnostic.

### Per-output-type summary, repaired arm

`rtol=1e-4` uniformly. These rows aggregate each file type over six decks;
[per-file.md](per-file.md) separately reports every actual file in BOTH arms,
including values, failures, extrema, bounds, and all three margins.

| Type | Values | Failing | atol | Max relative above atol | Max absolute at/below atol | Bulk margin | Near-zero margin |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| scalCls | 125,964 | 0 | 100 | 9.77833e-6 | 1.00000e-4 | 10.2267 | 1,000,000 |
| totCls | 104,970 | 0 | 0.1 | 2.56789e-4 | 1.66800e-5 | 0.389425 | 5,995.20 |
| lensedCls | 101,970 | 0 | 0.1 | 4.87633e-4 | 9.70000e-6 | 0.205072 | 10,309.3 |
| lensedtotCls | 101,970 | 0 | 0.1 | 3.99985e-4 | 9.70000e-6 | 0.250009 | 10,309.3 |
| lenspotentialCls | 167,952 | 0 | 0.1 | 2.56789e-4 | 1.66800e-5 | 0.389425 | 5,995.20 |
| scalarCovCls | 545,844 | 0 | 0.1 | 2.89089e-4 | 1.66700e-5 | 0.345914 | 5,998.80 |
| tensCls | 104,970 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference |
| matterpower | 7,846 | 0 | 1 | 4.72523e-6 | 0 | 21.1630 | no measured difference |
| transfer_out | 19,266 | 0 | 1,000 | 6.95986e-6 | 1.00000e-3 | 14.3681 | 1,000,000 |

### Margin flags requiring human review

The bulk and near-zero margins are not both near ten, and are not independent
pass requirements. K-mimic 2 retains a bulk margin below one. Its worst bulk
relative difference is in `5_Kmimic_2_lensedCls.dat`, row/ell 642, column 5:
nominal 0.110739, variant 0.110793. The relative change is 4.87633e-4, but the
absolute change is only 0.000054 and the unchanged additive allowance is
`0.1 + 1e-4*0.110739 = 0.1000110739`. This point is classified as bulk solely
because its nominal magnitude exceeds 0.1; the absolute allowance still
applies. It is not the worst combined-bound point. We do not hide this
relative tail or claim that every magnitude-carrying entry is at the print
floor.

Most bulk margins are below the skill's 50 review-order flag, while several
near-zero margins exceed its 10,000 flag. These are reasons to read the
scientific warrants, not automatic failures or permission to change the
bounds. Tensor outputs are identical within each nominal/variant pair and
provide no measured spread in this run. No bound is finalized here.

## Background-to-perturbation evidence

The saved background expansion H(a) was compared with the independently
evaluated continuous K-mimic identity using 60-digit arithmetic. Forty saved
rows per model/arm cover both ICs. The remaining repaired residual is
consistent with the saved text precision.

The coefficient audit uses the existing pi-equation arithmetic with exact
continuous background functions. It compares 252, 251 and 251 recorded
nominal initialization states for K-mimic 1/2/3. The C error below is normalized
by the analytic `A1*H_conformal^2`, NOT by the often cancellation-small C itself.

| Model | Stock max H relative error | Repaired max H relative error | Stock max normalized C error | Repaired max normalized C error |
| --- | ---: | ---: | ---: | ---: |
| K-mimic 1 | 5.49162e-5 | 5.42506e-11 | 2.50620e-3 | 1.01642e-13 |
| K-mimic 2 | 5.48345e-5 | 5.81355e-11 | 2.50764e-3 | 9.38147e-14 |
| K-mimic 3 | 5.73265e-5 | 5.78197e-11 | 2.51030e-3 | 1.05528e-12 |

The largest repaired relative discrepancy in A1 is 3.35e-13 and in D1 is
1.48e-13 over these traces. C's internal finite-difference error estimate
divided by the derivative magnitude falls from roughly 17-22 to 0.00149,
0.00130 and 0.01405 respectively. These last numbers are the numerical
routine's error estimates, not independently established derivative errors;
some floating-point cancellation remains.

All 108 stock graded files reproduce the previous calibration byte-for-byte.
All 54 ordinary K-mouflage graded files are byte-identical between stock and
repair arms. All enabled stability gates pass with the original flags;
mass-stability flags remain off. We have not established mass/tachyon
stability or the absence of every possible physical instability.

Taken together, the controlled repair strongly supports independent
background interpolation as a cause of the observed calibration failures.
It fixes the previously demonstrated coefficient inconsistency and removes
all failures in this run without changing the physical decks or tolerances.

## Important: the nominal solution changes substantially

Passing nominal versus variant shows reduced numerical sensitivity. It does
not show that a new physical solution is accurate. At the same printed
wavenumber `k/h=0.316529`, the matter-power output changes as follows:

| Model | Stock P(k), printed units | Repaired P(k), printed units |
| --- | ---: | ---: |
| K-mimic 1 | 4,969.46 | 434.372 |
| K-mimic 2 | 4,201.88 | 462.826 |
| K-mimic 3 | 1,003.4 | 696.463 |

The K-mimic 1 lensed TT column at ell=2990 changes from 1961.78 to 27.177.
The former passing K-mimic 3 also changes substantially, demonstrating that
insensitivity to an IC perturbation alone was never proof of physical
accuracy. The repaired P(k) at `k/h=8.24543` is 0.273009, 0.278321 and 0.306454;
the enormous spikes seen in the earlier non-nested 2,000-node diagnostic are
not present at that sampled coordinate in this run. That observation is not
a convergence test across the full spectrum.

Cross-arm spectrum comparisons use identical printed coordinates only.
All six K-mimic transfer-table pairs have zero exactly shared printed k
coordinates between stock and repair, and are explicitly marked NOT COMPARED
in `cross-arm-summary.json`. No row-index pairing or interpolation is used to
manufacture an agreement. Within each arm the actual nominal-versus-variant
transfer checks did run on matching coordinates and pass in the repair arm.

## Decision boundary and next step

This is a promising scientific source repair, not an approved source pin.
The modified file belongs to the models/background module, not the packaged
perturbation module. Before adopting it as the benchmark oracle, validate
the repaired spectra with an independent convergence/precision comparison
and review the scientific patch and provenance. Any new model run needs a
new exact plan and `APPROVE RUN`; any push or comment needs its own preview
and approval. No further model run is authorized by this completed plan.

The vendored source, task checks, rubrics and CLI-written self-validation
record remain unchanged. The full task still has a stale self-validation
record. STOP 4 remains in effect; no push, merge or external comment occurred.

## Reproducibility

Approved payload and full patches: `../kmimic-background-fix-plan/`.
Raw evidence: `jobs/eftcamb-kmimic-background-fix-probe-01/` in the repository.
All raw files are hashed in `evidence-sha256.json`. Recounting the saved text
with the independent report script exactly reproduces every validator count
and extremum. The report script performs no CAMB execution or Docker action:

```bash
python3 -u -B jobs/eftcamb-background-fix-report.py jobs/eftcamb-kmimic-background-fix-probe-01 --out tasks/eftcamb/eftcamb-linear-perturbations/comment/kmimic-background-fix-probe-01
```

Machine-readable details: `summary.json`, `per-deck.csv`, `per-file.csv`,
`failing-values.csv` (stock failures), `worst-points.csv`, `background-identity.csv`,
`coefficient-identity.csv`, `derivative-estimates.csv`, and `cross-arm-summary.json`.
