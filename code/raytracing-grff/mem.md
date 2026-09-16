# GRFFradioSun — project memory

Notes for future sessions (human + agent). Last updated after unifying HCC coordinates (`raytracingGRFF.coords`).

## Purpose

Synthetic radio **free–free** brightness maps and spectra from **MAS** coronal MHD models, using the **GRFF** Fortran library (`PyGET_MW` via `GRFF_DEM_Transfer.so`). Two main geometry modes:

1. **Straight LOS** — `LOS/resample_MAS_LOS.py` samples Ne, Te, B along vertical columns; `script/synthetic_FF_map_single_thread.py` or `LOS/grff_image_from_LOS.py` integrates with GRFF.
2. **Ray tracing** — `script/resample_with_ray_tracing.py` traces rays through an ω_pe grid, samples the model on curved paths, optional cross-section weighting (`--s-input-on`).

## Repo layout (sibling tree)

Typical checkout under `dev/`:

```
dev/
  GRFF/              # GRFF sources + binaries/GRFF_DEM_Transfer.so
  GRFFradioSun/      # this repo
  corona2298/        # MAS HDF snapshots (not in git)
  kappatest/         # Maxwellian vs κ=4 tests (separate folder)
```

`PROJECT_ROOT` in scripts = parent of `GRFFradioSun` → `dev/`. GRFF `.so` path: `dev/GRFF/binaries/GRFF_DEM_Transfer.so`.

## Environment

- **Conda env `lwa`** is the working Python stack (psipy, astropy, GRFF ctypes).
- Loading `GRFF_DEM_Transfer.so` often needs **`LD_PRELOAD=$CONDA_PREFIX/lib/libgomp.so.1`** because the `.so` uses OpenMP but may not link `libgomp` in `DT_NEEDED` → otherwise `undefined symbol: omp_get_thread_num`.

## GRFF external parameters (important)

Updated GRFF uses **17 doubles per voxel** (Fortran `(17, Nz)`):

| Row | Name | Usage here |
|-----|------|------------|
| 0–7 | plasma, angles, mechanism | `ds`, `T_e`, `N_e`, `B`, viewing 90°, flags |
| 14 | `S_loc` | Ray workflow: `S × pixel_area` when `--s-input-on` |
| 15 | `Dist_E` | **0** Maxwellian, **1** kappa, **2** n-dist |
| 16 | `kappa` | Index when `Dist_E` ≠ 0 |

Python: `raytracingGRFF.grff_parms.fill_grff_parms_ext_column`, `rl_stokes_to_tb_vi`, `vi_plot_vmax`.  
Loader: `raytracingGRFF.grff_ctypes.initGET_MW`, `default_grff_lib_path()`.

**fastGRFF** GPU path is still **15-parameter** — use `--grff-backend get_mw` for non-Maxwellian `Dist_E` / `kappa`.

## MAS / coordinates

Reference: PSI *MAS Coordinate System* (internal PSI doc); public equivalents in [psi-io overview](https://predsci.com/doc/psi-io/guide/overview.html), [pyvisual coordinates](https://predsci.com/doc/pyvisual/guide/overview.html), [psipy getting started](https://psipy.readthedocs.io/en/stable/guide/getting_started.html).

### PSI MAS native grid

MAS solves MHD on a **staggered spherical grid** \((r, \theta, \varphi)\):

| Symbol | Name | Range | Units in HDF |
|--------|------|-------|--------------|
| \(r\) | radius | \(r \geq 1\,R_\odot\) | \(R_\odot\) |
| \(\theta\) | **co-latitude** (0 at north pole) | \([0, \pi]\) | radians |
| \(\varphi\) | **Carrington longitude** | \([0, 2\pi)\) | radians |

- NumPy / psipy array shape: **`(N_φ, N_θ, N_r)`** — φ slowest, \(r\) fastest ([psi-io](https://predsci.com/doc/psi-io/guide/overview.html)).
- psipy converts HDF co-latitude → **latitude** on read (`theta_lat = π/2 − theta_colat` in `psipy/io/mas.py`).
- Vector components: **`br`, `bt`, `bp`** (radial, co-latitude, longitude); scalars (`rho`, `te`, `t`, …) on cell corners.
- **PSI Cartesian** (visualization frame): \((X,Y,Z)\) with **`+Ẑ` = solar north** ([pyvisual](https://predsci.com/doc/pyvisual/guide/overview.html)). Standard map:
  - \(X = r\sin\theta\cos\varphi\), \(Y = r\sin\theta\sin\varphi\), \(Z = r\cos\theta\).

**Simulation frame** (`omas` namelist):

- `calculation_frame='COROTATING'`: grid co-rotates with the Sun; \(\varphi\) is **Carrington longitude** (not inertial).
- `phishift` (degrees): longitude offset **baked into the MAS run** when the model was generated (`phishift=0` for `corona2298`). This is **not** the same as `phi0_offset` below.

### GRFFradioSun observer / image frame

Cartesian is **heliocentric (HCC)** everywhere (`raytracingGRFF.coords`):

- \(+x\): solar west
- \(+y\): projected solar north
- \(+z\): toward the observer

\((x, y)\) is helioprojective. Observer at large \(+z\); rays launch toward \(-z\) at each \((x, y)\).

**Map to PSI Cartesian** (\(+Z_\mathrm{psi}\) = solar north): \((X, Y, Z)_\mathrm{psi} = (z, x, y)\). Call `cart_to_sph(x, y, z, phi0_offset)` with **no extra permutation**.

```text
r     = sqrt(x² + y² + z²)
colat = arccos(y / r)
lon   = arctan2(x, z) + phi0_offset  [deg → rad]
lat   = π/2 − colat                  [for psipy]
```

Sampling: `var.sample_at_coords(lon_deg, lat_deg, r * u.R_sun)`. Helper: `cart_to_mas_lonlat`.

**Do not** permute arguments (`(x, -z, y)` etc.). That was the old convention and disagrees with HCC.

### `phi0_offset` vs Carrington / Earth view

`phi0_offset` (degrees) is added to geometric longitude so **disk center** is sampled at Carrington longitude `phi0_offset`. It is **not** auto-derived from UTC in any script.

| Setting | Meaning |
|---------|---------|
| **`phi0_offset = 0`** | Disk center → MAS \(\varphi = 0°\). Fixed lab frame; **not** Earth central meridian on a given date. |
| **`phi0_offset ≈ L0`** | Earth-aligned map: disk center Carrington longitude matches observation. **L0** = apparent Carrington longitude of disk center (SunPy `sunpy.coordinates.sun.L0`). |

Carrington/Stonyhurst relation at disk center: \(\Phi_C \approx \Phi_S + L_0\) (see [SunPy coordinates](https://docs.sunpy.org/en/stable/reference/coordinates/index.html)).

**`corona2298` example** (CR 2298, `phishift=0`, run 2025‑06‑26):

- Observation **2025‑06‑08 20:07 UTC** → L0 ≈ **+141.3°** → **`--phi0-offset 141`** (`PHI0_EARTH_CORONA2298`).

The previous pub default **−129°** with `cart_to_sph(x, -z, y)` placed disk center at \(\varphi = -90° - 129° \equiv 141°\). Same Earth view; do not keep −129 with the new conversion.

```python
from astropy.time import Time
from sunpy.coordinates import sun

t = Time("2025-06-08T20:07:00", scale="utc")
phi0_offset = sun.L0(t).to_value("deg")
```

### Repo defaults (easy to confuse)

| Script / area | Default `phi0_offset` |
|---------------|----------------------|
| `resample_with_ray_tracing.py` CLI | `0` |
| `script/pub/*.py` | `141` (`PHI0_EARTH_CORONA2298`) |
| `kappatest` | `130` (was −140 under the old axis permutation) |
| `LOS/resample_MAS_LOS.py` | CLI does not pass it; function default `0` |
| `build_rays.py` `PHI0_OFFSET` | `0` |

Always pass **`--phi0-offset` explicitly** for publication-quality Earth alignment.

### Sampling guards

- **`r_min ≈ 1 R☉`**: mask inside photosphere; invalid → `NaN`.
- **Limb stability**: `np.hypot(x,y)` and `max(radicand, 0)` for `z_start` so float noise at \(\rho \approx R_\odot\) does not NaN the LOS.

## Brightness temperature & V/I

- GRFF returns Stokes-related flux in **SFU**; conversion to **T_b (K)** uses Rayleigh–Jeans + solid angle `pixel_area / AU²` — see `rl_stokes_to_tb_vi`.
- **V/I** at low signal or failed pixels: set **NaN** when `RL5+RL6 ≤ 0` or |V/I| > 1; plot scale uses **99th percentile** of |V/I| so outliers do not dominate the colorbar.

## CLI flags (kappa)

- `--grff-dist-e` / `--grff-kappa` on `synthetic_FF_map_single_thread.py`, `grff_image_from_LOS.py`, `resample_with_ray_tracing.py`.

## Plot-time Gaussian beam (`script/pub/compare_LOS_raytracing*.py`)

Optional beam convolution at **plot time only** (`--plot-consider-beam`, default on; `--no-plot-beam` to disable). Does not affect ray-tracing or LOS synthesis.

| Quantity | Definition |
|----------|------------|
| **FWHM** [R☉] | `beam_factor / f[Hz]` |
| **Gaussian σ** [pixels] | `(FWHM / pix_size_Rsun) / 2.355` |
| **White circle** | radius `FWHM / 2` (diameter = FWHM) |

**Pub array-size convention** (used for defaults and captions):

```text
D[km] ≈ 32×10⁶ / beam_factor
```

| `beam_factor` | Nominal array size | Script default |
|---------------|-------------------|----------------|
| `32e6` | ~1 km | high band (`compare_LOS_raytracing_highband.py`) |
| `16e6` | ~2 km | low band (`compare_LOS_raytracing.py`) |

**Verification:** with `FWHM = beam_factor / f`, the standard small-angle λ/D estimate `θ_FWHM[R☉] ≈ 64.5 / (D[km] · f[MHz])` would require `beam_factor ≈ 6.45×10⁷ / D[km]` (about **2×** the pub values). Pub defaults therefore use a **~½× narrower** FWHM than strict λ/D at the quoted `D`; the `32e6 ↔ 1 km` mapping is the project calibration above, not the full 64.5/(Df) formula unless `beam_factor` is doubled.

## Quality knobs (limb / resolution)

- Finer maps: increase **`N_pix`**, **`N_z`**, decrease **`dz0`** (irregular z grid starts at `dz0` in R☉; README warns `7e4` is wrong — use ~`2e-4`–`4e-4`).
- kappatest realistic defaults (external): 192×192, N_z=480, dz0=2e-4 on `corona2298`.

## What not to commit

- MAS directories (`corona2298/`), `*.npz`, `*.png`, `*.hdf`, CASA logs, `fastGRFF/` submodule clone, `build/`, `.pytest_cache/` — see `.gitignore`.

## Related work outside this repo

- **`kappatest/`** — `run_realistic_corona2298_kappa60MHz.py` + `run_realistic_lwa.sh` for Maxwellian vs κ=4 at 60 MHz.
- **`corhel/synth_radio_CME.py`** (if present in dev) forwards `--grff-dist-e` / `--grff-kappa` into ray tracing.
