# AMR Magnetic Reconnection Test

This is the AMR counterpart of the uniform reconnection test
(`tests/reconnection`): the same `FadeevIC` equilibrium, but on a **two-level
AMR grid** that refines the thin layer around the central reconnecting current
sheet, so the reconnection zone is resolved more finely than the surrounding
region at a fraction of the uniform-grid cost.

Two solver variants share this grid and setup — see `#SOLVEEM` / `#HYBRIDPIC`:

- **Full PIC** (`PARAM.in`): Maxwell + implicit GMRES solve.
- **Hybrid PIC** (`PARAM.in.hybrid`): kinetic ions + fluid electrons

The runner auto-detects `PARAM.in.hybrid` and executes both variants, listing
them as `RECONNECTION_AMR` and `RECONNECTION_AMR (HYBRID)` in
`tests/summary.md`.

## Grid hierarchy

The grid is built against a **2D AMReX library** (`AMReX_SPACEDIM = 2`,
`./Config.pl -amrex2d -lev=2`); only `x` and `y` exist (`x` = periodic drive
direction, `y` = current-sheet normal).  Two levels:

| Level | Cells `(nx, ny)` | Resolution `(dx, dy)` |
|-------|------------------|-----------------------|
| 0 (base) | `32 x 16` | `1.9625 d_i` |
| 1 (fine) | `64 x 16` | `0.9813 d_i` |

- Box: `Lx = 2*pi*L*num_islands ~ 62.8 d_i`, `Ly = Lx/2 ~ 31.4 d_i`.
- The base grid matches the uniform reconnection test; level 1 refines the
  current-sheet layer to **2x resolution** (`#REFINEMENTRATIO 2`).
- `#MAXBLOCKSIZE 16 2` makes the refined layer align with whole base blocks so
  it refines cleanly.
- The refined layer (`|y| < 7.85 d_i`) covers the current sheet (`L = 5 d_i`)
  with margin; the ion inertial length `d_i ~ 1` is resolved at `0.98 d_i` on
  the fine level.

## Refinement criteria

- **`#REGION`** `layer` = box `x in [-31.4, 31.4]`, `y in [-7.85, 7.85]`: the
  central current-sheet layer (full `x`, thin `y`).
- **`#REFINEREGION`** `iLev 0 +layer`: tags that layer for refinement at level 0
  (`iLev` must be `< max_level`, so with `nLevMax = 2` it tags level 0).
- **`#REFINEMENTRATIO 2`**: each tagged coarse cell becomes `2 x 2` fine cells.
- **`#GRIDEFFICIENCY 1.0`**: only base blocks lying **entirely** inside the
  layer refine.  With `maxBlockSizeY = 2` this yields a clean, block-aligned
  refined band `|y| < 7.85 d_i` (no partially-refined blocks).

## Boundary conditions

- **`#PERIODICITY`**: periodic in `x`, not in `y`.
- **`#FIELDBOXBOUNDARY`**: `periodic` on the `x` faces, **`outflow`** (float B)
  on the `y` faces — the same BC as the uniform reconnection test.  The sheet
  (`L = 5 d_i`) is far from the `y`-walls (`Ly/2 = 15.7 d_i`), so the walls and
  the refined band (`|y| < 7.85 d_i`) do not interact.

## Initial condition

`FadeevIC` seeds the same force-free Fadeev equilibrium and m=1 perturbation as
the uniform reconnection test (`L = 5`, `eps = 0.4`, `num_islands = 2`,
`bg = 0`, `perturb = -0.05`, `nb = 0.2`), on both levels.

## Solver variants

| Parameter | Full-PIC | Hybrid |
|-----------|------------------------|----------------------------|
| `#SOLVEEM` | `T` | `F` |
| `#HYBRIDPIC` | `F` | `T` |
| `#PLASMA` | 2 species (ions + electrons, `m_i/m_e=25`) | 1 species (ions only; fluid electrons) |
| `#RESISTIVITY` | — | `4.0e5 m^2/s` (etaCode ~ 0.001) |
| `#ELECTRONTEMPERATURE` | — | `5 eV`, isothermal |
| `#HYPERRESISTIVITY` | — | grid mode, `C_h = 0.001` |
| `#BSUBCYCLE` | — | `4` |
| `#FIELDINTEGRATOR` | — | `rk4` |
| `#AVGFIELDB` | — | `T`, `nAvgFieldB = 20` |
| `#PARTICLES` | `4 4` (16 macroparticles/cell/species) | `5 5` (25 macroparticles/cell) |
| `#TIMESTEPPING dt` | `0.005` (semi-implicit; resolves electron timescale for `m_i/m_e=25`) | `0.02` (explicit; ion-scale step) |
| `#STOP TimeMax` | `3` | `20` (ion-scale onset is slower) |

## Validation

The automated check (`validate.py`) verifies the same criteria for both
variants, with solver-aware thresholds:

1. **Energy-log sanity**: finite, bounded `Eb` (no blow-up), finite `Epart`
   growing as magnetic energy is converted into kinetic energy.
2. **AMR refinement present**: coarse + fine cell spacings in the `z=0` `.out`
   plots, with the fine cells forming the central current-sheet layer.
3. **Active reconnection**: on the refined midplane `y ~ 0`, the equilibrium
   X-point (By null near `x ~ 0`) is present at `t=0`, and the seeded m=1
   perturbation grows (midplane `By` profile changes by `> 0.05`).
4. **Perturbation growth**: `|delta By|` grows past a solver-aware threshold —
   `0.1` for full-PIC, `0.05` for hybrid (the ion-scale onset is slower but the
   run is longer).

The hybrid test is **not** expected to reproduce the full-PIC reconnection rate
quantitatively (different electron physics), but it must show a stable, bounded
run with clear reconnection signatures.

## Running

This test needs the **2D AMReX** build with AMR support (`-amrex2d -lev=2`; see
`tests/README.md` for the build steps).  Then run (both variants together):

```bash
python3 tests/validate_tests.py --test=reconnection_amr
```

To run a **single variant**, append its token to the test name:

```bash
python3 tests/validate_tests.py --test=reconnection_amr.full     # full PIC only
python3 tests/validate_tests.py --test=reconnection_amr.hybrid   # hybrid only
```

See `Doc/AMR_hybrid_implementation_plan.md` for the full analysis of the AMR +
hybrid integration, including the cell-centred AMR method adaptation plan.
