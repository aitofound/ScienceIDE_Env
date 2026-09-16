"""NumPy-only rigid-body kinematics from immutable MuJoCo model input facts.

The manifest is generated from the pinned input model during authoring and is
part of the check, never accepted from candidate output. Joint enumeration is
MuJoCo's public ordering: free=0, ball=1, slide=2, hinge=3. Frame enumeration in
this check is body=0, geom=1, site=2. No simulator or candidate module is imported.
"""
import numpy as np


def quaternion_matrix(q):
    q = np.asarray(q, dtype=np.float64)
    norm = np.linalg.norm(q, axis=-1, keepdims=True)
    if np.any(norm < 1e-15) or not np.isfinite(norm).all():
        raise ValueError("invalid quaternion")
    w, x, y, z = np.moveaxis(q / norm, -1, 0)
    out = np.empty(q.shape[:-1] + (3, 3), dtype=np.float64)
    out[..., 0, 0] = 1 - 2 * (y*y + z*z)
    out[..., 0, 1] = 2 * (x*y - z*w)
    out[..., 0, 2] = 2 * (x*z + y*w)
    out[..., 1, 0] = 2 * (x*y + z*w)
    out[..., 1, 1] = 1 - 2 * (x*x + z*z)
    out[..., 1, 2] = 2 * (y*z - x*w)
    out[..., 2, 0] = 2 * (x*z - y*w)
    out[..., 2, 1] = 2 * (y*z + x*w)
    out[..., 2, 2] = 1 - 2 * (x*x + y*y)
    return out


def quaternion_product(a, b):
    aw, av = a[..., :1], a[..., 1:]
    bw, bv = b[..., :1], b[..., 1:]
    return np.concatenate((aw*bw - np.sum(av*bv, axis=-1, keepdims=True),
                           aw*bv + bw*av + np.cross(av, bv)), axis=-1)


def rotation_vector_quaternion(v):
    theta = np.linalg.norm(v, axis=-1, keepdims=True)
    # np.sinc(x)=sin(pi*x)/(pi*x), including the analytic value at zero.
    return np.concatenate((np.cos(theta/2), .5*np.sinc(theta/(2*np.pi))*v), axis=-1)


def transform(pos, quat):
    r = quaternion_matrix(quat)
    result = np.broadcast_to(np.eye(4), r.shape[:-2] + (4, 4)).copy()
    result[..., :3, :3] = r
    result[..., :3, 3] = pos
    return result


def forward_kinematics(q, manifest):
    """Return world transforms for the trusted ordered frame inventory."""
    q = np.asarray(q, dtype=np.float64)
    if q.shape[-1] != len(manifest["qpos0"]) or not np.isfinite(q).all():
        raise ValueError("invalid q dimension or nonfinite state")
    batch = q.shape[:-1]
    parent = np.asarray(manifest["body_parentid"], dtype=int)
    body_ids = []
    fixed_frames = []
    for kind, idx in zip(manifest["frame_kind"], manifest["frame_id"], strict=True):
        kind, idx = int(kind), int(idx)
        if kind == 0:
            body_ids.append(idx)
            fixed_frames.append(np.eye(4))
        elif kind in (1, 2):
            prefix = "geom" if kind == 1 else "site"
            body_ids.append(int(manifest[prefix + "_bodyid"][idx]))
            fixed_frames.append(transform(manifest[prefix + "_pos"][idx], manifest[prefix + "_quat"][idx]))
        else:
            raise ValueError("unsupported frame kind")
    needed = {0}
    for body in body_ids:
        while body:
            if body < 0 or body >= len(parent) or parent[body] >= body:
                raise ValueError("invalid parent topology")
            needed.add(body)
            body = int(parent[body])
    world = {0: np.broadcast_to(np.eye(4), batch + (4, 4))}
    mocap = manifest.get("body_mocapid", np.full(len(parent), -1))
    for body in sorted(needed - {0}):
        if mocap[body] >= 0:
            raise ValueError("frame has a mocap ancestor; immutable mocap inputs are required")
        local = np.broadcast_to(transform(manifest["body_pos"][body], manifest["body_quat"][body]), batch + (4, 4)).copy()
        start = int(manifest["body_jntadr"][body])
        count = int(manifest["body_jntnum"][body])
        for jid in range(start, start + count):
            kind = int(manifest["jnt_type"][jid])
            adr = int(manifest["jnt_qposadr"][jid])
            axis = np.asarray(manifest["jnt_axis"][jid])
            anchor = np.asarray(manifest["jnt_pos"][jid])
            if kind == 0:
                if parent[body] != 0 or count != 1:
                    raise ValueError("free joint must be the only joint of a world child")
                local = transform(q[..., adr:adr+3], q[..., adr+3:adr+7])
                continue
            delta = np.broadcast_to(np.eye(4), batch + (4, 4)).copy()
            if kind == 2:
                delta[..., :3, 3] = (q[..., adr] - manifest["qpos0"][adr])[..., None] * axis
            elif kind in (1, 3):
                quat = q[..., adr:adr+4] if kind == 1 else rotation_vector_quaternion((q[..., adr] - manifest["qpos0"][adr])[..., None] * axis)
                rot = quaternion_matrix(quat)
                delta[..., :3, :3] = rot
                delta[..., :3, 3] = anchor - np.einsum("...ij,j->...i", rot, anchor)
            else:
                raise ValueError("unsupported joint kind")
            local = local @ delta
        world[body] = world[int(parent[body])] @ local
    if not body_ids:
        return np.empty(batch + (0, 4, 4), dtype=np.float64)
    return np.stack([world[b] @ t for b, t in zip(body_ids, fixed_frames, strict=True)], axis=-3)


def integrate(q, v, dt, manifest):
    """Independent scalar/free/ball position integration in tangent coordinates."""
    q = np.asarray(q, dtype=np.float64)
    v = np.asarray(v, dtype=np.float64)
    dt = np.asarray(dt, dtype=np.float64)
    out = q.copy()
    dq = v * dt[..., None]
    for kind, qa, va in zip(manifest["jnt_type"], manifest["jnt_qposadr"], manifest["jnt_dofadr"], strict=True):
        kind, qa, va = int(kind), int(qa), int(va)
        if kind in (2, 3):
            out[..., qa] += dq[..., va]
        else:
            if kind == 0:
                out[..., qa:qa+3] += dq[..., va:va+3]
                qa, va = qa+3, va+3
            elif kind != 1:
                raise ValueError("unsupported joint kind")
            rotated = quaternion_product(q[..., qa:qa+4], rotation_vector_quaternion(dq[..., va:va+3]))
            norm = np.linalg.norm(rotated, axis=-1, keepdims=True)
            if np.any(norm < 1e-15):
                raise ValueError("zero quaternion")
            out[..., qa:qa+4] = rotated / norm
    return out
