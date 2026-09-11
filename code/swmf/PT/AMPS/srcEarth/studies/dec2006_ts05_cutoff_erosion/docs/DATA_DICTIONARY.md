# Generated-product data dictionary

## Universal conventions

- UTC timestamps use `YYYY-MM-DDTHH:MM:SSZ`.
- Rigidity is in GV.
- Boundary latitude is the magnitude of AACGM latitude unless a signed
  coordinate is explicitly named.
- Residual is always `model - observation`.  Positive residual therefore means
  excessive modeled shielding, or a boundary that is too poleward.
- Positive lag means that the cutoff diagnostic follows the driver.
- Unresolved access state is `2` and is never treated as forbidden.

## Mesh-reuse execution records

`morphology/command_inventory.json` contains one row per actual MPI launch,
including its ordered epoch list, shell-altitude list, execution layout, exact
command, and the explicit statement that the magnetic field is reinitialized at
every epoch. `morphology_result.json` distinguishes `n_cases` (logical
epoch-altitude products) from `n_amps_launches` (processes in the complete
plan) and `n_amps_launches_required_this_invocation` (processes not reused by
`--keep`). `epochs_per_batch` is the requested maximum number of snapshots in
one native BATCHED process; `epochs_per_batch_group` is retained as an equal
compatibility alias. `mesh_reused_across_epochs=true` never means field values
are frozen: `magnetic_field_reinitialized_each_epoch` must also be true.

`staged_observation_products.json` maps each shared single-shell file to the
historical C9 or C10 path at which it was analyzed. This is the provenance link
between the production command and the observation-specific `--skip-run`
result; the latter is not evidence of an additional AMPS execution.
Every staged sample also has `SHARED_MODEL_PRODUCT.json`, containing its source
path, SHA-256 digest, epoch, altitude, layout, and explicit execution status.

`mesh_reuse_access_comparison.csv` and `mesh_reuse_equivalence.json` are written
by `verify_mesh_reuse.py`. They record access-grid closure, resolved-state
agreement, unresolved fractions, and derived-boundary equality between a
shared-mesh run and the `STANDALONE` baseline.

## `paired_model_observation.csv`

| Column | Meaning |
|---|---|
| `dataset` | `PAMELA_TABLE_S1` or `NOAA_POES_METOP_SEM2` |
| `epoch_utc` | Orbit/window midpoint used for pairing |
| `rigidity_gv` | PAMELA bin center or MEPED nominal lower-threshold rigidity |
| `channel` | Empty for PAMELA; P6--P9 for MEPED |
| `hemisphere` | `ABS_MEDIAN_NS` for PAMELA or N/S for POES/MetOp |
| `mlt_hour` | Empty for PAMELA or center of the three-hour MLT sector |
| `observed_boundary_aacgm_deg` | Observation-derived T50 latitude magnitude |
| `modeled_boundary_aacgm_deg` | AMPS observation-equivalent ACCESS_T50 |
| `model_minus_observation_deg` | Signed paired residual |
| `validation_role` | Primary or diagnostic |
| `used_for_primary_metrics` | Whether the row enters the predeclared primary score |

## `morphology_boundaries.csv`

Each row is one epoch, altitude, rigidity, hemisphere, and MLT sector.  The
boundary is valid only when the resolved transmission brackets 0.5 and is at
least one degree inside the retained AACGM latitude range.

## Per-epoch `cutoff_rigidity_map.csv` and map manifest

Each map row is one GEO longitude/latitude cell on one altitude shell and
epoch. `cutoff_rigidity_r50_gv` is the 0.5 crossing of an equal-weight isotonic
fit to the resolved exact-rigidity access states. It is populated only for
`cutoff_status=BRACKETED`. `BELOW_RANGE` and `ABOVE_RANGE` are one-sided
censoring classifications; `UNBRACKETED` identifies a mixed access sequence
whose fitted endpoints do not bracket 0.5; `INCOMPLETE` identifies missing,
duplicate, unexpected, or insufficient resolved samples. The lower/upper
bracket, bracket span, resolved fraction, transition count, and forbidden-
after-allowed nonmonotonic count quantify map reliability. Unresolved state 2
is excluded, never recoded.

`morphology/cutoff_rigidity_map_manifest.csv` has one row per shell and epoch,
gives the relative source path and sampled rigidity limits, and closes the
counts of all five map statuses.

## `morphology_harmonics.csv`

`mean_latitude_deg`, `amplitude_deg`, and `phase_mlt_hour` describe the first
MLT harmonic. `second_harmonic_amplitude_deg` and
`second_harmonic_phase_mlt_hour` describe the semidiurnal deformation;
`fit_rms_deg` and `two_harmonic_fit_rms_deg` retain both fit qualities. The
accessible-area fraction is the fraction of the configured 35--85 degree
analysis band poleward of the T50 boundary, averaged over valid MLT sectors.
`accessible_area_equivalent_km2` converts that fraction to a spherical-shell
area at the modeled altitude; it is a comparison metric, not an assertion that
AACGM is an equal-area coordinate system.

## `cutoff_dynamics_timeseries.csv`

Adds a quiet reference, `cutoff_erosion_deg`, and centered finite-difference
boundary speed. The erosion is `mean_latitude_deg -
quiet_reference_mean_latitude_deg`; negative erosion is equatorward motion and
reduced shielding. Grouping is preserved by altitude, rigidity, and hemisphere
so values from unlike observation operators or physical shells are never mixed.
`cutoff_degradation_deg=max(0,-cutoff_erosion_deg)` is the corresponding
non-negative loss-of-shielding magnitude.

## `boundary_cell_dynamics.csv`

Preserves every epoch/altitude/rigidity/hemisphere/MLT boundary cell. Each row
adds its exact precompression quiet-cell median, signed erosion, non-negative
degradation, event phase, and linearly sampled `Pdyn`, IMF `Bz`, `SYM-H`, and
TS05 `W1`--`W6`. This is the traceable source for local-time evolution figures;
harmonic averages are not substituted for missing cells.

## `altitude_response.csv`

Pairs the lowest and highest modeled shells at identical epoch, rigidity, and
hemisphere. `high_minus_low_boundary`, `high_minus_low_erosion`, and
`high_minus_low_accessible_fraction` quantify altitude dependence without
mixing physical keys.

## `storm_extrema_summary.csv` and `recovery_timescales.csv`

The extrema table gives minimum/maximum mean boundary, peak non-negative
degradation, the associated epochs, and maximum first/second harmonic
amplitudes for every altitude/rigidity/hemisphere series. Recovery times are
linearly interpolated first crossings of one-half and `1/e` of the post-main-
phase peak degradation. `status` is `AVAILABLE` only with at least six recovery
epochs and both crossings; otherwise it is `DIAGNOSTIC_ONLY`.

## `cutoff_map_event_change.csv`

One row per shell/GEO cell with a usable precompression quiet reference and at
least one event decrease estimate. The quiet value is the median of BRACKETED
R50 values before the configured compression-search start. The table reports
the minimum event cutoff (or upper bound), maximum quiet-relative decrease,
the epoch and AACGM/MLT coordinates of that maximum, and the exact/censored
event counts. `maximum_decrease_is_lower_bound=true` means the event R50 fell
below the sampled floor, so the reported positive decrease is conservative.

## `cutoff_map_change_timeseries.csv`

One row per shell and epoch. `area_weighted_mean_cutoff_change_gv` uses only
exact BRACKETED pairs with spherical `cos(latitude)` weights. Median, 90th-
percentile, maximum, and threshold-area decrease statistics additionally
retain conservative lower bounds from event `BELOW_RANGE` cells. Exact and
censored cell counts and the map-coverage fraction prevent a large decrease
from being interpreted without its spatial support. The companion
`cutoff_map_change_summary.json` records the largest decrease location for each
shell and the precise sign/censoring convention.

## `lag_correlations.csv`

Contains every predeclared driver, lag, altitude, rigidity, and hemisphere.
Intervals use a moving-block bootstrap with the configured three-hour block.
The raw five-minute count is not treated as independent sample size.
`inference_status` is `DIAGNOSTIC_ONLY` below 24 paired model epochs, in which
case bootstrap limits are intentionally blank. `best_lag_summary.csv` selects
the maximum absolute finite correlation per driver and physical series while
retaining the paired count, confidence limits, and status; the complete lag
curve remains authoritative.

## `hysteresis_pairs.csv` and `hysteresis_summary.csv`

Pairs use identical altitude, rigidity, hemisphere, and MLT.  `SYMH_ONLY`
requires the configured 10-nT match.  `STRICT` additionally requires dynamic
pressure within 20% and IMF Bz within 2 nT.  The reported contrast is recovery
minus main phase; its physical sign must be interpreted together with the event
driver and confidence interval.

## `analysis_availability.csv` and `.json`

Machine-readable interpretation contract for rigidity dependence, altitude
dependence, MLT morphology, accessible area, driver lag, matched-driver
hysteresis, recovery time, TS05 driver attribution, and directional topology.
The only allowed states are `AVAILABLE`, `DIAGNOSTIC_ONLY`, and
`NOT_AVAILABLE`. A SMOKE archive is expected to contain a mixture of these
states; sparse temporal diagnostics are never silently promoted to scientific
inference.

## Dedicated global-shell products

`global_maps/postprocessing/canonical_maps/cutoff_rigidity_map_manifest.csv`
indexes one complete physical spherical map for every epoch and altitude. Pole
longitudes emitted by the rectangular sampling grid are collapsed to one cell
at each pole. Every canonical map retains the original R50 value and status,
the number of collapsed coordinate records, a pole-status consistency flag,
and the cutoff spread among coincident pole records.

`global_map_quality_summary.csv` records expected and actual cell counts, grid
closure, fractions in every cutoff status, and pole diagnostics. A missing or
unexpected coordinate makes the postprocessing stage fail.

`global_cutoff_event_change.csv` and
`global_cutoff_change_timeseries.csv` use the same sign and censoring
conventions as `cutoff_map_event_change.csv` and
`cutoff_map_change_timeseries.csv`, but their source is the complete physical
shell rather than the high-latitude observation-facing grid.

The associated global-map figures use a cyclic filled longitude/latitude
field. Plotting converts 0--360-degree GEO longitude to 180 W--180 E and
duplicates only the date-line display column. Canonical pole records remain
single physical cells in every CSV; their value is expanded across longitude
only while drawing. Gray regions identify masked non-numerical cutoff states,
not zero rigidity. Continental outlines are read from the AMPS source-tree
`earth-continental-map.dat` file and do not enter any numerical reduction.
