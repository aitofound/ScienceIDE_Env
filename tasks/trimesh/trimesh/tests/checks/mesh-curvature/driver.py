#!/usr/bin/env python3
"""Restricted curvature fields; independent all-edge/all-vertex oracle, no spatial tree."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import trimesh


def independent_geometry(mesh):
    v, f = np.asarray(mesh.vertices), np.asarray(mesh.faces)
    tri = v[f]
    defect = np.full(len(v), 2 * np.pi)
    for k in range(3):
        a = tri[:, (k + 1) % 3] - tri[:, k]
        b = tri[:, (k + 2) % 3] - tri[:, k]
        angles = np.arctan2(np.linalg.norm(np.cross(a, b), axis=1), np.einsum('ij,ij->i', a, b))
        np.add.at(defect, f[:, k], -angles)
    normals = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    owners = {}
    for i, face in enumerate(f):
        for k in range(3):
            owners.setdefault(tuple(sorted((int(face[k]), int(face[(k + 1) % 3])))), []).append(i)
    edges = np.array(list(owners))
    pair = np.array(list(owners.values()))
    assert pair.shape[1] == 2
    n1, n2 = normals[pair[:, 0]], normals[pair[:, 1]]
    angle = np.arctan2(np.linalg.norm(np.cross(n1, n2), axis=1), np.einsum('ij,ij->i', n1, n2))
    # This oracle is restricted to convex generated spheres (all edge signs positive).
    return defect, v[edges[:, 0]], v[edges[:, 1]], angle


def oracle(mesh, points, radius, geometry):
    defect, start, end, angle = geometry
    direction = end - start
    length = np.linalg.norm(direction, axis=1)
    unit = direction / length[:, None]
    gaussian, mean = [], []
    margin = np.inf
    for point in points:
        dist = np.linalg.norm(mesh.vertices - point, axis=1)
        margin = min(margin, np.min(np.abs(dist - radius)))
        gaussian.append(defect[dist < radius].sum())
        # Closest point on infinite edge line; clip chord in metric arclength, not production quadratic t.
        projection = np.einsum('ij,ij->i', point - start, unit)
        perpendicular = start + projection[:, None] * unit - point
        d2 = np.einsum('ij,ij->i', perpendicular, perpendicular)
        margin = min(margin, np.min(np.abs(np.sqrt(d2) - radius)))
        half = np.sqrt(np.maximum(0, radius * radius - d2))
        segment = np.maximum(0, np.minimum(length, projection + half) - np.maximum(0, projection - half))
        segment[d2 >= radius * radius] = 0
        mean.append(np.dot(segment, angle) / 2)
    return np.array(gaussian), np.array(mean), float(margin)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--input', required=True)
    p.add_argument('--output', required=True)
    p.add_argument('--samples', type=int, default=64)
    a = p.parse_args()
    if not 8 <= a.samples <= 512:
        raise ValueError('samples must be between 8 and 512')
    factor = float(json.loads(Path(a.input).read_text())['radius_factor'])
    # No random stream: fixed Fibonacci surface sites, fixed generated triangulation.
    i = np.arange(a.samples) + 0.5
    z = 1 - 2 * i / a.samples
    phi = i * np.pi * (3 - np.sqrt(5)) + 0.2718281828459045
    points = np.column_stack((np.sqrt(1-z*z)*np.cos(phi), np.sqrt(1-z*z)*np.sin(phi), z))
    mesh = trimesh.creation.icosphere(subdivisions=3, radius=1.0)
    geometry = independent_geometry(mesh)
    np.testing.assert_allclose(mesh.vertex_defects, geometry[0], atol=1e-13, rtol=1e-12)
    radii = np.array([.137, .319, .563, .827, 1.113, 1.397, 1.731, 2.1]) * factor
    gauss, mean, margins, areas = [], [], [], []
    raw_mean = []
    support = hashlib.sha256()
    for radius in radii:
        g = trimesh.curvature.discrete_gaussian_curvature_measure(mesh, points, radius)
        m = trimesh.curvature.discrete_mean_curvature_measure(mesh, points, radius)
        og, om, margin = oracle(mesh, points, radius, geometry)
        np.testing.assert_allclose(g, og, atol=2e-12, rtol=2e-12)
        np.testing.assert_allclose(m, om, atol=2e-12, rtol=2e-12)
        if margin < 1e-9:
            raise RuntimeError('sample/radius too close to a discrete support boundary')
        area = trimesh.curvature.sphere_ball_intersection(1.0, radius)
        np.testing.assert_allclose(area, np.pi * min(radius*radius, 4.0), atol=2e-14, rtol=2e-14)
        gauss.append(g / area)
        mean.append(m / area)
        raw_mean.append(m)
        # Audit discrete memberships, never grade hashes or tree storage order.
        start, end = geometry[1:3]
        delta = end - start
        for point in points:
            support.update(np.packbits(np.linalg.norm(mesh.vertices-point, axis=1) < radius).tobytes())
            t = np.clip(np.einsum('ij,ij->i', point-start, delta) / np.einsum('ij,ij->i', delta, delta), 0, 1)
            support.update(np.packbits(np.linalg.norm(start+t[:,None]*delta-point, axis=1) < radius).tobytes())
        margins.append(margin)
        areas.append(area)
    # Whole-sphere Gauss-Bonnet and integrated mean-curvature limits, point by point.
    np.testing.assert_allclose(gauss[-1], 1.0, atol=.01, rtol=0)
    np.testing.assert_allclose(mean[-1], 1.0, atol=.01, rtol=0)
    box = trimesh.primitives.Box().subdivide()
    keys = np.lexsort(np.asarray(box.vertices).T[::-1])
    defect = box.vertex_defects[keys]
    corners = np.all(np.isclose(np.abs(box.vertices[keys]), .5), axis=1)
    np.testing.assert_allclose(defect, np.where(corners, np.pi/2, 0), atol=2e-14, rtol=2e-14)
    # Dump physical fields keyed by fixed surface query and radius, not unordered mesh storage.
    out = Path(a.output)
    out.mkdir(parents=True, exist_ok=True)
    for name, value in [('gaussian', gauss), ('mean', mean), ('cap_area', areas), ('box_defects', defect)]:
        np.save(out / (name + '.npy'), value)
    np.save(out / 'raw_mean_diagnostic.npy', raw_mean)
    (out / 'diagnostics.json').write_text(json.dumps({'minimum_support_margin': min(margins), 'support_sha256': support.hexdigest(), 'shape': [len(radii), a.samples], 'randomness': 'none; deterministic analytic sites'}, indent=2))


if __name__ == '__main__':
    main()
