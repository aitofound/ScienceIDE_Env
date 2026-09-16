#!/usr/bin/env python
"""
Script to load PSI MAS model from ./corona and plot density slices.
"""

import matplotlib
matplotlib.use('Agg')
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.colors as colors
from psipy.model import MASOutput

# Load the MAS model from the corona directory
print("Loading MAS model from ./corona...")
model = MASOutput("./corona")

# Print available variables
print(f"\nAvailable variables: {model.variables}")

# Check if density variable exists
if "rho" not in model.variables:
    print("Error: Density variable (rho) not found!")
    exit(1)

# Get density variable to access coordinates
rho = model["rho"]

# Find phi index closest to phi=0 (or 180) degrees for z=0 slice
phi_coords = rho.phi_coords  # These are in radians
# Convert to degrees for easier handling
phi_coords_deg = np.rad2deg(phi_coords)

# Find index closest to 0 degrees (or 180 degrees)
phi_0_idx = np.argmin(np.abs(phi_coords_deg))
phi_180_idx = np.argmin(np.abs(phi_coords_deg - 180))

# Use phi=0 if it's closer, otherwise use phi=180
if np.abs(phi_coords_deg[phi_0_idx]) < np.abs(phi_coords_deg[phi_180_idx] - 180):
    phi_idx = phi_0_idx
    phi_value = phi_coords_deg[phi_0_idx]
else:
    phi_idx = phi_180_idx
    phi_value = phi_coords_deg[phi_180_idx]

print(f"\nUsing phi index {phi_idx} (phi = {phi_value:.2f} degrees) for z=0 slice")

# Create figure with two subplots
fig = plt.figure(figsize=(12, 5))
cbar_kwargs = {"orientation": "vertical"}

# Set log scale normalization with lower and upper limits
log_norm = colors.LogNorm(vmin=5e4, vmax=1e9)

# Plot 1: Equatorial slice (theta=90 degrees)
ax1 = plt.subplot(121, projection="polar")
print("\nPlotting equatorial slice (theta=90 degrees)...")
rho.plot_equatorial_cut(ax=ax1, cbar_kwargs=cbar_kwargs, norm=log_norm)
ax1.set_title("Density - Equatorial Slice (θ=90°)", pad=20)
ax1.set_rlim(0, 4)  # Limit radial extent to 5 R_sun

# Plot 2: z=0 slice (phi=0 or 180 degrees)
ax2 = plt.subplot(122, projection="polar")
print(f"Plotting z=0 slice (phi={phi_value:.2f} degrees)...")
rho.plot_phi_cut(phi_idx, ax=ax2, cbar_kwargs=cbar_kwargs, norm=log_norm)
ax2.set_title(f"Density - z=0 Slice (φ={phi_value:.2f}°)", pad=20)
ax2.set_rlim(0, 4)  # Limit radial extent to 5 R_sun

plt.tight_layout()
plt.savefig("corona_model_plot.png", dpi=150, bbox_inches="tight")
print("\nPlot saved to corona_model_plot.png")
