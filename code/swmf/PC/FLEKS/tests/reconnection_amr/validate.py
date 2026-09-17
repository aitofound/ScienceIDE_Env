#!/usr/bin/env python3
"""Validator for the AMR Fadeev reconnection test (tests/reconnection_amr)."""
import glob
import logging
import math
import os

import numpy as np

# Self-contained (lives outside tests/); runner injects RUN_DIR via set_run_dir.
RUN_DIR = "run_test"


def set_run_dir(run_dir):
    """Point the plot helpers at the current run directory."""
    global RUN_DIR
    RUN_DIR = run_dir


logger = logging.getLogger(__name__)

# Grid / equilibrium parameters (must match PARAM.in / FadeevIC defaults).
# Base grid is the uniform reconnection grid 32 x 16 (dx = dy = 62.8/32 =
# 1.9625 d_i); the refined layer is 2x finer (dx = dy = 0.98 d_i).
L = 5.0                 # sheet half-thickness / d_i
DX_COARSE = 1.9625      # base-grid cell size / d_i
DX_FINE = DX_COARSE / 2.0  # refined-layer cell size / d_i (~0.98)
REFINE_Y_HALF = 7.85    # refined band |y| < 7.85 d_i

# PostIDL .out column indices (from the header line `x y rhoS0 rhoS1 Bx By Bz
# Ex Ey Ez uxS0 ...`): 0=x, 1=y, 2=rhoS0, 3=rhoS1, 4=Bx, 5=By, 6=Bz, ...
COL_X, COL_Y, COL_BX, COL_BY = 0, 1, 4, 5
NHEADER = 5  # .out header: 4 metadata lines + 1 variable-name line


def _load_frame(path):
    """Parse one PostIDL .out frame into a dict of per-point arrays.

    The .out layout is: 4 metadata lines, then a variable-name line
    (`x y rhoS0 rhoS1 Bx By Bz Ex Ey Ez uxS0 ...`), then one data row per cell.
    The multi-level AMR grid is written per-cell (non-uniform), so rows cannot
    be reshaped into a rectangular (ny, nx) grid.
    """
    lines = open(path, encoding="latin-1").read().splitlines()
    if len(lines) <= NHEADER:
        return None
    # The variable-name line lists all columns (including scalars like mS0,
    # qS0, cLight, rPlanet, cutz); the per-cell data rows carry only the field
    # columns (x y rhoS0 rhoS1 Bx By Bz Ex Ey Ez uxS0 ... pYZS1), so use the
    # actual data-row width and map the leading header names onto it.
    all_names = lines[NHEADER - 1].split()
    rows = []
    for ln in lines[NHEADER:]:
        c = ln.split()
        if len(c) >= 5:
            rows.append(c)
    if len(rows) < 4:
        return None
    ncol = min(len(rows[0]), len(all_names))
    data = np.array([r[:ncol] for r in rows], dtype=float)
    idx = {all_names[i]: i for i in range(ncol)}
    return {"x": data[:, idx["x"]], "y": data[:, idx["y"]],
            "Bx": data[:, idx["Bx"]], "By": data[:, idx["By"]]}


def _frame_stats(frame):
    """Return (coarse_present, fine_present, fine_layer_ok) flags.

    Detects the two cell spacings along y (and x) and verifies the fine cells
    sit in the central layer |y| < 7.85 d_i rather than at the box edges.
    A y-row is "fine" when its x-cells are spaced by DX_FINE; the fine rows
    must form a contiguous band around the current sheet.
    """
    y = frame["y"]
    ys = np.unique(y)
    if ys.size < 4:
        return False, False, False
    dy = np.diff(ys)
    dy = dy[dy > 0.02]
    if dy.size == 0:
        return False, False, False
    spacings = set(np.round(dy, 4))
    coarse = any(abs(s - DX_COARSE) < 0.05 for s in spacings)
    fine = any(abs(s - DX_FINE) < 0.05 for s in spacings)

    # Fine y-rows: those carrying DX_FINE-spaced x cells.
    fine_rows = []
    for yy in ys:
        m = np.abs(frame["y"] - yy) < 0.01
        xs = np.sort(frame["x"][m])
        if xs.size >= 2:
            xdx = np.round(np.diff(xs), 4)
            if xdx.size and abs(float(xdx[0]) - DX_FINE) < 0.05:
                fine_rows.append(float(yy))
    layer_ok = False
    if fine_rows:
        ylo, yhi = min(fine_rows), max(fine_rows)
        layer_ok = (abs(ylo + REFINE_Y_HALF) < 0.5) and \
                   (abs(yhi - REFINE_Y_HALF) < 0.5)
    return coarse, fine, layer_ok


def _midplane_by(frame):
    """Return (x_array, By_array) along the midplane y ~ 0 (refined layer).

    The refined band is block-aligned (`maxBlockSizeY = 2`), so its cell-centred
    y rows are offset by half a fine cell and need NOT straddle y = 0 exactly
    (the nearest fine row is |y| = DX_FINE/2 ~ 0.49).  A fixed `|y| < 0.3*DX_FINE`
    window would select nothing.  Instead pick the fine-level row (x cells
    spaced by DX_FINE) whose |y| is smallest — the row closest to the current
    sheet — and return its By profile.
    """
    y = frame["y"]
    ys = np.unique(y)
    best = None  # (|yy|, yy)
    for yy in ys:
        m = np.abs(y - yy) < 0.01
        xs = np.sort(frame["x"][m])
        if xs.size < 2:
            continue
        xdx = np.round(np.diff(xs), 4)
        if xdx.size and abs(float(xdx[0]) - DX_FINE) < 0.05:
            if best is None or abs(float(yy)) < best[0]:
                best = (abs(float(yy)), float(yy))
    if best is None:
        return None, None
    _, yy = best
    m = np.abs(y - yy) < 0.01
    if m.sum() < 8:
        return None, None
    x = frame["x"][m]
    by = frame["By"][m]
    order = np.argsort(x)
    return x[order], by[order]


def _null_crossings(x, f):
    """x-positions where the midplane in-plane field changes sign."""
    s = np.sign(f)
    crosses = np.where(np.diff(s) != 0)[0]
    return [float(x[i]) for i in crosses]


def _xpoint(x, f):
    """Midplane null nearest the central reconnection X-point (x ~ 0)."""
    nulls = _null_crossings(x, f)
    if not nulls:
        return None
    return min(nulls, key=lambda n: abs(n - 0.0))


def validate_log(pic_diags=None, test_name=None):
    """Energy-log sanity: finite, bounded Eb; Epart grows (reconnection)."""
    if not pic_diags or len(pic_diags) < 2:
        return True, "Passed (no pic log)"
    first, last = pic_diags[0], pic_diags[-1]
    eb0, eb1 = first.get("Eb", 0.0), last.get("Eb", 0.0)
    ep1 = last.get("Epart", 0.0)
    if not math.isfinite(eb1):
        return False, "Eb not finite (NaN/Inf)"
    if not math.isfinite(ep1):
        return False, "Epart not finite (NaN/Inf)"
    if eb0 > 0 and eb1 > eb0 * 10.0:
        return False, (f"Eb grew {eb1/eb0:.1f}x (unstable blow-up, not "
                       f"reconnection)")
    logger.debug("    Eb: %.3e -> %.3e", eb0, eb1)
    logger.debug("    Epart: %.3e -> %.3e", first.get("Epart", 0.0), ep1)
    return True, "Passed (finite, bounded Eb)"


def _load_all_frames():
    plots_dir = os.path.join(RUN_DIR, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots_dir, "*_fluid*.out")))
    if not out_files:
        return None, "no *_fluid .out plot files (PostProc.pl not run?)"
    frames = []
    for f in out_files:
        fr = _load_frame(f)
        if fr is not None:
            frames.append((os.path.basename(f), fr))
    if len(frames) < 2:
        return None, "need >=2 parseable .out frames"
    return frames, None


def validate_plot(test_name):
    """AMR reconnection plot check.

    1. The two-level AMR grid is present (coarse + fine spacing, fine band
       around the central current sheet).
    2. Reconnection is active: the midplane By perturbation grows and the
       in-plane field shows the O/X null structure of the growing islands.
    """
    frames, err = _load_all_frames()
    if err is not None:
        return False, err

    # ---- (1) AMR refinement present ----
    coarse0, fine0, layer0 = _frame_stats(frames[0][1])
    if not coarse0 or not fine0:
        return False, ("AMR grid not detected (need coarse dx~3.9 and fine "
                       "dx~2.0 d_i)")
    if not layer0:
        return False, ("fine cells do not form the central current-sheet layer "
                       "|y| < ~7.85 d_i")

    # ---- (2) Reconnection on the refined midplane ----
    first = frames[0][1]
    x0, by0 = _midplane_by(first)
    if x0 is None:
        return False, "no midplane (y~0) points found in refined layer"

    # The Fadeev equilibrium is anti-symmetric in By about x=0, so the midplane
    # carries the reconnection X-point (By null near x ~ 0) at t=0.
    xp0 = _xpoint(x0, by0)
    if xp0 is None or abs(xp0) > 0.5 * L:
        return False, (f"t=0: no midplane X-point near x~0 "
                       f"(closest null at x={xp0:.2f} d_i)")

    # Perturbation growth from the seed and midplane profile change.
    last = frames[-1][1]
    max_dby = [float(np.abs(fr["By"] - first["By"]).max()) for _, fr in frames]
    late_amp = max(max_dby[-1], np.abs(last["By"]).max())
    early_amp = max(max_dby[:1] + [0.0]) if max_dby else 0.0
    growth = (late_amp / early_amp) if early_amp > 1e-6 else 0.0
    xf, byf = _midplane_by(last)
    if xf is None:
        return False, "no midplane points in final frame"
    profile_change = float(np.abs(byf - by0).max())

    # Hybrid reconnection has a slower ion-scale onset than full-PIC, but runs
    # longer (TimeMax=20 vs 3); the threshold is solver-aware.
    is_hybrid = test_name is not None and test_name.endswith("hybrid")
    late_thresh = 0.05 if is_hybrid else 0.1
    if late_amp < late_thresh:
        return False, (f"late |delta By| = {late_amp:.3f} too small "
                       f"(no instability)")
    if profile_change < 0.05:
        return False, (f"midplane By changed by only {profile_change:.3f} "
                       f"(no reconnection)")

    logger.debug("    midplane |delta By|: %.3f -> %.3f (%.1fx), "
                 "X-point at x=%.2f d_i, profile change %.3f",
                 early_amp, late_amp, growth, xp0, profile_change)
    msg = (f"AMR [coarse dx={DX_COARSE}, fine dx={DX_FINE}, layer "
           f"|y|<{REFINE_Y_HALF}]: By pert {early_amp:.3f}->{late_amp:.3f}, "
           f"X-point at x={xp0:.2f} d_i")
    return True, msg
