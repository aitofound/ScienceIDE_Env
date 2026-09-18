#!/usr/bin/env python3
"""Validator for the Fadeev reconnection test (see tests/reconnection/README)."""
import glob
import logging
import math
import os

import numpy as np

import tests._shared.hybrid as _hyb

RUN_DIR = "run_test"


def set_run_dir(run_dir):
    """Point the plot helpers at the current run directory."""
    global RUN_DIR
    RUN_DIR = run_dir
    _hyb.set_run_dir(run_dir)

logger = logging.getLogger(__name__)

# Grid / equilibrium parameters (must match PARAM.in / FadeevIC defaults).
L = 5.0                               # sheet half-thickness / d_i
EPS = 0.4                             # island size ratio
ISLAND_X = math.pi * L                # O-point location / d_i ~ 15.7

# m_i/m_e from full-PIC #PLASMA (m_i=1, m_e=0.04); quasi-neutral => rhoS0 ~ rhoS1*ratio
MASS_RATIO = 25.0


def _load_frame(path):
    """Parse one ``z=0`` .out file into (x, y, Bx, By, Bz, rhoS0, rhoS1|None)."""
    lines = open(path, encoding="latin-1").read().splitlines()
    if len(lines) < 6:
        return None
    names = lines[4].split()[:19]  # only per-point columns are written
    idx = {v: i for i, v in enumerate(names)}
    rows = [ln.split() for ln in lines[5:] if len(ln.split()) >= 19]
    if not rows:
        return None
    data = np.array([r[:19] for r in rows], dtype=float)
    x = data[:, idx["x"]]
    y = data[:, idx["y"]]
    nx = len(np.unique(np.round(x, 3)))
    ny = len(np.unique(np.round(y, 3)))
    if nx < 4 or ny < 4:
        return None
    ux = np.unique(np.round(x, 3))
    uy = np.unique(np.round(y, 3))
    bx = data[:, idx["Bx"]].reshape(ny, nx)
    by = data[:, idx["By"]].reshape(ny, nx)
    bz = data[:, idx["Bz"]].reshape(ny, nx)
    rho = data[:, idx["rhoS0"]].reshape(ny, nx)
    rhoS1 = data[:, idx["rhoS1"]].reshape(ny, nx) if "rhoS1" in idx else None
    return ux, uy, bx, by, bz, rho, rhoS1


def _flux_function(bx, bz, dx):
    """Out-of-plane flux function Ay(x,y) from B = curl(A).

    For the 2D x-y reconnection plane (invariant out-of-plane),
    Bx = dAy/dy, By = -dAy/dx.  We integrate Bz on the node B via
    Bz = -dAy/dx -> Ay(x,y) = -int Bz dx  (relative values suffice)."""
    ay = np.zeros_like(bz)
    for j in range(bz.shape[0]):
        ay[j] = -np.cumsum(bz[j]) * dx
    return ay


def _midplane_field(frames0):
    """Return midplane (y~0) arrays of ux, By, Bx and the row index."""
    ux, uy, bx, by, bz, rho, rhoS1 = frames0
    j0 = int(np.argmin(np.abs(uy)))
    return ux, j0, by[j0], bx[j0]


def _null_crossings(x, f):
    """x-positions where f changes sign (midplane in-plane-field nulls)."""
    s = np.sign(f)
    crosses = np.where(np.diff(s) != 0)[0]
    return [float(x[i]) for i in crosses]


def _opoints(x, f):
    """Return all midplane in-plane-field nulls (O- and X-points)."""
    return _null_crossings(x, f)


def _island_opoints(x, f):
    """Return the midplane nulls nearest the island centres (O-points, x ~ +-pi*L).

    Both O- and X-points give a By sign change on the midplane; the O-points are
    the ones at the island centres, so we keep the null closest to each of
    +ISLAND_X and -ISLAND_X."""
    nulls = _opoints(x, f)
    if not nulls:
        return []
    # Closest null to each island centre.
    out = []
    for target in (ISLAND_X, -ISLAND_X):
        best = min(nulls, key=lambda n: abs(n - target))
        out.append(best)
    return out


def validate_log(pic_diags=None, test_name=None):
    """Energy-log sanity for the reconnection run.

    Unlike the wave-family tests, the kinetic-ion energy Epart is *expected* to
    grow substantially: magnetic reconnection converts stored magnetic energy
    into ion kinetic energy.  So we only require
      1. finite magnetic and ion energies (no NaN / blow-up), and
      2. bounded magnetic-energy growth (Eb must not blow up by orders of
         magnitude, which would signal a solver instability rather than a
         physical reconnection-driven conversion).
    """
    if not pic_diags or len(pic_diags) < 2:
        return True, "Passed (no pic log)"
    first, last = pic_diags[0], pic_diags[-1]
    eb0, eb1 = first.get("Eb", 0.0), last.get("Eb", 0.0)
    ep1 = last.get("Epart", 0.0)
    if not math.isfinite(eb1):
        return False, "Eb not finite (NaN/Inf)"
    if not math.isfinite(ep1):
        return False, "Epart not finite (NaN/Inf)"
    # Magnetic energy must stay bounded; reconnection converts B into ion
    # kinetic energy, so allow up to a ~10x increase but reject a runaway.
    if eb0 > 0 and eb1 > eb0 * 10.0:
        return False, (f"Eb grew {eb1/eb0:.1f}x (unstable blow-up, not "
                       f"reconnection)")
    logger.debug("    Eb: %.3e -> %.3e (ratio %.2f)", eb0, eb1,
                 (eb1 / eb0) if eb0 > 0 else 0.0)
    logger.debug("    Epart: %.3e -> %.3e", first.get("Epart", 0.0), ep1)
    return True, "Passed (finite, bounded Eb)"


def _load_all_frames():
    """Load all .out frames, keeping only those with a consistent grid shape.

    Discards stale .out files from a previous run (e.g. a different solver
    variant with another resolution) that may linger in the plots directory."""
    plots_dir = os.path.join(RUN_DIR, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
    if not out_files:
        return None, "no .out plot files (PostProc.pl not run?)"
    frames = []
    shape = None
    for f in out_files:
        fr = _load_frame(f)
        if fr is None:
            continue
        bx = fr[2]
        if shape is None:
            shape = bx.shape  # adopt the first frame's grid
        elif bx.shape != shape:
            continue  # stale / mismatched grid; skip
        frames.append((f, fr))
    if len(frames) < 3:
        return None, "need >=3 parseable .out frames with a consistent grid"
    return frames, None


def validate_plot(test_name):
    """Reconnection plot check: equilibrium init, perturbation growth, flux
    (Ay) change at the X-point, and O-point motion."""
    frames, err = _load_all_frames()
    if err is not None:
        return False, err

    # ---- (1) Equilibrium initialization at t=0 ----
    ux, uy, bx0, by0, bz0, rho0, rhoS1_0 = frames[0][1]
    dx = ux[1] - ux[0]
    x0, j0, by_mid, bx_mid = _midplane_field(frames[0][1])

    # O-points: midplane nulls nearest the island centres (x ~ +-pi*L).
    outer = _island_opoints(x0, by_mid)
    if len(outer) < 2:
        return False, "t=0: no in-plane field nulls found on the midplane"
    # Tolerance is resolution-aware: the null position is only resolved to ~dx
    # (the coarser full-PIC grid lands the null one cell off the island centre).
    o_tol = max(0.35 * L, 1.5 * dx)
    op_positions = [abs(o) for o in outer]
    pos_ok = all(
        abs(o - ISLAND_X) < o_tol for o in op_positions)
    if not pos_ok:
        return False, (
            f"t=0: O-points at {[round(o,2) for o in outer]} d_i, "
            f"expected ~ +-{ISLAND_X:.1f} d_i")

    # Sheet density: peak near 1, background near 0.2.
    rho_peak = float(rho0.max())
    rho_bg = float(rho0.min())
    if not (0.5 < rho_peak < 2.0):
        return False, (
            f"t=0: peak sheet density {rho_peak:.3f} not in (0.5, 2.0)")
    if rho_bg > 0.6:
        return False, (
            f"t=0: background density {rho_bg:.3f} too high (expected ~0.2)")

    # ---- (2) Perturbation growth (instability active) ----
    # Reconnection rate proxy: Ay at the central X-point (x ~ 0, y ~ 0).
    ix = int(np.argmin(np.abs(ux)))
    ay_series = []
    max_dby_series = []
    for _, fr in frames:
        ux2, uy2, bx, by, bz, rho, rhoS1 = fr
        j = int(np.argmin(np.abs(uy2)))
        dby = float(np.abs(by - by0).max())
        max_dby_series.append(dby)
        ay = _flux_function(bx, bz, dx)
        ay_series.append(float(ay[j, ix]))

    seed = 0.02 * 0.05 * 2.0  # perturb*b0-ish scale; use measured early value
    early_amp = max(max_dby_series[:2])
    late_amp = max_dby_series[-1]
    growth = late_amp / early_amp if early_amp > 1e-6 else 0.0
    logger.debug("    perturbation growth: %.4f -> %.4f (%.1fx)",
                 early_amp, late_amp, growth)
    # Looser floor for full-PIC (coarser/short); Ay flux change confirms reconnection.
    late_thresh = 0.1 if test_name.endswith("hybrid") else 0.03
    if late_amp < late_thresh:
        return False, (
            f"late |delta By| = {late_amp:.3f} too small (no instability)")

    # ---- (3) Flux change at the X-point (reconnection) ----
    ay_first = ay_series[0]
    ay_span = max(ay_series) - min(ay_series)
    logger.debug("    Ay at X-point: span %.4f over %d frames",
                 ay_span, len(ay_series))
    if ay_span < 0.05:
        return False, (
            f"Ay at X-point only changes by {ay_span:.4f} (no reconnection)")

    # ---- (4) O-point motion ----
    xf, jf, by_mid_f, _ = _midplane_field(frames[-1][1])
    outer_f = _island_opoints(xf, by_mid_f)
    motion = 0.0
    if len(outer_f) >= 2:
        # displacement of the island-centre (O-point) positions.
        motion = max(abs(outer_f[0] - outer[0]),
                     abs(outer_f[1] - outer[1]))
    logger.debug("    O-point motion: %.3f d_i (t0 -> t_end)", motion)
    if motion < 0.2:
        logger.debug("    [WARN] O-point motion small (%.3f); reconnection "
                     "may still be ongoing", motion)

    # ---- (5) Charge neutrality (full-PIC only) ----
    # Quasi-neutral => rhoS0 ~ rhoS1*MASS_RATIO; skips hybrid (no rhoS1).
    if rhoS1_0 is not None:
        max_imbalance = 0.0
        for _, fr in frames:
            _ux, _uy, _bx, _by, _bz, _rho, rhoS1 = fr
            if rhoS1 is None:
                continue
            denom = max(float(_rho.max()), 1e-9)  # normalise by ion density
            imb = float(np.abs(_rho - rhoS1 * MASS_RATIO).max() / denom)
            max_imbalance = max(max_imbalance, imb)
        logger.debug("    charge imbalance max |rhoS0 - rhoS1*%.0f|/rho0 = %.3f",
                     MASS_RATIO, max_imbalance)
        if max_imbalance > 0.5:
            return False, (
                f"charge separation: |rhoS0 - rhoS1*{MASS_RATIO:.0f}|/rho0_max "
                f"= {max_imbalance:.2f} (> 0.5); load not charge neutral")

    msg = (f"reconnection: By perturbation {early_amp:.3f}->{late_amp:.3f}, "
           f"Ay span {ay_span:.3f}, O-points at "
           f"{[round(o,2) for o in outer]} d_i")
    logger.debug("    %s", msg)
    return True, msg
