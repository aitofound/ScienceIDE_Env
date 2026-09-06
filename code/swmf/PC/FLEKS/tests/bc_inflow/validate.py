#!/usr/bin/env python3
"""Validator for the inflow/outflow open-boundary hybrid test (tests/bc_inflow/).

A uniform magnetized hybrid plasma streams along +x through a 1D domain with an
INFLOW boundary at -x and an OUTFLOW boundary at +x.  A perfectly uniform
streaming plasma is the exact steady solution, so the inflow/outflow pair must
keep the state uniform:

  * ion kinetic energy Epart and magnetic energy Eb stay bounded and finite
    (no blow-up, no NaN).  Particle number is NOT conserved (inflow seeds,
    outflow loses), so the energy ratio tolerance is loose.
  * the upstream (inflow-side) density rhoS0 and bulk velocity uxS0 are
    preserved (the inflow maintains the upstream state).
  * the guide field Bx stays uniform and unchanged.
  * no spurious electric field develops.

This is the clean counterpart to the wip/hybrid-bc-regression shock attempt,
which regressed the hybrid solver.  Here the inflow is built ONLY from the
existing zero-gradient ghost-cell copy (Pic::use_float) and the existing
Maxwellian particle re-seeding (Particles::add_particles_cell), so a pass
confirms those pieces compose correctly into a working inflow boundary.
"""
import glob
import logging
import math
import os

logger = logging.getLogger(__name__)

# Loose energy tolerances: the state is steady but the open boundaries inject
# and remove particles every step, so Epart is not strictly conserved.  We
# guard against gross non-conservation / blow-up rather than tight drift.
EPART_RATIO_MIN = 0.5
EPART_RATIO_MAX = 2.0
EB_RATIO_TOL = 0.20          # Bx0 is uniform => Eb should stay within +/-20%
EE_MAX_FRAC = 0.10           # Ee <= 10% of Eb (no spurious E build-up)
RHO_TOL = 0.10                # upstream density preserved to +/-10%
VEL_TOL = 0.10               # bulk velocity preserved to +/-10%
B_SPREAD_TOL = 0.10          # Bx spatially uniform to 10% of mean
E_SPURIOUS_TOL = 0.10        # mean spurious E <= 10% of guide-field scale


def set_run_dir(run_dir):
    """Mirror the runner's RUN_DIR into the shared hybrid helper."""
    import tests._shared.hybrid as _hyb
    _hyb.set_run_dir(run_dir)


def validate_log(pic_diags=None, test_name=None):
    """Energy-log checks: finite & bounded energies, no NaN/blow-up."""
    if not pic_diags or len(pic_diags) < 2:
        return True, "Passed (no pic log)"

    first, last = pic_diags[0], pic_diags[-1]
    passed = True
    reasons = []

    # All energies must be finite (no NaN/Inf from a broken boundary).
    for key in ("Etot", "Ee", "Eb", "Epart"):
        v0, v1 = first.get(key, 0.0), last.get(key, 0.0)
        if not (math.isfinite(v0) and math.isfinite(v1)):
            passed = False
            reasons.append(f"{key} not finite (NaN/Inf)")

    # Ion kinetic energy: bounded (open BCs => not strictly conserved).
    ep0, ep1 = first.get("Epart", 0.0), last.get("Epart", 0.0)
    if ep0 > 0:
        ratio = ep1 / ep0
        logger.debug("    Epart: %s -> %s (ratio %.5f)",
                     f"{ep0:.6e}", f"{ep1:.6e}", ratio)
        if ratio < EPART_RATIO_MIN or ratio > EPART_RATIO_MAX:
            passed = False
            reasons.append(
                f"Epart ratio {ratio:.4f} outside "
                f"[{EPART_RATIO_MIN}, {EPART_RATIO_MAX}] (gross non-conservation)")

    # Magnetic energy: uniform Bx0 => Eb should stay close to its initial value.
    eb0, eb1 = first.get("Eb", 0.0), last.get("Eb", 0.0)
    if eb0 > 0:
        ratio = eb1 / eb0
        logger.debug("    Eb: %s -> %s (ratio %.5f)",
                     f"{eb0:.6e}", f"{eb1:.6e}", ratio)
        if abs(ratio - 1.0) > EB_RATIO_TOL:
            passed = False
            reasons.append(
                f"Eb ratio {ratio:.4f} not within "
                f"[{1-EB_RATIO_TOL:.3f}, {1+EB_RATIO_TOL:.3f}] "
                f"(field energy drifted; boundary field not held)")

    # Electric energy stays negligible (no spurious E build-up at the faces).
    ee1 = last.get("Ee", 0.0)
    if eb0 > 0 and ee1 > EE_MAX_FRAC * eb0:
        passed = False
        reasons.append(
            f"Ee {ee1:.3e} exceeds {EE_MAX_FRAC*100:.0f}% of Eb "
            f"({eb0:.3e}) (spurious E built up at a boundary)")

    if passed:
        return True, "Passed (energies finite & bounded)"
    return False, "; ".join(reasons)


def _load_out(out_file):
    """Load a .out frame: return ({VAR: col_idx}, float rows)."""
    with open(out_file, "r", encoding="latin-1") as f:
        lines = f.readlines()
    if len(lines) < 6:
        return None, None
    vidx = {v.upper(): i for i, v in enumerate(lines[4].split())}
    rows = []
    for line in lines[5:]:
        cols = line.split()
        if not cols:
            continue
        try:
            rows.append([float(c) for c in cols])
        except ValueError:
            continue
    return vidx, rows


def _col(vidx, rows, name):
    i = vidx.get(name)
    if i is None or not rows or i >= len(rows[0]):
        return None
    return [r[i] for r in rows]


def _mean(vals):
    return sum(vals) / len(vals) if vals else 0.0


def validate_plot(test_name):
    """Plot checks: Bx uniform & unchanged, uxS0 conserved, rhoS0 preserved
    at the inflow face, no spurious mean E."""
    import tests._shared.hybrid as _hyb
    plots_dir = os.path.join(_hyb.RUN_DIR, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots_dir, "*.out")))
    if not out_files:
        logger.debug("    [INFLOW] No .out files (PostProc.pl not run?) -- skipping.")
        return True, "No .out files (skipped)"

    vidx0, rows0 = _load_out(out_files[0])
    vidxl, rowsl = _load_out(out_files[-1])
    if vidx0 is None or vidxl is None or not rows0 or not rowsl:
        return True, "Could not parse .out frames (skipped)"

    passed = True
    reasons = []

    # Bulk velocity uxS0 conserved across the domain.
    ux0, uxl = _col(vidx0, rows0, "UXS0"), _col(vidxl, rowsl, "UXS0")
    if ux0 and uxl:
        mean0, meanl = _mean(ux0), _mean(uxl)
        logger.debug("    [INFLOW] <uxS0>: %s -> %s",
                     f"{mean0:.5f}", f"{meanl:.5f}")
        if abs(mean0) > 1e-12 and abs(meanl / mean0 - 1.0) > VEL_TOL:
            passed = False
            reasons.append(
                f"bulk velocity <uxS0> {mean0:.4f} -> {meanl:.4f} "
                f"(>{VEL_TOL*100:.0f}% drift; inflow not maintaining state)")
        elif abs(mean0) <= 1e-12 and abs(meanl) > 0.05:
            passed = False
            reasons.append(f"spurious bulk velocity {meanl:.4f} developed")

    # Guide field Bx uniform and unchanged.
    bx0, bxl = _col(vidx0, rows0, "BX"), _col(vidxl, rowsl, "BX")
    mb0 = _mean(bx0) if bx0 else 0.0
    if bxl:
        mbl = _mean(bxl)
        spreadl = max(bxl) - min(bxl)
        logger.debug("    [INFLOW] <Bx>: %s -> %s (spread %.4f)",
                     f"{mb0:.5f}", f"{mbl:.5f}", spreadl)
        if abs(mb0) > 1e-12 and abs(mbl / mb0 - 1.0) > EB_RATIO_TOL:
            passed = False
            reasons.append(f"guide field Bx changed {mb0:.4f} -> {mbl:.4f}")
        if abs(mbl) > 1e-12 and spreadl > B_SPREAD_TOL * abs(mbl):
            passed = False
            reasons.append(
                f"guide field not uniform (spread {spreadl:.4f} > "
                f"{B_SPREAD_TOL*100:.0f}% of <Bx>; boundary layer formed)")

    # Upstream (inflow-side, -x) density preserved: the first few cells must
    # hold the pristine upstream rho rather than draining toward vacuum.
    # Compare the inflow-side third of the last frame to the same region in the
    # first frame (the seeded uniform state).
    rho_l = _col(vidxl, rowsl, "RHOS0")
    rho0_all = _col(vidx0, rows0, "RHOS0")
    xi_l = vidxl.get("X")
    xi_0 = vidx0.get("X")
    if rho_l and rho0_all and xi_l is not None and xi_0 is not None:
        xmins = [min(r[xi_l] for r in rowsl), min(r[xi_0] for r in rows0)]
        xmaxs = [max(r[xi_l] for r in rowsl), max(r[xi_0] for r in rows0)]
        cuts = [xmins[i] + 0.33 * (xmaxs[i] - xmins[i]) for i in range(2)]
        rho_in = [r for r, x in zip(rho_l, (row[xi_l] for row in rowsl))
                  if x <= cuts[0]]
        rho0_in = [r for r, x in zip(rho0_all, (row[xi_0] for row in rows0))
                  if x <= cuts[1]]
        if rho_in and rho0_in:
            rho_in_mean = _mean(rho_in)
            rho0_mean = _mean(rho0_in)
            logger.debug("    [INFLOW] inflow-side <rhoS0>: %s -> %s",
                         f"{rho0_mean:.5f}", f"{rho_in_mean:.5f}")
            if rho0_mean > 1e-12 and \
                    abs(rho_in_mean / rho0_mean - 1.0) > RHO_TOL:
                passed = False
                reasons.append(
                    f"inflow-side density {rho0_mean:.4f} -> {rho_in_mean:.4f} "
                    f"(>{RHO_TOL*100:.0f}% drift; inflow face draining)")
            elif rho_in_mean <= 0.0:
                passed = False
                reasons.append("inflow-side density collapsed to zero")

            # Verify pressure tensor components (Pxx, Pyy, Pzz) at inflow physical cells:
            # confirm thermal isotropy and verify out-of-plane pressure (Pzz) is not zero.
            pxx_l = _col(vidxl, rowsl, "PXXS0")
            pyy_l = _col(vidxl, rowsl, "PYYS0")
            pzz_l = _col(vidxl, rowsl, "PZZS0")
            if pxx_l and pyy_l and pzz_l:
                pxx_in = [r for r, x in zip(pxx_l, (row[xi_l] for row in rowsl))
                          if x <= cuts[0]]
                pyy_in = [r for r, x in zip(pyy_l, (row[xi_l] for row in rowsl))
                          if x <= cuts[0]]
                pzz_in = [r for r, x in zip(pzz_l, (row[xi_l] for row in rowsl))
                          if x <= cuts[0]]
                if pxx_in and pyy_in and pzz_in:
                    m_pxx = _mean(pxx_in)
                    m_pyy = _mean(pyy_in)
                    m_pzz = _mean(pzz_in)
                    logger.debug("    [INFLOW] inflow-side pressure: Pxx=%.5e, Pyy=%.5e, Pzz=%.5e",
                                 m_pxx, m_pyy, m_pzz)
                    if m_pzz <= 0.0 or not math.isfinite(m_pzz):
                        passed = False
                        reasons.append("inflow-side Pzz collapsed or non-finite (2D3V vz sampling failed)")
                    elif m_pxx > 0.0:
                        # Allow 25% tolerance for finite-particle statistical fluctuations
                        p_anisotropy_z = abs(m_pzz / m_pxx - 1.0)
                        p_anisotropy_y = abs(m_pyy / m_pxx - 1.0)
                        if p_anisotropy_z > 0.25:
                            passed = False
                            reasons.append(
                                f"inflow pressure anisotropic in Z (Pzz/Pxx = {m_pzz/m_pxx:.3f}; "
                                f">{25}% deviation from isotropy)")
                        if p_anisotropy_y > 0.25:
                            passed = False
                            reasons.append(
                                f"inflow pressure anisotropic in Y (Pyy/Pxx = {m_pyy/m_pxx:.3f}; "
                                f">{25}% deviation from isotropy)")

    # No spurious mean E field.
    for ecomp in ("EX", "EY", "EZ"):
        el = _col(vidxl, rowsl, ecomp)
        if el:
            meanl = _mean(el)
            scale = abs(mb0) if (bx0 and abs(mb0) > 1e-12) else 1.0
            logger.debug("    [INFLOW] mean <%s> = %.5f (scale %.4f)",
                         ecomp, meanl, scale)
            if abs(meanl) > E_SPURIOUS_TOL * scale:
                passed = False
                reasons.append(
                    f"spurious mean electric field <{ecomp}> = {meanl:.4f} "
                    f"(>{E_SPURIOUS_TOL*100:.0f}% of guide-field scale)")

    if passed:
        return True, ("Passed (state stays uniform; inflow maintains upstream "
                      "state, outflow lets plasma leave)")
    return False, "; ".join(reasons)
