from pathlib import Path

import math


SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = Path(__file__).resolve().parents[2]
PARAM_FILE = ROOT_DIR / "DATA" / "Par_file_SEM_DSM"
DSM_MODEL = ROOT_DIR / "DATA" / "dsm_model_base"
OUTPUT_FILE = SCRIPT_DIR / "latlon_ICB_topo.txt"


def distance_between_points_m(lonlat_a, lonlat_b):
    lon1, lat1 = map(math.radians, lonlat_a)
    lon2, lat2 = map(math.radians, lonlat_b)
    dlon = lon2 - lon1
    dlat = lat2 - lat1
    a = math.sin(dlat / 2.0) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2.0) ** 2
    return 6371000.0 * 2.0 * math.atan2(math.sqrt(a), math.sqrt(1.0 - a))


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


def clean_model_line(line):
    return line.split("#", 1)[0].replace("d", "e").replace("D", "e").strip()


def eval_poly(coeffs, radius, earth_radius):
    x = radius / earth_radius
    value = 0.0
    xp = 1.0
    for coeff in coeffs:
        value += coeff * xp
        xp *= x
    return value


def find_solid_fluid_interface(model_path, bottom_radius, top_radius, earth_radius):
    lines = model_path.read_text().splitlines()
    nz = int(clean_model_line(lines[3]).split()[0])
    idx = 4
    zones = []
    for _ in range(nz):
        rmin, rmax, *_ = map(float, clean_model_line(lines[idx]).split())
        vsh = list(map(float, clean_model_line(lines[idx + 3]).split()))
        vsv = list(map(float, clean_model_line(lines[idx + 4]).split()))
        rmid = 0.5 * (rmin + rmax)
        fluid = abs(eval_poly(vsh, rmid, earth_radius)) <= 1.0e-6 and abs(eval_poly(vsv, rmid, earth_radius)) <= 1.0e-6
        zones.append((rmin, rmax, fluid))
        idx += 6
    for (_, rmax, fluid), (_, _, next_fluid) in zip(zones, zones[1:]):
        if bottom_radius < rmax < top_radius and fluid != next_fluid:
            return rmax
    raise RuntimeError(f"Could not find a solid/fluid interface between {bottom_radius} and {top_radius} km")


params = read_params(PARAM_FILE)
R_Earth = param_float(params, "R_EARTH", 6371.0)
center_depth = param_float(params, "CENTER_DEPTH_KM", 0.0)
depth_block = param_float(params, "DEPTH_BLOCK_KM", 1.0)
center_lat = param_float(params, "CENTER_LATITUDE_IN_DEGREES", 0.0)
center_lon = param_float(params, "CENTER_LONGITUDE_IN_DEGREES", 0.0)
width_xi = param_float(params, "ANGULAR_WIDTH_XI_IN_DEGREES", 5.0)
width_eta = param_float(params, "ANGULAR_WIDTH_ETA_IN_DEGREES", 5.0)
margin = param_float(params, "ICB_TOPOGRAPHY_LATLON_MARGIN_DEGREES", 5.0)
spacing = param_float(params, "ICB_TOPOGRAPHY_SPACING_DEGREES", 0.1)

top_radius = R_Earth - center_depth + 0.5 * depth_block
bottom_radius = R_Earth - center_depth - 0.5 * depth_block
R_this_interface = find_solid_fluid_interface(DSM_MODEL, 0.0, R_Earth, R_Earth)
interface_depth_m = (R_this_interface - top_radius) * 1000.0

# Define a flat ICB interface over the SEM box footprint.  Large cubed-sphere
# chunks can project outside the simple center +/- half-width lat/lon box, so
# use full longitude coverage and clamp latitude for wide regional boxes.
dlat = dlon = spacing
lat0 = max(-90.0, center_lat - 0.5 * width_eta - margin)
lat1 = min(90.0, center_lat + 0.5 * width_eta + margin)
if max(width_xi, width_eta) >= 60.0:
    lon0, lon1 = 0.0, 360.0
else:
    lon0, lon1 = center_lon - 0.5 * width_xi - margin, center_lon + 0.5 * width_xi + margin

def arange_inclusive(start, stop, step):
    count = int(round((stop - start) / step)) + 1
    return [start + i * step for i in range(count)]


lat = arange_inclusive(lat0, lat1, dlat)
lon = arange_inclusive(lon0, lon1, dlon)
nlat, nlon = len(lat), len(lon)

with OUTPUT_FILE.open("w") as file:
    file.write(f"{nlon} {nlat}\n")
    file.write(f"{lon0} {lat0} {lon1} {lat1}\n")
    file.write(f"{dlon} {dlat}\n")

    for ilat in range(nlat):
        line = []
        for ilon in range(nlon):
            line.append(f"{interface_depth_m:.2f}")
        file.write(" ".join(line) + "\n")

print(f"Wrote flat {OUTPUT_FILE} with ICB radius {R_this_interface:.3f} km and baseline {interface_depth_m:.2f} m")
