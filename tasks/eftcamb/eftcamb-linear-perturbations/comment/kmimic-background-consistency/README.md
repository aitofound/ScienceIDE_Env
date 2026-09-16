# K-mimic: background consistency and perturbation-coefficient audit

Status: STOP 4 diagnostic, not a new calibration. The source, physical decks,
perturbation initialization, rubrics, validators and CLI records are unchanged.
No Docker, CAMB execution, source build or differential-equation integration was
performed. The 1,000-node background configuration is the pinned source default,
not a newly chosen physical parameter.

## Result

Independent interpolation of the background EFT functions and their derivatives
breaks derivative consistency and the cancellations in the pi-equation C
coefficient. This is now supported by direct reconstruction of 754 saved
initialization records, rather than just a correlation with grid phase. The
actual ghost and gradient sign checks remain positive in 2,997 interior samples
per deck. Their success is compatible with the C error because those checks do
not inspect C in this model.

This identifies a numerical coefficient defect. It does not establish a
physical tachyon, explain all final spectrum failures, or supply an accepted
benchmark fix. A controlled evolution comparison is still required.

## Direct reconstruction of an actual recorded point

For K-mimic 1 at a=1.032675384295327e-8 and k=0.20892938168261962 Mpc^-1:

| Quantity | Continuous model identities | Source-style independent interpolation | Saved stock trace |
| --- | ---: | ---: | ---: |
| C, source units Mpc^-4 | +0.3313000772 | -578665.117435 | -578665.117452 |
| C/(A1 Hconf^2) | +1.42696e-9 | -0.00249079 | -0.00249079 |
| (C+k^2 D1)/A1, Mpc^-2 | +0.0146115 | -108.7147 | Derived from the same recorded coefficients |

The large relative change of C partly reflects a small exact value after
cancellation. The normalized second row gives its scale relative to the Hubble
terms; it must not be replaced by a claim of a million-fold observable error.
The last row is an instantaneous coefficient in the pi equation, not a mass
eigenvalue of the full coupled scalar/matter system.

Across the saved nominal stock traces:

| Deck | Records matched | Maximum C reconstruction residual / (recorded A1 Hconf^2) | Maximum A1 relative reconstruction error | Maximum D1 relative reconstruction error |
| --- | ---: | ---: | ---: | ---: |
| K-mimic 1 | 252 | 7.11e-14 | 1.73e-14 | 1.64e-14 |
| K-mimic 2 | 251 | 8.90e-14 | 2.91e-14 | 2.21e-14 |
| K-mimic 3 | 251 | 6.57e-13 | 2.01e-13 | 1.64e-13 |

Hconf is reproduced to at most 3.28e-16 relatively. All calculations use the
original nominal parameters and reconstruct the original uncorrected-source
background. No coefficients were fitted to the traces. Records may share a
time because different k modes initialize there; these are not 754 independent
times or evolved trajectories. See `saved-trace-reconstruction.csv`.

## Where consistency is lost

The background mapping writes separate arrays for Omega, its first through third
derivatives, the scaled c and Lambda values and their conformal-time derivatives,
and Gamma1 and its first derivative. At a grid node, those expressions are
consistent with the continuous model. Between nodes, the interpolation class
linearly blends each array separately in ln(a).

In general, interpolating a derivative is not the same as differentiating the
interpolated value. This matters even when both arrays are individually good
approximations to their continuous counterparts. The discrepancy enters the
Friedmann derivative relations and the cancellations in the perturbation
coefficients.

The audit samples phases 0.1, 0.5 and 0.9 of all 999 grid intervals, over
1e-9 < a < 1. Define x=ln(a), and f as the scaled cache value a^2 c_phys/m0^2.
The consistency identities tested are:

```text
a Omega'_stored = d Omega_interpolated / dx
a Gamma1'_stored = d Gamma1_interpolated / dx
c_dot_stored = Hconf * (d f_interpolated/dx - 2 f_interpolated)
Hdot_stored = Hconf * d Hconf_interpolated/dx
```

The subtraction of 2f is essential: the cache's `EFTcdot` stores
a^2 dot(c_phys)/m0^2, not the derivative of a^2 c_phys/m0^2.

| Deck | Max Omega derivative mismatch | Max Gamma1 derivative mismatch | Max c derivative mismatch | Max abs(Hdot consistency residual)/Hconf^2 |
| --- | ---: | ---: | ---: | ---: |
| K-mimic 1 | 0.9933% | 5.7711% | 0.8325% | 1.1348% |
| K-mimic 2 | 0.9933% | 3.8171% | 0.8325% | 1.1279% |
| K-mimic 3 | 0.9933% | 5.8009% | 0.8325% | 1.1881% |

The first three columns use abs(x-y)/max(abs(x),abs(y)); they compare two ways
of differentiating the represented function. They are NOT relative errors in
the original spectrum or necessarily in either derivative compared with the
continuous physical derivative. Lambda's corresponding ratio can approach 2
near its derivative zero/sign crossing; that raw maximum is retained in the
CSV/JSON but is not advertised as a global 200% physical error.

## How the error enters evolution

For this model Gamma2 through Gamma6 vanish, so A2, B2 and D2 vanish. The actual
source evolution reduces to the following form (retaining its source term):

```text
pi'' = -(B1/A1) pi' - ((C+k^2 D1)/A1) pi - (H0 E/A1)
```

Thus the reconstructed C error is in a coefficient used throughout pi evolution,
not merely in a diagnostic printout. The same combination also appears in the
case-1 initialization formula. Background consistency is an upstream issue
shared by both paths; the prior one-line D2 initialization-helper correction
does not repair this background inconsistency.

In the leading radiation-era limit, A approaches a constant, c_cache scales
as a^-2 and Gamma1 as a^-4. Their analytic contributions to C cancel at leading
order. Independently interpolating quantities with different powers of a and
their derivatives breaks that cancellation. This is consistent with the
directly reconstructed negative C between grid nodes.

The source locations establishing the chain are:

- `08f_full_models/008p3_Kmouflage.f90:815-833`: tabulated EFT values/derivatives.
- `08f_full_models/008p3_Kmouflage.f90:342-375`: independent interpolation reads.
- `02_equispaced_interpolation_linear_1D.f90:182-295`: value/derivative blending.
- `06p1_abstract_EFTCAMB_full_map.f90:94-125`: Hconf and its derivatives.
- `06_abstract_EFTCAMB_model.f90:329-350,449-500`: effective-fluid and pi factors.
- `../equations.f90:3004-3007`: use in the evolved pi acceleration.
- `09_EFTCAMB_IC.f90:282-285`: use in initialization.

These paths are relative to `code/eftcamb/fortran/eftcamb/` except the explicitly
parent-relative equations path. The [EFTCAMB numerical notes](https://arxiv.org/abs/1405.3590)
provide the published description of the implementation; the pinned source is
the authority for the actual expressions evaluated here.

## Why the enabled stability gates do not catch it

Setting Gamma2 through Gamma6 to zero in the implemented formulas gives exactly:

```text
EFT_kinetic  = 36 (1+Omega)^2 A1
EFT_gradient = 36 (1+Omega)^2 D1
```

The audit checks these identities for both continuous and interpolated fields.
Neither expression contains C. In every one of the 2,997 interior samples per
deck, the interpolated kinetic and gradient values remain positive. Their
D1/A1 minima are 0.00331556, 0.00308209 and 0.00326082, respectively.

This is not a stability flag accidentally disabled by the benchmark: the
ghost/gradient flags are enabled, and the sign tests do what the source says.
They are not tests of derivative consistency or of the accuracy of C.

Both mass gates remain off in the original decks, so the existing evidence
does not settle physical mass stability. A negative C or negative instantaneous
(C+k^2 D1)/A1 is not sufficient to claim a physical tachyon. The relevant
analysis considers the coupled system's kinetic, gradient and mass eigenvalues;
see [De Felice, Frusciante and Papadomanolakis](https://arxiv.org/abs/1609.03599).
Moreover, the implemented additional mass derivatives use finite differences
of these interpolated functions, so any future mass diagnostic needs its own
derivative-reliability checks.

## Grid knots explain a second numerical hazard

At the exact a=1e-8 node, the source-style C is continuous but has distinct
left and right derivatives. For K-mimic 1, second-order one-sided stencils in
ln(a), refined from step 1e-5 to 1e-7, give:

| Derivative dC/da at a=1e-8 | Value |
| --- | ---: |
| Continuous physical-background expression | -6.80233e7 |
| Interpolated expression, left side | +1.32755e16 |
| Interpolated expression, right side | -1.22807e16 |

At that knot there is no single derivative of the represented C. The existing
finite-difference helper crosses nearby knots; its large internal error estimates
are therefore compatible with this nonsmooth representation. The report does
not equate that helper's numerical estimate with an independently known true
derivative error at every point. See `knot-derivatives.csv` for all decks and
three step sizes.

This also exposes a pitfall in the prior 2,000-node experiment. On a grid uniform
in ln(a) from 1e-9 to 1, a=1e-8 has index coordinate (N-1)/9. For N=1000 that
coordinate is 111, exactly a node. For N=2000 it is 222+1/9, between nodes.

| Algebraic grid reconstruction at a=1e-8, K-mimic 1 | C |
| --- | ---: |
| Exact continuous expression | +0.352517 |
| 1,000 nodes | +0.352517 |
| 1,999 nodes, nested refinement | +0.352517 |
| 2,000 nodes, not nested | -64336.8168 |
| 3,997 nodes, nested refinement | +0.352517 |

These are algebraic evaluations, NOT new CAMB experiments. Their node equality
is exact-arithmetic reasoning; a real binary64 startup can lie slightly off the
node, as the existing traces do. Nested grids still have interpolation errors
and derivative discontinuities away from nodes. No grid size is being proposed
as an accepted fix. The mechanism explains why a non-nested refinement can
change the startup coefficient drastically; it does not prove the cause of the
previous huge final matter-power spikes.

## What is established, and the next experiment

Established: source-style background interpolation breaks consistency, changes
the actual C coefficient, and reproduces the recorded anomalous C. Enabled
stability checks can remain positive while this happens. The analytic background
formulas and the original physical parameters have not been shown to violate
the sampled ghost/gradient conditions.

Not established: that replacing the representation will remove the 3,967
existing additive-comparison failures; that the third deck is converged just
because it passes; that the full coupled physical system is mass-stable; or that
the old D2 helper inconsistency is harmless.

The clean causal test is a separate scratch comparison that changes only
background/EFT evaluation to a consistent representation, keeps the physical
decks and the existing perturbation initialization fixed, then compares
coefficient traces and nominal/variant spectra at the same bounds. K-mimic 3
must remain as a passing-control deck. Any new computation requires a complete
run preview and fresh explicit approval. This document does not authorize it.

If a source change is needed, it must be reviewed as a source correction, not
silently inserted into the benchmark oracle for the current pinned source.
Neither tolerance relaxation nor removing the K-mimic checks is justified by
this audit.

## Reproducibility

`jobs/eftcamb-background-consistency.py` uses Python's standard library and
60-digit Decimal arithmetic. It reuses the independently checked analytic
background identities in `jobs/eftcamb-kmimic-background-audit.py`, and parses
only explicit arithmetic assignments for the pinned source's Hubble, pi and
stability coefficients. The restricted AST evaluator does not execute Fortran
or arbitrary Python source.

The separate exact-field controls reproduce analytic Hdot, Hdotdot and the
kinetic sound-speed identity to below 4.04e-59 relatively. A selected-point
60-versus-90-digit repeat changes exact C, interpolated C and the normalized
Hdot consistency residual by at most 1.90e-47 relatively.

Files: `per-deck.csv`, `summary.json`, `interior-coefficients.csv`,
`saved-trace-reconstruction.csv`, `identity-controls.csv`, `grid-alignment.csv`,
and `knot-derivatives.csv`. These are repository-visible diagnostic records,
not runtime inputs, calibration spreads or a replacement self-validation record.
