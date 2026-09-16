# GRFFradioSun

Synthetic radio free-free emission from a MAS coronal model using GRFF.

## Requirements

- **Python**: `psipy`, `numpy`, `matplotlib`, `astropy`, `xarray`
- **Data**: MAS model in `corona/` (e.g. `rho002.hdf`, `t002.hdf`, `br002.hdf`, etc.)
- **GRFF**: `GRFF/binaries/GRFF_DEM_Transfer.so`
- **Optional GPU**: NVIDIA CUDA + CuPy (example: `pip install cupy-cuda12x`)

## Layout

- `raytracingGRFF/`: installable Python package (ray tracing and LOS sampling code; includes `grff_parms.py` for GRFF `PyGET_MW` voxel layout)
- `script/`: runnable workflows
- `fastGRFF/`: external module; not included in `raytracingGRFF` packaging

## Environment

```bash
source /home/pjzhang/miniconda3/etc/profile.d/conda.sh
conda activate lwa
```

## Scripts

### `script/resampling_MAS_LOS.py`

Non-raytracing baseline: resample MAS along straight LOS and write `LOS_data.npz`.

Example:

```bash
python script/resampling_MAS_LOS.py -m ./corona -o LOS_data_75MHz.npz --dz0 7e-4 -f 2.2
```

Parameters:

| Flag | Type | Default | Description |
|---|---|---|---|
| `-m`, `--model-path` | `str` | `./corona` | Path to MAS model directory. |
| `-n`, `--N-pix` | `int` | `256` | Image size `N_pix x N_pix`. |
| `-f`, `--X-FOV` | `float` | `1.44` | Half field-of-view in `R_sun`; x,y in `[-X-FOV, X-FOV]`. |
| `-z`, `--N-z` | `int` | `400` | Number of samples along each LOS. |
| `-d`, `--dz0` | `float` | `3e-4` | Initial spacing for irregular z-grid, in `R_sun`. |
| `-v`, `--no-variable-spacing-z` | flag | `False` | Use regular linear z spacing instead of irregular spacing. |
| `-zr`, `--z-range` | `min,max` | `None` | Z extent in `R_sun` for linear spacing mode. |
| `-o`, `--out-path` | `str` | `LOS_data.npz` | Output LOS `.npz` path. |
| `-p`, `--no-plots` | flag | `False` | Disable LOS profile/slice plots. |
| `-q`, `--quiet` | flag | `False` | Suppress progress messages. |

Note: `--dz0` is in `R_sun`; `7e4` is invalid for this use case. Use values like `7e-4`.

### GRFF `PyGET_MW` parameters (kappa / non-thermal)

Updated GRFF expects **17** doubles per LOS voxel (Fortran `(17, Nz)`), matching GRFF `InSize_ext` and `getparms` `arr3`. Summary:

| Row | GRFF name | Role in this repo |
|-----|-----------|-------------------|
| 0–7 | `dR`, plasma, angles, mechanism, `s_max` | `ds`, `T_e`, `N_e`, `B`, viewing angle, emission flags |
| 8–13 | neutrals, DEM/DDM keys, abundance | Usually zero (solar ionization when T_e is below 1e5 K) |
| 14 | `S_loc` | Source area (cm²); ray workflow: `S ×` pixel area when `--s-input-on` |
| 15 | `Dist_E` | **0** Maxwellian, **1** kappa, **2** n-distribution |
| 16 | `kappa` | Kappa or n index when `Dist_E` is 1 or 2 |

Python helper: `raytracingGRFF.grff_parms.fill_grff_parms_ext_column`.

The optional **fastGRFF** GPU backend uses a fixed **15**-parameter layout; use **`--grff-backend get_mw`** when `Dist_E` or `kappa` must be non-zero.

### `script/synthetic_FF_map_single_thread.py`

Non-raytracing baseline: compute synthetic free-free map from LOS `.npz` using GRFF.

Example:

```bash
python script/synthetic_FF_map_single_thread.py -i LOS_data_75MHz.npz -o emission_map
```

Parameters:

| Flag | Type | Default | Description |
|---|---|---|---|
| `-i`, `--input` | `str` | `LOS_data.npz` | Input LOS `.npz` file. |
| `-o`, `--output` | `str` | `emission_map` | Output base path (no extension). |
| `-f`, `--freq0` | `float` | `450e6` | Start frequency in Hz. |
| `-n`, `--Nfreq` | `int` | `4` | Number of frequency channels. |
| `-s`, `--freq-log-step` | `float` | `0.1` | `log10` step between frequencies. |
| `--do-inspection-plot` | flag | `False` | Save center-pixel LOS inspection plot. |
| `--grff-dist-e` | `float` | `0.0` | GRFF `Dist_E`: 0 Maxwellian, 1 kappa, 2 n-distribution. |
| `--grff-kappa` | `float` | `0.0` | Kappa / n index when `--grff-dist-e` is 1 or 2. |

### `script/resample_with_ray_tracing.py`

Raytracing workflow: resample MAS onto cube, trace rays, sample LOS, then run GRFF.

Example (CPU):

```bash
python script/resample_with_ray_tracing.py -m ./corona -o ray_tracing_emission.npz --device cpu --raytrace-device cpu
```

Example (GPU + external `fastGRFF` backend):

```bash
python script/resample_with_ray_tracing.py --device cuda --raytrace-device cuda --grff-backend fastgrff
```

Parameters:

| Flag | Type | Default | Description |
|---|---|---|---|
| `-m`, `--model-path` | `str` | `./corona` | MAS model directory. |
| `-n`, `--N-pix` | `int` | `64` | Image size `N_pix x N_pix`. |
| `-f`, `--X-FOV` | `float` | `1.44` | Half FOV in `R_sun`. |
| `--freq` | `float` | `75e6` | Ray frequency in Hz. |
| `--grid-n` | `int` | `128` | 3D cube points per axis. |
| `--grid-extent` | `float` | `3.0` | Cube extent in `R_sun` (`[-extent, extent]`). |
| `--z-observer` | `float` | `3.0` | Ray start z in `R_sun`. |
| `--dt` | `float` | `6e-3` | Ray integration timestep. |
| `--n-steps` | `int` | `5000` | Number of ray integration steps. |
| `--record-stride` | `int` | `10` | Record every N integration steps. |
| `-w`, `--workers` | `int` | `1` | Processes for parallel ray tracing. |
| `-o`, `--out-path` | `str` | `ray_tracing_emission.npz` | Output `.npz` path. |
| `--grff-lib` | `str` | `GRFF/binaries/GRFF_DEM_Transfer.so` | Path to GRFF shared library. |
| `--grff-backend` | `str` | `get_mw` | `get_mw` or `fastgrff`. |
| `--s-input-on` | flag | `False` | Pass cross-section ratio `S` into `Parms[14]`. |
| `--grff-dist-e` | `float` | `0.0` | GRFF `Dist_E` (0 Maxwellian, 1 kappa, 2 n); requires `--grff-backend get_mw` if non-zero. |
| `--grff-kappa` | `float` | `0.0` | GRFF kappa / n index (`Parms[16]`) when `Dist_E` is 1 or 2. |
| `--device` | `str` | `cpu` | LOS sampler device: `cpu` or `cuda`. |
| `--raytrace-device` | `str` | `cpu` | Ray integrator device: `cpu` or `cuda`. |
| `--no-fallback` | flag | `False` | Disable CUDA-to-CPU fallback. |
| `--no-plots` | flag | `False` | Disable emission map plot output. |
| `--phi0-offset` | `float` | `0` | Longitude offset (deg) for MAS spherical coords; see below. |
| `-q`, `--quiet` | flag | `False` | Reduce log output. |

### MAS coordinate system and `phi0_offset`

MAS (PSI *Magnetohydrodynamics Around a Sphere*) stores fields on a **Carrington spherical grid** \((r, \theta, \varphi)\): radius in \(R_\odot\), **co-latitude** \(\theta\) (0 at north pole), longitude \(\varphi \in [0, 2\pi)\). In Python/psipy, arrays have shape **`(N_φ, N_θ, N_r)`**; psipy exposes **latitude** \(=\pi/2 - \theta\) when sampling. See [psi-io](https://predsci.com/doc/psi-io/guide/overview.html) and [psipy](https://psipy.readthedocs.io/en/stable/guide/getting_started.html).

**PSI Cartesian** (used by pyvisual and standard spherical↔Cartesian formulas): **\(+\hat z\) points to solar north**. With co-latitude \(\theta\) and Carrington longitude \(\varphi\),

```text
X = r sinθ cosφ,   Y = r sinθ sinφ,   Z = r cosθ
```

**GRFFradioSun Cartesian frame** is heliocentric (HCC), used with no axis permutation:

```text
+x  solar west
+y  projected solar north
+z  toward the observer
```

Sky-plane \((x, y)\) is helioprojective. Rays launch from observer-side \(+z\) toward \(-z\). The single conversion is `raytracingGRFF.coords.cart_to_sph(x, y, z, phi0_offset)`:

```text
r     = sqrt(x² + y² + z²)
colat = arccos(y / r)                 # north = +y
lon   = arctan2(x, z) + phi0_offset   # west = +x, observer = +z
lat   = 90° − colat
```

This is PSI Cartesian \((X, Y, Z) = (z, x, y)\) with PSI \(+\hat Z\) = solar north. Disk center \((x=0, y=0, z>0)\) has Carrington longitude \(\varphi =\) `phi0_offset`.

**`phishift`** in a MAS `omas` file is the longitude offset used when the model was run (e.g. `phishift=0` for `corona2298`). **`phi0_offset`** is a separate rotation applied at sampling time to align the synthetic image with an observer.

| `--phi0-offset` | Effect |
|-----------------|--------|
| `0` (ray-tracing CLI default) | Disk center at MAS \(\varphi = 0°\); not Earth-aligned on a given date. |
| `≈ L0` | Earth-aligned: disk-center Carrington longitude matches the observation. **L0** = Carrington longitude of disk center (SunPy `sun.L0`). |

```python
from astropy.time import Time
from sunpy.coordinates import sun

t = Time("2025-06-08T20:07:00", scale="utc")
phi0_offset = sun.L0(t).to_value("deg")   # L0 ≈ +141°
```

Pass to LOS / ray-tracing, e.g. `--phi0-offset 141`. For **`corona2298`** (CR 2298), `script/pub/` scripts default to **`PHI0_EARTH_CORONA2298 = 141°`**. (Older checkouts used `cart_to_sph(x, -z, y)` and `--phi0-offset -129`, which sampled the same Carrington disk center.)

**COROTATING** models (`calculation_frame='COROTATING'` in `omas`) use a grid that co-rotates with the Sun; \(\varphi\) is Carrington longitude. Match the observation epoch to the model Carrington rotation when comparing to data.

### Plot-time beam convolution (pub LOS vs ray figures)

`script/pub/compare_LOS_raytracing.py` and `compare_LOS_raytracing_highband.py` optionally smooth maps at plot time (`--plot-consider-beam`, default on):

- **FWHM** [R☉] = `beam_factor / f[Hz]`
- Convolution uses Gaussian **σ** = FWHM / 2.355; overlay circle has **radius** FWHM / 2
- **Array size (pub convention):** `D[km] ≈ 32×10⁶ / beam_factor` → **`32e6` ≈ 1 km** (high band default), **`16e6` ≈ 2 km** (low band default)

Strict λ/D with `θ_FWHM[R☉] ≈ 64.5 / (D[km] · f[MHz])` would need `beam_factor ≈ 6.45×10⁷ / D[km]` (~2× larger). See `mem.md` for details.

## Validation

```bash
python -m pytest -q tests/test_gpu_raytrace.py
python bench_raytrace.py --n-pix 256 --n-steps 256
```
