# Global cutoff-rigidity map calculation

## Purpose

`scripts/run_global_cutoff_maps.py` calculates complete, spacecraft-independent
vertical cutoff-rigidity maps for the December 2006 event. It is intentionally
separate from the C9/PAMELA and C10/POES validation operators. Those tests
sample the model in observation-equivalent ways; this workflow samples the
entire geographic shell for the research analysis.

The calculation uses the same frozen IGRF+TS05 magnetic snapshots, five-minute
driver, RK4 backtracing, Mode3D field interpolation, and three-state trajectory
classification as the validated morphology workflow. No electric field is
included. An allowed trajectory reaches the configured outer boundary, a
forbidden trajectory returns to the atmosphere, and an unresolved trajectory
remains state 2 and is excluded from numerical cutoff inversion.

## Scientific and execution-test grids

The checked-in overlay `config/global_cutoff_maps.json` changes only controls
needed by the global product; the event and numerical settings continue to
come from `config/study.json`.

| Quantity | SMOKE execution test | ROUTINE/FULL scientific grid |
|---|---:|---:|
| GEO longitude | 0--330 degrees, 30-degree spacing | 0--350 degrees, 10-degree spacing |
| GEO latitude | -90--90 degrees, 10-degree spacing | -90--90 degrees, 2-degree spacing |
| Geodetic altitude | 475 and 850 km | 475 and 850 km |
| Rigidity | 17 values, 0.025--20 GV | 53 values, 0.025--20 GV |
| Epoch selection | Quiet and main phase | Profile cadence and landmarks |
| Sampling | Vertical | Vertical |
| Mover | RK4 | RK4 |
| Field | IGRF+TS05, refreshed at every epoch | IGRF+TS05, refreshed at every epoch |
| Electric field | Off | Off |

The broad rigidity range is essential. The observation-oriented 0.15--1.25 GV
grid cannot bracket typical low-latitude or equatorial LEO cutoffs, which can
exceed 10 GV. The nonuniform global list retains fine low-rigidity sampling and
extends beyond the nominal equatorial cutoff. `BELOW_RANGE` and `ABOVE_RANGE`
remain valid censored outcomes rather than being replaced by the nearest grid
endpoint.

`RIGIDITY_LIST` creates one trajectory for every longitude, latitude, shell,
and rigidity tuple. The publication grid therefore contains 347,256
trajectories per epoch. Applying that same grid to the original four-epoch
SMOKE selection required 1,389,024 trajectories, which was not a practical
runner test. The global-map overlay now applies a SMOKE-only reduction to two
epochs, a 30-by-10-degree complete globe, and 17 rigidity samples. This is
7,752 trajectories per epoch and 15,504 total, approximately 90 times fewer
than the former SMOKE workload.

The reduced grid deliberately preserves the full globe, both altitudes, the
low- and high-rigidity limits, and two distinct magnetic snapshots. It tests
native cross-epoch and cross-shell mesh reuse, field refresh, three-state
parsing, R50 inversion, quiet-to-storm differencing, and figure production.
It is not suitable for publication claims about spatial gradients, extrema,
storm timing, or recovery; use ROUTINE/FULL for those purposes.

## Guaranteed mesh reuse

The dedicated runner does not expose `PER_EPOCH` or `STANDALONE` as production
layouts. It always invokes `run_morphology.py` with `--mesh-layout BATCHED` and
requires at least two epochs per batch.

Within one native AMPS process:

1. Mode3D allocates the AMR topology once.
2. `SNAPSHOT_LIST` advances through up to eight epochs by default.
3. IGRF, TS05, Geopack state, and compact interpolation arrays are recomputed
   for every epoch. Magnetic-field values are never reused across time.
4. `SHELL_COUNT 2` evaluates both altitudes against the same epoch field.
5. Each epoch receives a deterministic, UTC-bearing raw filename.

The runner fails its model-stage contract unless the morphology result records
`BATCHED`, cross-shell reuse, cross-epoch reuse, per-epoch field refresh, and
one map for every shell/epoch combination.

## Commands

Installation/pipeline calculation:

```bash
srcEarth/studies/dec2006_ts05_cutoff_erosion/scripts/run_global_cutoff_maps.py \
  --profile SMOKE --amps ./amps -np 4 -nt 16
```

Before AMPS starts, the runner prints the effective grid dimensions and its
estimated trajectory count. For the checked-in SMOKE profile this must report
`7,752/epoch x 2 epoch(s) = 15,504`. A substantially larger number indicates
that a custom overlay has replaced or disabled the SMOKE-only reduction.

Publication calculation:

```bash
srcEarth/studies/dec2006_ts05_cutoff_erosion/scripts/run_global_cutoff_maps.py \
  --profile FULL --keep --amps ./amps -np 4 -nt 16
```

Inspect all generated AMPS commands without running the solver:

```bash
srcEarth/studies/dec2006_ts05_cutoff_erosion/scripts/run_global_cutoff_maps.py \
  --profile FULL --stage model --prepare-only --amps ./amps -np 4 -nt 16
```

Rebuild first-pass maps from preserved raw AMPS files and then regenerate every
global analysis and figure without retracing particles:

```bash
srcEarth/studies/dec2006_ts05_cutoff_erosion/scripts/run_global_cutoff_maps.py \
  --profile FULL --stage model --stage postprocess --stage figures --reuse-raw
```

If the first-pass maps already exist and only analysis/visualization changed:

```bash
srcEarth/studies/dec2006_ts05_cutoff_erosion/scripts/run_global_cutoff_maps.py \
  --profile FULL --stage postprocess --stage figures
```

## Map reconstruction and quality control

At each geographic cell, resolved access states are sorted by rigidity and
fitted with an equal-weight nondecreasing isotonic curve. The reported
`cutoff_rigidity_r50_gv` is the interpolated rigidity where that curve crosses
0.5. Each cell retains its raw transition count, nonmonotonic transition count,
resolved fraction, bracket endpoints, and one of five statuses:

- `BRACKETED`: a numerical R50 lies inside the sampled range;
- `BELOW_RANGE`: all resolved samples are allowed;
- `ABOVE_RANGE`: all resolved samples are forbidden;
- `UNBRACKETED`: resolved penumbra states do not define a 0.5 crossing;
- `INCOMPLETE`: expected samples are missing, duplicated, or insufficient.

The global postprocessor verifies every configured geographic cell. Because a
regular shell writer may emit all longitudes at each pole, it collapses each
pole to one physical cell. Status disagreement and cutoff spread among the
collapsed records are preserved as quality diagnostics rather than averaged
away.

## Event-change analysis

For every altitude and physical GEO cell, the quiet reference is the median of
exactly bracketed maps before the configured compression-search window. The
signed change is

\[
\Delta R_c(t,\lambda,\phi,h)=R_c(t,\lambda,\phi,h)
 -\widetilde{R}_{c,\mathrm{quiet}}(\lambda,\phi,h),
\]

and the shielding decrease is

\[
E_R=\max(0,-\Delta R_c).
\]

Area means use spherical `cos(latitude)` weights and only exact bracketed
pairs. An event value below the sampled rigidity floor supplies a conservative
lower bound on the decrease and is explicitly flagged; it is not inserted into
an exact mean. The outputs locate the maximum decrease, its epoch, geographic
coordinates, AACGM latitude and MLT, and quantify the spatial area affected.

## Geographic-map visualization

The per-epoch products are continuous equirectangular maps rather than point
clouds. Canonical GEO longitudes are transformed from 0--360 degrees to
180 W--180 E, a cyclic copy of the first longitude column closes the date-line
seam, and Matplotlib `contourf` performs piecewise-linear interpolation only
between neighboring valid grid nodes. The plotting layer expands each single
canonical pole value across the display row without adding records to, or
changing statistics in, the numerical map.

Continental outlines come from the lightweight
`srcEarth/earth-continental-map.dat` data already distributed with AMPS; no
Cartopy, Basemap, network access, or external GIS installation is required.
Unresolved, unbracketed, and incomplete cells stay masked and appear gray.
`BELOW_RANGE` and `ABOVE_RANGE` cells use the lower and upper color-scale edge,
respectively, and this censoring convention is stated on the color bar. The
maximum-decrease publication map uses the same filled-map machinery and adds a
star only at the objectively identified global maximum.

## Output organization

The default root is:

```text
test_output/dec2006_ts05_cutoff_erosion/global_maps/
```

| Directory/file | Content |
|---|---|
| `morphology/shared_mesh/` | Raw batched AMPS inputs, logs, and access products |
| `morphology/alt_*km/*/cutoff_rigidity_map.csv` | First-pass R50 maps |
| `postprocessing/canonical_maps/` | Complete physical-shell maps with one cell per pole |
| `postprocessing/global_map_quality_summary.csv` | Grid, status, and pole diagnostics |
| `postprocessing/global_cutoff_event_change.csv` | Maximum event erosion per shell/GEO cell |
| `postprocessing/global_cutoff_change_timeseries.csv` | Spatial erosion metrics per shell/epoch |
| `postprocessing/global_cutoff_change_summary.json` | Largest-decrease locations and conventions |
| `figures/cutoff_rigidity_maps/` | One filled, coastlined two-shell PNG per epoch |
| `figures/figure_maximum_cutoff_decrease_map.*` | Publication spatial summary |
| `figures/figure_cutoff_decrease_evolution.*` | Publication temporal/spatial-extent summary |

SMOKE is appropriate for execution, mesh-reuse, and reduction/figure contract
verification. Its maps are intentionally coarse. ROUTINE/FULL use the
publication spatial and rigidity grid; the FULL cadence is required for
conclusions about time evolution, extrema, storm-phase response, and recovery.
