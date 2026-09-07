import numpy as np
import great_circle_calculator.great_circle_calculator as gcc
#import matplotlib.pyplot as plt

# Define parameters
reference_depth=-15000.0
lat0, lat1 = -5.0, 5.0
lon0, lon1 = -5.0, 5.0
dlat, dlon = 0.01, 0.01
bump_radius = 40000.0  # meters
bump_height = 2000.0  # meters
bump_center_lon, bump_center_lat = 0.0, 0.0

R_this_interface=1217.1
R_Earth=6371.0

# Create latitude and longitude grids
lat = np.arange(lat0, lat1 + dlat, dlat)
lon = np.arange(lon0, lon1 + dlon, dlon)
nlat, nlon = len(lat), len(lon)

# Initialize the topography array
topo = np.zeros((nlon, nlat))

# File to save topography
with open("latlon_ICB_topo.txt", "w") as file:
    # Write header
    file.write(f"{nlon} {nlat}\n")
    file.write(f"{lon0} {lat0} {lon1} {lat1}\n")
    file.write(f"{dlon} {dlat}\n")

    # Calculate topography
    for ilat in range(nlat):
        line = []
        for ilon in range(nlon):
            distance_to_bump_center_surf = gcc.distance_between_points(
                (bump_center_lon, bump_center_lat), 
                (lon[ilon], lat[ilat]), 
                unit='meters'
            )
            distance_to_bump_center=distance_to_bump_center_surf*R_this_interface/R_Earth
            if distance_to_bump_center > bump_radius:
                topo[ilon, ilat] = 0.0
            else:
                topo[ilon, ilat] = (
                    (np.cos(distance_to_bump_center / bump_radius * np.pi) + 1.0) / 2.0 * bump_height
                )
            line.append(f"{topo[ilon, ilat]+reference_depth:.2f}")
        file.write(" ".join(line) + "\n")

# Plot topography along the middle latitude line
#plt.plot(lon, topo[:, nlat // 2])
#plt.xlabel("Longitude (deg)")
#plt.ylabel("Topography (m)")
#plt.title("Topography Along Central Latitude")
#plt.grid()
#plt.show()

