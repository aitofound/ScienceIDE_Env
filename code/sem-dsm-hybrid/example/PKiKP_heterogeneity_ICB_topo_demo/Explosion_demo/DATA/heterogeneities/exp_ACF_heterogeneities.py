import numpy as np
from scipy.fftpack import fftn, ifftn
import matplotlib.pyplot as plt

# Define grid parameters
dx = dy = dz = 1000.0  # Grid spacing in meters
Lx = 820e3  # Domain size
Ly = 1800e3 # Domain size 
Lz = 1300e3  # Domain size 

correlation_scale = 2000.0  # Correlation scale in meters
rms = 0.01  # RMS value of perturbations

# Bounds for physical properties (Vp, Vs, and density)
VP_MIN, VP_MAX = 500.0, 14000.0
VS_MIN, VS_MAX = 300.0, 8000.0
RHO_MIN, RHO_MAX = 1000.0, 10000.0

# Filename to save results
output_file = "tomography_model.xyz"

# Anomaly center coordinates (latitude, longitude in degrees, radius in meters)
lat_center = 0.0
lon_center = 0.0
r_top = 1217100.0+50000.0
r_center = r_top - 50000.0 -250000.0 # Center radius

# Convert spherical to Cartesian coordinates for anomaly center
theta = np.radians(90.0 - lat_center)  # Colatitude in radians
phi = np.radians(lon_center)  # Longitude in radians

x_center = r_center * np.sin(theta) * np.cos(phi)
y_center = r_center * np.sin(theta) * np.sin(phi)
z_center = r_center * np.cos(theta)

# Create coordinate grids
nx = int(Lx / dx) + 1
ny = int(Ly / dy) + 1
nz = int(Lz / dz) + 1

x = np.linspace(-Lx / 2, Lx / 2, nx) + x_center
y = np.linspace(-Ly / 2, Ly / 2, ny) + y_center
z = np.linspace(-Lz / 2, Lz / 2, nz) + z_center

kx = np.fft.fftfreq(nx, dx)
ky = np.fft.fftfreq(ny, dy)
kz = np.fft.fftfreq(nz, dz)

KX, KY, KZ = np.meshgrid(kx, ky, kz, indexing='ij')
k_squared = KX**2 + KY**2 + KZ**2

# Define power spectrum for exponential ACF
power_spectrum = 1.0 / (1.0 + (2 * np.pi * np.sqrt(k_squared) * correlation_scale)**2)**2

# Gaussian ACF
#power_spectrum = np.exp(-(2*np.pi*np.sqrt(k_squared) * correlation_scale)**2/4.0)

power_spectrum[0, 0, 0] = 0  # Remove the zero-frequency component to avoid a mean shift

# Generate random Gaussian field
random_field = np.random.normal(size=(nx, ny, nz)) + 1j * np.random.normal(size=(nx, ny, nz))

# Apply power spectrum in Fourier space
perturbation_field_k = fftn(random_field) * np.sqrt(power_spectrum)
perturbation_field = np.real(ifftn(perturbation_field_k))

# Normalize to the desired RMS value
perturbation_field -= np.mean(perturbation_field)
perturbation_field *= rms / np.std(perturbation_field)

print("Generated 3D perturbation field with shape:", perturbation_field.shape)

#x[0]=-5810999.6459736377 
#y[0]=-242667.24894584288
#z[0]=-2389260.301914987
#x[-1]=-4970849.436294971
#y[-1]=307332.75105415715
#z[-1]=-1789260.3019149872

# Write to ASCII file
with open(output_file, "w") as file:
    # Write metadata
    file.write(f"{x[0]} {y[0]} {z[0]} {x[-1]} {y[-1]} {z[-1]}\n")
    file.write(f"{dx} {dy} {dz}\n")
    file.write(f"{len(x)} {len(y)} {len(z)}\n")
    file.write(f"{VP_MIN} {VP_MAX} {VS_MIN} {VS_MAX} {RHO_MIN} {RHO_MAX}\n")
    # Write perturbation data
    for iz in range(nz):
        for iy in range(ny):
            perturb_row = perturbation_field[:, iy, iz]
            file.write(" ".join(f"{val:.6e}" for val in perturb_row) + "\n")

# Optional: Check ACF and plot (set check_ACF and plot to True if needed)
check_ACF = False
plot = False

if check_ACF:
    # Compute empirical ACF averaged over all (ix, iy) positions
    acf_empirical_sum = np.zeros(nz)
    count = 0
    for ix in range(0, nx, 10):
        for iy in range(0, ny, 10):
            profile = perturbation_field[ix, iy, :]
            acf = np.correlate(profile, profile, mode='full')
            acf = acf[len(acf) // 2:]
            acf_empirical_sum[:len(acf)] += acf
            count += 1
    acf_empirical_avg = acf_empirical_sum / count
    acf_empirical_avg /= acf_empirical_avg[0]  # Normalize

    # Compute distances corresponding to the ACF
    distances = np.arange(len(acf_empirical_avg)) * dz  # Distance in meters

    # Compute theoretical ACF (Gaussian form)
    #predicted_acf = np.exp(-distances**2 / (correlation_scale)**2)

    # Compute theoretical ACF (Exponential form)
    predicted_acf = np.exp(-distances / correlation_scale)

    # Plot empirical and theoretical ACF
    plt.figure(figsize=(10, 6))
    plt.plot(distances, acf_empirical_avg, label='Empirical ACF', marker='o')
    plt.plot(distances, predicted_acf, label='Predicted ACF (Exponential)', linestyle='--')
    plt.xlabel('Distance (m)')
    plt.ylabel('Autocorrelation')
    plt.legend()
    plt.title('Comparison of Empirical and Predicted ACF')
    plt.grid()
    plt.xlim([0, 5 * correlation_scale])
    plt.show()

if plot:
    # Plot a 2D profile (e.g., at the middle of the z-axis)
    z_index = nz // 2
    plt.figure(figsize=(10, 6))
    plt.imshow(perturbation_field[:, :, z_index], extent=[-Lx/2, Lx/2, -Ly/2, Ly/2], cmap='seismic', origin='lower')
    plt.colorbar(label='Perturbation Amplitude')
    plt.xlabel('X (m)')
    plt.ylabel('Y (m)')
    plt.title(f'2D Profile at Z={z[z_index]:.1f} m')
    plt.show()

    # Plot 2D profile for x=0
    y_idy = np.argmin(np.abs(y))  # Find index closest to y=0
    perturb_2d = perturbation_field[:,y_idy, :]  # Slice for y=0

    # Plotting
    plt.figure(figsize=(8, 6))
    plt.contourf(x, z, perturb_2d.T, levels=50, cmap="viridis")
    plt.colorbar(label="Perturbation")
    plt.title("2D Profile of Perturbation at x=0")
    plt.xlabel("Y (km)")
    plt.ylabel("Z (km)")
    plt.tight_layout()
    plt.show()


    # Create a 3D plot for z=0 and y=0 planes
    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(111, projection='3d')

    # Profile at z=0
    z_idx = np.argmin(np.abs(z))  # Index for z=0
    perturb_z0 = perturbation_field[:, :, z_idx]
    X, Y = np.meshgrid(x, y, indexing='ij')
    ax.plot_surface(X, Y, np.zeros_like(perturb_z0.T), facecolors=plt.cm.viridis(perturb_z0.T / rms), rstride=1, cstride=1, alpha=0.8, edgecolor='none')

    # Profile at y=0
    y_idx = np.argmin(np.abs(y))  # Index for y=0
    perturb_y0 = perturbation_field[:, y_idx, :]
    X, Z = np.meshgrid(x, z, indexing='ij')
    ax.plot_surface(X, np.zeros_like(perturb_y0.T), Z, facecolors=plt.cm.viridis(perturb_y0.T / rms), rstride=1, cstride=1, alpha=0.8, edgecolor='none')

    # Customize plot
    ax.set_xlabel("X (km)")
    ax.set_ylabel("Y (km)")
    ax.set_zlabel("Z (km)")
    ax.set_title("3D View of Perturbation Profiles (z=0 and y=0)")
    ax.view_init(elev=30, azim=45)  # Adjust the viewing angle
    plt.tight_layout()
    plt.show()
