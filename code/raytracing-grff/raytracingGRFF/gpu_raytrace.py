"""Unified GPU/CPU ray-tracing + LOS model sampling.

Public APIs:
- trace_ray(...): integrate ray trajectories (CPU or CUDA)
- sample_model_with_rays(...): sample model fields along traced rays (CPU or CUDA)
"""

from __future__ import annotations

from typing import Dict, Any

import numpy as np

from .build_rays import C_R, ray_trace as trace_ray_cpu_impl


def _as_float32_c(a: np.ndarray) -> np.ndarray:
    return np.ascontiguousarray(a, dtype=np.float32)


def _cupy_as_numpy(arr) -> np.ndarray:
    """Explicit CuPy→NumPy transfer (CuPy disallows implicit conversion in np.asarray)."""
    if hasattr(arr, "get"):
        return arr.get()
    return np.asarray(arr)


def _check_uniform_grid(grid: np.ndarray, name: str) -> tuple[float, float]:
    g = np.asarray(grid, dtype=np.float64)
    if g.ndim != 1 or g.size < 2:
        raise ValueError(f"{name} must be 1D with at least 2 points")
    d = np.diff(g)
    step = float(np.mean(d))
    if not np.isfinite(step) or step <= 0.0:
        raise ValueError(f"{name} has invalid spacing")
    max_dev = float(np.max(np.abs(d - step)))
    tol = max(1e-6 * abs(step), 1e-7 * max(abs(g[0]), abs(g[-1]), 1.0))
    if max_dev > tol:
        raise ValueError(f"{name} must be uniformly spaced")
    return float(g[0]), step


def _get_cupy():
    try:
        import cupy as cp
    except Exception as exc:  # pragma: no cover
        raise RuntimeError(
            "CuPy is not available. Install a matching CUDA wheel, e.g. cupy-cuda12x."
        ) from exc
    return cp


def _trilinear_uniform_cp(cp, field, points, x0, y0, z0, inv_dx, inv_dy, inv_dz, fill_value=np.inf):
    x = points[:, 0]
    y = points[:, 1]
    z = points[:, 2]

    fx = (x - x0) * inv_dx
    fy = (y - y0) * inv_dy
    fz = (z - z0) * inv_dz

    nx, ny, nz = field.shape
    i0 = cp.floor(fx).astype(cp.int32)
    j0 = cp.floor(fy).astype(cp.int32)
    k0 = cp.floor(fz).astype(cp.int32)
    i0 = cp.clip(i0, 0, nx - 2)
    j0 = cp.clip(j0, 0, ny - 2)
    k0 = cp.clip(k0, 0, nz - 2)
    inb = (
        (fx >= 0.0) & (fy >= 0.0) & (fz >= 0.0) &
        (fx <= nx - 1) & (fy <= ny - 1) & (fz <= nz - 1)
    )

    out = cp.full((points.shape[0],), fill_value, dtype=cp.float32)
    if not bool(cp.any(inb)):
        return out

    ii = i0[inb]
    jj = j0[inb]
    kk = k0[inb]

    tx = cp.clip(fx[inb] - ii, 0.0, 1.0).astype(cp.float32)
    ty = cp.clip(fy[inb] - jj, 0.0, 1.0).astype(cp.float32)
    tz = cp.clip(fz[inb] - kk, 0.0, 1.0).astype(cp.float32)

    c000 = field[ii, jj, kk]
    c100 = field[ii + 1, jj, kk]
    c010 = field[ii, jj + 1, kk]
    c110 = field[ii + 1, jj + 1, kk]
    c001 = field[ii, jj, kk + 1]
    c101 = field[ii + 1, jj, kk + 1]
    c011 = field[ii, jj + 1, kk + 1]
    c111 = field[ii + 1, jj + 1, kk + 1]

    c00 = c000 * (1.0 - tx) + c100 * tx
    c10 = c010 * (1.0 - tx) + c110 * tx
    c01 = c001 * (1.0 - tx) + c101 * tx
    c11 = c011 * (1.0 - tx) + c111 * tx
    c0 = c00 * (1.0 - ty) + c10 * ty
    c1 = c01 * (1.0 - ty) + c11 * ty
    v = c0 * (1.0 - tz) + c1 * tz
    out[inb] = cp.where(cp.isfinite(v), v, fill_value)
    return out


_CUDA_RAYTRACE_SRC = r'''
extern "C" {

__device__ inline float trilinear_sample(
    const float* field,
    int nx, int ny, int nz,
    float x, float y, float z,
    float x0, float y0, float z0,
    float inv_dx, float inv_dy, float inv_dz
) {
    float fx = (x - x0) * inv_dx;
    float fy = (y - y0) * inv_dy;
    float fz = (z - z0) * inv_dz;

    if (fx < 0.0f || fy < 0.0f || fz < 0.0f || fx > (float)(nx - 1) || fy > (float)(ny - 1) || fz > (float)(nz - 1)) {
        return nanf("");
    }
    int i0 = (int)floorf(fx);
    int j0 = (int)floorf(fy);
    int k0 = (int)floorf(fz);
    if (i0 > nx - 2) i0 = nx - 2;
    if (j0 > ny - 2) j0 = ny - 2;
    if (k0 > nz - 2) k0 = nz - 2;

    float tx = fx - (float)i0;
    float ty = fy - (float)j0;
    float tz = fz - (float)k0;
    tx = fminf(fmaxf(tx, 0.0f), 1.0f);
    ty = fminf(fmaxf(ty, 0.0f), 1.0f);
    tz = fminf(fmaxf(tz, 0.0f), 1.0f);

    int sy = nz;
    int sx = ny * nz;

    int off000 = i0 * sx + j0 * sy + k0;
    int off100 = (i0 + 1) * sx + j0 * sy + k0;
    int off010 = i0 * sx + (j0 + 1) * sy + k0;
    int off110 = (i0 + 1) * sx + (j0 + 1) * sy + k0;
    int off001 = i0 * sx + j0 * sy + (k0 + 1);
    int off101 = (i0 + 1) * sx + j0 * sy + (k0 + 1);
    int off011 = i0 * sx + (j0 + 1) * sy + (k0 + 1);
    int off111 = (i0 + 1) * sx + (j0 + 1) * sy + (k0 + 1);

    float c000 = field[off000];
    float c100 = field[off100];
    float c010 = field[off010];
    float c110 = field[off110];
    float c001 = field[off001];
    float c101 = field[off101];
    float c011 = field[off011];
    float c111 = field[off111];

    float c00 = c000 * (1.0f - tx) + c100 * tx;
    float c10 = c010 * (1.0f - tx) + c110 * tx;
    float c01 = c001 * (1.0f - tx) + c101 * tx;
    float c11 = c011 * (1.0f - tx) + c111 * tx;
    float c0 = c00 * (1.0f - ty) + c10 * ty;
    float c1 = c01 * (1.0f - ty) + c11 * ty;
    return c0 * (1.0f - tz) + c1 * tz;
}

__device__ inline void rhs_eval(
    float rx, float ry, float rz,
    float kx, float ky, float kz,
    const float* omega,
    const float* gx,
    const float* gy,
    const float* gz,
    int nx, int ny, int nz,
    float x0, float y0, float z0,
    float inv_dx, float inv_dy, float inv_dz,
    float c_r,
    float* drx, float* dry, float* drz,
    float* dkx, float* dky, float* dkz
) {
    float wpe = trilinear_sample(omega, nx, ny, nz, rx, ry, rz, x0, y0, z0, inv_dx, inv_dy, inv_dz);
    float om = sqrtf(fmaxf(wpe * wpe + kx * kx + ky * ky + kz * kz, 0.0f));
    bool valid = isfinite(wpe) && isfinite(om) && (om > 0.0f);
    if (!valid) {
        *drx = 0.0f; *dry = 0.0f; *drz = 0.0f;
        *dkx = 0.0f; *dky = 0.0f; *dkz = 0.0f;
        return;
    }
    float dwdx = trilinear_sample(gx, nx, ny, nz, rx, ry, rz, x0, y0, z0, inv_dx, inv_dy, inv_dz);
    float dwdy = trilinear_sample(gy, nx, ny, nz, rx, ry, rz, x0, y0, z0, inv_dx, inv_dy, inv_dz);
    float dwdz = trilinear_sample(gz, nx, ny, nz, rx, ry, rz, x0, y0, z0, inv_dx, inv_dy, inv_dz);
    if (!(isfinite(dwdx) && isfinite(dwdy) && isfinite(dwdz))) {
        *drx = 0.0f; *dry = 0.0f; *drz = 0.0f;
        *dkx = 0.0f; *dky = 0.0f; *dkz = 0.0f;
        return;
    }
    float inv_om = 1.0f / om;
    *drx = c_r * inv_om * kx;
    *dry = c_r * inv_om * ky;
    *drz = c_r * inv_om * kz;
    float a = -wpe * inv_om * c_r;
    *dkx = a * dwdx;
    *dky = a * dwdy;
    *dkz = a * dwdz;
}

__device__ inline void rk4_step(
    float* rx, float* ry, float* rz,
    float* kx, float* ky, float* kz,
    const float* omega, const float* gx, const float* gy, const float* gz,
    int nx, int ny, int nz,
    float x0, float y0, float z0,
    float inv_dx, float inv_dy, float inv_dz,
    float c_r, float dt
) {
    float k1rx, k1ry, k1rz, k1kx, k1ky, k1kz;
    rhs_eval(*rx, *ry, *rz, *kx, *ky, *kz, omega, gx, gy, gz, nx, ny, nz, x0, y0, z0, inv_dx, inv_dy, inv_dz, c_r, &k1rx, &k1ry, &k1rz, &k1kx, &k1ky, &k1kz);

    float r2x = *rx + 0.5f * dt * k1rx, r2y = *ry + 0.5f * dt * k1ry, r2z = *rz + 0.5f * dt * k1rz;
    float k2x = *kx + 0.5f * dt * k1kx, k2y = *ky + 0.5f * dt * k1ky, k2z = *kz + 0.5f * dt * k1kz;
    float k2rx, k2ry, k2rz, k2kx, k2ky, k2kz;
    rhs_eval(r2x, r2y, r2z, k2x, k2y, k2z, omega, gx, gy, gz, nx, ny, nz, x0, y0, z0, inv_dx, inv_dy, inv_dz, c_r, &k2rx, &k2ry, &k2rz, &k2kx, &k2ky, &k2kz);

    float r3x = *rx + 0.5f * dt * k2rx, r3y = *ry + 0.5f * dt * k2ry, r3z = *rz + 0.5f * dt * k2rz;
    float k3x = *kx + 0.5f * dt * k2kx, k3y = *ky + 0.5f * dt * k2ky, k3z = *kz + 0.5f * dt * k2kz;
    float k3rx, k3ry, k3rz, k3kx, k3ky, k3kz;
    rhs_eval(r3x, r3y, r3z, k3x, k3y, k3z, omega, gx, gy, gz, nx, ny, nz, x0, y0, z0, inv_dx, inv_dy, inv_dz, c_r, &k3rx, &k3ry, &k3rz, &k3kx, &k3ky, &k3kz);

    float r4x = *rx + dt * k3rx, r4y = *ry + dt * k3ry, r4z = *rz + dt * k3rz;
    float k4x = *kx + dt * k3kx, k4y = *ky + dt * k3ky, k4z = *kz + dt * k3kz;
    float k4rx, k4ry, k4rz, k4kx, k4ky, k4kz;
    rhs_eval(r4x, r4y, r4z, k4x, k4y, k4z, omega, gx, gy, gz, nx, ny, nz, x0, y0, z0, inv_dx, inv_dy, inv_dz, c_r, &k4rx, &k4ry, &k4rz, &k4kx, &k4ky, &k4kz);

    float c = dt / 6.0f;
    *rx += c * (k1rx + 2.0f * k2rx + 2.0f * k3rx + k4rx);
    *ry += c * (k1ry + 2.0f * k2ry + 2.0f * k3ry + k4ry);
    *rz += c * (k1rz + 2.0f * k2rz + 2.0f * k3rz + k4rz);
    *kx += c * (k1kx + 2.0f * k2kx + 2.0f * k3kx + k4kx);
    *ky += c * (k1ky + 2.0f * k2ky + 2.0f * k3ky + k4ky);
    *kz += c * (k1kz + 2.0f * k2kz + 2.0f * k3kz + k4kz);
}

__global__ void trace_ray_step_kernel(
    const float* omega,
    const float* gx,
    const float* gy,
    const float* gz,
    int nx, int ny, int nz,
    float x0, float y0, float z0,
    float inv_dx, float inv_dy, float inv_dz,
    float c_r,
    float dt,
    int n_rays,
    int trace_crosssections,
    float perturb_ratio,
    float* state,
    float* s_ratio_out
) {
    int idx = blockDim.x * blockIdx.x + threadIdx.x;
    if (idx >= n_rays) return;

    int base = idx * 6;
    float rx = state[base + 0];
    float ry = state[base + 1];
    float rz = state[base + 2];
    float kx = state[base + 3];
    float ky = state[base + 4];
    float kz = state[base + 5];

    float r0x = rx, r0y = ry, r0z = rz;
    float k0x = kx, k0y = ky, k0z = kz;

    rk4_step(&rx, &ry, &rz, &kx, &ky, &kz, omega, gx, gy, gz, nx, ny, nz, x0, y0, z0, inv_dx, inv_dy, inv_dz, c_r, dt);

    state[base + 0] = rx;
    state[base + 1] = ry;
    state[base + 2] = rz;
    state[base + 3] = kx;
    state[base + 4] = ky;
    state[base + 5] = kz;

    if (!trace_crosssections) {
        s_ratio_out[idx] = 1.0f;
        return;
    }

    float drx = rx - r0x, dry = ry - r0y, drz = rz - r0z;
    float dnorm = sqrtf(drx * drx + dry * dry + drz * drz) + 1e-32f;
    float tx = drx / dnorm, ty = dry / dnorm, tz = drz / dnorm;

    float ax = 0.0f, ay = (fabsf(tz) < 0.9f) ? 0.0f : 1.0f, az = (fabsf(tz) < 0.9f) ? 1.0f : 0.0f;
    float e1x = ay * tz - az * ty;
    float e1y = az * tx - ax * tz;
    float e1z = ax * ty - ay * tx;
    float e1n = sqrtf(e1x * e1x + e1y * e1y + e1z * e1z) + 1e-30f;
    e1x /= e1n; e1y /= e1n; e1z /= e1n;

    float e2x = ty * e1z - tz * e1y;
    float e2y = tz * e1x - tx * e1z;
    float e2z = tx * e1y - ty * e1x;
    float e2n = sqrtf(e2x * e2x + e2y * e2y + e2z * e2z) + 1e-30f;
    e2x /= e2n; e2y /= e2n; e2z /= e2n;

    float eps = perturb_ratio * dnorm;

    float r1x = r0x + eps * e1x, r1y = r0y + eps * e1y, r1z = r0z + eps * e1z;
    float r2x = r0x + eps * e2x, r2y = r0y + eps * e2y, r2z = r0z + eps * e2z;
    float k1x = k0x, k1y = k0y, k1z = k0z;
    float k2x = k0x, k2y = k0y, k2z = k0z;

    rk4_step(&r1x, &r1y, &r1z, &k1x, &k1y, &k1z, omega, gx, gy, gz, nx, ny, nz, x0, y0, z0, inv_dx, inv_dy, inv_dz, c_r, dt);
    rk4_step(&r2x, &r2y, &r2z, &k2x, &k2y, &k2z, omega, gx, gy, gz, nx, ny, nz, x0, y0, z0, inv_dx, inv_dy, inv_dz, c_r, dt);

    float d1x = r1x - rx, d1y = r1y - ry, d1z = r1z - rz;
    float d2x = r2x - rx, d2y = r2y - ry, d2z = r2z - rz;

    float cx = d1y * d2z - d1z * d2y;
    float cy = d1z * d2x - d1x * d2z;
    float cz = d1x * d2y - d1y * d2x;
    float num = fabsf(cx * tx + cy * ty + cz * tz);
    s_ratio_out[idx] = num / (eps * eps + 1e-30f);
}

}
'''


def _get_trace_step_kernel(cp):
    if not hasattr(_get_trace_step_kernel, "_kernel"):
        module = cp.RawModule(code=_CUDA_RAYTRACE_SRC, options=("-std=c++11",))
        _get_trace_step_kernel._kernel = module.get_function("trace_ray_step_kernel")
    return _get_trace_step_kernel._kernel


def _trace_ray_gpu(
    omega_pe_3d,
    x_grid,
    y_grid,
    z_grid,
    freq_hz,
    x_start,
    y_start,
    z_start,
    kvec_in_norm,
    dt,
    n_steps,
    record_stride=10,
    trace_crosssections=False,
    perturb_ratio=2,
):
    cp = _get_cupy()

    x0, dx = _check_uniform_grid(x_grid, "x_grid")
    y0, dy = _check_uniform_grid(y_grid, "y_grid")
    z0, dz = _check_uniform_grid(z_grid, "z_grid")

    inv_dx = np.float32(1.0 / dx)
    inv_dy = np.float32(1.0 / dy)
    inv_dz = np.float32(1.0 / dz)

    omega_pe = cp.asarray(np.ascontiguousarray(omega_pe_3d, dtype=np.float32))
    domega_dx = cp.gradient(omega_pe, np.float32(dx), axis=0)
    domega_dy = cp.gradient(omega_pe, np.float32(dy), axis=1)
    domega_dz = cp.gradient(omega_pe, np.float32(dz), axis=2)

    start = cp.asarray(np.column_stack([x_start, y_start, z_start]).astype(np.float32))
    kdir = cp.asarray(np.asarray(kvec_in_norm, dtype=np.float32))

    omega0 = np.float32(2.0 * np.pi * freq_hz)
    omega_pe_start = _trilinear_uniform_cp(
        cp, omega_pe, start, x0, y0, z0, inv_dx, inv_dy, inv_dz
    )
    # Avoid NaN kc0 when ray start is in bad region (e.g. R<1) so ray doesn't get stuck
    omega_pe_start = cp.nan_to_num(omega_pe_start, nan=0.0, posinf=0.0, neginf=0.0)
    kc0 = cp.sqrt(cp.maximum(omega0 * omega0 - omega_pe_start * omega_pe_start, 0.0))

    state = cp.ascontiguousarray(cp.concatenate([start, kdir * kc0[:, None]], axis=1).reshape(-1))

    r_record = []
    crosssection_record = []
    n_rays = int(start.shape[0])
    s_ratio = cp.ones((n_rays,), dtype=cp.float32)

    s_ratio_accumulated = 1.0

    kernel = _get_trace_step_kernel(cp)
    block = 64
    grid = ((n_rays + block - 1) // block,)

    for i in range(int(n_steps)):
        kernel(
            grid,
            (block,),
            (
                omega_pe, domega_dx, domega_dy, domega_dz,
                np.int32(omega_pe.shape[0]), np.int32(omega_pe.shape[1]), np.int32(omega_pe.shape[2]),
                np.float32(x0), np.float32(y0), np.float32(z0),
                np.float32(inv_dx), np.float32(inv_dy), np.float32(inv_dz),
                np.float32(C_R), np.float32(dt),
                np.int32(n_rays), np.int32(1 if trace_crosssections else 0), np.float32(perturb_ratio),
                state, s_ratio,
            ),
        )

        s_ratio_accumulated *= cp.asnumpy(s_ratio)
        if i % int(record_stride) == 0:
            st2 = state.reshape(n_rays, 6)
            r_record.append(cp.asnumpy(st2[:, 0:3]))
            if trace_crosssections:
                if len(crosssection_record) > 0:
                    previous_cs = crosssection_record[-1]
                else:
                    previous_cs = 1.0
                crosssection_record.append(previous_cs * s_ratio_accumulated)
            s_ratio_accumulated = 1.0


    return np.array(r_record), crosssection_record


def trace_ray(
    device: str,
    omega_pe_3d,
    x_grid,
    y_grid,
    z_grid,
    freq_hz,
    x_start,
    y_start,
    z_start,
    kvec_in_norm,
    dt,
    n_steps,
    record_stride=10,
    trace_crosssections=False,
    perturb_ratio=1.5,
):
    """Trace rays on CPU or CUDA.

    Returns (r_record, crosssection_record) as in build_rays.ray_trace.
    """
    dev = device.lower()
    if dev == "cpu":
        return trace_ray_cpu_impl(
            omega_pe_3d=omega_pe_3d,
            x_grid=x_grid,
            y_grid=y_grid,
            z_grid=z_grid,
            freq_hz=freq_hz,
            x_start=x_start,
            y_start=y_start,
            z_start=z_start,
            kvec_in_norm=kvec_in_norm,
            dt=dt,
            n_steps=n_steps,
            record_stride=record_stride,
            trace_crosssections=trace_crosssections,
            perturb_ratio=perturb_ratio,
        )
    if dev != "cuda":
        raise ValueError(f"Unsupported device '{device}'. Use 'cpu' or 'cuda'.")
    return _trace_ray_gpu(
        omega_pe_3d=omega_pe_3d,
        x_grid=x_grid,
        y_grid=y_grid,
        z_grid=z_grid,
        freq_hz=freq_hz,
        x_start=x_start,
        y_start=y_start,
        z_start=z_start,
        kvec_in_norm=kvec_in_norm,
        dt=dt,
        n_steps=n_steps,
        record_stride=record_stride,
        trace_crosssections=trace_crosssections,
        perturb_ratio=perturb_ratio,
    )


def _compute_ds_from_valid(positions: np.ndarray, valid_mask: np.ndarray, ray_start: np.ndarray, r_sun_cm: float) -> np.ndarray:
    n_steps, n_rays, _ = positions.shape
    ds = np.zeros((n_steps, n_rays), dtype=np.float32)
    for r in range(n_rays):
        idx = np.flatnonzero(valid_mask[:, r])
        if idx.size == 0:
            continue
        p = positions[idx, r, :]
        d = np.empty(idx.size, dtype=np.float32)
        d[0] = np.float32(np.linalg.norm(p[0] - ray_start[r]) * r_sun_cm)
        if idx.size > 1:
            d[1:] = (np.linalg.norm(p[1:] - p[:-1], axis=1) * r_sun_cm).astype(np.float32)
        ds[idx, r] = d
    return ds


def _trilinear_numpy_uniform(positions: np.ndarray, field_xyz: np.ndarray, x0: float, y0: float, z0: float, inv_dx: float, inv_dy: float, inv_dz: float, fill_value: float) -> np.ndarray:
    px = positions[..., 0]
    py = positions[..., 1]
    pz = positions[..., 2]
    nx, ny, nz = field_xyz.shape

    fx = (px - x0) * inv_dx
    fy = (py - y0) * inv_dy
    fz = (pz - z0) * inv_dz

    i0 = np.floor(fx).astype(np.int32)
    j0 = np.floor(fy).astype(np.int32)
    k0 = np.floor(fz).astype(np.int32)
    i0 = np.clip(i0, 0, nx - 2)
    j0 = np.clip(j0, 0, ny - 2)
    k0 = np.clip(k0, 0, nz - 2)

    inb = (fx >= 0.0) & (fy >= 0.0) & (fz >= 0.0) & (fx <= nx - 1) & (fy <= ny - 1) & (fz <= nz - 1)
    out = np.full(px.shape, np.float32(fill_value), dtype=np.float32)
    if not np.any(inb):
        return out

    ii = i0[inb]
    jj = j0[inb]
    kk = k0[inb]

    tx = np.clip(fx[inb] - ii, 0.0, 1.0).astype(np.float32)
    ty = np.clip(fy[inb] - jj, 0.0, 1.0).astype(np.float32)
    tz = np.clip(fz[inb] - kk, 0.0, 1.0).astype(np.float32)

    c000 = field_xyz[ii, jj, kk]
    c100 = field_xyz[ii + 1, jj, kk]
    c010 = field_xyz[ii, jj + 1, kk]
    c110 = field_xyz[ii + 1, jj + 1, kk]
    c001 = field_xyz[ii, jj, kk + 1]
    c101 = field_xyz[ii + 1, jj, kk + 1]
    c011 = field_xyz[ii, jj + 1, kk + 1]
    c111 = field_xyz[ii + 1, jj + 1, kk + 1]

    c00 = c000 * (1.0 - tx) + c100 * tx
    c10 = c010 * (1.0 - tx) + c110 * tx
    c01 = c001 * (1.0 - tx) + c101 * tx
    c11 = c011 * (1.0 - tx) + c111 * tx
    c0 = c00 * (1.0 - ty) + c10 * ty
    c1 = c01 * (1.0 - ty) + c11 * ty
    out[inb] = (c0 * (1.0 - tz) + c1 * tz).astype(np.float32)
    return out


_CUDA_SAMPLE_KERNEL = r'''
extern "C" __global__
void trilinear_sample_uniform(
    const float* pos,
    const float* field,
    const float* s,
    int n_steps,
    int n_rays,
    int n_x,
    int n_y,
    int n_z,
    float x0,
    float y0,
    float z0,
    float inv_dx,
    float inv_dy,
    float inv_dz,
    float fill_value,
    float* out,
    unsigned char* valid_mask
) {
    int idx = blockDim.x * blockIdx.x + threadIdx.x;
    int n_total = n_steps * n_rays;
    if (idx >= n_total) return;

    int base = idx * 3;
    float x = pos[base + 0];
    float y = pos[base + 1];
    float z = pos[base + 2];
    float sv = s[idx];

    bool finite = isfinite(x) && isfinite(y) && isfinite(z) && isfinite(sv);
    bool valid = finite && (sv > 0.0f);
    valid_mask[idx] = valid ? 1 : 0;
    if (!finite) {
        out[idx] = fill_value;
        return;
    }

    float fx = (x - x0) * inv_dx;
    float fy = (y - y0) * inv_dy;
    float fz = (z - z0) * inv_dz;

    if (fx < 0.0f || fy < 0.0f || fz < 0.0f || fx > (float)(n_x - 1) || fy > (float)(n_y - 1) || fz > (float)(n_z - 1)) {
        out[idx] = fill_value;
        return;
    }
    int i0 = (int)floorf(fx);
    int j0 = (int)floorf(fy);
    int k0 = (int)floorf(fz);
    if (i0 > n_x - 2) i0 = n_x - 2;
    if (j0 > n_y - 2) j0 = n_y - 2;
    if (k0 > n_z - 2) k0 = n_z - 2;

    float tx = fx - (float)i0;
    float ty = fy - (float)j0;
    float tz = fz - (float)k0;
    tx = fminf(fmaxf(tx, 0.0f), 1.0f);
    ty = fminf(fmaxf(ty, 0.0f), 1.0f);
    tz = fminf(fmaxf(tz, 0.0f), 1.0f);

    int stride_y = n_z;
    int stride_x = n_y * n_z;

    int off000 = i0 * stride_x + j0 * stride_y + k0;
    int off100 = (i0 + 1) * stride_x + j0 * stride_y + k0;
    int off010 = i0 * stride_x + (j0 + 1) * stride_y + k0;
    int off110 = (i0 + 1) * stride_x + (j0 + 1) * stride_y + k0;
    int off001 = i0 * stride_x + j0 * stride_y + (k0 + 1);
    int off101 = (i0 + 1) * stride_x + j0 * stride_y + (k0 + 1);
    int off011 = i0 * stride_x + (j0 + 1) * stride_y + (k0 + 1);
    int off111 = (i0 + 1) * stride_x + (j0 + 1) * stride_y + (k0 + 1);

    float c000 = field[off000];
    float c100 = field[off100];
    float c010 = field[off010];
    float c110 = field[off110];
    float c001 = field[off001];
    float c101 = field[off101];
    float c011 = field[off011];
    float c111 = field[off111];

    float c00 = c000 * (1.0f - tx) + c100 * tx;
    float c10 = c010 * (1.0f - tx) + c110 * tx;
    float c01 = c001 * (1.0f - tx) + c101 * tx;
    float c11 = c011 * (1.0f - tx) + c111 * tx;
    float c0 = c00 * (1.0f - ty) + c10 * ty;
    float c1 = c01 * (1.0f - ty) + c11 * ty;
    float v = c0 * (1.0f - tz) + c1 * tz;
    out[idx] = isfinite(v) ? v : fill_value;
}
'''


def _sample_model_with_rays_cpu(
    x_grid, y_grid, z_grid,
    ne_xyz, te_xyz, b_xyz,
    r_record, s_arr, ray_start, r_sun_cm,
    fill_ne=0.0, fill_te=1e4, fill_b=0.0,
    br_xyz=None, bt_xyz=None, bp_xyz=None,
):
    x0, dx = _check_uniform_grid(np.asarray(x_grid), "x_grid")
    y0, dy = _check_uniform_grid(np.asarray(y_grid), "y_grid")
    z0, dz = _check_uniform_grid(np.asarray(z_grid), "z_grid")

    pos = _as_float32_c(np.asarray(r_record))
    s = _as_float32_c(np.asarray(s_arr))
    valid = np.isfinite(pos).all(axis=2) & np.isfinite(s) & (s > 0.0)

    inv_dx, inv_dy, inv_dz = 1.0 / dx, 1.0 / dy, 1.0 / dz
    ne = _trilinear_numpy_uniform(pos, _as_float32_c(ne_xyz), x0, y0, z0, inv_dx, inv_dy, inv_dz, float(fill_ne))
    te = _trilinear_numpy_uniform(pos, _as_float32_c(te_xyz), x0, y0, z0, inv_dx, inv_dy, inv_dz, float(fill_te))
    b = _trilinear_numpy_uniform(pos, _as_float32_c(b_xyz), x0, y0, z0, inv_dx, inv_dy, inv_dz, float(fill_b))
    ds = _compute_ds_from_valid(pos, valid, _as_float32_c(ray_start), float(r_sun_cm))
    out = {"ne": ne, "te": te, "b": b, "ds": ds, "valid_mask": valid, "s": s}
    if br_xyz is not None and bt_xyz is not None and bp_xyz is not None:
        from raytracingGRFF.grff_parms import mas_spherical_b_to_cartesian

        br = _trilinear_numpy_uniform(pos, _as_float32_c(br_xyz), x0, y0, z0, inv_dx, inv_dy, inv_dz, 0.0)
        bt = _trilinear_numpy_uniform(pos, _as_float32_c(bt_xyz), x0, y0, z0, inv_dx, inv_dy, inv_dz, 0.0)
        bp = _trilinear_numpy_uniform(pos, _as_float32_c(bp_xyz), x0, y0, z0, inv_dx, inv_dy, inv_dz, 0.0)
        bx, by, bz = mas_spherical_b_to_cartesian(pos[..., 0], pos[..., 1], pos[..., 2], br, bt, bp)
        out["bx"] = bx.astype(np.float64)
        out["by"] = by.astype(np.float64)
        out["bz"] = bz.astype(np.float64)
        out["b"] = np.sqrt(out["bx"] ** 2 + out["by"] ** 2 + out["bz"] ** 2).astype(np.float32)
    return out


def _sample_model_with_rays_cuda(
    x_grid, y_grid, z_grid,
    ne_xyz, te_xyz, b_xyz,
    r_record, s_arr, ray_start, r_sun_cm,
    fill_ne=0.0, fill_te=1e4, fill_b=0.0,
    br_xyz=None, bt_xyz=None, bp_xyz=None,
):
    cp = _get_cupy()

    x0, dx = _check_uniform_grid(np.asarray(x_grid), "x_grid")
    y0, dy = _check_uniform_grid(np.asarray(y_grid), "y_grid")
    z0, dz = _check_uniform_grid(np.asarray(z_grid), "z_grid")

    pos_np = _as_float32_c(np.asarray(r_record))
    s_np = _as_float32_c(np.asarray(s_arr))
    ray_start_np = _as_float32_c(np.asarray(ray_start))

    n_steps, n_rays, _ = pos_np.shape
    n_total = n_steps * n_rays

    pos = cp.asarray(pos_np.reshape(n_total, 3))
    s = cp.asarray(s_np.reshape(n_total))

    kernel = cp.RawKernel(_CUDA_SAMPLE_KERNEL, "trilinear_sample_uniform")
    block = 256
    grid = ((n_total + block - 1) // block,)

    def _sample(field_np: np.ndarray, fill: float):
        field = cp.asarray(_as_float32_c(field_np))
        out = cp.full((n_total,), float(fill), dtype=cp.float32)
        valid = cp.zeros((n_total,), dtype=cp.uint8)
        kernel(
            grid,
            (block,),
            (
                pos, field, s,
                np.int32(n_steps), np.int32(n_rays),
                np.int32(field.shape[0]), np.int32(field.shape[1]), np.int32(field.shape[2]),
                np.float32(x0), np.float32(y0), np.float32(z0),
                np.float32(1.0 / dx), np.float32(1.0 / dy), np.float32(1.0 / dz),
                np.float32(fill), out, valid,
            ),
        )
        return out, valid

    ne_out, valid_u8 = _sample(ne_xyz, float(fill_ne))
    te_out, _ = _sample(te_xyz, float(fill_te))
    b_out, _ = _sample(b_xyz, float(fill_b))

    cp.cuda.runtime.deviceSynchronize()

    ne = _cupy_as_numpy(ne_out).reshape(n_steps, n_rays)
    te = _cupy_as_numpy(te_out).reshape(n_steps, n_rays)
    b = _cupy_as_numpy(b_out).reshape(n_steps, n_rays)
    valid = _cupy_as_numpy(valid_u8).reshape(n_steps, n_rays).astype(bool)
    ds = _compute_ds_from_valid(pos_np, valid, ray_start_np, float(r_sun_cm))
    out = {"ne": ne, "te": te, "b": b, "ds": ds, "valid_mask": valid, "s": s_np.reshape(n_steps, n_rays)}
    if br_xyz is not None and bt_xyz is not None and bp_xyz is not None:
        from raytracingGRFF.grff_parms import mas_spherical_b_to_cartesian

        br, _ = _sample(br_xyz, 0.0)
        bt, _ = _sample(bt_xyz, 0.0)
        bp, _ = _sample(bp_xyz, 0.0)
        br_np = _cupy_as_numpy(br).reshape(n_steps, n_rays)
        bt_np = _cupy_as_numpy(bt).reshape(n_steps, n_rays)
        bp_np = _cupy_as_numpy(bp).reshape(n_steps, n_rays)
        bx, by, bz = mas_spherical_b_to_cartesian(
            pos_np[..., 0], pos_np[..., 1], pos_np[..., 2], br_np, bt_np, bp_np
        )
        out["bx"] = bx.astype(np.float64)
        out["by"] = by.astype(np.float64)
        out["bz"] = bz.astype(np.float64)
        out["b"] = np.sqrt(out["bx"] ** 2 + out["by"] ** 2 + out["bz"] ** 2).astype(np.float32)
    return out


def sample_model_with_rays(
    device: str,
    x_grid: np.ndarray,
    y_grid: np.ndarray,
    z_grid: np.ndarray,
    ne_xyz: np.ndarray,
    te_xyz: np.ndarray,
    b_xyz: np.ndarray,
    r_record: np.ndarray,
    s_arr: np.ndarray,
    ray_start: np.ndarray,
    r_sun_cm: float,
    fill_ne: float = 0.0,
    fill_te: float = 1e4,
    fill_b: float = 0.0,
    fallback_to_cpu: bool = True,
    verbose: bool = True,
    br_xyz: np.ndarray | None = None,
    bt_xyz: np.ndarray | None = None,
    bp_xyz: np.ndarray | None = None,
) -> Dict[str, np.ndarray]:
    """Sample model fields along rays on CPU/CUDA."""
    dev = device.lower()
    if dev == "cpu":
        return _sample_model_with_rays_cpu(
            x_grid, y_grid, z_grid,
            ne_xyz, te_xyz, b_xyz,
            r_record, s_arr, ray_start, r_sun_cm,
            fill_ne=fill_ne, fill_te=fill_te, fill_b=fill_b,
            br_xyz=br_xyz, bt_xyz=bt_xyz, bp_xyz=bp_xyz,
        )
    if dev != "cuda":
        raise ValueError(f"Unsupported device '{device}'. Use 'cpu' or 'cuda'.")

    try:
        return _sample_model_with_rays_cuda(
            x_grid, y_grid, z_grid,
            ne_xyz, te_xyz, b_xyz,
            r_record, s_arr, ray_start, r_sun_cm,
            fill_ne=fill_ne, fill_te=fill_te, fill_b=fill_b,
            br_xyz=br_xyz, bt_xyz=bt_xyz, bp_xyz=bp_xyz,
        )
    except Exception as exc:
        if not fallback_to_cpu:
            raise
        if verbose:
            print(f"[gpu_raytrace] CUDA sampling unavailable ({exc}); falling back to CPU.")
        return _sample_model_with_rays_cpu(
            x_grid, y_grid, z_grid,
            ne_xyz, te_xyz, b_xyz,
            r_record, s_arr, ray_start, r_sun_cm,
            fill_ne=fill_ne, fill_te=fill_te, fill_b=fill_b,
            br_xyz=br_xyz, bt_xyz=bt_xyz, bp_xyz=bp_xyz,
        )


# Backward-compatible aliases (can be removed later).
def trace_los_cpu(*args, **kwargs):
    return _sample_model_with_rays_cpu(*args, **kwargs)


def trace_los_gpu_cupy(*args, **kwargs):
    return _sample_model_with_rays_cuda(*args, **kwargs)


def trace_los_dispatch(*args, **kwargs):
    return sample_model_with_rays(*args, **kwargs)


def trace_los_gpu(*args, **kwargs):
    return sample_model_with_rays(*args, **kwargs)


def ray_trace_gpu(*args, **kwargs):
    return _trace_ray_gpu(*args, **kwargs)
