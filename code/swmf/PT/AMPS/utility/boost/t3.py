import numpy as np
from scipy.spatial import Delaunay

def tetrahedralize_and_filter(cell_corners, is_inside, intersection_points, edge_intersections):
    """
    Tetrahedralizes the active region of a cube and removes tetrahedra connected to "inside" points.

    Parameters:
        cell_corners: Array or list of shape (8, 3) with the coordinates of the cell corners.
        is_inside: Boolean list of size 8 indicating whether each corner is inside the surface.
        intersection_points: Array or list of intersection points on the edges of the cell.
        edge_intersections: Dictionary mapping edge indices to indices of intersection_points.

    Returns:
        vertices: Array of vertex coordinates used in the tetrahedralization.
        tetrahedra: Array of tetrahedra indices.
    """
    # Convert inputs to numpy arrays if they aren't already
    cell_corners = np.array(cell_corners, dtype=np.float64)
    intersection_points = np.array(intersection_points, dtype=np.float64)
    
    # Start with cell corners as vertices
    vertices = cell_corners.tolist()
    vertex_map = {i: i for i in range(len(cell_corners))}

    # Add intersection points
    for edge, idx in edge_intersections.items():
        vertex_map[f'edge_{edge}'] = len(vertices)
        vertices.append(intersection_points[idx].tolist() if isinstance(intersection_points[idx], np.ndarray) 
                       else intersection_points[idx])

    vertices = np.array(vertices)

    # Perform Delaunay triangulation
    delaunay = Delaunay(vertices)

    # Filter tetrahedra: Remove tetrahedra connected to "inside" points
    filtered_tetrahedra = []
    for tet in delaunay.simplices:
        # Exclude if any vertex is an "inside" point
        if not any(v < len(is_inside) and is_inside[v] for v in tet):
            filtered_tetrahedra.append(tet.tolist())

    return vertices.tolist(), filtered_tetrahedra
