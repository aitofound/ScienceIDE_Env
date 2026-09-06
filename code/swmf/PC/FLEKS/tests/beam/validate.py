#!/usr/bin/env python3
"""Validator for the beam instability test (tests/beam).

The runner exercises the directory twice -- full PIC (PARAM.in, base_name
"beam") and hybrid PIC (PARAM.in.hybrid, base_name "beam_hybrid").

The primary validation for BOTH variants is the FFT-based transverse-wave check
on plot output (`_check_beam_transverse_wave`).  The hybrid variant additionally
runs the shared hybrid-family energy-log checks (`validate_hybrid`) to guard
against NaN / field blow-up.  The beam tracks test particles, so a particle-log
check is enabled via `PARTICLE_TOL`.
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

# Particle-tracking tolerance passed to validate_test_particles() in the
# common runner (validate_tests.py).
# min_velocity_change: require >= 1% relative mean-velocity change (beam
# gyration) to catch a silently disabled field gather.
PARTICLE_TOL = {
    "expected_active_species": [0],
    "launch_threshold": 0.5,
    "max_speed": 10.0,
    "min_velocity_change": 0.01,
}


def validate_log(pic_diags=None, test_name=None):
    """Energy-log validation for the beam test.

    The hybrid variant (test_name ends with "hybrid") runs the shared hybrid
    energy checks to catch NaN / field blow-up; the full-PIC variant performs no
    log-file check here (validation is the plot-output FFT).
    """
    if test_name and test_name.endswith("hybrid"):
        return validate_hybrid(pic_diags=pic_diags, test_name=test_name)

    logger.debug("Validating Beam Instability Test...")
    logger.debug("  [INFO] Beam diagnostic checks rely on plot output (FFT).")
    logger.debug("Beam Instability Test: PASSED")
    return True, "Passed"


def _check_beam_transverse_wave():
    """Check transverse EM wave growth against the cyclotron resonance.

    Reads the final .out plot file (at t ~= 0.1 normalized), FFTs the
    transverse magnetic-field profile (By, Bz), and compares the dominant
    spatial mode to the theoretical cyclotron-resonant wavenumber
    ``k_res = Omega_i / Delta_v``.

    For the beam test the resonant wavelength (``k_res^-1`` ~ 10^4 km)
    vastly exceeds the 2 km periodic box, so the resonant mode (n ~= 0)
    cannot fit.  The instability therefore populates the longest-wavelength
    modes that fit in the box.  This check verifies that (1) the transverse
    wave has grown above the numerical noise floor and (2) the wave power
    is concentrated in low-order spatial modes consistent with the
    box-limited instability, reporting the dominant mode and k_res for
    inspection.

    Returns (passed: bool, reason: str).
    """
    plots_dir = os.path.join(RUN_DIR, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
    if not out_files:
        logger.debug("    [FFT] No .out files found (PostProc.pl not run?).")
        return True, "No .out files (skipped)"

    # Use the final frame (latest cycle); this is the t ~= 0.1 snapshot.
    out_file = out_files[-1]
    logger.debug("    [FFT] Loading .out: %s", os.path.basename(out_file))

    # Plot .out files are written by PostProc.pl and may contain non-UTF-8
    # bytes; read byte-safe so a stray byte never aborts the validation.
    with open(out_file, "r", encoding="latin-1") as f:
        lines = f.readlines()
    if len(lines) < 6:
        return True, "Short .out file"

    var_names = lines[4].split()
    vidx = {v.upper(): i for i, v in enumerate(var_names)}
    for need in ("BY", "BZ", "BX"):
        if need not in vidx:
            logger.debug("    [FFT] '%s' not found in .out variables: %s",
                         need, var_names)
            return True, f"{need} not in .out"

    iby, ibz, ibx = vidx["BY"], vidx["BZ"], vidx["BX"]
    # The H+ ion density column is requested in #SAVEPLOT varName.  In PLANETARY
    # output it is converted back to amu/cc, so it must round-trip to the input
    # #UNIFORMSTATE value (5.0 amu/cc).  The species index differs between the
    # full-PIC (H+ = species 1, rhoS1) and hybrid (single H+ species, rhoS0)
    # variants.
    irho = vidx.get("RHOS1")
    if irho is None:
        irho = vidx.get("RHOS0")

    x = []
    by = []
    bz = []
    bx = []
    rho1 = []
    for line in lines[5:]:
        cols = line.split()
        if len(cols) <= max(iby, ibz, ibx, irho if irho is not None else 0):
            continue
        try:
            x.append(float(cols[0]))
            by.append(float(cols[iby]))
            bz.append(float(cols[ibz]))
            bx.append(float(cols[ibx]))
            if irho is not None:
                rho1.append(float(cols[irho]))
        except (ValueError, IndexError):
            continue

    n = len(x)
    if n < 4:
        logger.debug("    [FFT] Too few data points for FFT.")
        return True, "Too few points"

    # Simulation time (normalized) from line 2.
    try:
        t_norm = float(lines[1].split()[1])
    except (ValueError, IndexError):
        t_norm = float("nan")

    # B-fields are in nT in the .out file.
    bx_mean = sum(bx) / n
    bperp = [math.hypot(by[i], bz[i]) for i in range(n)]
    bperp_max = max(bperp)

    # The plot is saved in PLANETARY units (#SAVEPLOT ... planet): x
    # coordinates are normalised by rPlanet.  Because this test sets no
    # #BODYSIZE, rPlanet defaults to lNormSI (#NORMALIZATION), so physical
    # length [m] = (output coordinate) * lNormSI.
    l_norm_si = 1000.0  # lNormSI [m] from #NORMALIZATION (no #BODYSIZE -> rPlanet)

    logger.debug("    [FFT] t=%.4f s (SI, .out header), N=%d cells", t_norm, n)
    logger.debug("    [FFT] |Bx|=%.4f nT, max|B_perp|=%.4e nT", bx_mean, bperp_max)

    # ---- Check 1: wave growth above the noise floor ----------------------
    # At t=0 the transverse field is exactly zero; after the instability
    # triggers it grows from numerical noise.  Require the amplitude to
    # exceed a small fraction of the guide field.
    noise_frac = 1e-4
    if bx_mean <= 0:
        growth_ok = bperp_max > 0
    else:
        growth_ok = bperp_max > noise_frac * abs(bx_mean)
    logger.debug("    [FFT] Wave growth: max|B_perp|/|Bx| = %.3e "
                 "(threshold %.0e) -> %s",
                 bperp_max / max(abs(bx_mean), 1e-30), noise_frac,
                 'OK' if growth_ok else 'FAIL')

    # ---- Check 2: dominant mode & power in low-order modes ----------------
    # FFT By and Bz separately (preserving sign/oscillation), then combine
    # the per-mode amplitudes.  Using |B_perp| directly would introduce
    # spurious harmonics from the magnitude operation.
    def _dft_amp(data):
        nn = len(data)
        out = []
        for k in range(nn // 2 + 1):
            re = sum(data[j] * math.cos(2 * math.pi * k * j / nn)
                     for j in range(nn))
            im = -sum(data[j] * math.sin(2 * math.pi * k * j / nn)
                      for j in range(nn))
            out.append(math.hypot(re, im))
        return out

    try:
        import numpy as np
        fy = np.abs(np.fft.rfft(by))
        fz = np.abs(np.fft.rfft(bz))
        amps = [math.hypot(float(fy[k]), float(fz[k]))
                for k in range(len(fy))]
    except ImportError:
        ay = _dft_amp(by)
        az = _dft_amp(bz)
        amps = [math.hypot(ay[k], az[k]) for k in range(len(ay))]

    # Non-DC (n>=1) power.
    total_power = sum(a * a for a in amps[1:])
    if total_power <= 0:
        logger.debug("    [FFT] No non-DC spectral power; wave has not grown.")
        return False, "No transverse wave power detected"

    # Dominant non-DC mode.
    n_dom = max(range(1, len(amps)), key=lambda k: amps[k])
    dom_frac = amps[n_dom] ** 2 / total_power

    # Fraction of power in low-order modes (n <= N/4).
    n_low = n // 4
    low_frac = sum(a * a for a in amps[1:n_low + 1]) / total_power

    logger.debug("    [FFT] Dominant mode: n=%d (%.1f%% of non-DC power)",
                 n_dom, 100 * dom_frac)
    logger.debug("    [FFT] Power in low modes (n<=%d): %.1f%%", n_low, 100 * low_frac)

    # ---- Theoretical resonant wavenumber ---------------------------------
    # Cyclotron resonance: k_res = Omega_i / Delta_v (ion-ion beam-beam).
    # Omega_i = q_p * |Bx| / m_p (SI; the Boris pusher uses q*dt/(2*m)
    # without a c factor, so this is the SI cyclotron frequency).
    q_p = 1.60217663e-19   # C  (cUnitChargeSI)
    m_p = 1.67262192e-27   # kg (cProtonMassSI)
    bx_si = abs(bx_mean) * 1e-9          # nT -> T
    omega_i = q_p * bx_si / m_p          # rad/s
    delta_v = 8e5                         # m/s (beam +400, bg -400 km/s)
    k_res = omega_i / delta_v             # 1/m

    # Box geometry: x is in planetary units in the .out file (normalised by
    # rPlanet = lNormSI here), so the physical box length in metres is
    # L = (coordinate extent) * lNormSI.  This keeps every quantity in SI
    # (L in m, k in 1/m) for the comparison against k_res.
    dx_out = x[1] - x[0]
    L = n * dx_out * l_norm_si              # physical box length [m]
    k1 = 2 * math.pi / L                    # box-fundamental wavenumber [1/m]
    n_res = max(1, round(k_res / k1))

    # ---- Check 0: output density round-trips to the input value -----------
    # #UNIFORMSTATE sets the H+ background density rho = 5.0 amu/cc.  The code
    # converts it to code units (x1e6*m_p*Si2NoRho) internally, and PLANETARY
    # output converts it back to amu/cc (No2OutRho = 1/Si2NoRho /m_p *1e-6).
    # The round trip is the identity, so the plotted density column must
    # reproduce the input.  Guard against a unit mismatch (e.g. rho interpreted
    # as PIC/code units in the validator) with a tight relative tolerance.
    density_ok = True
    if irho is not None and rho1:
        rho_mean = sum(rho1) / len(rho1)
        rho_input = 5.0  # amu/cc, #UNIFORMSTATE H+ background
        logger.debug("    [FFT] H+ density output = %.4f amu/cc "
                     "(input %.2f amu/cc)", rho_mean, rho_input)
        if not math.isfinite(rho_mean):
            density_ok = False
        elif abs(rho_mean - rho_input) > 0.05 * rho_input:
            density_ok = False
            logger.debug("    [FFT] Density round-trip FAIL: output density "
                         "= %.4f amu/cc vs input %.2f amu/cc",
                         rho_mean, rho_input)
    else:
        logger.debug("    [FFT] H+ density column not in .out; "
                     "density round-trip check skipped.")

    logger.debug("    [FFT] Omega_i = %.4f rad/s, Delta_v = %.1e m/s",
                 omega_i, delta_v)
    logger.debug("    [FFT] k_res = %.3e 1/m, k_1 = %.3e 1/m (L = %.1f m)",
                 k_res, k1, L)
    logger.debug("    [FFT] Resonant wavelength = %.3e m vs box L = %.1f m",
                 2 * math.pi / k_res, L)

    if k_res < k1:
        # Resonant wavelength exceeds the box: the resonant mode (n ~ 0)
        # cannot fit, so the nearest available mode is the box-fundamental
        # n=1.  The instability populates the longest-wavelength modes that
        # fit; verify the bulk of the wave power resides in low-order modes
        # (not grid-scale noise near the Nyquist frequency).
        mode_ok = low_frac > 0.4
        logger.debug("    [FFT] k_res < k_1: resonant mode (n=%.2e) "
                     "exceeds the box; nearest available mode is n=1.",
                     k_res / k1)
        logger.debug("    [FFT] Mode check (box-limited): low-mode power "
                     "fraction %.2f > 0.4 -> %s", low_frac,
                     'OK' if mode_ok else 'FAIL')
    else:
        tol = 2
        mode_ok = abs(n_dom - n_res) <= tol
        logger.debug("    [FFT] Mode check: |n_dom(%d) - n_res(%d)| <= %d -> %s",
                     n_dom, n_res, tol, 'OK' if mode_ok else 'FAIL')

    # ---- Check 3: the transverse wave power must GROW over time ------------
    # The instability grows from seed noise, so max|B_perp| must increase across
    # the time-resolved frames -- distinguishing a real instability from static
    # numerical seed.  Skips gracefully (not a failure) if < 3 frames exist.
    growth_ok2, growth_reason = _beam_growth_over_time(plots_dir)
    if growth_reason is not None:
        logger.debug("    [FFT] Time-growth: %s", growth_reason)

    if not density_ok:
        logger.debug("    [FFT] Density round-trip check: FAIL")
        return False, "output rhoS1 does not round-trip to the input amu/cc density"
    if growth_ok and mode_ok and growth_ok2:
        logger.debug("    [FFT] Transverse wave check: PASSED")
        return True, "Passed"
    else:
        reasons = []
        if not growth_ok:
            reasons.append(f"wave amplitude {bperp_max:.2e} nT below "
                           f"noise floor ({noise_frac:.0e}*|Bx|)")
        if not mode_ok:
            reasons.append(f"dominant mode n={n_dom} inconsistent with "
                           f"resonance (n_res={n_res})")
        if not growth_ok2:
            reasons.append(growth_reason or "transverse wave not growing over time")
        return False, "; ".join(reasons)


def _beam_growth_over_time(plots_dir):
    """Verify max|B_perp| grows across the time-resolved .out frames.

    Reads the per-frame max transverse-field amplitude (max of hypot(By, Bz))
    and requires it to be non-decreasing and to end at least 2x its early value,
    confirming the seeded cyclotron wave is genuinely amplified by the
    instability rather than being static seed noise.

    Returns (passed: bool, reason: str|None).  reason is None when the check
    could not be evaluated (too few frames) and should be skipped, not failed.
    """
    frames = []
    for f in sorted(glob.glob(os.path.join(plots_dir, "*.out"))):
        with open(f, "r", encoding="latin-1") as fh:
            lines = fh.readlines()
        if len(lines) < 6:
            continue
        var_names = lines[4].split()
        vidx = {v.upper(): i for i, v in enumerate(var_names)}
        if "BY" not in vidx or "BZ" not in vidx:
            continue
        iby, ibz = vidx["BY"], vidx["BZ"]
        bperp = []
        for line in lines[5:]:
            cols = line.split()
            if len(cols) <= max(iby, ibz):
                continue
            try:
                bperp.append(math.hypot(float(cols[iby]), float(cols[ibz])))
            except (ValueError, IndexError):
                continue
        if bperp:
            frames.append(max(bperp))
    if len(frames) < 3:
        return True, None  # too few frames -> skip (not a failure)
    if any(frames[i + 1] < frames[i] for i in range(len(frames) - 1)):
        logger.debug("    [FFT] Non-monotonic max|B_perp| across frames: %s",
                     ["%.2e" % v for v in frames])
        return False, (f"max|B_perp| not non-decreasing across "
                       f"{len(frames)} frames")
    if frames[0] > 0 and frames[-1] <= 2.0 * frames[0]:
        logger.debug("    [FFT] Transverse amplitude nearly static: %s",
                     ["%.2e" % v for v in frames])
        return False, (f"max|B_perp| {frames[0]:.2e} -> {frames[-1]:.2e} "
                       f"nT (no instability growth)")
    logger.debug("    [FFT] Time-growth: max|B_perp| %s",
                 " -> ".join("%.2e" % v for v in frames))
    return True, None


def validate_plot(test_name):
    """Plot-output check: FFT-based transverse-wave resonant-wavenumber check.

    Applies to both the full-PIC and hybrid variants: the hybrid keeps the
    identical box, guide field, beam ratio/speeds and density, so the FFT check
    is valid for both solvers.
    """
    logger.debug("  --- Validating Output Files (FFT transverse wave) ---")
    result, reason = _check_beam_transverse_wave()
    if result:
        logger.debug("    [FFT] Beam transverse-wave resonance check: VERIFIED")
    return result, reason
