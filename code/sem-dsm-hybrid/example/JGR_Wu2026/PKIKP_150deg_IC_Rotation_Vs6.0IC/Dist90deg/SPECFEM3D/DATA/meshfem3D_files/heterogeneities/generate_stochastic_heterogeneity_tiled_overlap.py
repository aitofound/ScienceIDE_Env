"""
==============================================================
Stochastic Heterogeneity Generation with Tiled Overlap
==============================================================

This script generates a 3D stochastic perturbation field using an
exponential autocorrelation function (ACF) with RMS scaling.

Tiles are generated sequentially with **right and top overlaps only**,
and the overlapping regions are blended using a smooth **cosine taper**
to ensure continuity across tile boundaries.

Limitations:
- Long-scale features larger than the tile dimension are NOT preserved.
  The exponential ACF is correctly realized only up to the scale of a tile
  plus its overlap.
- RMS is preserved locally in the overlap regions using theoretical scaling.

Outputs:
- "tiles/tomography_model_box{tile_id}.xyz"  (ASCII XYZ format)
- "tiles/tomography_model_box{tile_id}.vtk"  (VTK structured points)

==============================================================
"""

import numpy as np
import os
from numpy.fft import fftn, ifftn, fftfreq

# =====================================================
# PARAMETERS
# =====================================================
dx = dy = dz = 1500.0            # grid spacing (m). Typically samller than dx_eff.
Lx, Ly, Lz = 900e3,2000e3, 2000e3

tile_xy    = 250e3              # tile size (m)
overlap_xy = 40e3               # overlap width (m)

rms = 0.02                      # <<< NEW RMS
Lcorr = 12000.0                  # <<< NEW correlation length (m)
random_seed = 22345              # fixed seed for reproducible heterogeneity tiles
np.random.seed(random_seed)

VP_MIN, VP_MAX = 500.0, 14000.0
VS_MIN, VS_MAX = 300.0, 8000.0
RHO_MIN, RHO_MAX = 1000.0, 10000.0

# The maximum GLL spacing is available in the SPECFEM3D ouput file output_mesher.txt.
dx_eff = 1849.0  # <<< SEM effective grid spacing (features smaller than dx_eff filtered out)

# =====================================================
# GRID SIZES
# =====================================================
nx_tile = int(tile_xy / dx)
ny_tile = int(tile_xy / dy)
nz      = int(Lz / dz) + 1

nover = int(overlap_xy / dx)
assert nover > 0

# Only extend to the RIGHT and TOP
nx_ext = nx_tile + nover
ny_ext = ny_tile + nover

ntx = int(Lx / tile_xy)
nty = int(Ly / tile_xy)

# =====================================================
# GLOBAL CENTER SHIFT
# =====================================================
lat_center = 0
lon_center = 0

r_top = 1217100.0+50000.0
r_center = r_top - 50000.0 -250000.0 # Center radius

theta = np.radians(90.0 - lat_center)
phi   = np.radians(lon_center)

x_center = r_center * np.sin(theta) * np.cos(phi)
y_center = r_center * np.sin(theta) * np.sin(phi)
z_center = r_center * np.cos(theta)

x_shift = x_center - Lx / 2
y_shift = y_center - Ly / 2
z_shift = z_center - Lz / 2

# =====================================================
# OUTPUT
# =====================================================
os.makedirs("tiles", exist_ok=True)

# =====================================================
# COSINE TAPER (0 → 1) for overlap averaging
# =====================================================
# We use a smooth cosine taper to blend the overlapping edges between tiles.
# Without a taper, the overlap would abruptly switch from one tile to the next,
# creating discontinuities (jumps) in the field.
# w[i] = 0 at the start of the overlap (keep previous tile) 
# w[i] = 1 at the end of the overlap (use current tile)
# The weighted sum ensures a smooth transition across tiles.
w = 0.5 * (1.0 - np.cos(np.pi * np.arange(nover) / nover))

# =====================================================
# STOCHASTIC FIELD WITH EXPONENTIAL ACF
# =====================================================

def generate_field(ix, jy):
    """
    Generates a stochastic 3-D field with exponential ACF (C(r) = exp(-r/Lcorr)), but only keeping
    Fourier modes that SEM can resolve (wavelength >= dx_eff). RMS normalized to `rms`.
    """

    # --- white noise (complex) ---
    noise = (
        np.random.normal(size=(nx_ext, ny_ext, nz)) +
        1j * np.random.normal(size=(nx_ext, ny_ext, nz))
    )

    # --- wavenumbers ---
    kx = fftfreq(nx_ext, dx)
    ky = fftfreq(ny_ext, dy)
    kz = fftfreq(nz, dz)
    KX, KY, KZ = np.meshgrid(kx, ky, kz, indexing="ij")
    k = 2.0 * np.pi * np.sqrt(KX**2 + KY**2 + KZ**2)

    # --- exponential ACF power spectrum ---
    power = 1.0 / (1.0 + (k * Lcorr)**2)**2
    power[0,0,0] = 0.0  # remove mean

    # --- zero-out high-frequency modes that SEM cannot resolve ---
    # SEM can resolve wavenumbers k <= k_max = pi / dx_eff
    k_max = np.pi / dx_eff
    power[k > k_max] = 0.0

    # --- filter in Fourier space ---
    field_k = fftn(noise) * np.sqrt(power)
    field   = np.real(ifftn(field_k))

    # --- normalize RMS ---
    field -= field.mean()
    field *= rms / np.std(field)

    return field

# =====================================================
# FILE OUTPUT
# =====================================================
def save_xyz(fname, core, x0, y0,z0):
    with open(fname, "w") as f:
        f.write(f"{x0} {y0} {z0} {x0+tile_xy} {y0+tile_xy} {z0+Lz}\n")
        f.write(f"{dx} {dy} {dz}\n")
        f.write(f"{nx_tile} {ny_tile} {nz}\n")
        f.write(f"{VP_MIN} {VP_MAX} {VS_MIN} {VS_MAX} {RHO_MIN} {RHO_MAX}\n")
        for iz in range(nz):
            for iy in range(ny_tile):
                f.write(" ".join(f"{core[ix,iy,iz]:.6e}"
                                 for ix in range(nx_tile)) + "\n")

def save_vtk(fname, core, x0, y0,z0):
    with open(fname, "w") as f:
        f.write("# vtk DataFile Version 3.0\n")
        f.write("Exponential ACF stochastic perturbation\n")
        f.write("ASCII\n")
        f.write("DATASET STRUCTURED_POINTS\n")
        f.write(f"DIMENSIONS {nx_tile} {ny_tile} {nz}\n")
        f.write(f"ORIGIN {x0} {y0} z0\n")
        f.write(f"SPACING {dx} {dy} {dz}\n")
        f.write(f"POINT_DATA {nx_tile*ny_tile*nz}\n")
        f.write("SCALARS perturbation float 1\n")
        f.write("LOOKUP_TABLE default\n")
        for iz in range(nz):
            for iy in range(ny_tile):
                for ix in range(nx_tile):
                    f.write(f"{core[ix,iy,iz]:.6e}\n")

# =====================================================
# OVERLAP STORAGE
# =====================================================
right_overlap = {}
top_overlap   = {}

# =====================================================
# MAIN LOOP
# =====================================================
tile_id = 1

for jy in range(nty):
    for ix in range(ntx):

        field = generate_field(ix, jy)

        # ---- LEFT overlap ----
        if ix > 0:
            left = right_overlap[(ix-1, jy)]
            for i in range(nover):
                wi = w[i]
                # RMS-preserving renormalization:
                # The overlap is a weighted sum of two *independent* stochastic fields.
                # Without correction, variance would be reduced by:
                #     (1-w)^2 + w^2
                # We therefore rescale by:
                #     1 / sqrt((1-w)^2 + w^2)
                # so that the blended field retains the target RMS.
                scale = 1.0 / np.sqrt((1-wi)**2 + wi**2)
                field[i,:,:] = (
                    (1-wi) * left[i,:,:] + wi * field[i,:,:]
                ) * scale

        # ---- BOTTOM overlap ----
        if jy > 0:
            bottom = top_overlap[(ix, jy-1)]
            for j in range(nover):
                wj = w[j]
                # Same RMS-preserving renormalization for bottom overlap
                scale = 1.0 / np.sqrt((1-wj)**2 + wj**2)
                field[:,j,:] = (
                    (1-wj) * bottom[:,j,:] + wj * field[:,j,:]
                ) * scale

        # ---- CORE ONLY ----
        core = field[0:nx_tile, 0:ny_tile, :]

        core_rms = np.sqrt(np.mean(core**2))
        print(f"Tile {tile_id}: RMS = {core_rms:.4e}")

        x0 = ix * tile_xy + x_shift
        y0 = jy * tile_xy + y_shift
        z0 = z_shift

        save_xyz(f"tiles/tomography_model_box{tile_id}.xyz", core, x0, y0,z0)
        save_vtk(f"tiles/tomography_model_box{tile_id}.vtk", core, x0, y0,z0)

        # ---- STORE EXTENSIONS ----
        right_overlap[(ix, jy)] = field[nx_tile:nx_tile+nover,:,:].copy()
        top_overlap[(ix, jy)]   = field[:, ny_tile:ny_tile+nover,:].copy()

        tile_id += 1

print("Done. Exponential-ACF stochastic tiles generated.")

