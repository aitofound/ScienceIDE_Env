#!/usr/bin/env python3
"""
Trajectory Processing and Surface Mesh Generation Script

This script reads trajectory data from an input file, extracts points at a specified
distance along each trajectory, generates a surface mesh from these points, visualizes
both the trajectories and the mesh within a specified maximum distance from the origin,
optionally displays a Sun sphere at the center, and saves the mesh to output files
in various formats (VTK, Tecplot, STL, OBJ, PLY).

Usage:
    python script.py <input_file> -L <distance> [--units <units>] [--input_file_units <units>] [--max_distance <R>] [--sun]
                  [--vtk_output <filename>] [--tecplot_output <filename>] [--stl_output <filename>]
                  [--obj_output <filename>] [--ply_output <filename>]

Example:
    python script.py all-field-lines.dat -L 10 --units rSun --input_file_units AU -R 13 --sun \
        --vtk_output out.vtk --tecplot_output mesh.dat --stl_output mesh.stl --obj_output mesh.obj --ply_output mesh.ply

Run the example with input from data/input/FieldLines: 
    python field-line-distance-cut.py -L 10 --units rSun --input_file_unit AU -R 13 --sun all-field-lines.dat --tecplot_output mesh.dat 
"""

import sys
import numpy as np
import pyvista as pv
import os
import argparse

# Define unit conversion factors to meters
UNIT_FACTORS = {
    'meters': 1.0,
    'rSun': 6.957e8,    # Solar radius in meters
    'AU': 1.496e11      # Astronomical Unit in meters
}

def convert_units(data, from_unit, to_unit):
    """
    Converts data from one unit to another.

    Parameters:
    - data (float or np.ndarray): The data to convert.
    - from_unit (str): The current unit of the data.
    - to_unit (str): The desired unit for the data.

    Returns:
    - float or np.ndarray: The converted data.
    """
    try:
        factor = UNIT_FACTORS[from_unit] / UNIT_FACTORS[to_unit]
    except KeyError as e:
        raise ValueError(f"Unsupported unit: {e.args[0]}")
    return data * factor

def read_trajectories(filename, input_units, desired_units):
    """
    Reads trajectory data from a file, converting units as necessary.

    Parameters:
    - filename (str): Path to the input file containing trajectory data.
    - input_units (str): Units of the input file data.
    - desired_units (str): Units to convert the data to.

    Returns:
    - List[np.ndarray]: A list of NumPy arrays, each representing a trajectory with shape (N, 3).
    """
    trajectories = []
    current_trajectory = []

    try:
        with open(filename, 'r') as file:
            for line in file:
                line = line.strip()
                if line.startswith("ZONE"):
                    if current_trajectory:
                        traj_array = np.array(current_trajectory)
                        traj_converted = convert_units(traj_array, input_units, desired_units)
                        trajectories.append(traj_converted)
                    current_trajectory = []
                else:
                    try:
                        x, y, z = map(float, line.split()[:3])
                        current_trajectory.append([x, y, z])
                    except ValueError:
                        pass  # Skip non-numerical lines

        if current_trajectory:
            traj_array = np.array(current_trajectory)
            traj_converted = convert_units(traj_array, input_units, desired_units)
            trajectories.append(traj_converted)
    except FileNotFoundError:
        print(f"Error: The file '{filename}' does not exist.")
        sys.exit(1)
    except Exception as e:
        print(f"An error occurred while reading the file: {e}")
        sys.exit(1)

    return trajectories

def find_point_at_distance(trajectory, distance):
    """
    Finds a point along a trajectory at a specified cumulative distance from the start.

    Parameters:
    - trajectory (np.ndarray): An array of shape (N, 3) representing the trajectory points.
    - distance (float): The cumulative distance at which to find the point.

    Returns:
    - np.ndarray: The interpolated point at the specified distance.
    """
    # Ensure the trajectory has at least two points for distance calculation
    if len(trajectory) < 2:
        if len(trajectory) == 1:
            return trajectory[0]
        else:
            raise ValueError("Trajectory must contain at least one point.")

    # Calculate the distances between consecutive points
    segment_distances = np.linalg.norm(np.diff(trajectory, axis=0), axis=1)
    cumulative_distance = np.cumsum(segment_distances)

    # Handle zero or negative distance inputs
    if distance <= 0:
        return trajectory[0]

    # If the distance exceeds the total length, return the last point
    total_length = cumulative_distance[-1]
    if distance >= total_length:
        return trajectory[-1]

    # Find the index where the cumulative distance exceeds the specified distance
    index = np.searchsorted(cumulative_distance, distance)

    # Handle the case where index is 0 separately to avoid negative indexing
    if index == 0:
        t = distance / segment_distances[0]
        return (1 - t) * trajectory[0] + t * trajectory[1]

    # Compute the fractional distance between the two surrounding points
    previous_cumulative = cumulative_distance[index - 1]
    segment_length = segment_distances[index]
    t = (distance - previous_cumulative) / segment_length

    # Interpolate between trajectory[index] and trajectory[index + 1]
    return (1 - t) * trajectory[index] + t * trajectory[index + 1]

def create_surface_mesh(points):
    """
    Generates a 2D Delaunay triangulated surface mesh from a set of 3D points.

    Parameters:
    - points (List[np.ndarray]): A list of 3D points.

    Returns:
    - pv.PolyData: The generated surface mesh.
    """
    if not points:
        raise ValueError("No points provided for mesh creation.")

    cloud = pv.PolyData(points)
    try:
        surface = cloud.delaunay_2d()
    except Exception as e:
        print(f"An error occurred during Delaunay triangulation: {e}")
        sys.exit(1)
    return surface

def save_to_tecplot(surface, filename):
    """
    Saves the surface mesh to a Tecplot ASCII file.

    Parameters:
    - surface (pv.PolyData): The surface mesh to save.
    - filename (str): Path to the output Tecplot file.
    """
    try:
        with open(filename, 'w') as f:
            # Write header
            f.write("TITLE = \"Surface Mesh\"\n")
            f.write("VARIABLES = \"X\" \"Y\" \"Z\"\n")
            
            # Extract points and cells
            points = surface.points
            faces = surface.faces.reshape((-1, 4))  # Each face starts with the number of points (3 for triangles)
            cells = faces[:, 1:4]  # Extract the vertex indices

            num_points = len(points)
            num_cells = len(cells)
            
            # Write zone header
            f.write(f"ZONE T=\"Surface Zone\", N={num_points}, E={num_cells}, F=FEPOINT, ET=FETRIANGLE\n")
            
            # Write point coordinates
            for point in points:
                f.write(f"{point[0]} {point[1]} {point[2]}\n")
            
            # Write connectivity (1-based indexing for Tecplot)
            for cell in cells:
                f.write(f"{int(cell[0])+1} {int(cell[1])+1} {int(cell[2])+1}\n")
        
        print(f"Surface mesh successfully saved to Tecplot file '{filename}'.")
    except Exception as e:
        print(f"An error occurred while saving the Tecplot mesh: {e}")
        sys.exit(1)

def save_to_stl(surface, filename):
    """
    Saves the surface mesh to an STL file.

    Parameters:
    - surface (pv.PolyData): The surface mesh to save.
    - filename (str): Path to the output STL file.
    """
    try:
        surface.save(filename)
        print(f"Surface mesh successfully saved to STL file '{filename}'.")
    except Exception as e:
        print(f"An error occurred while saving the STL mesh: {e}")
        sys.exit(1)

def save_to_obj(surface, filename):
    """
    Saves the surface mesh to an OBJ file.

    Parameters:
    - surface (pv.PolyData): The surface mesh to save.
    - filename (str): Path to the output OBJ file.
    """
    try:
        surface.save(filename)
        print(f"Surface mesh successfully saved to OBJ file '{filename}'.")
    except Exception as e:
        print(f"An error occurred while saving the OBJ mesh: {e}")
        sys.exit(1)

def save_to_ply(surface, filename):
    """
    Saves the surface mesh to a PLY file.

    Parameters:
    - surface (pv.PolyData): The surface mesh to save.
    - filename (str): Path to the output PLY file.
    """
    try:
        surface.save(filename)
        print(f"Surface mesh successfully saved to PLY file '{filename}'.")
    except Exception as e:
        print(f"An error occurred while saving the PLY mesh: {e}")
        sys.exit(1)

def visualize(trajectories, surface, units, max_distance, sun):
    """
    Visualizes the trajectories and the generated surface mesh in a 3D plot,
    optionally displaying a Sun sphere at the origin.

    Parameters:
    - trajectories (List[np.ndarray]): A list of trajectory points.
    - surface (pv.PolyData): The surface mesh to visualize.
    - units (str): The units of measurement for axis labels.
    - max_distance (float or None): Maximum distance from the origin to display. If None, no limit.
    - sun (bool): Flag indicating whether to display the Sun sphere.
    """
    plotter = pv.Plotter()

    # Define a list of colors for different trajectories
    colors = ['blue', 'green', 'cyan', 'magenta', 'yellow', 'orange', 'purple', 'brown']

    # Plot trajectories within max_distance
    for idx, trajectory in enumerate(trajectories, start=1):
        if len(trajectory) < 2:
            continue  # Skip trajectories that cannot form a line

        # Filter trajectory points within max_distance
        if max_distance is not None:
            distances = np.linalg.norm(trajectory, axis=1)
            within = distances <= max_distance
            if not np.any(within):
                print(f"Warning: Trajectory {idx} has no points within max_distance {max_distance}. Skipping.")
                continue
            # Find the last point within max_distance
            last_within = np.where(within)[0][-1] + 1  # +1 to include the point
            clipped_trajectory = trajectory[:last_within]
        else:
            clipped_trajectory = trajectory

        if len(clipped_trajectory) < 2:
            continue  # Need at least two points to form a line

        polyline = pv.lines_from_points(clipped_trajectory)
        color = colors[(idx - 1) % len(colors)]
        plotter.add_mesh(
            polyline,
            color=color,
            line_width=2,
            render_lines_as_tubes=True,
            name=f"Trajectory_{idx}"
        )

    # Plot surface mesh (opaque)
    plotter.add_mesh(surface, color='red', opacity=1.0, show_edges=True, name="Surface Mesh")

    # Plot triangulation edges
    edges = surface.extract_feature_edges(feature_angle=30, boundary_edges=True, non_manifold_edges=False)
    plotter.add_mesh(edges, color='black', line_width=1, name="Mesh Edges")

    # Add coordinate axes without labels
    plotter.add_axes(
        color='black',
        line_width=2,
        labels_off=True,
        interactive=True  # Allow interactive manipulation
    )

    # Add text annotations for axis labels
    # Positioning text at the ends of each axis
    if max_distance is not None:
        axis_length = max_distance
    else:
        # Determine axis length based on data
        all_points = np.vstack(trajectories + [surface.points])
        axis_length = np.max(np.abs(all_points)) * 1.1  # Slightly larger than max point

    # Adding labels manually
    # Adjust positions as needed to prevent overlapping
    plotter.add_text(f'X ({units})', position='upper_right', font_size=10, color='black')
    plotter.add_text(f'Y ({units})', position='lower_left', font_size=10, color='black')
    plotter.add_text(f'Z ({units})', position='upper_left', font_size=10, color='black')

    # Optionally, add the Sun sphere
    legend_entries = [f"Trajectory {idx}" for idx in range(1, len(trajectories)+1)]
    legend_colors = colors[:len(trajectories)]

    if sun:
        # Radius of the Sun in plotting units
        radius_meters = UNIT_FACTORS['rSun']  # 6.957e8 meters
        radius_plot_units = convert_units(radius_meters, 'meters', units)
        print(f"Sun sphere radius in {units}: {radius_plot_units}")

        # Create a sphere representing the Sun
        sun_sphere = pv.Sphere(radius=radius_plot_units, center=(0, 0, 0))
        plotter.add_mesh(sun_sphere, color='yellow', opacity=1.0, name='Sun')

        # Add Sun to legend
        legend_entries.append('Sun')
        legend_colors.append('yellow')

    # Adjust camera to focus within max_distance
    if max_distance is not None:
        camera_distance = max_distance * 2  # Position the camera at twice the max_distance for better visibility
        camera_position = [(camera_distance, camera_distance, camera_distance),  # Camera location
                           (0, 0, 0),                                         # Focal point
                           (0, 0, 1)]                                         # View up direction
        plotter.camera_position = camera_position
    else:
        # Automatically adjust the view to fit all data
        plotter.reset_camera()

    # Add a legend to differentiate trajectories and the Sun
    if legend_entries:
        plotter.add_legend(labels=list(zip(legend_entries, legend_colors)), bcolor='white')

    plotter.show_grid()
    plotter.show()

def save_to_tecplot(surface, filename):
    """
    Saves the surface mesh to a Tecplot ASCII file.

    Parameters:
    - surface (pv.PolyData): The surface mesh to save.
    - filename (str): Path to the output Tecplot file.
    """
    try:
        with open(filename, 'w') as f:
            # Write header
            f.write("TITLE = \"Surface Mesh\"\n")
            f.write("VARIABLES = \"X\" \"Y\" \"Z\"\n")
            
            # Extract points and cells
            points = surface.points
            faces = surface.faces.reshape((-1, 4))  # Each face starts with the number of points (3 for triangles)
            cells = faces[:, 1:4]  # Extract the vertex indices

            num_points = len(points)
            num_cells = len(cells)
            
            # Write zone header
            f.write(f"ZONE T=\"Surface Zone\", N={num_points}, E={num_cells}, F=FEPOINT, ET=FETRIANGLE\n")
            
            # Write point coordinates
            for point in points:
                f.write(f"{point[0]} {point[1]} {point[2]}\n")
            
            # Write connectivity (1-based indexing for Tecplot)
            for cell in cells:
                f.write(f"{int(cell[0])+1} {int(cell[1])+1} {int(cell[2])+1}\n")
        
        print(f"Surface mesh successfully saved to Tecplot file '{filename}'.")
    except Exception as e:
        print(f"An error occurred while saving the Tecplot mesh: {e}")
        sys.exit(1)

def save_to_stl(surface, filename):
    """
    Saves the surface mesh to an STL file.

    Parameters:
    - surface (pv.PolyData): The surface mesh to save.
    - filename (str): Path to the output STL file.
    """
    try:
        surface.save(filename)
        print(f"Surface mesh successfully saved to STL file '{filename}'.")
    except Exception as e:
        print(f"An error occurred while saving the STL mesh: {e}")
        sys.exit(1)

def save_to_obj(surface, filename):
    """
    Saves the surface mesh to an OBJ file.

    Parameters:
    - surface (pv.PolyData): The surface mesh to save.
    - filename (str): Path to the output OBJ file.
    """
    try:
        surface.save(filename)
        print(f"Surface mesh successfully saved to OBJ file '{filename}'.")
    except Exception as e:
        print(f"An error occurred while saving the OBJ mesh: {e}")
        sys.exit(1)

def save_to_ply(surface, filename):
    """
    Saves the surface mesh to a PLY file.

    Parameters:
    - surface (pv.PolyData): The surface mesh to save.
    - filename (str): Path to the output PLY file.
    """
    try:
        surface.save(filename)
        print(f"Surface mesh successfully saved to PLY file '{filename}'.")
    except Exception as e:
        print(f"An error occurred while saving the PLY mesh: {e}")
        sys.exit(1)

def save_surface_mesh(surface, filename):
    """
    Saves the generated surface mesh to a file in a standard format.

    Parameters:
    - surface (pv.PolyData): The surface mesh to save.
    - filename (str): Path to the output file.
    """
    try:
        surface.save(filename)
        print(f"Surface mesh successfully saved to '{filename}'.")
    except Exception as e:
        print(f"An error occurred while saving the mesh: {e}")
        sys.exit(1)

def print_trajectory_statistics(trajectories):
    """
    Computes and prints statistical information about the trajectories.

    Parameters:
    - trajectories (List[np.ndarray]): A list of trajectory points.
    """
    num_trajectories = len(trajectories)
    total_points = sum(len(traj) for traj in trajectories)
    avg_points = total_points / num_trajectories if num_trajectories > 0 else 0
    min_points = min(len(traj) for traj in trajectories) if trajectories else 0
    max_points = max(len(traj) for traj in trajectories) if trajectories else 0

    print("\nTrajectory Statistics:")
    print(f"  Number of trajectories: {num_trajectories}")
    print(f"  Total number of points: {total_points}")
    print(f"  Average points per trajectory: {avg_points:.2f}")
    print(f"  Minimum points in a trajectory: {min_points}")
    print(f"  Maximum points in a trajectory: {max_points}")

def parse_arguments():
    """
    Parses and validates command-line arguments.

    Returns:
    - argparse.Namespace: The parsed arguments.
    """
    parser = argparse.ArgumentParser(
        description="Process trajectory data, generate a surface mesh, visualize within a specified maximum distance, optionally display a Sun sphere, and save the mesh in various formats. Run the example with input from data/input/FieldLines: python field-line-distance-cut.py -L 10 --units rSun --input_file_unit AU -R 13 --sun all-field-lines.dat --tecplot_output mesh.dat ", 
        formatter_class=argparse.RawTextHelpFormatter
    )

    # Positional arguments
    parser.add_argument('input_file', type=str, help='Path to the input file containing trajectory data.')

    # Optional arguments
    parser.add_argument('-L', '--distance', type=float, required=True,
                        help='Distance from the beginning of the trajectories where the surface is created.')
    parser.add_argument('--units', type=str, choices=UNIT_FACTORS.keys(), default='meters',
                        help='Units for plotting (default: meters). Options: meters, rSun, AU.')
    parser.add_argument('--input_file_units', type=str, choices=UNIT_FACTORS.keys(), default='meters',
                        help='Units used in the input file to define trajectory points (default: meters).')
    parser.add_argument('-R', '--max_distance', type=float, default=None,
                        help='Maximum distance from the origin for plotting trajectories.')
    parser.add_argument('--sun', action='store_true',
                        help='Display a non-transparent sphere representing the Sun at the origin with a radius of 1 rSun.')
    parser.add_argument('--vtk_output', type=str, default=None,
                        help='Path to the output VTK file to save the surface mesh (e.g., mesh.vtk).')
    parser.add_argument('--tecplot_output', type=str, default=None,
                        help='Path to the output Tecplot file to save the surface mesh (e.g., mesh.dat).')
    parser.add_argument('--stl_output', type=str, default=None,
                        help='Path to the output STL file to save the surface mesh (e.g., mesh.stl).')
    parser.add_argument('--obj_output', type=str, default=None,
                        help='Path to the output OBJ file to save the surface mesh (e.g., mesh.obj).')
    parser.add_argument('--ply_output', type=str, default=None,
                        help='Path to the output PLY file to save the surface mesh (e.g., mesh.ply).')

    return parser.parse_args()

def main():
    """
    Main function to orchestrate the workflow.
    """
    args = parse_arguments()

    input_file = args.input_file
    distance = args.distance
    units = args.units
    input_units = args.input_file_units
    max_distance = args.max_distance
    sun = args.sun
    vtk_output = args.vtk_output
    tecplot_output = args.tecplot_output
    stl_output = args.stl_output
    obj_output = args.obj_output
    ply_output = args.ply_output

    # Validate distance
    if distance < 0:
        print("Error: Distance (-L) must be a non-negative number.")
        sys.exit(1)

    # Validate max_distance if provided
    if max_distance is not None and max_distance <= 0:
        print("Error: Maximum distance (-R) must be a positive number.")
        sys.exit(1)

    # Check if at least one output format is specified
    if not any([vtk_output, tecplot_output, stl_output, obj_output, ply_output]):
        print("Error: At least one output format must be specified using --vtk_output, --tecplot_output, --stl_output, --obj_output, or --ply_output.")
        sys.exit(1)

    # Validate output file extensions
    if vtk_output:
        valid_vtk_extensions = ['.vtk', '.stl', '.obj', '.ply']
        _, ext = os.path.splitext(vtk_output)
        if ext.lower() not in valid_vtk_extensions:
            print(f"Warning: The VTK output file extension '{ext}' is not standard. Consider using one of the following extensions for VTK: {', '.join(valid_vtk_extensions)}")

    if tecplot_output:
        valid_tecplot_extensions = ['.dat', '.plt']
        _, tec_ext = os.path.splitext(tecplot_output)
        if tec_ext.lower() not in valid_tecplot_extensions:
            print(f"Warning: The Tecplot output file extension '{tec_ext}' is not standard. Consider using '.dat' or '.plt'.")

    if stl_output:
        valid_stl_extensions = ['.stl']
        _, stl_ext = os.path.splitext(stl_output)
        if stl_ext.lower() not in valid_stl_extensions:
            print(f"Warning: The STL output file extension '{stl_ext}' is not standard. Consider using '.stl'.")

    if obj_output:
        valid_obj_extensions = ['.obj']
        _, obj_ext = os.path.splitext(obj_output)
        if obj_ext.lower() not in valid_obj_extensions:
            print(f"Warning: The OBJ output file extension '{obj_ext}' is not standard. Consider using '.obj'.")

    if ply_output:
        valid_ply_extensions = ['.ply']
        _, ply_ext = os.path.splitext(ply_output)
        if ply_ext.lower() not in valid_ply_extensions:
            print(f"Warning: The PLY output file extension '{ply_ext}' is not standard. Consider using '.ply'.")

    # Read and convert trajectories
    trajectories = read_trajectories(input_file, input_units, units)

    # Print trajectory statistics
    print_trajectory_statistics(trajectories)

    # Extract points at the specified distance from each trajectory
    points_at_distance = []
    for i, traj in enumerate(trajectories):
        try:
            point = find_point_at_distance(traj, distance)
            # Check if the point is within max_distance if max_distance is set
            if max_distance is not None:
                distance_from_origin = np.linalg.norm(point)
                if distance_from_origin > max_distance:
                    print(f"Warning: Trajectory {i+1}'s point at distance {distance} exceeds max_distance {max_distance}. Skipping this point.")
                    continue
            points_at_distance.append(point)
        except ValueError as ve:
            print(f"Warning: Trajectory {i+1} skipped due to error: {ve}")

    if not points_at_distance:
        print("No valid points extracted for mesh construction within the specified max_distance.")
        sys.exit(1)

    # Print the extracted points
    print("\nPoints used for surface mesh construction:")
    for i, point in enumerate(points_at_distance):
        distance_from_origin = np.linalg.norm(point)
        print(f"  Trajectory {i+1}: {point} | Distance from origin: {distance_from_origin} {units}")

    # Create the surface mesh from the extracted points
    surface = create_surface_mesh(points_at_distance)

    # Visualize the trajectories and the surface mesh, optionally displaying the Sun
    visualize(trajectories, surface, units, max_distance, sun)

    # Save the surface mesh to the specified output formats
    if vtk_output:
        save_surface_mesh(surface, vtk_output)

    if tecplot_output:
        save_to_tecplot(surface, tecplot_output)

    if stl_output:
        save_to_stl(surface, stl_output)

    if obj_output:
        save_to_obj(surface, obj_output)

    if ply_output:
        save_to_ply(surface, ply_output)

if __name__ == "__main__":
    main()

