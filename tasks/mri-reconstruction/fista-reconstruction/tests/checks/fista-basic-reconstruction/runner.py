#!/usr/bin/env python3
"""Deterministic adapters from upstream FISTA tests to graded NumPy output."""
from __future__ import annotations

import json
import importlib.util
import os
import sys
from pathlib import Path

import numpy as np


def fft2c(image: np.ndarray) -> np.ndarray:
    return np.fft.fftshift(np.fft.fft2(np.fft.ifftshift(image)))


def ifft2c(kspace: np.ndarray) -> np.ndarray:
    return np.fft.fftshift(np.fft.ifft2(np.fft.ifftshift(kspace)))


def phantom(size: int, amplitude: float) -> np.ndarray:
    y, x = np.ogrid[:size, :size]
    cx = cy = size // 2
    image = np.zeros((size, size), dtype=np.float64)
    image[((x - cx) / (0.69 * size / 2)) ** 2 + ((y - cy) / (0.92 * size / 2)) ** 2 <= 1] = 1.0
    image[((x - cx) / (0.6624 * size / 2)) ** 2 + ((y - cy) / (0.874 * size / 2)) ** 2 <= 1] = -0.98
    image[(((x - cx + 0.22 * size / 2) / (0.11 * size / 2)) ** 2 + ((y - cy) / (0.31 * size / 2)) ** 2) <= 1] = -0.8
    image[(((x - cx - 0.22 * size / 2) / (0.16 * size / 2)) ** 2 + ((y - cy) / (0.41 * size / 2)) ** 2) <= 1] = -0.8
    image = (image - image.min()) / (image.max() - image.min())
    return amplitude * image


def fixture(size: int, amplitude: float):
    image = phantom(size, amplitude)
    kspace = fft2c(image)
    mask = np.zeros((size, size), dtype=bool)
    mask[::2, :] = True
    mask[size // 2 - 1 : size // 2 + 1, :] = True
    measured = kspace * mask
    forward = fft2c
    adjoint = lambda values: np.real(ifft2c(values))
    return image, kspace, mask, measured, forward, adjoint


def flatten(*arrays) -> np.ndarray:
    return np.concatenate([np.asarray(value, dtype=np.float64).ravel() for value in arrays])


def reconstruct(FISTA, measured, mask, forward, adjoint, *, iterations, tolerance, regularization, line_search, initial=None):
    solver = FISTA(max_iterations=iterations, tolerance=tolerance, lambda_reg=regularization, line_search=line_search)
    identity = lambda values: values
    image, info = solver.reconstruct(measured, mask, forward, adjoint, initial_estimate=initial,
                                     sparsity_transform=identity, sparsity_adjoint=identity)
    history = info["history"]
    values = flatten(image, history["cost"], history["data_fidelity"], history["regularization"], history["step_size"])
    return solver, image, info, values


def main() -> None:
    input_path, output_path, source_dir = sys.argv[1:]
    config = json.loads(Path(input_path).read_text(encoding="utf-8"))
    source_file = Path(source_dir) / "algorithms" / "classical" / "fista.py"
    spec = importlib.util.spec_from_file_location("sciaccel_pinned_fista", source_file)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load pinned FISTA source: {source_file}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    FISTA = module.FISTAReconstructor

    operation = config["operation"]
    amplitude = float(config["amplitude"])
    size = int(config.get("size", 32))
    iterations = int(os.environ.get("SAB_ITERATIONS", config.get("iterations", 10)))
    repeats = int(os.environ.get("SAB_REPEATS", "1"))
    result = None

    for _ in range(repeats):
        image, full, mask, measured, forward, adjoint = fixture(size, amplitude)
        solver = FISTA(max_iterations=iterations, tolerance=0.0, lambda_reg=0.01, line_search=True)

        if operation == "initialization":
            custom = FISTA(max_iterations=50, tolerance=1e-4, lambda_reg=0.05 * amplitude, line_search=False, verbose=True)
            result = np.array([solver.max_iterations, solver.tolerance, solver.lambda_reg,
                               float(solver.line_search), float(solver.verbose), custom.max_iterations,
                               custom.tolerance, custom.lambda_reg, float(custom.line_search),
                               float(custom.verbose)], dtype=np.float64)
        elif operation == "soft-threshold":
            x = amplitude * np.array([-2.0, -0.5, 0.0, 0.5, 2.0])
            result = flatten(solver._soft_threshold(x, 1.0), solver._soft_threshold(x, 0.0),
                             solver._soft_threshold(x, 10.0))
        elif operation == "gradient-tv":
            gradient = solver._gradient_data_fidelity(image, measured, mask, forward, adjoint)
            tv = solver._total_variation_transform(image)
            adjoint_tv = solver._total_variation_adjoint(tv)
            result = flatten(gradient, tv, adjoint_tv)
        elif operation == "cost-computation":
            total, data, regularization = solver._compute_cost(image, measured, mask, forward)
            perfect = adjoint(full)
            ptotal, pdata, preg = solver._compute_cost(perfect, full, np.ones_like(mask), forward)
            result = np.array([total, data, regularization, ptotal, pdata, preg], dtype=np.float64)
        elif operation == "line-search":
            gradient = np.linspace(-0.25, 0.75, size * size, dtype=np.float64).reshape(size, size) * amplitude
            step = solver._line_search_step_size(image, gradient, measured, mask, forward)
            result = flatten(np.array([step]), image - step * gradient)
        elif operation == "basic-reconstruction":
            _, _, _, result = reconstruct(FISTA, measured, mask, forward, adjoint, iterations=iterations,
                                           tolerance=0.0, regularization=0.01, line_search=True)
        elif operation == "reconstruction-improvement":
            zero_filled = np.abs(ifft2c(measured))
            _, reconstructed, _, values = reconstruct(FISTA, measured, mask, forward, adjoint, iterations=iterations,
                                                       tolerance=0.0, regularization=0.0, line_search=False)
            result = flatten(zero_filled, reconstructed, np.array([np.mean((image - zero_filled) ** 2),
                                                                    np.mean((image - reconstructed) ** 2)]), values)
        elif operation == "convergence-behavior":
            _, _, _, result = reconstruct(FISTA, measured, mask, forward, adjoint, iterations=iterations,
                                           tolerance=0.0, regularization=0.0, line_search=False)
        elif operation == "regularization-effect":
            no_reg, image_no, _, values_no = reconstruct(FISTA, measured, mask, forward, adjoint, iterations=iterations,
                                                          tolerance=0.0, regularization=0.0, line_search=True)
            high_reg, image_high, _, values_high = reconstruct(FISTA, measured, mask, forward, adjoint, iterations=iterations,
                                                                tolerance=0.0, regularization=1.0, line_search=True)
            l1_no = np.sum(np.abs(image_no))
            l1_high = np.sum(np.abs(image_high))
            result = flatten(image_no, image_high, np.array([l1_no, l1_high]), values_no, values_high)
        elif operation == "edge-cases":
            zero_solver = FISTA(max_iterations=0)
            zero_image, _ = zero_solver.reconstruct(measured, mask, forward, adjoint)
            custom_initial = np.full_like(image, 0.125 * amplitude)
            _, custom_image, _, values = reconstruct(FISTA, measured, mask, forward, adjoint, iterations=iterations,
                                                      tolerance=0.0, regularization=0.01, line_search=True,
                                                      initial=custom_initial)
            result = flatten(zero_image, custom_image, values)
        else:
            raise ValueError(f"unknown operation: {operation}")

    assert result is not None and np.all(np.isfinite(result))
    result.astype("<f8").tofile(output_path)


if __name__ == "__main__":
    main()
