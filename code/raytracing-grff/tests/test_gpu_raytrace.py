import numpy as np
import pytest

from raytracingGRFF.gpu_raytrace import sample_model_with_rays

try:
    import cupy as _cp  # noqa: F401
    _HAS_CUPY = True
except Exception:
    _HAS_CUPY = False


def _make_synth_case(seed=0):
    rng = np.random.default_rng(seed)

    nx = ny = nz = 33
    xg = np.linspace(-1.0, 1.0, nx, dtype=np.float32)
    yg = np.linspace(-1.0, 1.0, ny, dtype=np.float32)
    zg = np.linspace(-1.0, 1.0, nz, dtype=np.float32)

    x, y, z = np.meshgrid(xg, yg, zg, indexing="ij")
    ne = (x + y + z).astype(np.float32)  # linear field
    te = (x * x + 2.0 * y + 3.0 * z).astype(np.float32)
    b = (2.0 * x - y + 0.5 * z).astype(np.float32)

    n_steps = 64
    n_rays = 128

    origin = rng.uniform(-0.8, 0.8, size=(n_rays, 3)).astype(np.float32)
    dirs = rng.normal(size=(n_rays, 3)).astype(np.float32)
    dirs /= np.linalg.norm(dirs, axis=1, keepdims=True)

    s = (np.arange(n_steps, dtype=np.float32) * 0.03)[:, None]
    r_record = origin[None, :, :] + s[:, :, None] * dirs[None, :, :]

    s_arr = np.ones((n_steps, n_rays), dtype=np.float32)
    s_arr[::9, ::7] = 0.0
    s_arr[::13, ::11] = np.nan

    # Force some out-of-bounds points to validate fill behavior.
    r_record[-5:, :8, 0] = 2.5

    ray_start = origin.copy()
    return xg, yg, zg, ne, te, b, r_record, s_arr, ray_start


def test_trace_los_cpu_linear_field_accuracy():
    xg, yg, zg, ne, te, b, r_record, s_arr, ray_start = _make_synth_case(seed=1)
    out = sample_model_with_rays('cpu', xg, yg, zg, ne, te, b, r_record, s_arr, ray_start, r_sun_cm=1.0)

    valid = out["valid_mask"]
    # Restrict to in-bounds valid samples for analytic linear check.
    inb = (
        (r_record[..., 0] >= xg[0])
        & (r_record[..., 0] <= xg[-1])
        & (r_record[..., 1] >= yg[0])
        & (r_record[..., 1] <= yg[-1])
        & (r_record[..., 2] >= zg[0])
        & (r_record[..., 2] <= zg[-1])
    )
    mask = valid & inb

    expected_ne = r_record[..., 0] + r_record[..., 1] + r_record[..., 2]
    np.testing.assert_allclose(out["ne"][mask], expected_ne[mask], rtol=2e-5, atol=2e-5)

    # Out-of-bounds points should use fill values.
    oob = valid & ~inb
    if np.any(oob):
        np.testing.assert_allclose(out["ne"][oob], 0.0)
        np.testing.assert_allclose(out["te"][oob], 1e4)
        np.testing.assert_allclose(out["b"][oob], 0.0)


def test_trace_los_cpu_valid_mask_and_ds_shape():
    xg, yg, zg, ne, te, b, r_record, s_arr, ray_start = _make_synth_case(seed=2)
    out = sample_model_with_rays('cpu', xg, yg, zg, ne, te, b, r_record, s_arr, ray_start, r_sun_cm=1.0)

    assert out["ne"].shape == s_arr.shape
    assert out["te"].shape == s_arr.shape
    assert out["b"].shape == s_arr.shape
    assert out["ds"].shape == s_arr.shape
    assert out["valid_mask"].shape == s_arr.shape

    # invalid S must be invalid in mask
    assert np.all(~out["valid_mask"][::9, ::7])

    # ds values are non-negative and only non-zero where valid can occur.
    assert np.all(out["ds"] >= 0.0)


@pytest.mark.skipif(not _HAS_CUPY, reason="cupy not installed")
def test_trace_los_gpu_matches_cpu():
    import cupy as cp

    # Skip if CUDA runtime is unavailable even if cupy imports.
    try:
        _ = cp.cuda.runtime.getDeviceCount()
    except Exception:
        pytest.skip("CUDA runtime unavailable")

    xg, yg, zg, ne, te, b, r_record, s_arr, ray_start = _make_synth_case(seed=3)

    cpu = sample_model_with_rays('cpu', xg, yg, zg, ne, te, b, r_record, s_arr, ray_start, r_sun_cm=1.0)
    gpu = sample_model_with_rays('cuda', xg, yg, zg, ne, te, b, r_record, s_arr, ray_start, r_sun_cm=1.0)

    assert np.array_equal(cpu["valid_mask"], gpu["valid_mask"])
    np.testing.assert_allclose(cpu["ne"], gpu["ne"], rtol=1e-5, atol=1e-5)
    np.testing.assert_allclose(cpu["te"], gpu["te"], rtol=1e-5, atol=1e-5)
    np.testing.assert_allclose(cpu["b"], gpu["b"], rtol=1e-5, atol=1e-5)
    np.testing.assert_allclose(cpu["ds"], gpu["ds"], rtol=1e-6, atol=1e-6)
