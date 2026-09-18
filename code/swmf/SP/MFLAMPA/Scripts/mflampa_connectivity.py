# -*- coding: utf-8 -*-
"""
@author: nikolett@umich.edu
"""

import math

class Point:
    def __init__(self, x, y):
        self.x, self.y = x, y

    def __repr__(self):
        return f"({self.x:.2f}, {self.y:.2f})"

    def __eq__(self, other):
        return math.isclose(self.x, other.x) and math.isclose(self.y, other.y)

    def __hash__(self):
        return hash((round(self.x, 8), round(self.y, 8)))

class Triangle:
    def __init__(self, p1, p2, p3):
        if not self._is_ccw(p1, p2, p3):
            p2, p3 = p3, p2  # Enforce CCW orientation
        self.points = [p1, p2, p3]
        self._circumcenter, self._radius_sq = self._circumcircle()

    @staticmethod
    def _is_ccw(a, b, c):
        # Orientation test: positive means counter-clockwise
        return (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x) > 0

    def _circumcircle(self):
        A, B, C = self.points
        ax, ay = A.x, A.y
        bx, by = B.x, B.y
        cx, cy = C.x, C.y

        d = 2 * (ax*(by - cy) + bx*(cy - ay) + cx*(ay - by))
        if d == 0:
            return Point(float('inf'), float('inf')), float('inf')  # Degenerate

        ux = ((ax**2 + ay**2)*(by - cy) +
              (bx**2 + by**2)*(cy - ay) +
              (cx**2 + cy**2)*(ay - by)) / d
        uy = ((ax**2 + ay**2)*(cx - bx) +
              (bx**2 + by**2)*(ax - cx) +
              (cx**2 + cy**2)*(bx - ax)) / d
        center = Point(ux, uy)
        radius_sq = (center.x - ax)**2 + (center.y - ay)**2
        return center, radius_sq

    # the correct method would be determinant calculation,
    # but this is computationally heavy and slows the process
    """
    def contains_point_in_circumcircle(self, p):
        a, b, c = self.points
        ax, ay = a.x - p.x, a.y - p.y
        bx, by = b.x - p.x, b.y - p.y
        cx, cy = c.x - p.x, c.y - p.y

        det = (ax * ax + ay * ay) * (bx * cy - cx * by) \
            - (bx * bx + by * by) * (ax * cy - cx * ay) \
            + (cx * cx + cy * cy) * (ax * by - bx * ay)
        return det > 0
    """
    def contains_point_in_circumcircle(self, p):
        dx = self._circumcenter.x - p.x
        dy = self._circumcenter.y - p.y
        dist_sq = dx * dx + dy * dy
        return dist_sq < self._radius_sq
    
    def edges(self):
        return [(self.points[i], self.points[(i+1)%3]) for i in range(3)]

    def __repr__(self):
        return f"Triangle({self.points[0]}, {self.points[1]}, {self.points[2]})"

def bowyer_watson(points):
    # Step 1: Super-triangle that encompasses all
    min_x = min(p.x for p in points)
    max_x = max(p.x for p in points)
    min_y = min(p.y for p in points)
    max_y = max(p.y for p in points)

    dx = max_x - min_x
    dy = max_y - min_y
    delta_max = max(dx, dy)
    mid_x = (min_x + max_x) / 2
    mid_y = (min_y + max_y) / 2

    p1 = Point(mid_x - 20 * delta_max, mid_y - delta_max)
    p2 = Point(mid_x, mid_y + 20 * delta_max)
    p3 = Point(mid_x + 20 * delta_max, mid_y - delta_max)

    triangles = [Triangle(p1, p2, p3)]

    for point in points:
        bad_triangles = [t for t in triangles if t.contains_point_in_circumcircle(point)]

        # Find boundary edges (edges that are not shared by two triangles)
        edge_count = {}
        for tri in bad_triangles:
            for edge in tri.edges():
                e = tuple(sorted(edge, key=lambda p: (p.x, p.y)))
                if e in edge_count:
                    edge_count[e] += 1
                else:
                    edge_count[e] = 1

        boundary = [e for e, count in edge_count.items() if count == 1]

        # Remove bad triangles
        for tri in bad_triangles:
            triangles.remove(tri)

        # Re-triangulate the hole
        for edge in boundary:
            new_tri = Triangle(edge[0], edge[1], point)
            triangles.append(new_tri)

    # Remove triangles using super-triangle vertices
    final_tris = [
        t for t in triangles if p1 not in t.points and p2 not in t.points and p3 not in t.points
    ]

    return final_tris

class DelaunayTriangulator:
    def __init__(self, points):
        self.points = points
        self.triangles = []
        self._super_triangle = None
        self._build()

    def _build(self):
        # Step 1: Create super triangle
        min_x = min(p.x for p in self.points)
        max_x = max(p.x for p in self.points)
        min_y = min(p.y for p in self.points)
        max_y = max(p.y for p in self.points)

        dx = max_x - min_x
        dy = max_y - min_y
        delta_max = max(dx, dy)
        mid_x = (min_x + max_x) / 2
        mid_y = (min_y + max_y) / 2

        p1 = Point(mid_x - 20 * delta_max, mid_y - delta_max)
        p2 = Point(mid_x, mid_y + 20 * delta_max)
        p3 = Point(mid_x + 20 * delta_max, mid_y - delta_max)
        self._super_triangle = (p1, p2, p3)

        self.triangles = [Triangle(p1, p2, p3)]

        # Step 2: Insert each point into the triangulation
        for point in self.points:
            self._add_point(point)

        # Step 3: Remove triangles connected to super triangle vertices
        self.triangles = [
            t for t in self.triangles
            if all(v not in self._super_triangle for v in t.points)
        ]

    def _add_point(self, point):
        bad_triangles = [
            t for t in self.triangles if t.contains_point_in_circumcircle(point)
        ]

        # Step 1: Find boundary edges (edges not shared by two triangles)
        edge_count = {}
        for tri in bad_triangles:
            for edge in tri.edges():
                e = tuple(sorted(edge, key=lambda p: (p.x, p.y)))
                edge_count[e] = edge_count.get(e, 0) + 1

        boundary = [e for e, count in edge_count.items() if count == 1]

        # Step 2: Remove bad triangles
        for tri in bad_triangles:
            self.triangles.remove(tri)

        # Step 3: Retriangulate the hole
        for edge in boundary:
            new_tri = Triangle(edge[0], edge[1], point)
            self.triangles.append(new_tri)

    def get_triangles(self):
        return self.triangles

def plot_triangulation(points, triangles, show_point_labels=False, title ="Delaunay Triangulation"):
    fig, ax = plt.subplots()

    xs = [p.x for p in points]
    ys = [p.y for p in points]
    ax.plot(xs, ys, 'o', color='black')

    if show_point_labels:
        for i, p in enumerate(points):
            ax.text(p.x + 0.01, p.y + 0.01, f"P{i}", fontsize=9, color='darkblue')

    for tri in triangles:
        pts = tri.points + [tri.points[0]]  # Close the triangle
        x_coords = [p.x for p in pts]
        y_coords = [p.y for p in pts]
        ax.plot(x_coords, y_coords, color='red', linewidth=1)

    ax.set_aspect('equal')
    plt.title(title)
    plt.grid(True)
    plt.show()

cols = ['LagrID', 'X', 'Y', 'Z', 'Rho', 'T', 'Ux', 'Uy', 'Uz', 'Bx', 'By', 'Bz',
       'Wave1', 'Wave2', 'flux_total', 'flux_Channel01', 'flux_Channel02',
       'flux_Channel03', 'flux_Channel04', 'flux_Channel05', 'flux_Channel06', 'eflux']

def read_custom_csv(filename, skiprows=16, cols = cols):
    with open(filename, 'r') as f:
        lines = f.readlines()[skiprows:]  # Skip first 16 lines

    data = [line.replace("\n", "  ").strip().split()[1:] for line in lines if line.strip()]
    return data


# THIS CODE IS HEAVILY IN DEVELOPMENT
# THE DELANUAY-TRIANGULATION AND THE FIELD LINE FIELD READ-IN FUNCTIONS ARE FINISHED
# THE TRIANGULATION USES THE BOWYER-WATSON ALGORYTHM
# https://gdmc.nl/publications/2002/Bowyer_Watson_algorithm.pdf

# TO BE DONE:
# - ITERATIVE FIELD-LINE READ-IN BASED ON EPHEMERIS FILES
# - FINDING CLOSEST FIELD-LINES BASED ON TRIANGULATION

if __name__ == "__main__":
    
    # TRIANGULATION SANITY CHECK 1
    # CREATING RANDOM COORDINATES TO CALCULATE TRIANGULATION ON
    
    import random    
    coords = [[random.uniform(0, 1), random.uniform(0, 1)] for _ in range(15)]
    input_points = [Point(x, y) for x, y in coords]
    tris = bowyer_watson(input_points)
    triangulator = DelaunayTriangulator(input_points)
    triangles = triangulator.get_triangles()

    
    # TRIANGULATION SANITY CHECK 2
    # PLOT THE TRIANGULATION
    
    import matplotlib.pyplot as plt
    plot_triangulation(input_points, tris, show_point_labels=True, title="Function Definition")
    plot_triangulation(input_points, triangles, show_point_labels=True, title="Class Definition")
    
        
    # TRIANGULATION SANITY CHECK 3
    # COMPARE WITH SCIPY'S DELANUAY METHOD - GIVES THE SAME RESULT
    """
    import numpy as np
    from scipy.spatial import Delaunay
    
    coords = np.array(coords)
    tri = Delaunay(coords)
    
    fig, ax = plt.subplots()
    ax.triplot(coords[:,0], coords[:,1], tri.simplices)
    ax.plot(coords[:,0], coords[:,1], 'o')
    ax.set_aspect('equal')
    plt.show()
    """
    
    # TRIANGULATION SANITY CHECK 4
    # CHECK RUNTIME FOR FUNCTION AND CLASS - ROUGHLY THE SAME
    # THE CLASS RUNS FASTER, RECOMMEND USING THAT
    """
    import time 
    coords = [[random.uniform(0, 100), random.uniform(0, 100)] for _ in range(10000)]
    input_points = [Point(x, y) for x, y in coords]
    start_time = time.perf_counter()
    tris = bowyer_watson(input_points)
    end_time = time.perf_counter()
    execution_time = end_time - start_time
    print(f"The function took {execution_time} seconds to run.")
    start_time = time.perf_counter()
    triangulator = DelaunayTriangulator(input_points)
    triangles = triangulator.get_triangles()
    end_time = time.perf_counter()
    execution_time = end_time - start_time
    print(f"The class took {execution_time} seconds to run.")
    """
    
    # FIELD LINE READ-IN SANITY CHECK
    # READING IN THE FIELDLINE FILE 
    """
    import numpy as np
    data_dict = read_custom_csv(filename)
    data = {col.lower(): np.array([float(row[i]) for row in data_dict], dtype=float) for i, col in enumerate(cols)}
    """






