import sys
import numpy as np
import math
import re
from scipy.spatial import Delaunay
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from matplotlib.lines import Line2D

# Conversion factors for units
unit_conversion = {
    'm': 1.0,
    'rsun': 6.957e8,  # meters in one solar radius
    'au': 1.496e11    # meters in one astronomical unit
}

def print_help():
    help_message = """
    Usage:
        For a Sphere:
            python script.py <filename> sphere <R> <R2> <R3> <tecplot_output_filename> [units] [input_file_units] [--equal_axes]
        
        For an Ellipsoid:
            python script.py <filename> ellipsoid <a> <b> <c> <R2> <R3> <tecplot_output_filename> [--equal_axes]

        Run the example with input from data/input/FieldLines 
            python field-line-surface-intersection.py all-field-lines.dat ellipsoid 1 1 1 2 0.2 out.dat rsun au --equal_axes
            python field-line-surface-intersection.py all-field-lines.dat sphere 3 6 1 out.dat rsun au --equal_axes


    
    Arguments:
        - <filename>                 : Path to the ASCII file containing the trajectories.
        - <shape>                    : 'sphere' or 'ellipsoid'.
        - <R>                        : Radius of the sphere for calculating intersections (positive float). Required if shape is 'sphere'.
        - <a> <b> <c>                : Semi-axes of the ellipsoid (positive floats). Required if shape is 'ellipsoid'.
        - <R2>                       : Radius beyond which trajectory points will not be plotted (positive float).
        - <R3>                       : Radius of the non-transparent yellow sphere to be plotted (positive float).
        - <units>                    : units used in the argument line [au,rsun,m]
        - <input_file_units]         : units used in the input file [au,rsun,m]
        - <tecplot_output_filename>  : Name of the Tecplot file where the triangulated surface will be saved.
    
    Optional Argument:
        - --equal_axes                : If specified, the x, y, and z axes will be set to the same scale.
    
    Description:
        This script performs the following tasks:
        1. Loads a set of trajectories from the input file.
        2. Computes the intersection points of the trajectories with either a sphere or an ellipsoid.
        3. Triangulates the surface mesh based only on the unique intersection points.
        4. Plots the original trajectories (Field Lines), the intersection points (Shock Locations), and a yellow sphere.
        5. Saves the triangulated surface to a Tecplot-compatible file.
    
    Example usage:
        - For a sphere:
            python script.py all-field-lines.dat sphere 2.0 3.0 1.0 output.dat --equal_axes
        
        - For an ellipsoid:
            python script.py all-field-lines.dat ellipsoid 2.0 1.5 1.0 3.0 1.0 output.dat --equal_axes
    """
    print(help_message)

def read_trajectories(filename,conversion_factor):
    """
    Reads trajectory data from a file.
    Each trajectory is a list of (x, y, z) tuples.
    Trajectories are separated by lines containing 'ZONE' or empty lines.
    """
    trajectories = []
    current_trajectory = []
    number_pattern = re.compile(r'-?\d+\.?\d*(?:[eE][-+]?\d+)?')  # Matches integers, decimals, and scientific notation

    try:
        with open(filename, 'r') as file:
            for line_number, line in enumerate(file, start=1):
                line = line.strip()

                # Skip empty lines or lines containing 'ZONE'
                if not line or 'ZONE' in line.upper():
                    if current_trajectory:
                        trajectories.append(current_trajectory)
                        current_trajectory = []
                    continue

                # Extract all numeric values from the line using regex
                numbers = number_pattern.findall(line)

                if len(numbers) < 3:
                    print(f"Warning: Line {line_number} does not have at least three numeric values. Skipping.")
                    continue

                try:
                    # Take the first three numbers as x, y, z
                    x, y, z = map(float, numbers[:3])

                    x=conversion_factor*x
                    y=conversion_factor*y
                    z=conversion_factor*z

                    current_trajectory.append((x, y, z))
                except ValueError:
                    print(f"Warning: Line {line_number} contains invalid numeric data. Skipping.")
                    continue

            # Add the last trajectory if it exists
            if current_trajectory:
                trajectories.append(current_trajectory)

    except FileNotFoundError:
        print(f"Error: File '{filename}' not found.")
        sys.exit(1)
    except Exception as e:
        print(f"An error occurred while reading the file: {e}")
        sys.exit(1)

    return trajectories

def calculate_sphere_intersections(trajectories, R):
    """
    Calculates intersection points between trajectories and a sphere of radius R.
    """
    all_intersection_points = []

    for trajectory in trajectories:
        num_points = len(trajectory)
        for i in range(num_points - 1):
            p1 = trajectory[i]
            p2 = trajectory[i + 1]
            intersect_pts = calculate_sphere_intersections_for_segment(p1, p2, R)
            all_intersection_points.extend(intersect_pts)

    return all_intersection_points

def calculate_sphere_intersections_for_segment(p1, p2, R):
    """
    Calculates intersection points between a line segment (p1, p2) and a sphere of radius R.
    """
    x1, y1, z1 = p1
    x2, y2, z2 = p2

    # Direction vector D = P2 - P1
    Dx = x2 - x1
    Dy = y2 - y1
    Dz = z2 - z1

    # Quadratic coefficients: a*t^2 + b*t + c = 0
    a = Dx**2 + Dy**2 + Dz**2
    b = 2 * (x1*Dx + y1*Dy + z1*Dz)
    c = x1**2 + y1**2 + z1**2 - R**2

    discriminant = b**2 - 4*a*c

    if discriminant < 0:
        return []
    else:
        sqrt_disc = math.sqrt(discriminant)
        t1 = (-b + sqrt_disc) / (2*a)
        t2 = (-b - sqrt_disc) / (2*a)
        intersections = []

        for t in [t1, t2]:
            if 0 <= t <= 1:
                intersect_point = (x1 + t*Dx, y1 + t*Dy, z1 + t*Dz)
                intersections.append(intersect_point)
        return intersections

def calculate_ellipsoid_intersections(trajectories, a, b, c):
    """
    Calculates intersection points between trajectories and an ellipsoid with semi-axes a, b, c.
    """
    all_intersection_points = []

    for trajectory in trajectories:
        num_points = len(trajectory)
        for i in range(num_points - 1):
            p1 = trajectory[i]
            p2 = trajectory[i + 1]
            intersect_pts = calculate_ellipsoid_intersections_for_segment(p1, p2, a, b, c)
            all_intersection_points.extend(intersect_pts)

    return all_intersection_points

def calculate_ellipsoid_intersections_for_segment(p1, p2, a, b, c):
    """
    Calculates intersection points between a line segment (p1, p2) and an ellipsoid with semi-axes a, b, c.
    """
    x1, y1, z1 = p1
    x2, y2, z2 = p2

    # Direction vector D = P2 - P1
    Dx = x2 - x1
    Dy = y2 - y1
    Dz = z2 - z1

    # Quadratic coefficients for intersection with ellipsoid
    A = (Dx / a) ** 2 + (Dy / b) ** 2 + (Dz / c) ** 2
    B = 2 * ((x1 * Dx) / a ** 2 + (y1 * Dy) / b ** 2 + (z1 * Dz) / c ** 2)
    C = (x1 / a) ** 2 + (y1 / b) ** 2 + (z1 / c) ** 2 - 1

    discriminant = B**2 - 4*A*C

    if discriminant < 0:
        return []
    else:
        sqrt_disc = math.sqrt(discriminant)
        t1 = (-B + sqrt_disc) / (2*A)
        t2 = (-B - sqrt_disc) / (2*A)
        intersections = []

        for t in [t1, t2]:
            if 0 <= t <= 1:
                intersect_point = (x1 + t*Dx, y1 + t*Dy, z1 + t*Dz)
                intersections.append(intersect_point)
        return intersections

def filter_trajectory_by_radius(trajectories, R2):
    """
    Filters out trajectory points that lie beyond radius R2.
    """
    filtered_trajectories = []
    for trajectory in trajectories:
        filtered_trajectory = []
        for point in trajectory:
            distance = np.linalg.norm(point)  # Calculate the distance from origin
            if distance <= R2:  # Only keep points within the distance of R2
                filtered_trajectory.append(point)
        if filtered_trajectory:
            filtered_trajectories.append(filtered_trajectory)
    return filtered_trajectories

def custom_triangulation(intersection_points, shape, params):
    """
    Perform triangulation based only on valid intersection points.

    For a sphere:
        - Convert points to spherical coordinates (theta, phi).
        - Perform Delaunay triangulation in (theta, phi) space.

    For an ellipsoid:
        - Convert points to parametric coordinates based on the ellipsoid.
        - Perform Delaunay triangulation in parametric space.
    """
    points_np = np.array(intersection_points)
    if len(points_np) < 3:
        print("Not enough points for triangulation.")
        return None

    if shape == 'sphere':
        # Convert to spherical coordinates (theta, phi)
        r = np.linalg.norm(points_np, axis=1)
        r = np.where(r == 0, 1e-6, r)  # Avoid division by zero
        theta = np.arctan2(points_np[:,1], points_np[:,0])  # Longitude
        phi = np.arccos(points_np[:,2] / r)  # Latitude
        param_coords = np.vstack((theta, phi)).T
    elif shape == 'ellipsoid':
        a, b, c = params
        # Convert to parametric coordinates based on ellipsoid
        theta = np.arctan2(points_np[:,1] * a, points_np[:,0] * b)  # Longitude
        denom = np.linalg.norm(points_np[:, :2] / [a, b], axis=1)
        denom = np.where(denom == 0, 1e-6, denom)  # Avoid division by zero
        phi = np.arccos(points_np[:,2] / (c * denom))  # Latitude
        param_coords = np.vstack((theta, phi)).T
    else:
        print("Unsupported shape for triangulation.")
        return None

    try:
        tri = Delaunay(param_coords)
        triangles = tri.simplices
        return triangles
    except Exception as e:
        print(f"Triangulation failed: {e}")
        return None

def save_tecplot(filename, unique_points, triangles):
    """
    Saves the triangulated surface to a Tecplot-compatible file.
    """
    with open(filename, 'w') as f:
        f.write("TITLE = \"Triangulated Surface\"\n")
        f.write("VARIABLES = \"X\", \"Y\", \"Z\"\n")
        f.write(f"ZONE T=\"Triangulation\" N={len(unique_points)} E={len(triangles)}\n")
        f.write("DATAPACKING=POINT ZONETYPE=FETRIANGLE\n")

        # Write the unique points
        for point in unique_points:
            f.write(f"{point[0]} {point[1]} {point[2]}\n")

        # Write the triangles (1-based index for Tecplot)
        for tri in triangles:
            f.write(f"{tri[0]+1} {tri[1]+1} {tri[2]+1}\n")

def plot_trajectories_and_surface(trajectories, unique_points_np, triangles, R3, equal_axes, shape, units, params):
    """
    Plots the field lines, shock locations, and the yellow sphere.
    """
    fig = plt.figure(figsize=(10, 8))
    ax = fig.add_subplot(111, projection='3d')

    # Plot the trajectories as Field Lines
    for trajectory in trajectories:
        trajectory_np = np.array(trajectory)
        ax.plot(trajectory_np[:, 0], trajectory_np[:, 1], trajectory_np[:, 2],
                color='blue', linewidth=1.0)

    # Plot the intersection points as Shock Locations
    if unique_points_np.size > 0:
        ax.scatter(unique_points_np[:, 0], unique_points_np[:, 1], unique_points_np[:, 2],
                   color='red', s=20)

        # Perform triangulation and plot the surface mesh
        if triangles is not None:
            tri_points = unique_points_np[triangles]
            # Only include valid intersection points in the surface mesh
            poly3d = Poly3DCollection(tri_points, edgecolor='k', alpha=1.0)
            poly3d.set_facecolor('cyan')
            ax.add_collection3d(poly3d)

    # Plot the yellow sphere with radius R3
    u, v = np.mgrid[0:2*np.pi:100j, 0:np.pi:50j]
    x = R3 * np.cos(u) * np.sin(v)
    y = R3 * np.sin(u) * np.sin(v)
    z = R3 * np.cos(v)
    ax.plot_surface(x, y, z, color='yellow', alpha=1.0)

    # Apply equal axis scaling if required
    if equal_axes and unique_points_np.size > 0:
        max_range = np.array([
            unique_points_np[:, 0].max() - unique_points_np[:, 0].min(),
            unique_points_np[:, 1].max() - unique_points_np[:, 1].min(),
            unique_points_np[:, 2].max() - unique_points_np[:, 2].min()
        ]).max() / 2

        mid_x = (unique_points_np[:, 0].max() + unique_points_np[:, 0].min()) * 0.5
        mid_y = (unique_points_np[:, 1].max() + unique_points_np[:, 1].min()) * 0.5
        mid_z = (unique_points_np[:, 2].max() + unique_points_np[:, 2].min()) * 0.5

        ax.set_xlim(mid_x - max_range, mid_x + max_range)
        ax.set_ylim(mid_y - max_range, mid_y + max_range)
        ax.set_zlim(mid_z - max_range, mid_z + max_range)

    # Set labels and title
    ax.set_xlabel(f'X [{units}]')
    ax.set_ylabel(f'Y [{units}]')
    ax.set_zlabel(f'Z [{units}]')

    ax.set_title('Field Lines and Shock Locations')

    # Create custom legend handles
    legend_elements = [
        Line2D([0], [0], color='blue', lw=1.0, label='Field Lines'),
        Line2D([0], [0], marker='o', color='w', label='Shock Locations',
               markerfacecolor='red', markersize=5)
    ]

    ax.legend(handles=legend_elements)

    plt.show()

def main():
    # Check if help is requested or insufficient arguments are provided
    if len(sys.argv) < 7 or '--help' in sys.argv:
        print_help()
        sys.exit(1)

    # Parse command-line arguments
    filename = sys.argv[1]
    shape = sys.argv[2].lower()

    # Units
    units='au'
    input_file_units='au'

    # Initialize variables
    R = R2 = R3 = a = b = c = tecplot_output_filename = None
    params = None

    # Validate shape and parse parameters
    if shape == 'sphere':
        # Expected arguments: filename, sphere, R, R2, R3, tecplot_output_filename, [--equal_axes]
        if len(sys.argv) < 7:
            print("Error: For 'sphere', provide exactly 6 arguments: <filename> <shape> <R> <R2> <R3> <tecplot_output_filename> [--equal_axes]")
            sys.exit(1)
        try:
            R = float(sys.argv[3])
            R2 = float(sys.argv[4])
            R3 = float(sys.argv[5])
            tecplot_output_filename = sys.argv[6]

            units=sys.argv[7].lower()
            input_file_units=sys.argv[8].lower()

            if R <= 0 or R2 <= 0 or R3 <= 0:
                raise ValueError
        except ValueError:
            print("Error: All radii (R, R2, R3) must be positive floating-point numbers.")
            sys.exit(1)
        params = None  # No additional parameters for sphere
    elif shape == 'ellipsoid':
        # Expected arguments: filename, ellipsoid, a, b, c, R2, R3, tecplot_output_filename, [--equal_axes]
        if len(sys.argv) < 9:
            print("Error: For 'ellipsoid', provide exactly 8 arguments: <filename> <shape> <a> <b> <c> <R2> <R3> <tecplot_output_filename> [--equal_axes]")
            sys.exit(1)
        try:
            a = float(sys.argv[3])
            b = float(sys.argv[4])
            c = float(sys.argv[5])
            R2 = float(sys.argv[6])
            R3 = float(sys.argv[7])
            tecplot_output_filename = sys.argv[8]

            units=sys.argv[9].lower()
            input_file_units=sys.argv[10].lower()

            if a <= 0 or b <= 0 or c <= 0 or R2 <= 0 or R3 <= 0:
                raise ValueError
        except ValueError:
            print("Error: Semi-axes (a, b, c) and radii (R2, R3) must be positive floating-point numbers.")
            sys.exit(1)
        params = (a, b, c)
    else:
        print("Error: Shape must be either 'sphere' or 'ellipsoid'.")
        sys.exit(1)

    # Check for optional argument '--equal_axes'
    equal_axes = False
    if '--equal_axes' in sys.argv:
        equal_axes = True

    # Read the trajectories from the input file
    conversion_factor=unit_conversion[input_file_units]/unit_conversion[units]  
    trajectories = read_trajectories(filename,conversion_factor)
    print(f"Total trajectories loaded: {len(trajectories)}")
    if not trajectories:
        print("No trajectories were loaded from the file.")
        sys.exit(0)

    # Filter trajectories to only include points within radius R2
    filtered_trajectories = filter_trajectory_by_radius(trajectories, R2)
    print(f"Total trajectories after filtering (within R2={R2}): {len(filtered_trajectories)}")
    if not filtered_trajectories:
        print(f"No trajectory points found within radius {R2}. Exiting.")
        sys.exit(0)

    # Compute intersection points based on the shape
    if shape == 'sphere':
        intersection_points = calculate_sphere_intersections(filtered_trajectories, R)
    elif shape == 'ellipsoid':
        intersection_points = calculate_ellipsoid_intersections(filtered_trajectories, *params)

    print(f"Total intersection points found: {len(intersection_points)}")
    if not intersection_points:
        print("No intersection points found. Exiting.")
        sys.exit(0)

    # Deduplicate intersection points
    intersection_points = list(map(tuple, set(map(tuple, intersection_points))))
    print(f"Total unique intersection points: {len(intersection_points)}")
    if len(intersection_points) < 3:
        print("Not enough unique intersection points for triangulation. Exiting.")
        sys.exit(0)

    # Perform triangulation
    triangles = custom_triangulation(intersection_points, shape, params)
    if triangles is None or len(triangles) == 0:
        print("No triangulation was performed. Exiting.")
        sys.exit(0)

    # Convert deduplicated intersection points to numpy array
    unique_points_np = np.array(intersection_points)

    # Save the triangulated surface to a Tecplot file
    save_tecplot(tecplot_output_filename, unique_points_np, triangles)
    print(f"Triangulated surface saved to '{tecplot_output_filename}'.")

    # Plot the field lines, shock locations, and the yellow sphere
    plot_trajectories_and_surface(filtered_trajectories, unique_points_np, triangles, R3, equal_axes, shape, units, params)

if __name__ == "__main__":
    main()

