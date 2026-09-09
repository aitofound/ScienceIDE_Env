"""
Generate stochastic volumetric heterogeneity tiles for the PKIIKP inner-core demo.

The tile geometry is centered from DATA/Par_file_SEM_DSM.  The output files are
written in the tomography_model_box*.xyz format consumed by SPECFEM3D.
"""

from pathlib import Path
import os
import math

import numpy as np
from numpy.fft import fftn, ifftn, fftfreq


# =====================================================
# PARAMETERS
# =====================================================
dx = dy = dz = 1500.0            # grid spacing (m). Typically smaller than dx_eff.
tile_xy = 250e3                  # tile size (m)
overlap_xy = 40e3                # overlap width (m)

rms = 0.02
Lcorr = 12000.0                  # correlation length (m)
random_seed = 22345              # fixed seed for reproducible heterogeneity tiles

VP_MIN, VP_MAX = 500.0, 14000.0
VS_MIN, VS_MAX = 300.0, 8000.0
RHO_MIN, RHO_MAX = 1000.0, 10000.0

np.random.seed(random_seed)

# =====================================================
# CASE GEOMETRY FROM Par_file_SEM_DSM
# =====================================================
ROOT_DIR = Path(__file__).resolve().parents[2]
PARAM_FILE = ROOT_DIR / "DATA" / "Par_file_SEM_DSM"


def read_params(path):
    params = {}
    with path.open() as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if not line or "=" not in line:
                continue
            key, value = line.split("=", 1)
            params[key.strip()] = value.strip()
    return params


def param_float(params, key, default):
    return float(params.get(key, str(default)).replace("d", "e").replace("D", "e"))


params = read_params(PARAM_FILE)
R_EARTH_M = 1000.0 * param_float(params, "R_EARTH", 6371.0)
center_depth_m = 1000.0 * param_float(params, "CENTER_DEPTH_KM", 0.0)
depth_block_m = 1000.0 * param_float(params, "DEPTH_BLOCK_KM", 0.0)
width_xi = param_float(params, "ANGULAR_WIDTH_XI_IN_DEGREES", 0.0)
width_eta = param_float(params, "ANGULAR_WIDTH_ETA_IN_DEGREES", 0.0)
lat_center = param_float(params, "CENTER_LATITUDE_IN_DEGREES", 0.0)
lon_center = param_float(params, "CENTER_LONGITUDE_IN_DEGREES", 0.0)
# Effective SEM GLL-point spacing used to remove heterogeneity wavelengths that
# the SEM mesh cannot resolve. Before the SEM databases exist, use the Par_file
# value as an initial estimate. After the xgenerate_databases job
# submitted by Step 3 finishes successfully, compare it with
# "*** Max GLL point distance" in WORK/SPECFEM3D/OUTPUT_FILES/output_mesher.txt.
# If significantly different, update the Par_file value, regenerate the tiles,
# and rerun xgenerate_databases using the existing mesh.
dx_eff = param_float(params, "HETEROGENEITY_DX_EFF_M", 2.0 * dx)
r_top = R_EARTH_M - center_depth_m + 0.5 * depth_block_m
r_center = R_EARTH_M - center_depth_m

# Cover the full SEM angular span at the top of the box and snap to whole tiles.
if width_xi <= 0.0 or width_eta <= 0.0 or depth_block_m <= 0.0:
    raise RuntimeError("ANGULAR_WIDTH_XI_IN_DEGREES, ANGULAR_WIDTH_ETA_IN_DEGREES, and DEPTH_BLOCK_KM must be set in Par_file_SEM_DSM")
Lx = max(tile_xy, math.ceil((math.radians(width_xi) * r_top) / tile_xy) * tile_xy)
Ly = max(tile_xy, math.ceil((math.radians(width_eta) * r_top) / tile_xy) * tile_xy)
Lz = max(dz, depth_block_m)

# =====================================================
# GRID SIZES
# =====================================================
nx_tile = int(tile_xy / dx)
ny_tile = int(tile_xy / dy)
nz = int(Lz / dz) + 1

nover = int(overlap_xy / dx)
assert nover > 0

# Only extend to the RIGHT and TOP.
nx_ext = nx_tile + nover
ny_ext = ny_tile + nover

ntx = int(Lx / tile_xy)
nty = int(Ly / tile_xy)

# =====================================================
# GLOBAL CENTER SHIFT
# =====================================================
theta = np.radians(90.0 - lat_center)
phi = np.radians(lon_center)

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
# COSINE TAPER (0 -> 1) for overlap averaging
# =====================================================
w = 0.5 * (1.0 - np.cos(np.pi * np.arange(nover) / nover))

# =====================================================
# STOCHASTIC FIELD WITH EXPONENTIAL ACF
# =====================================================
def generate_field(ix, jy):
    """Generate a stochastic 3-D field with exponential ACF and SEM-resolution filtering."""
    noise = (
        np.random.normal(size=(nx_ext, ny_ext, nz))
        + 1j * np.random.normal(size=(nx_ext, ny_ext, nz))
    )

    kx = fftfreq(nx_ext, dx)
    ky = fftfreq(ny_ext, dy)
    kz = fftfreq(nz, dz)
    KX, KY, KZ = np.meshgrid(kx, ky, kz, indexing="ij")
    k = 2.0 * np.pi * np.sqrt(KX**2 + KY**2 + KZ**2)

    power = 1.0 / (1.0 + (k * Lcorr) ** 2) ** 2
    power[0, 0, 0] = 0.0

    k_max = np.pi / dx_eff
    power[k > k_max] = 0.0

    field_k = fftn(noise) * np.sqrt(power)
    field = np.real(ifftn(field_k))

    field -= field.mean()
    field *= rms / np.std(field)

    return field

# =====================================================
# FILE OUTPUT
# =====================================================
def save_xyz(fname, core, x0, y0, z0):
    with open(fname, "w") as f:
        f.write(f"{x0} {y0} {z0} {x0 + tile_xy} {y0 + tile_xy} {z0 + Lz}\n")
        f.write(f"{dx} {dy} {dz}\n")
        f.write(f"{nx_tile} {ny_tile} {nz}\n")
        f.write(f"{VP_MIN} {VP_MAX} {VS_MIN} {VS_MAX} {RHO_MIN} {RHO_MAX}\n")
        for iz in range(nz):
            for iy in range(ny_tile):
                f.write(" ".join(f"{core[ix, iy, iz]:.6e}" for ix in range(nx_tile)) + "\n")


def save_vtk(fname, core, x0, y0, z0):
    with open(fname, "w") as f:
        f.write("# vtk DataFile Version 3.0\n")
        f.write("Exponential ACF stochastic perturbation\n")
        f.write("ASCII\n")
        f.write("DATASET STRUCTURED_POINTS\n")
        f.write(f"DIMENSIONS {nx_tile} {ny_tile} {nz}\n")
        f.write(f"ORIGIN {x0} {y0} {z0}\n")
        f.write(f"SPACING {dx} {dy} {dz}\n")
        f.write(f"POINT_DATA {nx_tile * ny_tile * nz}\n")
        f.write("SCALARS perturbation float 1\n")
        f.write("LOOKUP_TABLE default\n")
        for iz in range(nz):
            for iy in range(ny_tile):
                for ix in range(nx_tile):
                    f.write(f"{core[ix, iy, iz]:.6e}\n")

# =====================================================
# OVERLAP STORAGE
# =====================================================
right_overlap = {}
top_overlap = {}

# =====================================================
# MAIN LOOP
# =====================================================
tile_id = 1

for jy in range(nty):
    for ix in range(ntx):
        field = generate_field(ix, jy)

        if ix > 0:
            left = right_overlap[(ix - 1, jy)]
            for i in range(nover):
                wi = w[i]
                scale = 1.0 / np.sqrt((1 - wi) ** 2 + wi**2)
                field[i, :, :] = ((1 - wi) * left[i, :, :] + wi * field[i, :, :]) * scale

        if jy > 0:
            bottom = top_overlap[(ix, jy - 1)]
            for j in range(nover):
                wj = w[j]
                scale = 1.0 / np.sqrt((1 - wj) ** 2 + wj**2)
                field[:, j, :] = ((1 - wj) * bottom[:, j, :] + wj * field[:, j, :]) * scale

        core = field[0:nx_tile, 0:ny_tile, :]
        core_rms = np.sqrt(np.mean(core**2))
        print(f"Tile {tile_id}: RMS = {core_rms:.4e}")

        x0 = ix * tile_xy + x_shift
        y0 = jy * tile_xy + y_shift
        z0 = z_shift

        save_xyz(f"tiles/tomography_model_box{tile_id}.xyz", core, x0, y0, z0)
        save_vtk(f"tiles/tomography_model_box{tile_id}.vtk", core, x0, y0, z0)

        right_overlap[(ix, jy)] = field[nx_tile:nx_tile + nover, :, :].copy()
        top_overlap[(ix, jy)] = field[:, ny_tile:ny_tile + nover, :].copy()

        tile_id += 1

print(
    "Done. Exponential-ACF stochastic tiles generated "
    f"around lat={lat_center}, lon={lon_center}, r_top={r_top:.1f} m, r_center={r_center:.1f} m."
)
