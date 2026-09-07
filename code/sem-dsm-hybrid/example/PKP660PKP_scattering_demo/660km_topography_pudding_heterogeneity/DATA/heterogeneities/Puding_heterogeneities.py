import numpy as np
import matplotlib.pyplot as plt
from scipy.ndimage import rotate

# ===============================================
# GRID / GEO / MODEL PARAMETERS
# ===============================================
dx = dy = dz = 1200.0      # grid spacing (m)
Lx = 900e3                # domain size in x (m)
Ly = 1400e3                # domain size in y (m)
Lz = 2500e3                # domain size in z (m)

# Physical bounds
VP_MIN, VP_MAX = 500.0, 14000.0
VS_MIN, VS_MAX = 300.0, 8000.0
RHO_MIN, RHO_MAX = 1000.0, 10000.0

output_file = "tomography_model.xyz"

# -----------------------------------------------
# ANOMALY CENTER (LAT, LON, RADIUS)
# -----------------------------------------------
lat_center = 12.0
lon_center = 179.99

r_top = 6371000.0 - (660000.0 - 60000.0)
r_center = r_top - 60000.0

theta = np.radians(90.0 - lat_center)
phi = np.radians(lon_center)

x_center = r_center * np.sin(theta) * np.cos(phi)
y_center = r_center * np.sin(theta) * np.sin(phi)
z_center = r_center * np.cos(theta)

# -----------------------------------------------
# COMPUTE GRID
# -----------------------------------------------
nx = int(Lx / dx) + 1
ny = int(Ly / dy) + 1
nz = int(Lz / dz) + 1

x = np.linspace(-Lx / 2, Lx / 2, nx) + x_center
y = np.linspace(-Ly / 2, Ly / 2, ny) + y_center
z = np.linspace(-Lz / 2, Lz / 2, nz) + z_center

# ===============================================
# RANDOMLY ROTATED SHEETS (UPDATED FOR dx=1500m)
# ===============================================

NX, NY, NZ = nx, ny, nz

# Sheet thickness scaled to same physical thickness as before (~10 km)
sheet_thickness_km = 10_000.0       # physical thickness in meters
sheet_thickness = max(2, int(sheet_thickness_km / dz))

# Random sheet length in x-y should also scale
def random_sheet_length():
    """Random sheet length scaled to dx=600 m."""
    L_mean_m = 10000.0              # original ~7 km
    L_std_m  = 3000.0              # original ~3 km std

    L_gp = np.random.normal(L_mean_m, L_std_m) / dx
    return int(np.clip(L_gp, 3, 40))  # slightly expanded bounds

fill_fraction = 0.2
perturb_value = -0.05

field = np.zeros((NX, NY, NZ))
occupied = np.zeros((NX, NY, NZ), dtype=bool)

target_volume = fill_fraction * NX * NY * NZ
current_volume = 0
max_attempts = 60_000_000
attempts = 0

print("Generating randomly rotated sheet structures...")

while current_volume < target_volume and attempts < max_attempts:
    attempts += 1

    lx = random_sheet_length()
    ly = random_sheet_length()

    sheet_mask = np.ones((lx, ly), dtype=bool)

    # Random rotation
    angle = np.random.uniform(0, 360)
    rotated_mask = rotate(sheet_mask.astype(float), angle,
                          reshape=True, order=0) > 0.5
    rx, ry = rotated_mask.shape

    # Random center
    cx = np.random.randint(0, NX)
    cy = np.random.randint(0, NY)

    x0 = max(0, cx - rx // 2)
    x1 = min(NX, cx + rx // 2)
    y0 = max(0, cy - ry // 2)
    y1 = min(NY, cy + ry // 2)

    # Random z slice
    if NZ - sheet_thickness <= 1:
        break
    z0 = np.random.randint(0, NZ - sheet_thickness)
    z1 = z0 + sheet_thickness

    # Ensure shape fits
    if (x1 - x0 != rx) or (y1 - y0 != ry):
        continue

    # Avoid overlap
    if occupied[x0:x1, y0:y1, z0:z1].any():
        continue

    # Fill sheet
    for zi in range(z0, z1):
        field[x0:x1, y0:y1, zi][rotated_mask] = perturb_value
        occupied[x0:x1, y0:y1, zi][rotated_mask] = True

    current_volume += np.sum(rotated_mask) * sheet_thickness
    if(attempts % 10000==0):
        print(f"fill fraction: {current_volume / (NX * NY * NZ):.2%}")

print(f"Final fill fraction = {current_volume / (NX * NY * NZ):.2%}")
# ===============================================
# SAVE THE MODEL
# ===============================================

with open(output_file, "w") as f:
    f.write(f"{x[0]} {y[0]} {z[0]} {x[-1]} {y[-1]} {z[-1]}\n")
    f.write(f"{dx} {dy} {dz}\n")
    f.write(f"{nx} {ny} {nz}\n")
    f.write(f"{VP_MIN} {VP_MAX} {VS_MIN} {VS_MAX} {RHO_MIN} {RHO_MAX}\n")

    for iz in range(nz):
        for iy in range(ny):
            row = field[:, iy, iz]
            f.write(" ".join(f"{v:.6e}" for v in row) + "\n")

print(f"Model written to {output_file}")


# ===============================================
# VISUAL CHECK
# ===============================================

slice_index = NZ // 2

plt.figure(figsize=(10, 6))
plt.imshow(field[:, :, slice_index].T, origin="lower",
           cmap="viridis", aspect="auto")
plt.colorbar(label="Perturbation")
plt.title(f"Slice at z-index {slice_index}")
plt.xlabel("X index")
plt.ylabel("Y index")
plt.tight_layout()
#plt.show()

