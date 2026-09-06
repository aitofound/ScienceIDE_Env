#!/usr/bin/env python3
"""Generic pyamg.krylov probe: run one or two solvers on one shipped problem for a
fixed iteration count (tol=0) and grade the solution and the residual history.

Reads ic/<nominal|variant>/input.json, whose only active value is rhs_scale
(a uniform scale factor on every entry of the right-hand side); seed is carried
for provenance only. Writes observable.npy: for each requested (method,
criteria) combination, in order, [real(solution), imag(solution) (zeros if
real), residual history padded with zeros to maxiter+2] concatenated.

The solver's second return value is deliberately NOT graded: with tol=0 pyamg's
halting status degenerates to the iteration count (pyamg/krylov/_cg.py:196
`return (x, it)`), and an iteration count is bookkeeping, not physics. A
candidate that stops early still fails on the zero-padded residual history and
on the solution itself.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import pyamg
from pyamg.krylov import (bicgstab, cg, cgne, cgnr, cr, fgmres, gmres,
                           minimal_residual, steepest_descent)
from pyamg.krylov._gmres_householder import gmres_householder
from pyamg.krylov._gmres_mgs import gmres_mgs

METHODS = {
    'bicgstab': bicgstab, 'cg': cg, 'cgne': cgne, 'cgnr': cgnr, 'cr': cr,
    'fgmres': fgmres, 'gmres': gmres, 'gmres_mgs': gmres_mgs,
    'gmres_householder': gmres_householder, 'minimal_residual': minimal_residual,
    'steepest_descent': steepest_descent,
}


def build_problem(name, size):
    if name == 'poisson2d':
        return pyamg.gallery.poisson((size, size), format='csr').astype(np.float64), None
    if name == 'poisson3d':
        return pyamg.gallery.poisson((size, size, size), format='csr').astype(np.float64), None
    if name == 'advection2d':
        A, b = pyamg.gallery.advection_2d((size, size))
        return A.tocsr().astype(np.float64), b.astype(np.float64)
    if name in ('unit_square', 'unit_cube', 'bar', 'recirc_flow', 'helmholtz_2D',
                'airfoil', 'knot', 'local_disc_galerkin_diffusion'):
        data = pyamg.gallery.load_example(name)
        A = data['A'].tocsr()
        return A.astype(np.complex128 if np.issubdtype(A.dtype, np.complexfloating) else np.float64), None
    raise ValueError(f'unknown problem {name!r}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--input', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--method', required=True, choices=sorted(METHODS))
    ap.add_argument('--method2', choices=sorted(METHODS), default=None,
                     help='optional second method graded on the same problem and right-hand side (agreement checks)')
    ap.add_argument('--problem', required=True,
                     choices=['poisson2d', 'poisson3d', 'unit_square', 'unit_cube', 'bar', 'recirc_flow',
                              'advection2d', 'helmholtz_2D', 'airfoil', 'knot',
                              'local_disc_galerkin_diffusion'])
    ap.add_argument('--size', type=int, default=0, help='grid dimension per axis for poisson2d/poisson3d/advection2d')
    ap.add_argument('--iterations', type=int, required=True)
    ap.add_argument('--precond', choices=['none', 'sa'], default='none')
    ap.add_argument('--criteria', default=None, help='comma-separated stopping criteria (cg/cr/cgne/cgnr/bicgstab/steepest_descent only)')
    a = ap.parse_args()

    ic = json.loads(Path(a.input).read_text(encoding='utf-8'))
    rhs_scale = float(ic['rhs_scale'])

    A, shipped_b = build_problem(a.problem, a.size)
    n = A.shape[0]
    is_complex = np.issubdtype(A.dtype, np.complexfloating)
    if shipped_b is not None:
        b = shipped_b.astype(A.dtype)
    else:
        b = np.linspace(0.5, 1.5, n).astype(A.dtype)
    # Scale the whole right-hand side rather than one entry: on some shipped
    # operators a single perturbed entry is dominated, in every subsequent dot
    # product and norm, by unperturbed entries of comparable or larger
    # magnitude and rounds back to the same float (verified empirically: a
    # single-entry ~1e-15 relative perturbation on pyamg/gallery/example_data/
    # bar.mat's and helmholtz_2D.mat's own right-hand side left every graded
    # value bit-identical between ic/nominal and ic/variant). Scaling every
    # entry by the same factor keeps the same two-ulp-per-entry sensitivity
    # test the skill recommends while remaining active regardless of which
    # entries dominate the shipped operator's own norms.
    b = (b * rhs_scale).astype(b.dtype)
    x0 = np.zeros(n, dtype=b.dtype)

    M = None
    if a.precond == 'sa':
        ml = pyamg.smoothed_aggregation_solver(A)
        M = ml.aspreconditioner()

    criteria_list = a.criteria.split(',') if a.criteria else [None]
    methods = [a.method] + ([a.method2] if a.method2 else [])

    values = []
    for method_name in methods:
        method = METHODS[method_name]
        for criteria in criteria_list:
            kwargs = dict(x0=x0, tol=0.0, maxiter=a.iterations)
            if M is not None:
                kwargs['M'] = M
            if criteria is not None:
                kwargs['criteria'] = criteria
            residuals = []
            kwargs['residuals'] = residuals
            solution, flag = method(A, b, **kwargs)
            solution = np.asarray(solution).ravel()
            padded = np.zeros(a.iterations + 2, dtype=np.float64)
            take = min(len(residuals), padded.size)
            padded[:take] = np.asarray(residuals[:take], dtype=np.float64)
            sol_r = np.real(solution).astype(np.float64)
            sol_i = np.imag(solution).astype(np.float64) if is_complex else np.zeros_like(sol_r)
            values.extend([sol_r, sol_i, padded])

    np.save(a.out, np.concatenate(values), allow_pickle=False)


if __name__ == '__main__':
    raise SystemExit(main())
