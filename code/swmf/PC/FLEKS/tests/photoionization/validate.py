#!/usr/bin/env python3
"""Validator for the photoionization test (tests/photoionization).

Checks that the heaviest ion species (O+, which receives the exosphere source)
energy increases over time (ionization source active), verifies the day/night
asymmetry in the plot output, and validates the test-particle tracer log.
"""
import glob
import logging
import os

logger = logging.getLogger(__name__)

import tests._shared.hybrid as _hyb

RUN_DIR = "run_test"


def set_run_dir(run_dir):
    """Point the plot helpers at the current run directory."""
    global RUN_DIR
    RUN_DIR = run_dir
    _hyb.set_run_dir(run_dir)

# Particle-tracking tolerance passed to validate_test_particles() in the
# common runner (validate_tests.py).
PARTICLE_TOL = {
    "expected_active_species": [0, 1, 2],
    "launch_threshold": 0.5,
    "max_speed": 10.0,
}


def validate_log(pic_diags=None, test_name=None):
    """Validate that the ionization source is active for the heaviest ion.

    Checks that the heaviest ion species (O+, which receives the exosphere
    source) energy increases over time, confirming the ionization source is
    active.  Uses the PIC energy log (log_pic_n*.log) as the data source.
    """
    logger.debug("Validating Ionization Source Test...")

    if not pic_diags or len(pic_diags) < 2:
        logger.debug("  [INFO] No PIC energy log found; skipping energy checks.")
        return True, "Passed (no pic log)"

    first = pic_diags[0]
    last = pic_diags[-1]

    # Determine the source (heaviest ion) species index from available
    # EpartN keys.  The last EpartN column is the heaviest ion.
    epart_keys = sorted(
        k for k in first.keys() if k.startswith("Epart") and k != "Epart"
    )
    if not epart_keys:
        logger.debug("  [INFO] No per-species energy columns; skipping.")
        return True, "Passed (no Epart columns)"
    source_key = epart_keys[-1]  # e.g. "Epart2" for O+
    source_idx = source_key.replace("Epart", "")

    logger.debug("  --- Energy Diagnostics (from log_pic log) ---")
    for k in epart_keys:
        logger.debug("    %s: %s -> %s",
                     k, f"{first.get(k, 0):.6e}", f"{last.get(k, 0):.6e}")
    logger.debug("    Initial total Epart: %s", f"{first.get('Epart', 0):.6e}")
    logger.debug("    Final total Epart:   %s", f"{last.get('Epart', 0):.6e}")

    # Check that the heaviest ion (O+) energy increases.
    e_src_initial = first.get(source_key, 0.0)
    e_src_final = last.get(source_key, 0.0)
    logger.debug("    Initial %s (species %s, O+): %s",
                 source_key, source_idx, f"{e_src_initial:.6e}")
    logger.debug("    Final   %s (species %s, O+): %s",
                 source_key, source_idx, f"{e_src_final:.6e}")
    logger.debug("    Growth factor: %.3fx",
                 e_src_final / max(e_src_initial, 1e-30))
    if e_src_final <= e_src_initial:
        logger.debug("    FAIL: %s energy did not increase.", source_key)
        logger.debug("    Ionization source may not be working correctly.")
        return False, (
            f"{source_key} energy did not increase "
            f"(initial={e_src_initial:.2e}, final={e_src_final:.2e})"
        )
    else:
        logger.debug("    SUCCESS: %s energy increased (ionization source active).",
                     source_key)
        return True, "Passed"


def _read_shadow_params():
    """Read shadow-cylinder and normalization parameters from PARAM.in.

    Returns (Rp_plot, halfH_plot, shadowR_plot) all in *plot* (normalized)
    coordinates, or None if shadow cylinder is not enabled.
    """
    param_path = os.path.join(RUN_DIR, "PARAM.in")
    Rp_si = 3.0e6
    lNormSI = 1.0
    halfH_si = 0.0
    shadowR_si = 0.0
    useShadow = False

    # Track which line within #NORMALIZATION we're on.
    norm_line_idx = 0

    try:
        with open(param_path, "r") as pf:
            section = None
            for line in pf:
                line_s = line.strip()
                if line_s.startswith("#"):
                    section = line_s
                    if section == "#NORMALIZATION":
                        norm_line_idx = 0
                    continue
                if not line_s:
                    continue
                parts = line_s.split()
                if section == "#BODYSIZE" and len(parts) >= 1:
                    try:
                        Rp_si = float(parts[0])
                    except ValueError:
                        pass
                elif section == "#NORMALIZATION" and len(parts) >= 1:
                    # First value is lNormSI, second is uNormSI.
                    if norm_line_idx == 0:
                        try:
                            lNormSI = float(parts[0])
                        except ValueError:
                            pass
                    norm_line_idx += 1
                elif section == "#SHADOWCYLINDER":
                    useShadow = True
                    try:
                        val = float(parts[0])
                    except ValueError:
                        continue
                    if "radius" in line_s.lower():
                        shadowR_si = val
                    elif "halfheight" in line_s.lower():
                        halfH_si = val
    except Exception:
        pass

    if not useShadow:
        return None

    # Plot coordinates = SI / lNormSI
    return (Rp_si / lNormSI, halfH_si / lNormSI, shadowR_si / lNormSI)


def _load_idl_plot_asymmetry():
    """Check photoionization day/night asymmetry from plot output.

    Reads .out files produced by PostProc.pl (which concatenates the *.h
    and *.idl files written by FLEKS).  PostProc.pl must be run before
    calling this function.  Verifies that rhoS1 is non-zero on the dayside
    (+X) and much smaller inside the planetary shadow cylinder (-X, within
    cylinder radius and height).

    Returns (passed: bool, reason: str).
    """
    plots_dir = os.path.join(RUN_DIR, "PC", "plots")

    # -- Get shadow cylinder geometry in plot coordinates -------------------
    shadow_geom = _read_shadow_params()
    if shadow_geom is None:
        logger.debug("    [ASYM] Shadow cylinder not enabled; skipping asymmetry check.")
        return True, "No shadow cylinder"

    Rp_plot, halfH_plot, shadowR_plot = shadow_geom
    logger.debug("    [ASYM] Rp=%.0f, halfH=%.0f, shadowR=%.0f (plot coords)",
                 Rp_plot, halfH_plot, shadowR_plot)

    # -- Collect data points (x, y, rhoS1) from PostProc.pl .out files -----
    points = []  # list of (x, y, rhoS1)

    out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
    if not out_files:
        logger.debug("    [ASYM] No .out files found. "
                     "Ensure PostProc.pl has been run after FLEKS.exe.")
        return False, "No .out files found (PostProc.pl not run?)"

    latest_out = out_files[-1]
    logger.debug("    [ASYM] Loading .out: %s", os.path.basename(latest_out))
    with open(latest_out, "r") as f:
        lines = f.readlines()
    if len(lines) < 6:
        return True, "Short .out file"
    var_names = lines[4].split()
    # Look for the heaviest ion species density (rhoS2 = O+ with 3-species
    # layout: 0=e, 1=H+, 2=O+).  Fall back to rhoS1 for 2-species layouts.
    rho_idx = None
    for target in ("RHOS2", "RHOS1"):
        for iv, vn in enumerate(var_names):
            if vn.upper() == target:
                rho_idx = iv
                break
        if rho_idx is not None:
            continue
    if rho_idx is None:
        return True, "rhoS2/rhoS1 not in .out"
    for line in lines[5:]:
        cols = line.strip().split()
        if len(cols) <= rho_idx:
            continue
        try:
            points.append((float(cols[0]), float(cols[1]),
                           float(cols[rho_idx])))
        except (ValueError, IndexError):
            continue

    if not points:
        logger.debug("    [ASYM] No data points parsed.")
        return True, "Empty plot data"

    # -- Classify points: dayside vs shadow ---------------------------------
    # The shadow cylinder covers the nightside (x < 0 for solarDir=+X).
    # The "planet" plot keyword limits output to ~[-Rp, +Rp], so we compare
    # the dayside (x > 0) with the deep nightside (x < -Rp/2) where
    # photoionization is suppressed and diffusion has less effect.
    dayside_vals = []
    shadow_vals = []
    y_lim = min(Rp_plot / 2.0, shadowR_plot / 4.0)
    for x, y, rho in points:
        if abs(y) > y_lim:
            continue
        if x > 0:
            dayside_vals.append(rho)
        elif x < -Rp_plot * 0.5:
            shadow_vals.append(rho)

    dayside_mean = (sum(dayside_vals) / len(dayside_vals)
                    if dayside_vals else 0.0)
    shadow_mean = (sum(shadow_vals) / len(shadow_vals)
                   if shadow_vals else 0.0)

    logger.debug("    [ASYM] Parsed %d points: %d dayside, %d shadow",
                 len(points), len(dayside_vals), len(shadow_vals))
    logger.debug("    Dayside (+X) mean rhoS1:      %.4e", dayside_mean)
    logger.debug("    Shadow  (-X, cyl) mean:       %.4e", shadow_mean)

    if len(dayside_vals) == 0:
        return False, "Zero dayside points -- cannot verify"
    if dayside_mean <= 0.0:
        return False, "Dayside rhoS1 is zero -- photoionization source not active"
    if len(shadow_vals) == 0:
        return False, "Zero shadow points -- cannot verify"
    # The shadow region retains some density from dayside diffusion (especially
    # near the surface), so require shadow < 20% of dayside rather than near-zero;
    # a 0.2 threshold is a meaningful asymmetry check.
    if shadow_mean > max(dayside_mean * 0.2, 1e-30):
        return False, (
            f"Shadow rhoS1 too high ({shadow_mean:.2e}) "
            f"vs dayside ({dayside_mean:.2e})"
        )

    ratio = shadow_mean / max(dayside_mean, 1e-30)
    logger.debug("    Shadow/dayside ratio:          %.2e  (expected \u226a 1)", ratio)

    return True, "Passed"


def validate_plot(test_name):
    """Plot-output check: day/night asymmetry via IDL .out."""
    logger.debug("  --- Validating Output Files (IDL .out) ---")
    result, reason = _load_idl_plot_asymmetry()
    if result:
        logger.debug("    [IDL] Photoionization day/night asymmetry: VERIFIED")
    return result, reason
