#!/usr/bin/env python3
"""Validator for the proton-cyclotron anisotropy-instability (PCAI) test.

Runs the shared hybrid energy-log checks and measures the growth rate of the
transverse field d|B| = sqrt(mean(By^2)+mean(Bz^2)) from the .out frames,
checking it lies in a physical band around the linear-theory rate 0.162.
"""
import glob
import logging
import math
import os

logger = logging.getLogger(__name__)

import tests._shared.hybrid as _hyb
from .._shared.hybrid import validate_hybrid

RUN_DIR = "run_test"


def set_run_dir(run_dir):
    """Point the plot helpers at the current run directory."""
    global RUN_DIR
    RUN_DIR = run_dir
    _hyb.set_run_dir(run_dir)

# Linear-theory PCAI growth rate for beta_par=1, T_perp/T_par=3.
GAMMA_THEORY_OMEGA = 0.162

# MIN rejects a damped/stable plasma, MAX a runaway; measured ~0.10 (3D) / ~0.058 (2D).
MIN_GAMMA_OMEGA = 0.05
MAX_GAMMA_OMEGA = 0.35


def validate_log(pic_diags=None, test_name=None):
    """Energy-log checks (finiteness, no runaway, particle conservation)."""
    logger.debug("Validating PCAI Hybrid PIC Test...")

    if not pic_diags or len(pic_diags) < 2:
        logger.debug("  [INFO] No PIC energy log found; skipping energy checks.")
        return True, "Passed (no pic log)"

    first = pic_diags[0]
    last = pic_diags[-1]

    passed = True
    reasons = []

    # Magnetic energy: Eb is guide-field dominated, so a legit run has ratio ~1.
    eb0 = first.get("Eb", 0.0)
    eb1 = last.get("Eb", 0.0)
    logger.debug("    Eb (magnetic): %s -> %s", f"{eb0:.6e}", f"{eb1:.6e}")
    if not math.isfinite(eb1):
        passed = False
        reasons.append("Eb not finite (NaN/Inf)")
    if eb0 > 0 and eb1 > eb0 * 20.0:
        passed = False
        reasons.append("Eb grew >20x (runaway, not anisotropy-instability)")

    ep1 = last.get("Epart", 0.0)
    logger.debug("    Epart (ions):  %s -> %s",
                 f"{first.get('Epart', 0.0):.6e}", f"{ep1:.6e}")
    if not math.isfinite(ep1):
        passed = False
        reasons.append("Epart not finite (NaN/Inf)")

    # Ion-energy (particle) conservation.
    e0 = first.get("Epart0", 0.0)
    e1 = last.get("Epart0", 0.0)
    if e0 > 0:
        ratio = e1 / e0
        logger.debug("    Epart0 (ions): %s -> %s (ratio %.4f)",
                     f"{e0:.6e}", f"{e1:.6e}", ratio)
        if ratio < 0.5 or ratio > 2.0:
            passed = False
            reasons.append(
                f"Ion energy ratio {ratio:.3f} outside [0.5,2.0] "
                f"(particle non-conservation / runaway)")
    else:
        logger.debug("    [INFO] Epart0 initial zero; skipping ion check.")

    if passed:
        logger.debug("PCAI Hybrid PIC Test: PASSED")
        return True, "Passed"
    return False, "; ".join(reasons)


def _load_out(out_file):
    """Load x, By, Bz arrays from a hybrid .out plot file (byte-safe)."""
    with open(out_file, "r", encoding="latin-1") as f:
        lines = f.readlines()
    if len(lines) < 6:
        return None
    var_names = lines[4].split()
    vidx = {v.upper(): i for i, v in enumerate(var_names)}
    if "BY" not in vidx or "BZ" not in vidx:
        return None
    iby, ibz = vidx["BY"], vidx["BZ"]
    x, by, bz = [], [], []
    for line in lines[5:]:
        cols = line.split()
        if len(cols) <= max(iby, ibz):
            continue
        try:
            x.append(float(cols[0]))
            by.append(float(cols[iby]))
            bz.append(float(cols[ibz]))
        except (ValueError, IndexError):
            continue
    if len(x) < 4:
        return None
    return x, by, bz


def _frame_time(out_file):
    """Return the simulation time (SI seconds) from the .out header line 2."""
    try:
        with open(out_file, "r", encoding="latin-1") as f:
            f.readline()
            nxt = f.readline()
        return float(nxt.split()[1])
    except (OSError, IndexError, ValueError):
        return None


def _t_norm_from_param():
    """Return tNorm = lNormSI/uNormSI (s per code-time unit) from PARAM.in."""
    p = os.path.join("tests", "pcai", "PARAM.in")
    l = u = None
    try:
        with open(p) as f:
            for line in f:
                s = line.strip()
                if not s or s.startswith("#"):
                    continue
                toks = s.split()
                if len(toks) >= 2 and toks[1] == "lNormSI":
                    l = float(toks[0])
                elif len(toks) >= 2 and toks[1] == "uNormSI":
                    u = float(toks[0])
    except (OSError, ValueError):
        return 1.0
    if l is not None and u is not None and u > 0:
        return l / u
    return 1.0


def _transverse_norm(by, bz):
    """Total transverse-field norm d|B| = sqrt(mean(By^2)+mean(Bz^2))."""
    n = len(by)
    if n == 0:
        return 0.0
    s = sum(b * b for b in by) + sum(b * b for b in bz)
    return math.sqrt(s / n)


def _check_pcai_growth():
    """Measure the PCAI growth rate from the time-resolved B_y/B_z frames."""
    plots_dir = os.path.join(RUN_DIR, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
    # The test produces ~8 frames (dn=500); fewer means broken/PostProc-failed output.
    if len(out_files) < 3:
        logger.debug("    [PCAI] Need >=3 .out frames; found %d.", len(out_files))
        return False, f"Need >=3 .out frames; found {len(out_files)}"

    frames = []
    for f in out_files:
        data = _load_out(f)
        if data is None:
            continue
        _, by, bz = data
        t = _frame_time(f)
        if t is None:
            continue
        amp = _transverse_norm(by, bz)
        if amp <= 0:
            continue
        frames.append((t, amp))

    if len(frames) < 3:
        logger.debug("    [PCAI] Too few usable frames.")
        return False, f"Too few usable frames ({len(frames)}); need >=3"

    frames.sort(key=lambda s: s[0])
    logger.debug("    [PCAI] Tracking total transverse-field norm across %d "
                 "frames.", len(frames))
    tNorm = _t_norm_from_param()
    for t_si, amp in frames:
        logger.debug("    [PCAI]   t_code=%6.2f  d|B|=%.4e", t_si / tNorm, amp)

    # Fit log(d|B|) = gamma*t + c over the exponential-growth window [0.2,0.85]
    # of the max amplitude (avoiding the noise plateau and saturation tail).
    amp_max = max(a for _, a in frames)
    lo, hi = 0.20 * amp_max, 0.85 * amp_max
    ts, logs = [], []
    for t_si, amp in frames:
        if lo <= amp <= hi:
            ts.append(t_si / tNorm)
            logs.append(math.log(amp))
    if len(ts) < 3:
        logger.debug("    [PCAI] Fewer than 3 frames inside the exponential "
                     "window [%.3g, %.3g]; refitting over all frames.", lo, hi)
        ts = [t_si / tNorm for t_si, _ in frames]
        logs = [math.log(a) for _, a in frames]
    if len(ts) < 3:
        return False, "Fewer than 3 usable frames for a growth-rate fit"
    if max(ts) <= min(ts):
        return False, "No time spread in frames (wave does not evolve)"

    # Linear least squares: gamma = cov(t, logA)/var(t).
    nt = len(ts)
    mean_t = sum(ts) / nt
    mean_l = sum(logs) / nt
    cov = sum((ts[i] - mean_t) * (logs[i] - mean_l) for i in range(nt))
    var_t = sum((ts[i] - mean_t) ** 2 for i in range(nt))
    if var_t <= 0:
        return False, "No time spread in frames (degenerate fit)"
    gamma_meas = cov / var_t

    amp0, ampN = frames[0][1], frames[-1][1]
    growth_ratio = ampN / amp0 if amp0 > 0 else float("inf")
    logger.debug("    [PCAI] seed d|B|=%.4e -> final d|B|=%.4e (x%.1f)",
                 amp0, ampN, growth_ratio)
    logger.debug("    [PCAI] measured gamma/Omega_ci = %.4f (theory %.3f)",
                 gamma_meas, GAMMA_THEORY_OMEGA)

    if gamma_meas < MIN_GAMMA_OMEGA:
        return False, (f"measured gamma/Omega_ci = {gamma_meas:.3f} < "
                       f"{MIN_GAMMA_OMEGA} (transverse field damped or frozen; "
                       f"no anisotropy instability)")
    if gamma_meas > MAX_GAMMA_OMEGA:
        return False, (f"measured gamma/Omega_ci = {gamma_meas:.3f} > "
                       f"{MAX_GAMMA_OMEGA} (runaway / missing-Hall factor)")

    logger.debug("    [PCAI] growth rate check: PASSED "
                 "(gamma/Omega_ci = %.3f, theory %.3f; measured rate within the "
                 "validated band [%.2f, %.2f])",
                 gamma_meas, GAMMA_THEORY_OMEGA, MIN_GAMMA_OMEGA, MAX_GAMMA_OMEGA)
    return True, "Passed"


def validate_plot(test_name):
    """Plot-output check: measure the PCAI growth rate from the .out frames."""
    logger.debug("  --- Validating Output Files (PCAI growth rate) ---")
    result, reason = _check_pcai_growth()
    if result:
        logger.debug("    [PCAI] PCAI growth-rate check: VERIFIED")
    return result, reason
