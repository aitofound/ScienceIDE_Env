# Final numerical policy

The thresholds remain unchanged from the established numerical-equivalence policy. Final execution uses the fixed check inputs and default grid/resource settings. Noise amplitude was calibrated separately; no tolerance was enlarged to make a noise run pass.

## A. Compared quantities

| Quantity | Rule | Alignment |
|---|---|---|
| w, Omega_m, their reported errors | absolute 2e-5 | named fit quantities; official central-68% errors and synthetic standard deviations remain distinct |
| Correlation | absolute 6e-4 | rho(w,Omega_m) |
| Final chi-square | absolute 0.06 | named total statistic |
| Every SN/total delta-chi-square | 2e-4 + 6e-6 × abs(reference) | full physical grid coordinates |
| Every posterior weight | 1e-8 + 6e-6 × abs(reference) | same physical grid coordinates |
| Residual/model/observed/fiducial magnitudes and errors | absolute 1.2e-4 mag | all columns follow CID |
| Fit status, IDs, printed redshifts, coordinate sets | exact | ignores storage order, ordinal row labels and timing |

## B. Measured evidence

Fractions below are the largest error divided by its own observable-specific tolerance. Zero printed-output differences are uninformative about sub-output-digit arithmetic. Tiny tail-weight-only variants are weak calibration and are recorded without extrapolation.

| Check | Variant fraction | Compiler fraction | Cross-architecture fraction | Independent likelihood fraction |
|---|---:|---:|---:|---:|
| official-des3yr | 1e-18 | 0 | 0 | not implemented for CMB/BAO/legacy decks |
| official-k09 | 1e-08 | 0 | 0 | not implemented for CMB/BAO/legacy decks |
| official-pantheon | 0.129603 | 0 | 0 | not implemented for CMB/BAO/legacy decks |
| synthetic-correlated | 0.000997429 | 0 | 0 | 0.647971 |
| synthetic-diagonal | 1e-21 | 0 | 0 | 0.785081 |
| synthetic-inverse | 0.000997429 | 0 | 0 | 0.647971 |
| synthetic-npz | 0.000994868 | 0 | 0 | 0.647869 |
| synthetic-permutation | 0.0429112 | 0 | 0 | 0.647971 |
| synthetic-prior-shift | 0.000499075 | 0 | 0 | 0.644012 |
| synthetic-redshift-cut | 0.0848602 | 0 | 0 | 0.640563 |
| synthetic-scatter | 1e-09 | 0 | 0 | 0.712415 |

Fault separation from actual altered-physics solves against synthetic-correlated:

| Alteration | Worst tolerance multiple | Outcome |
|---|---:|---|
| drop-systematic-covariance-real-solve | 127491 | rejected |
| wrong-matter-prior-real-solve | 163288 | rejected |
| added-scatter-real-solve | 19694.7 | rejected |

All 18 corruption, physics and representation probes behaved as expected; the consistent all-field reorder passed. The independent quadrature audit passed 8 fixed synthetic cases. Per-observable values and finite/identity conditions are retained in the JSON reports.

## C. Compiler and architecture comparison

| Family | Alternative | Measured interpretation |
|---|---|---|
| SNANA wfit | GCC/G++ versus Clang/Clang++, both -O2, -fno-fast-math, -ffp-contract=off | All 11 comparisons pass on both architectures; every graded value is identical. This is a blind floor measurement, not proof of zero roundoff. |
| SNANA wfit | Linux ARM64 versus emulated Linux x86_64, nominal inputs | All 11 pass with identical graded values. Independent likelihood errors and physical-fault probes justify the retained bounds; architecture timings are not a performance comparison. |

Output-precision limits, untested models and official-deck omissions are documented in the task notes. The official self-validation record remains the source for the current contract fingerprint and final runtime.
