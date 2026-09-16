# Phase-diagram Docker calibration

All 56 checks passed; reward 1.0. The nominal suite took 54.2 seconds, excluding zero source-build seconds, on one allocated CPU and 4 GB RAM. Fifty-five checks produced non-identical outputs. The all-zero coplanar example is deliberately identical and supplies no tolerance calibration.

The largest observed error used 0.00192663% of its permitted bound (patched-phase-diagram-update-multiple-batches); margin 51904×. This margin concerns tiny input perturbations on this computer, not a measured cross-platform floor. No alternative build or GPU speedup was tested.

## Proposed rules for approval

- 50 direct-quantity checks: error <= 1e-7 + 1e-7 * abs(reference).
- 6 optimization/reaction-separation checks: error <= 1e-5 + 1e-7 * abs(reference).
- Phase identities, metric names and discrete counts agree exactly.
- Decomposition weights must be finite and at least -1e-8; mixtures are compared by conserved element fractions, weight sum and energy rather than a potentially nonunique list of coefficients.
- Keep the zero-energy coplanar check as explicitly identical, without claiming that it calibrates a numerical tolerance.

Bounds use each reported quantity's units. Total energy is eV per input composition, normalized energy eV/atom (or per transformed terminal unit), chemical potential eV per exchanged atom, and composition amounts/weights dimensionless. Moment outputs inherit the corresponding powers of these units.

The calibration emitted a wording warning because the planar variant description began with “Explicitly identical” instead of the CLI's required “Identical”. That prefix and evidence descriptions have now been corrected, and the existing decomposition-weight constraint documented explicitly. No numeric bound or workload was changed after calibration. These documentation edits make the calibration record stale against the current files; final validation is intentionally deferred until the user approves the policy.

The native suite also passed independent scientific spot checks and 15 validator probes. Set-moment and finite-direction boundary comparisons have documented blind spots; curator review remains necessary. The task depends on source PR #521. Interface task PR #523 separately passed its GitHub checks. Pourbaix implementation remains outstanding.

## Per-check calibration

Spread is the maximum relative error, using absolute error when the reference is zero. Margin is the allowed bound divided by the worst actual scalar error; it is not inferred directly from spread. Counts are exact.

| Check | Policy | Allowed scalar error | Spread | Margin | Identical |
|---|---|---|---:|---:|---|
| compound-phase-diagram-get-formation-energy | pointwise | 1e-07 + 1e-07 × abs(reference) | 8.03e-15 | 14209786× | no |
| compound-phase-diagram-stable-entries | invariants | 1e-07 + 1e-07 × abs(reference) | 5.58e-16 | 216593265× | no |
| get-equilibrium-reaction-energy-open-element-in-original-composition | pointwise | 1e-05 + 1e-07 × abs(reference) | 1.58e-15 | 2878081637× | no |
| grand-potential-phase-diagram-get-formation-energy | pointwise | 1e-07 + 1e-07 × abs(reference) | 4.13e-15 | 80516703× | no |
| grand-potential-phase-diagram-stable-entries | invariants | 1e-07 + 1e-07 × abs(reference) | 4.81e-16 | 235854506× | no |
| p-d-entry-composition | pointwise | 1e-07 + 1e-07 × abs(reference) | 4.44e-16 | 450359963× | no |
| p-d-entry-get-chemical-energy | pointwise | 1e-07 + 1e-07 × abs(reference) | 4.44e-16 | 300239975× | no |
| p-d-entry-get-energy | pointwise | 1e-07 + 1e-07 × abs(reference) | 4.26e-16 | 239253730× | no |
| p-d-entry-get-energy-per-atom | pointwise | 1e-07 + 1e-07 × abs(reference) | 4.26e-16 | 243944980× | no |
| patched-phase-diagram-get-decomp-and-e-above-hull | invariants | 1e-07 + 1e-07 × abs(reference) | 5.25e-11 | 56296902× | no |
| patched-phase-diagram-get-decomposition | invariants | 1e-07 + 1e-07 × abs(reference) | 1.3e-14 | 68056496× | no |
| patched-phase-diagram-get-equilibrium-reaction-energy | pointwise | 1e-05 + 1e-07 × abs(reference) | 1.91e-10 | 11397640× | no |
| patched-phase-diagram-get-form-energy | pointwise | 1e-07 + 1e-07 × abs(reference) | 6.15e-15 | 39474899× | no |
| patched-phase-diagram-get-hull-energy | pointwise | 1e-07 + 1e-07 × abs(reference) | 3.91e-15 | 26083377× | no |
| patched-phase-diagram-get-pd-for-entry | invariants | 1e-07 + 1e-07 × abs(reference) | 6.95e-16 | 172089809× | no |
| patched-phase-diagram-get-phase-separation-energy | pointwise | 1e-05 + 1e-07 × abs(reference) | 6.95e-15 | 1140295182× | no |
| patched-phase-diagram-get-stable-entries | invariants | 1e-07 + 1e-07 × abs(reference) | 6.22e-16 | 179584718× | no |
| patched-phase-diagram-update-all-positive-formation-energy | invariants | 1e-07 + 1e-07 × abs(reference) | 3.21e-15 | 31753676× | no |
| patched-phase-diagram-update-entry-in-existing-space | invariants | 1e-07 + 1e-07 × abs(reference) | 3.27e-15 | 31224422× | no |
| patched-phase-diagram-update-lower-elemental-ref-ignore | pointwise | 1e-07 + 1e-07 × abs(reference) | 1.56e-14 | 6520666× | no |
| patched-phase-diagram-update-lower-elemental-ref-recalculate | invariants | 1e-07 + 1e-07 × abs(reference) | 2.56e-15 | 39897873× | no |
| patched-phase-diagram-update-matches-full-rebuild | invariants | 1e-07 + 1e-07 × abs(reference) | 1.45e-12 | 69759× | no |
| patched-phase-diagram-update-multiple-batches | invariants | 1e-07 + 1e-07 × abs(reference) | 1.95e-12 | 51904× | no |
| patched-phase-diagram-update-qhull-dedup-across-batches | invariants | 1e-07 + 1e-07 × abs(reference) | 5.31e-15 | 19219330× | no |
| patched-phase-diagram-update-returns-removed-stable | invariants | 1e-07 + 1e-07 × abs(reference) | 2.37e-15 | 42960856× | no |
| patched-phase-diagram-update-same-comp-higher-energy-noop | invariants | 1e-07 + 1e-07 × abs(reference) | 6.22e-16 | 179584718× | no |
| patched-phase-diagram-update-same-comp-lower-energy-replaces | invariants | 1e-07 + 1e-07 × abs(reference) | 4.89e-15 | 20866702× | no |
| patched-phase-diagram-update-unstable-entry-no-change | invariants | 1e-07 + 1e-07 × abs(reference) | 3.49e-15 | 29213382× | no |
| phase-diagram-1d-pd | invariants | 1e-07 + 1e-07 × abs(reference) | 4.44e-16 | 450359963× | no |
| phase-diagram-dim1 | invariants | 1e-07 + 1e-07 × abs(reference) | 4.64e-16 | 285083377× | no |
| phase-diagram-get-all-chempots | invariants | 1e-07 + 1e-07 × abs(reference) | 2.56e-15 | 41335619× | no |
| phase-diagram-get-composition-chempots | pointwise | 1e-07 + 1e-07 × abs(reference) | 1.31e-15 | 95271060× | no |
| phase-diagram-get-critical-compositions | pointwise | 1e-07 + 1e-07 × abs(reference) | 5.03e-16 | 203551291× | no |
| phase-diagram-get-critical-compositions-fractional | pointwise | 1e-07 + 1e-07 × abs(reference) | 4.65e-16 | 252513825× | no |
| phase-diagram-get-decomposition | invariants | 1e-07 + 1e-07 × abs(reference) | 6.4e-16 | 184412447× | no |
| phase-diagram-get-e-above-hull | invariants | 1e-07 + 1e-07 × abs(reference) | 7e-11 | 57561877× | no |
| phase-diagram-get-element-profile | invariants | 1e-07 + 1e-07 × abs(reference) | 1.45e-15 | 69638489× | no |
| phase-diagram-get-equilibrium-reaction-energy | pointwise | 1e-05 + 1e-07 × abs(reference) | 4.89e-14 | 5631808230× | no |
| phase-diagram-get-form-energy | pointwise | 1e-07 + 1e-07 × abs(reference) | 9.38e-16 | 123667187× | no |
| phase-diagram-get-get-chempot-range-map | pointwise | 1e-07 + 1e-07 × abs(reference) | 3.65e-14 | 21503059× | no |
| phase-diagram-get-hull-energy | pointwise | 1e-07 + 1e-07 × abs(reference) | 4.96e-16 | 215519926× | no |
| phase-diagram-get-hull-energy-per-atom | pointwise | 1e-07 + 1e-07 × abs(reference) | 4.64e-16 | 254046343× | no |
| phase-diagram-get-phase-separation-energy | pointwise | 1e-05 + 1e-07 × abs(reference) | 2.94e-09 | 26555592× | no |
| phase-diagram-get-reference-energy | pointwise | 1e-07 + 1e-07 × abs(reference) | 5.76e-16 | 180743330× | no |
| phase-diagram-get-transition-chempots | invariants | 1e-07 + 1e-07 × abs(reference) | 1.09e-15 | 91952274× | no |
| phase-diagram-getmu-range-stability-phase | pointwise | 1e-07 + 1e-07 × abs(reference) | 1.31e-15 | 90160299× | no |
| phase-diagram-getmu-vertices-stability-phase | invariants | 1e-07 + 1e-07 × abs(reference) | 1.36e-15 | 74300436× | no |
| phase-diagram-inner-hull-reduction | invariants | 1e-05 + 1e-07 × abs(reference) | 7.57e-13 | 204841143× | no |
| phase-diagram-planar-inputs | invariants | 1e-07 + 1e-07 × abs(reference) | 0 | not measured | yes |
| phase-diagram-stable-entries | invariants | 1e-07 + 1e-07 × abs(reference) | 4.64e-16 | 254046343× | no |
| reaction-diagram-formula | pointwise | 1e-07 + 1e-07 × abs(reference) | 7.64e-15 | 21588061× | no |
| reaction-diagram-get-compound-pd | pointwise | 1e-07 + 1e-07 × abs(reference) | 7.78e-15 | 21188282× | no |
| transformed-p-d-entry-composition | pointwise | 1e-07 + 1e-07 × abs(reference) | 3.75e-15 | 427007965× | no |
| transformed-p-d-entry-get-energy | pointwise | 1e-07 + 1e-07 × abs(reference) | 4.02e-16 | 253327479× | no |
| transformed-p-d-entry-get-energy-per-atom | pointwise | 1e-07 + 1e-07 × abs(reference) | 4.11e-16 | 250267968× | no |
| transformed-p-d-entry-normalize | pointwise | 1e-07 + 1e-07 × abs(reference) | 1.76e-15 | 372036491× | no |
