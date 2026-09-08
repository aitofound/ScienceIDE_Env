# Remaining K-mimic sensitivity: evidence before another run

The additive comparison correction removes all four Horndeski-full failures. Its
combined margin is 20.0704 even though its diagnostic bulk component margin is 0.762:
the old comparator incorrectly discarded the absolute term for those values.

For kmouflage-kmimic, the corrected rule reduces 49,121 failing values to 3,967.
All remaining failures are in the second numeric column (temperature auto-spectrum,
or matter power) of ten physical files from `5_Kmimic_1` and `5_Kmimic_2`:

| Deck | Files / quantity | Failing values | Coordinate range (not necessarily continuous) |
| --- | --- | ---: | --- |
| Kmimic 1 | lenspotentialCls, scalarCovCls, totCls: TT / TxT | 200 in each file | L = 3180–3441 |
| Kmimic 1 | matterpower: P | 4 | k/h = 0.329447–0.386609 |
| Kmimic 2 | lensedCls, lensedtotCls: TT | 623 in each file | L = 2440–3062 and 2438–3062 |
| Kmimic 2 | lenspotentialCls, totCls: TT | 705 in each file | L = 2429–3304 |
| Kmimic 2 | scalarCovCls: TxT | 705 | L = 2434–3304 |
| Kmimic 2 | matterpower: P | 2 | k/h = 0.316529–0.342892 |

The repeated TT columns in distinct official tables are retained, not treated as
independent physical failures or dropped to change the score. The coordinate grids
match exactly between the saved paired files. All three Kmouflage decks and Kmimic 3
pass the additive rule. No failures remain in tensor, transfer_out, or scalCls tables;
different file scalings and absolute terms mean that passing alone does not prove
those underlying states have smaller relative sensitivity.

The largest remaining allowance fraction is 2.394049 in Kmimic 2 matter power:
4201.88 versus 4205.28, absolute difference 3.40, allowed difference
`1 + 1e-4*4201.88 = 1.420188`. This is not solely a near-zero or one-last-digit effect.
The root cause is not established by the saved output differences.

## Source-backed diagnostic candidates, not selected fixes

The pinned source supplies several distinct accuracy controls. In
`fortran/cmbmain.f90:1109-1111`, scalar evolution sets its local tolerance using
AccuracyBoost and IntTolBoost, then divides by 100 with transfer high precision.
Both selected decks inherit that flag and accuracy_boost=1. A local ODE tolerance
does not bound every downstream spectrum or interpolation error.

The official `base_params.ini:251-259` describes accuracy_boost as a control on
time steps and k sampling. Separately,
`fortran/eftcamb/08f_full_models/008p3_Kmouflage.f90:120` exposes
model_background_num_points with a default of 1000. That model integrates its
background with DLSODA at rtol=1e-12 and atol=1e-16 (lines 508–511), then supplies
tabulated EFT functions. Its K-mimic expressions include cancellations and divisions
by ufunc+vfunc and powers of chi and vfuncp (lines 708–764). These are mechanisms to
investigate, not proof that background tabulation causes the observed failure.

The proposed targeted diagnostic holds the scientific code, physical parameters,
two-ULP perturbation, L/k window, and comparator bounds fixed, and compares four
scratch configurations: original eight-thread settings; one thread; accuracy_boost=2;
and 2000 background points. It builds the unchanged source once in the existing
image and runs two decks on both ICs per configuration (16 model executions).
No configuration is adopted into the benchmark from a passing diagnostic alone.
The human approved that preview and all 16 executions have now completed. See
`../kmimic-probe-01/README.md` for the results. None of the four configurations
passes across both decks. In particular, the 2000-point background configuration
has only five paired failures but introduces a very large high-k matter-power and
raw-transfer spike relative to the baseline; it is not an accepted fix.
