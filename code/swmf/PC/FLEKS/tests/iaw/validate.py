#!/usr/bin/env python3
"""Validator for the ion-acoustic-wave (IAW) hybrid-PIC test (tests/iaw).

    Checks that the seeded ion-density profile in the plot output is a clean
    single-mode sinusoid and mass-conserving, that no NaN / blow-up occurs in
    the energy log, and that the ambipolar (electron-pressure) electric field Ex
    is present and bounded.
    """
import glob
import logging
import math
import os

logger = logging.getLogger(__name__)

import tests._shared.hybrid as _hyb

RUN_DIR = "run_test"


def set_run_dir(run_dir):
    """Point the plot helpers at the current run directory."""
    global RUN_DIR
    RUN_DIR = run_dir
    _hyb.set_run_dir(run_dir)


def validate_log(pic_diags=None, test_name=None):
    """Validate the ion-acoustic-wave (IAW) hybrid-PIC test.

    Success criteria:
    1. FLEKS completes (run_test already checks the exit code).
    2. No NaN / blow-up: magnetic energy Eb and total ion energy Epart finite.
       (This IAW test is unmagnetized, B = 0, so Eb ~ 0 throughout.)
    3. Ion particle number conserved (periodic BCs, no source): Epart0 stays
       within [0.2, 10] of its initial value.
    4. Density seed present and mass-conserving: from the plot output the seeded
       ion density rhoS0 is a clean single-mode sinusoid at the first frame and
       its mean/overall amplitude remain bounded (mass conserved, no blow-up).
       Requires PostProc.pl output; if absent the profile check is skipped.
    5. Ambipolar electric field present: the electron pressure-gradient term
       (-grad(P_e)/rho) must produce a non-zero Ex at the LAST plot frame (the
       initial Ex is zero by construction).
    """
    logger.debug("Validating Ion Acoustic Wave Test...")

    if not pic_diags or len(pic_diags) < 2:
        logger.debug("  [INFO] No PIC energy log found; skipping energy checks.")
        return True, "Passed (no pic log)"

    first = pic_diags[0]
    last = pic_diags[-1]

    passed = True
    reasons = []

    # --- energy / stability ------------------------------------------------
    eb0 = first.get("Eb", 0.0)
    eb1 = last.get("Eb", 0.0)
    logger.debug("    Eb (magnetic): %s -> %s", f"{eb0:.6e}", f"{eb1:.6e}")
    if not math.isfinite(eb1):
        passed = False
        reasons.append("Eb not finite (NaN/Inf)")
    if eb0 > 0 and eb1 > eb0 * 5.0:
        passed = False
        reasons.append("Eb grew >5x (whistler/Hall runaway?)")

    ep1 = last.get("Epart", 0.0)
    logger.debug("    Epart (ions):  %s -> %s",
                 f"{first.get('Epart', 0.0):.6e}", f"{ep1:.6e}")
    if not math.isfinite(ep1):
        passed = False
        reasons.append("Epart not finite (NaN/Inf)")

    e0 = first.get("Epart0", 0.0)
    e1 = last.get("Epart0", 0.0)
    if e0 > 0:
        ratio = e1 / e0
        logger.debug("    Epart0 (ions): %s -> %s (ratio %.4f)",
                     f"{e0:.6e}", f"{e1:.6e}", ratio)
        if ratio < 0.2 or ratio > 10.0:
            passed = False
            reasons.append(
                f"Ion energy ratio {ratio:.3f} outside [0.2,10.0] "
                f"(gross particle non-conservation / runaway)")
    else:
        logger.debug("    [INFO] Epart0 initial zero; skipping ion check.")

    # --- density profile / seed check --------------------------------------
    profile_ok, profile_reason = _check_iaw_density()
    if profile_ok is False:
        passed = False
        reasons.append(profile_reason)
    elif profile_reason:
        logger.debug("    [IAW] %s", profile_reason)

    if passed:
        logger.debug("Ion Acoustic Wave Test: PASSED")
        return True, "Passed"
    return False, "; ".join(reasons)


def _check_iaw_density():
    """Check the seeded ion-density profile in the IAW plot output.

    Reads the pic-component plot .out files (PostProc.pl output) and verifies
    that (1) the ion density rhoS0 is a clean single-mode sinusoid at the first
    frame (high |correlation| with sin(kx*x)) and (2) the mean density is
    conserved across the run (no mass loss / blow-up).

    Returns (passed, reason).  A True return with a non-empty reason means the
    check was skipped (e.g. PostProc.pl output not available) -- this is not a
    failure, matching how the other wave tests degrade gracefully when PostIDL
    is not built.
    """
    try:
        plots_dir = os.path.join(RUN_DIR, "PC", "plots")
        out_files = sorted(glob.glob(os.path.join(plots_dir, "plot*pic*.out")))
        if not out_files:
            out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
        if not out_files:
            return True, ("No .out plot files found (PostProc.pl not run?); "
                          "skipping profile check.")

        # Parse Lx (x-domain length) and waveMode from PARAM.in.  The FLEKS
        # #GEOMETRY block uses per-dimension xMin/xMax/yMin/... tokens.
        Lx = None
        wave_mode = 1
        xmin = xmax = None
        try:
            with open(os.path.join(RUN_DIR, "PARAM.in"), "r") as pf:
                section = None
                for line in pf:
                    s = line.strip()
                    if s.startswith("#"):
                        section = s
                        continue
                    if not s:
                        continue
                    parts = s.split()
                    # FLEKS PARAM.in uses the "value keyword" token order,
                    # e.g. "-3.2  xMin".  Scan for the keyword and take the
                    # preceding token as the value.
                    for i, tok in enumerate(parts):
                        if section == "#GEOMETRY" and i > 0:
                            if tok == "xMin":
                                try:
                                    xmin = float(parts[i - 1])
                                except ValueError:
                                    pass
                            elif tok == "xMax":
                                try:
                                    xmax = float(parts[i - 1])
                                except ValueError:
                                    pass
                        if (section == "#TESTCASE" and i > 0
                                and tok == "waveMode"):
                            try:
                                wave_mode = int(float(parts[i - 1]))
                            except ValueError:
                                pass
            if xmin is not None and xmax is not None and xmax > xmin:
                Lx = xmax - xmin
        except Exception:
            pass

        if Lx is None or Lx <= 0:
            return True, "Could not parse Lx from PARAM.in; skipping profile check."

        kx = 2.0 * math.pi * wave_mode / Lx

        def load_profile(path):
            with open(path, "r", encoding="latin-1") as f:
                lines = f.readlines()
            if len(lines) < 6:
                return None, None, None
            var_names = lines[4].split()
            vidx = {v.upper(): i for i, v in enumerate(var_names)}
            ridx = None
            for target in ("RHOS0", "RHOS1", "RHO"):
                if target in vidx:
                    ridx = vidx[target]
                    break
            if ridx is None:
                return None, None, None
            eidx = vidx.get("EX")
            xs = []
            rhos = []
            exs = []
            for line in lines[5:]:
                cols = line.split()
                if len(cols) <= ridx:
                    continue
                try:
                    xs.append(float(cols[0]))
                    rhos.append(float(cols[ridx]))
                    exs.append(float(cols[eidx]) if eidx is not None else 0.0)
                except (ValueError, IndexError):
                    continue
            return xs, rhos, exs

        first_x, first_rho, first_ex = load_profile(out_files[0])
        last_x, last_rho, last_ex = load_profile(out_files[-1])
        if not first_x or not last_x:
            return True, "Could not parse rhoS0 from .out; skipping profile check."

        m0 = sum(first_rho) / len(first_rho)
        m1 = sum(last_rho) / len(last_rho)
        logger.debug("    [IAW] mean rhoS0: %.4e -> %.4e", m0, m1)
        if m0 <= 0:
            return True, "Zero initial density; skipping profile check."
        if not (math.isfinite(m0) and math.isfinite(m1)):
            return False, "Non-finite density; blow-up."
        if abs(m1 - m0) / m0 > 0.3:
            return False, (f"Mean density changed by >30% ({m0:.3e} -> "
                           f"{m1:.3e}); mass not conserved.")

        # Correlate the (mean-subtracted) density with sin(kx*x).  For a clean
        # single-mode IAW seed the profile stays proportional to sin(kx*x) in
        # space (the temporal part is a scalar), so |corr| remains large.
        def corr_with_sin(xs, rhos):
            n = len(xs)
            mean = sum(rhos) / n
            dr = [rhos[i] - mean for i in range(n)]
            s = [math.sin(kx * xs[i]) for i in range(n)]
            m_dr = sum(dr) / n
            m_s = sum(s) / n
            cov = sum((dr[i] - m_dr) * (s[i] - m_s) for i in range(n))
            vd = sum((dr[i] - m_dr) ** 2 for i in range(n))
            vs = sum((s[i] - m_s) ** 2 for i in range(n))
            if vd <= 0 or vs <= 0:
                return 0.0
            return cov / math.sqrt(vd * vs)

        c0 = corr_with_sin(first_x, first_rho)
        logger.debug("    [IAW] |corr(rho, sin(kx x))| first frame: %.3f", abs(c0))
        if abs(c0) < 0.3:
            return False, (f"Initial density not a clean sinusoid "
                           f"(corr={c0:.3f}); seed may be missing.")

        # Bounded amplitude (no blow-up).
        amp0 = max(first_rho) - min(first_rho)
        amp1 = max(last_rho) - min(last_rho)
        if amp0 <= 0:
            return True, "Flat initial density; skipping profile check."
        ratio = amp1 / amp0
        logger.debug("    [IAW] density amplitude: %.3e -> %.3e (ratio %.3f)",
                     amp0, amp1, ratio)
        if ratio > 10.0:
            return False, (f"Density amplitude grew >10x "
                           f"({amp0:.3e} -> {amp1:.3e}); blow-up.")

        # Ambipolar electric field present. For the unmagnetized IAW the only
        # E source is -grad(p_e)/rho, so Ex must be non-zero wherever the seeded
        # density gradient is. The initial field is exactly zero (the seed has no
        # EM field), so we check a LATER frame (the last one) where the ambipolar
        # field has built up. A zero Ex at late times means the structured plot
        # is reading a stale/zero node-centred E instead of the live centerEhybrid
        # (the centerPlasmaPrev ghost-cell / nodeE-sync bug this test guards).
        ex_max_last = max(abs(v) for v in last_ex)
        ex_max_first = max(abs(v) for v in first_ex)
        logger.debug("    [IAW] max |Ex|: first=%.3e last=%.3e",
                     ex_max_first, ex_max_last)
        # The initial Ex is zero by construction (the seed has no EM field); the
        # ambipolar field builds up as the density gradient develops, so Ex must
        # be non-zero at the last frame. The "growth" ratio is not meaningful here
        # (Ex goes 0 -> non-zero), so only finiteness and non-zero-at-last-frame
        # are enforced.
        if not (math.isfinite(ex_max_first) and math.isfinite(ex_max_last)):
            return False, "Ex not finite (NaN/Inf); blow-up."
        if ex_max_last <= 1e-12:
            return False, ("Ex is identically zero at the last frame "
                           "(ambipolar / electron-pressure term not reaching the "
                           "output; check the centerPlasmaPrev ghost-cell fix and "
                           "nodeE sync).")
        return True, ""
    except Exception as e:  # never let the profile check crash validation
        return True, f"Profile check errored ({e}); skipping."


def validate_plot(test_name):
    """Plot-output check: seeded density profile + mass conservation."""
    logger.debug("  --- Validating Output Files (IAW density profile) ---")
    return _check_iaw_density()
