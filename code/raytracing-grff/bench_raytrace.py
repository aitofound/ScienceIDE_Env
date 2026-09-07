#!/usr/bin/env python
"""Benchmark CPU vs CUDA LOS sampling on a synthetic cube.

Usage:
  python bench_raytrace.py --n-pix 256 --n-steps 256 --repeat 3
"""

import argparse
import time

import numpy as np

from raytracingGRFF.gpu_raytrace import sample_model_with_rays


def make_case(n_pix: int, n_steps: int, grid_n: int, seed: int = 0):
    rng = np.random.default_rng(seed)

    xg = np.linspace(-2.0, 2.0, grid_n, dtype=np.float32)
    yg = np.linspace(-2.0, 2.0, grid_n, dtype=np.float32)
    zg = np.linspace(-2.0, 2.0, grid_n, dtype=np.float32)
    x, y, z = np.meshgrid(xg, yg, zg, indexing="ij")

    ne = (1.0e8 + 2.0e8 * np.exp(-(x * x + y * y + z * z))).astype(np.float32)
    te = (1.0e6 + 2.0e6 * (x + 2 * y - z)).astype(np.float32)
    b = (2.0 + x - y + 0.5 * z).astype(np.float32)

    n_rays = n_pix * n_pix
    origin_xy = rng.uniform(-1.2, 1.2, size=(n_rays, 2)).astype(np.float32)
    origin = np.column_stack([origin_xy, np.full(n_rays, 2.5, dtype=np.float32)])

    dirs = np.tile(np.array([[0.0, 0.0, -1.0]], dtype=np.float32), (n_rays, 1))
    # add small random angular jitter
    dirs[:, 0:2] += rng.normal(scale=0.02, size=(n_rays, 2)).astype(np.float32)
    dirs /= np.linalg.norm(dirs, axis=1, keepdims=True)

    s = (np.arange(n_steps, dtype=np.float32) * 0.02)[:, None]
    r_record = origin[None, :, :] + s[:, :, None] * dirs[None, :, :]

    s_arr = np.ones((n_steps, n_rays), dtype=np.float32)
    return xg, yg, zg, ne, te, b, r_record, s_arr, origin


def time_cpu(args, case):
    xg, yg, zg, ne, te, b, r_record, s_arr, ray_start = case
    best = None
    for _ in range(args.repeat):
        t0 = time.perf_counter()
        _ = sample_model_with_rays('cpu', xg, yg, zg, ne, te, b, r_record, s_arr, ray_start, r_sun_cm=6.957e10)
        dt = time.perf_counter() - t0
        best = dt if best is None else min(best, dt)
    return best


def time_gpu(args, case):
    xg, yg, zg, ne, te, b, r_record, s_arr, ray_start = case
    try:
        import cupy as cp
    except Exception:
        return None

    try:
        _ = cp.cuda.runtime.getDeviceCount()
    except Exception:
        return None

    # Warmup
    _ = sample_model_with_rays('cuda', xg, yg, zg, ne, te, b, r_record, s_arr, ray_start, r_sun_cm=6.957e10)
    cp.cuda.runtime.deviceSynchronize()

    best = None
    for _ in range(args.repeat):
        t0 = time.perf_counter()
        _ = sample_model_with_rays('cuda', xg, yg, zg, ne, te, b, r_record, s_arr, ray_start, r_sun_cm=6.957e10)
        cp.cuda.runtime.deviceSynchronize()
        dt = time.perf_counter() - t0
        best = dt if best is None else min(best, dt)
    return best


def main():
    p = argparse.ArgumentParser(description="Benchmark CPU vs CUDA LOS sampling")
    p.add_argument("--n-pix", type=int, default=256, help="Image size (n_pix x n_pix)")
    p.add_argument("--n-steps", type=int, default=256, help="LOS samples per ray")
    p.add_argument("--grid-n", type=int, default=128, help="Cube points per axis")
    p.add_argument("--repeat", type=int, default=3, help="Timing repeats")
    args = p.parse_args()

    case = make_case(args.n_pix, args.n_steps, args.grid_n)
    n_samples = args.n_pix * args.n_pix * args.n_steps

    cpu_t = time_cpu(args, case)
    print(f"CPU best: {cpu_t:.4f} s ({n_samples / cpu_t:,.0f} samples/s)")

    gpu_t = time_gpu(args, case)
    if gpu_t is None:
        print("CUDA benchmark skipped (CuPy/CUDA unavailable).")
        return

    print(f"GPU best: {gpu_t:.4f} s ({n_samples / gpu_t:,.0f} samples/s)")
    print(f"Speedup: {cpu_t / gpu_t:.2f}x")


if __name__ == "__main__":
    main()
