# Package manifest

## Intended Git location

Place this directory at:

```text
srcEarth/studies/dec2006_ts05_cutoff_erosion/
```

All runners share the repository-level default output root:

```text
test_output/dec2006_ts05_cutoff_erosion/
```

The location is resolved by finding the nearest ancestor that contains the
`srcEarth` directory. When the package is used outside an AMPS checkout, the
launch directory is used as the fallback codebase root. Passing
`--output-root PATH` overrides the default without changing any scientific
configuration.

## Included scientific inputs and observations

| File | Records | Purpose |
|---|---:|---|
| `data/drivers/ts05_dec2006_5min.txt` | 865 | Five-minute IGRF+TS05 event driver |
| `data/observations/pamela_table_s1.csv` | 259 | PAMELA orbit-scale cutoff latitude |
| `data/observations/poes_metop_meped_boundaries.csv.gz` | 3,904 | POES/MetOp MLT-resolved boundary cells |
| `inputs/AMPS_PARAM_DEC2006_475km.in` | 1 | Common-rigidity morphology at PAMELA altitude |
| `inputs/AMPS_PARAM_DEC2006_850km.in` | 1 | Common-rigidity morphology at POES/MetOp altitude |
| `inputs/AMPS_PARAM_DEC2006_multishell.in` | 2 shells | Shared-mesh 475/850-km production template |
| `config/global_cutoff_maps.json` | overlay | Complete-sphere 0.025--20-GV production controls and isolated reduced SMOKE grid |

Expected hashes, row counts, temporal coverage, and source descriptions are in
`data/provenance.json`. The `vendor/C9` and `vendor/C10` directories contain
the observation-equivalent runners and source-preserving reconstruction tools.

## Runner and analysis programs

| Program | Output subdirectory |
|---|---|
| `scripts/run_study.py` | Entire shared run tree |
| `scripts/run_global_cutoff_maps.py` | `global_maps/` |
| `scripts/postprocess_global_cutoff_maps.py` | `global_maps/postprocessing/` |
| `scripts/make_global_cutoff_map_figures.py` | `global_maps/figures/` |
| `scripts/run_morphology.py` | `morphology/` |
| `scripts/verify_mesh_reuse.py` | User-selected equivalence-check directory |
| `scripts/compare_observations.py` | `comparison/` |
| `scripts/analyze_dynamics.py` | `dynamics/` |
| `scripts/make_figures.py` | `figures/` |
| `scripts/run_sensitivity_suite.py` | `ts05_sensitivity/` |

The enhanced `dynamics/` contract contains the original erosion, lag, and
hysteresis tables plus cell-resolved erosion/drivers, two-shell altitude
response, storm extrema, recovery times, best-lag summaries, and machine-
readable analysis availability. The required publication figure set contains
seven panels (cutoff degradation, peak degradation, MLT evolution, altitude
response, accessible area, maximum spatial R50 decrease, and R50-decrease
evolution), each in PNG, EPS, and PDF. In addition, every modeled epoch has a
PNG containing one cutoff-rigidity-map panel per shell, traced through
`cutoff_rigidity_map_figure_manifest.csv` to the per-shell numerical map. SMOKE
exercises all spatial products and figures; temporal inference is explicitly labeled
`DIAGNOSTIC_ONLY` until the configured 24-epoch minimum is met.

The independent global-map workflow uses the same multi-shell template and
native Mode3D batching but does not consume observation tables. Its merged
effective configuration, commands, return codes, mesh-reuse assertions,
canonical maps, event-change tables, and figures are preserved beneath
`test_output/dec2006_ts05_cutoff_erosion/global_maps/`.

Global shell figures are cyclic filled longitude/latitude maps with graticules
and continental outlines read from `srcEarth/earth-continental-map.dat`.
Invalid cutoff states remain masked rather than being converted to numerical
colors; no external GIS package or runtime map download is required.

## Verified invocation

From the AMPS repository root:

```bash
python3 srcEarth/studies/dec2006_ts05_cutoff_erosion/scripts/run_study.py \
  --profile SMOKE --prepare-only --amps ./amps -np 4 -nt 16
```

This creates generated inputs and command inventories beneath
`test_output/dec2006_ts05_cutoff_erosion/` without launching AMPS. The same
command without `--prepare-only` executes the SMOKE calculation. The default
`BATCHED` uses native `SNAPSHOT_LIST` to run up to 16 explicit epochs and both
shell altitudes in one AMPS process. Mode3D allocates the AMR topology once,
then rebuilds the IGRF+TS05 field and writes a uniquely suffixed access product
for every epoch. `PER_EPOCH` and `STANDALONE` remain available as compatibility
and equivalence baselines. If every raw product in an interrupted batch already
exists, add `--keep` to rerun shell splitting, observation reduction, dynamics,
and figures without repeating that AMPS launch.

For the independent global-map runner, checked-in SMOKE is intentionally
bounded at 15,504 fixed-rigidity trajectories: two epochs, two shells, a
30-degree-by-10-degree complete globe, and 17 rigidities. ROUTINE/FULL retain
the 10-degree-by-2-degree, 53-rigidity scientific grid. This profile-specific
reduction is applied only in the effective configuration written beneath the
global-map output root and cannot change the main study or validation runners.
