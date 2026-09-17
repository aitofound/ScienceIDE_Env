#!/usr/bin/env python3
"""
test_density_analytic.py
========================
Compare AMPS gridless density/spectrum/flux output against closed-form analytic
predictions for four test cases in a pure DIPOLE magnetic field.

USAGE
-----
  python3 test_density_analytic.py [OPTIONS]

  Options:
    --dir PATH     directory containing AMPS output files  (default: '.')
    --tol FRAC     relative tolerance for exact tests       (default: 0.05)
    --no-plots     skip matplotlib figures
    -v             verbose (default: True)

OUTPUT FILES READ
-----------------
  gridless_points_density.dat    total number density per point
  gridless_points_spectrum.dat   T(E), J_b(E), J_loc(E) per point
  gridless_points_flux.dat       total + per-channel integral flux per point
                                 (present only if #ENERGY_CHANNELS is in the input)

=============================================================================
SECTION 1  PHYSICS BACKGROUND
=============================================================================

1.1  Liouville's theorem and directional transmissivity
  In a static magnetic field, Liouville's theorem states that phase-space
  density is constant along particle trajectories. For an isotropic boundary
  distribution J_b(E) [m^-2 s^-1 sr^-1 MeV^-1], the local intensity at x0 is

    J_loc(E, Ω; x0) = A(E, Ω; x0) · J_b(E)

  where A=1 if direction Ω backtraces to the outer boundary (ALLOWED) and
  A=0 if it terminates on the loss sphere (FORBIDDEN).

  The direction-averaged (isotropic) transmissivity is

    T(E; x0) = (1/4π) ∫ A dΩ  ≈  N_allowed / N_dirs

  The solver samples N_dirs = 24×48 = 1152 directions per energy per point.
  Statistical uncertainty: σ_T ≈ 1/√N_dirs × 0.5 ≈ 1.5%.

1.2  Number density  (1/v weighted)
    n(x0) = 4π ∫_{Emin}^{Emax} T(E;x0)·J_b(E) / v(E) dE         [m^-3]

  The factor 1/v(E) arises from the relation n = (4π/v)·J for isotropic f.
  Slow particles contribute more density per unit flux.

1.3  Omnidirectional integral flux  (NOT 1/v weighted)
    F(x0; E1,E2) = 4π ∫_{E1}^{E2} T(E;x0)·J_b(E) dE              [m^-2 s^-1]

  Flux counts particles crossing a unit surface per unit time from all
  directions. No velocity weighting: each particle counts once regardless
  of speed.

1.4  Density vs flux
    n [m^-3]      = 4π ∫ T·J_b/v dE
    F [m^-2 s^-1] = 4π ∫ T·J_b   dE

  Ratio: n/F = <1/v>_J  (energy-averaged inverse speed, weighted by J_b).
  For ultra-relativistic particles (v → c): F ≈ n·c.

=============================================================================
SECTION 2  ANALYTIC REFERENCE FORMULAS
=============================================================================

2.1  Geometric transmissivity (inner-sphere shadow)
  From any point at radius r, Earth blocks a cone of half-angle
  α = arcsin(r_inner/r). Fraction NOT blocked:

    T_geo(r) = (1 + cos α) / 2 = (1 + √(1 − (r_inner/r)²)) / 2

  This is EXACT when all energies are above the Størmer cutoff (T_Størmer=1).

2.2  Størmer cutoff rigidity  (Smart & Shea 2005; Störmer 1930)
  For a centred dipole with Størmer constant Cs ≈ 59.6 GV·Re²:

    Rc_max(r, λ) = Cs · cos⁴λ / r²   [GV]

  At the geomagnetic pole cos λ=0, Rc=0 for all r.

2.3  Analytic number density  (numerical quadrature)
  The integrand J_b(E)/v(E) has no closed-form antiderivative for relativistic
  protons. We use 4000-point log-spaced quadrature.

2.4  Analytic integral flux  (EXACT CLOSED FORM, power-law spectrum)
  For J_b(E) = J0·(E/E0)^{−γ} with γ ≠ 1:

    F(E1,E2) = 4π·T·J0·E0^γ / (γ−1) · (E1^{1−γ} − E2^{1−γ})

  For γ = 2 (this test):
    F(E1,E2) = 4π·T·J0·E0² · (1/E1 − 1/E2)

  Exact — independent of grid resolution. Primary benchmark for flux tests.

=============================================================================
SECTION 3  NUMERICAL IMPLEMENTATION (DensityGridless.cpp)
=============================================================================

3.1  Density integral: trapezoidal rule on the solver energy grid [J].
       n = 4π · Σ 0.5·(g_i + g_{i+1})·ΔE_J   where g_i = J_loc_i / v_i

3.2  Total flux integral (FluxIntegrateTotal): same rule, no 1/v.
       F = 4π · Σ 0.5·(J_loc_i + J_loc_{i+1})·ΔE_J

3.3  Channel flux integral (FluxIntegrateChannel):
     Step 1: clip channel [E1,E2] to the solver grid range.
     Step 2: augment sub-grid with linearly-interpolated T at E1, E2.
     Step 3: trapezoidal rule on the augmented sub-grid.
     Channels outside the grid return 0. Zero-width channels return 0.

3.4  MPI: flux is NOT sent over MPI. Workers send T[]. Master reconstructs
     flux from T[] after receiving it, reusing the same gSpectrum object.
"""

import sys
import os
import math
import argparse
import numpy as np

# ---------------------------------------------------------------------------
# Physical constants  (must match DipoleInterface.h / DensityGridless.cpp)
# ---------------------------------------------------------------------------
C_LIGHT  = 299792458.0
QE       = 1.602176634e-19
AMU      = 1.66053906660e-27
MP_KG    = 1.007276466621 * AMU
MP_MEV   = MP_KG * C_LIGHT**2 / (1e6 * QE)     # ~938.272 MeV
RE_KM    = 6371.2
RE_M     = RE_KM * 1e3
MU0_4PI  = 1e-7
B_EQ_RE  = 3.12e-5                               # T  (DipoleInterface.h)
M_E      = B_EQ_RE * RE_M**3 / MU0_4PI
CS_GV    = MU0_4PI * M_E / RE_M**2 * C_LIGHT / 1e9   # ~59.6 GV·Re²

# ---------------------------------------------------------------------------
# Spectrum parameters  (must match #SPECTRUM block in .in file)
# ---------------------------------------------------------------------------
J0_SPEC  = 1.0e4       # m^-2 s^-1 sr^-1 MeV^-1
E0_SPEC  = 100.0       # MeV
GAMMA    = 2.0

# Inner loss sphere (must match R_INNER in .in file)
R_INNER_KM = 6434.9    # km = 1.01 Re

# Energy channels  (must mirror CH_BEGIN...CH_END in the .in file)
ENERGY_CHANNELS = [
    ("CH1",    10.0,    100.0, "10–100 MeV"),
    ("CH2",   100.0,   1000.0, "100 MeV–1 GeV"),
    ("CH3",  1000.0,  10000.0, "1–10 GeV"),
    ("CH4", 10000.0,  20000.0, "10–20 GeV"),
]

# Solver energy grid boundaries  (must match DS_EMIN / DS_EMAX in .in file)
EMIN_GRID = 10.0        # MeV
EMAX_GRID = 20000.0     # MeV

# ---------------------------------------------------------------------------
# Test case definitions
# ---------------------------------------------------------------------------
TEST_CASES = [
    {"label": "TC1 — 8Re equator", "x_km": 8*RE_KM, "y_km": 0, "z_km": 0,
     "Emin_test": 2000.0, "Emax_test": 20000.0, "r_Re": 8.0, "lam_deg": 0.0,
     "test_type": "exact"},
    {"label": "TC2 — 5Re pole (Rc=0)", "x_km": 0, "y_km": 0, "z_km": 5*RE_KM,
     "Emin_test": 50.0, "Emax_test": 500.0, "r_Re": 5.0, "lam_deg": 90.0,
     "test_type": "exact"},
    {"label": "TC3 — 6Re equator", "x_km": 6*RE_KM, "y_km": 0, "z_km": 0,
     "Emin_test": 3000.0, "Emax_test": 30000.0, "r_Re": 6.0, "lam_deg": 0.0,
     "test_type": "exact"},
    {"label": "TC4 — 2Re equator (shielded)", "x_km": 2*RE_KM, "y_km": 0, "z_km": 0,
     "Emin_test": 10.0, "Emax_test": 500.0, "r_Re": 2.0, "lam_deg": 0.0,
     "test_type": "upper_bound"},
]

# =============================================================================
# SECTION 2 ANALYTIC HELPERS
# =============================================================================

def v_from_E_MeV(E_MeV):
    """Relativistic proton speed [m/s]."""
    g = 1.0 + np.asarray(E_MeV) / MP_MEV
    return C_LIGHT * np.sqrt(np.maximum(0.0, 1.0 - 1.0 / g**2))

def Jb(E_MeV):
    """Boundary intensity [m^-2 s^-1 sr^-1 MeV^-1]."""
    return J0_SPEC * (np.asarray(E_MeV) / E0_SPEC) ** (-GAMMA)

def T_geometric(r_Re, r_inner_km=R_INNER_KM):
    """T_geo from inner-sphere shadow (§2.1)."""
    sin_a = r_inner_km / (r_Re * RE_KM)
    if sin_a >= 1.0: return 0.0
    return (1.0 + math.sqrt(1.0 - sin_a**2)) / 2.0

def Rc_max_GV(r_Re, lam_deg):
    """Størmer cutoff [GV] (§2.2)."""
    return CS_GV * math.cos(math.radians(lam_deg))**4 / r_Re**2

def E_kin_from_R_GV(R_GV):
    """Proton kinetic energy [MeV] from rigidity [GV]."""
    if R_GV <= 0.0: return 0.0
    pc = R_GV * 1e3    # MeV for Z=1
    return math.sqrt(pc**2 + MP_MEV**2) - MP_MEV

def n_analytic(Emin, Emax, T, N=4000):
    """
    Number density [m^-3] via log-spaced numerical quadrature (§2.3).
      n = 4π·T·∫ J_b(E)/v(E) dE
    No closed form for relativistic v(E).
    """
    Es = np.logspace(np.log10(Emin), np.log10(Emax), N)
    return 4.0 * math.pi * T * float(np.trapezoid(Jb(Es) / v_from_E_MeV(Es), Es))

def F_analytic_closed(E1, E2, T):
    """
    Exact closed-form flux [m^-2 s^-1] for power-law spectrum (§2.4).

    γ ≠ 1: F = 4π·T·J0·E0^γ/(γ−1)·(E1^{1−γ} − E2^{1−γ})
    γ = 2: F = 4π·T·J0·E0²·(1/E1 − 1/E2)
    """
    if abs(GAMMA - 1.0) < 1e-10:
        I = J0_SPEC * E0_SPEC * math.log(E2 / E1)
    else:
        I = (J0_SPEC * E0_SPEC**GAMMA / (GAMMA - 1.0)
             * (E1**(1.0 - GAMMA) - E2**(1.0 - GAMMA)))
    return 4.0 * math.pi * T * I

def F_analytic_numeric(E1, E2, T, N=4000):
    """Numeric flux for closed-form cross-check."""
    Es = np.logspace(np.log10(E1), np.log10(E2), N)
    return 4.0 * math.pi * T * float(np.trapezoid(Jb(Es), Es))

# =============================================================================
# SECTION 3 FILE READERS
# =============================================================================

def _skip(s):
    u = s.upper()
    return (not s or s.startswith('!') or s.startswith('#')
            or u.startswith('TITLE') or u.startswith('AUXDATA'))

def read_density_dat(path):
    records, vnames, in_zone = [], [], False
    with open(path) as fh:
        for line in fh:
            s = line.strip()
            if _skip(s): continue
            u = s.upper()
            if u.startswith('VARIABLES'):
                vnames = [v.strip().strip('"') for v in s.split('=',1)[1].split()]
            elif u.startswith('ZONE'):
                in_zone = True
            elif in_zone:
                try:
                    vals = [float(v) for v in s.split()]
                    if len(vals) == len(vnames):
                        records.append(dict(zip(vnames, vals)))
                except ValueError:
                    pass
    return records

def read_spectrum_dat(path):
    """Returns list of per-point dicts, keys: E_MeV, T, J_boundary_perMeV, J_local_perMeV."""
    points, cur, vnames = [], None, []
    with open(path) as fh:
        for line in fh:
            s = line.strip()
            if _skip(s): continue
            u = s.upper()
            if u.startswith('VARIABLES'):
                vnames = [v.strip().strip('"') for v in s.split('=',1)[1].split()]
            elif u.startswith('ZONE'):
                if cur: points.append({k: np.array(v) for k,v in cur.items()})
                cur = {v:[] for v in vnames}
            elif cur is not None:
                try:
                    vals = [float(v) for v in s.split()]
                    if len(vals) == len(vnames):
                        for v,x in zip(vnames, vals): cur[v].append(x)
                except ValueError:
                    pass
    if cur: points.append({k: np.array(v) for k,v in cur.items()})
    return points

def read_flux_dat(path):
    records, vnames, in_zone = [], [], False
    with open(path) as fh:
        for line in fh:
            s = line.strip()
            if _skip(s): continue
            u = s.upper()
            if u.startswith('VARIABLES'):
                vnames = [v.strip().strip('"') for v in s.split('=',1)[1].split()]
            elif u.startswith('ZONE'):
                in_zone = True
            elif in_zone:
                try:
                    vals = [float(v) for v in s.split()]
                    if len(vals) == len(vnames):
                        records.append(dict(zip(vnames, vals)))
                except ValueError:
                    pass
    return records

# =============================================================================
# SECTION 4 DENSITY COMPARISON
# =============================================================================

def compare_density(tc, spectrum_zone, tol, verbose):
    """Compare number density against T_geo analytic prediction."""
    r, lam  = tc["r_Re"], tc["lam_deg"]
    E1, E2  = tc["Emin_test"], tc["Emax_test"]
    Tg      = T_geometric(r)
    na      = n_analytic(E1, E2, Tg)
    n_free  = n_analytic(E1, E2, 1.0)
    Rc      = Rc_max_GV(r, lam)
    Ec      = E_kin_from_R_GV(Rc)

    E_arr   = spectrum_zone.get("E_MeV", np.array([]))
    T_arr   = spectrum_zone.get("T",     np.array([]))
    if E_arr.size < 2:
        return {"passed": False, "ratio": float("nan"),
                "n_model": float("nan"), "n_analytic": na, "T_geo": Tg}

    mask  = (E_arr >= E1) & (E_arr <= E2)
    if mask.sum() < 2:
        return {"passed": False, "ratio": float("nan"),
                "n_model": float("nan"), "n_analytic": na, "T_geo": Tg}

    Es, Ts = E_arr[mask], T_arr[mask]
    nm = 4.0*math.pi * float(np.trapezoid(Ts * Jb(Es) / v_from_E_MeV(Es), Es))
    T_mean = float(np.mean(Ts))

    if tc["test_type"] == "exact":
        rel  = abs(nm - na) / na
        ok   = rel < tol
        verb = "PASS ✓" if ok else "FAIL ✗"
        if verbose:
            print(f"  [DENSITY] {tc['label']}")
            print(f"    T_geo={Tg:.5f}  Rc_max={Rc:.3f} GV → {Ec:.0f} MeV  "
                  f"range {E1:.0f}–{E2:.0f} MeV ({mask.sum()} bins)")
            print(f"    n_analytic={na:.4e}  n_model={nm:.4e}  "
                  f"rel={rel:.2%}  T_mean={T_mean:.5f}  {verb}")
    else:
        upper = 0.5 * na
        ok    = nm < upper
        rel   = nm / na
        verb  = "PASS ✓" if ok else "FAIL ✗"
        if verbose:
            print(f"  [DENSITY] {tc['label']} (upper-bound test)")
            print(f"    Rc_max={Rc:.3f} GV → {Ec:.0f} MeV  T_geo={Tg:.5f}")
            print(f"    n_Tgeo={na:.4e}  50%bound={upper:.4e}  "
                  f"n_model={nm:.4e}  ratio={rel:.3f}  {verb}")

    return {"passed": ok, "ratio": nm/na, "n_model": nm, "n_analytic": na,
            "T_geo": Tg, "T_mean_model": T_mean, "E_sub": Es, "T_sub": Ts,
            "rel_err": abs(nm-na)/na if na>0 else float("nan")}

# =============================================================================
# SECTION 5 FLUX COMPARISONS  (new)
# =============================================================================

def _interp_T(E_grid, T_grid, x):
    """Linear interpolation of T at energy x on the solver grid."""
    idx = int(np.searchsorted(E_grid, x))
    if idx == 0:           return float(T_grid[0])
    if idx >= len(E_grid): return float(T_grid[-1])
    Ea, Eb = E_grid[idx-1], E_grid[idx]
    Ta, Tb = T_grid[idx-1], T_grid[idx]
    return float(Ta + (Tb-Ta)*(x-Ea)/(Eb-Ea)) if Eb > Ea else float(Ta)

def flux_from_spectrum(spectrum_zone, E1, E2):
    """
    Reconstruct channel flux from the spectrum zone (mirrors FluxIntegrateChannel).

    Algorithm (§3.3):
      1. Clip [E1,E2] to grid range.
      2. Build augmented sub-grid with linearly-interpolated T at boundaries.
      3. Trapezoid over J_loc = T·J_b.
    """
    E_arr = spectrum_zone.get("E_MeV", np.array([]))
    T_arr = spectrum_zone.get("T",     np.array([]))
    if E_arr.size < 2: return 0.0
    lo = max(E1, float(E_arr[0]))
    hi = min(E2, float(E_arr[-1]))
    if lo >= hi: return 0.0

    mask = (E_arr > lo) & (E_arr < hi)
    E_sub = np.concatenate([[lo], E_arr[mask], [hi]])
    T_sub = np.concatenate([[_interp_T(E_arr, T_arr, lo)],
                             T_arr[mask],
                             [_interp_T(E_arr, T_arr, hi)]])
    return 4.0 * math.pi * float(np.trapezoid(T_sub * Jb(E_sub), E_sub))


def compare_total_flux(tc, spectrum_zone, flux_rec, tol, verbose):
    """
    Compare total omnidirectional flux F_tot over [EMIN_GRID, EMAX_GRID].

    If EMIN_GRID is above the Størmer cutoff at this point: exact test.
    Otherwise: upper-bound test (F_model ≤ 1.05·F_analytic).
    """
    Tg   = T_geometric(tc["r_Re"])
    Rc   = Rc_max_GV(tc["r_Re"], tc["lam_deg"])
    Ec   = E_kin_from_R_GV(Rc)
    Fa   = F_analytic_closed(EMIN_GRID, EMAX_GRID, Tg)
    F_py = flux_from_spectrum(spectrum_zone, EMIN_GRID, EMAX_GRID)
    F_sv = (flux_rec.get("F_tot_m2s1") if flux_rec else None)

    fully_above = (EMIN_GRID > Ec)
    if fully_above:
        rel   = abs(F_py - Fa) / Fa
        ok    = rel < tol
        crit  = f"|rel|<{tol:.0%}"
    else:
        rel   = F_py / Fa if Fa > 0 else float("nan")
        ok    = (F_py <= 1.05 * Fa)
        crit  = "≤ F_analytic"
    verb = "PASS ✓" if ok else "FAIL ✗"

    if verbose:
        tag = "(fully above cutoff)" if fully_above else "(straddles cutoff)"
        print(f"  [TOTAL FLUX] {tc['label']} {tag}")
        print(f"    F_analytic(closed)={Fa:.4e}  F_model={F_py:.4e}  "
              f"ratio={rel:.4f}  criterion:{crit}  {verb}")
        if F_sv is not None:
            cons = abs(F_sv - F_py) / max(abs(F_py), 1e-30)
            print(f"    F_solver(file)    ={F_sv:.4e}  consistency={cons:.5%}")

    return {"passed": ok, "F_analytic": Fa, "F_py": F_py,
            "F_solver": F_sv, "rel_err": rel, "T_geo": Tg}


def compare_flux_channels(tc, spectrum_zone, flux_rec, tol, verbose):
    """
    Compare per-channel integral flux against the analytic closed form (§2.4).

    For channels fully above the Størmer cutoff: exact test (|rel| < tol).
    For channels that straddle or lie below the cutoff: upper-bound test.

    Also verifies that the solver-written F_ch (from gridless_points_flux.dat)
    agrees with the Python reconstruction to < 0.1% (consistency check).
    """
    Tg  = T_geometric(tc["r_Re"])
    Rc  = Rc_max_GV(tc["r_Re"], tc["lam_deg"])
    Ec  = E_kin_from_R_GV(Rc)

    if verbose:
        print(f"  [FLUX CHANNELS] {tc['label']}  "
              f"T_geo={Tg:.5f}  Rc_max={Rc:.3f} GV → Ec={Ec:.0f} MeV")

    ch_res   = {}
    all_pass = True

    for name, E1, E2, desc in ENERGY_CHANNELS:
        Fa   = F_analytic_closed(E1, E2, Tg)
        F_py = flux_from_spectrum(spectrum_zone, E1, E2)
        col  = f"F_{name}_m2s1"
        F_sv = (flux_rec.get(col) if flux_rec else None)

        fully_above = (E1 > Ec)
        if fully_above:
            rel  = abs(F_py - Fa) / Fa if Fa > 0 else float("nan")
            ok   = rel < tol
            crit = f"|rel|<{tol:.0%}"
        else:
            rel  = F_py / Fa if Fa > 0 else float("nan")
            ok   = (F_py <= 1.05 * Fa)
            crit = "≤ F_analytic"

        # Consistency: solver file vs Python  (< 0.1%)
        if F_sv is not None:
            cons = abs(F_sv - F_py) / max(abs(F_py), 1e-30)
            ok   = ok and (cons < 0.001)
        else:
            cons = float("nan")

        all_pass = all_pass and ok
        verb = "PASS ✓" if ok else "FAIL ✗"
        tag  = "above" if fully_above else "below/straddle"

        if verbose:
            line = (f"    {name} [{E1:.0f}–{E2:.0f} MeV] ({tag} Ec)  "
                    f"F_a={Fa:.4e}  F_py={F_py:.4e}  ratio={rel:.4f}  {crit}  {verb}")
            if F_sv is not None:
                line += f"  [file cons={cons:.5%}]"
            print(line)

        ch_res[name] = {"passed": ok, "F_analytic": Fa, "F_py": F_py,
                        "F_solver": F_sv, "rel_err_py": rel,
                        "fully_above_cutoff": fully_above,
                        "E1_MeV": E1, "E2_MeV": E2}

    return {"all_passed": all_pass, "channels": ch_res, "T_geo": Tg}

# =============================================================================
# SECTION 6 SUMMARY PRINTERS
# =============================================================================

def print_header():
    W = 76
    print("=" * W)
    print("AMPS GRIDLESS  DENSITY / SPECTRUM / FLUX  —  ANALYTIC TEST SUITE")
    print("Magnetic field: centred DIPOLE  |  Species: proton  (Z=1, A≈1)")
    print("=" * W)
    print(f"  mp·c²      = {MP_MEV:.3f} MeV")
    print(f"  B_eq(Re)   = {B_EQ_RE:.3e} T")
    print(f"  Cs         = {CS_GV:.3f} GV·Re²  (Størmer constant)")
    print(f"  r_inner    = {R_INNER_KM:.1f} km = {R_INNER_KM/RE_KM:.4f} Re")
    print(f"  J_b(E)     = {J0_SPEC:.1e} · (E/{E0_SPEC:.0f} MeV)^(−{GAMMA:.1f})  "
          f"[m^-2 s^-1 sr^-1 MeV^-1]")
    print()
    print("  Closed-form flux: F(E1,E2) = 4π·T·J0·E0²·(1/E1−1/E2)  [γ=2]")
    print()
    print("  Cross-check closed form vs numeric quadrature (T=1):")
    print(f"  {'Ch':4} {'E1':>8} {'E2':>8} {'F_closed':>14} {'F_numeric':>14} {'err':>8}")
    for n, E1, E2, _ in ENERGY_CHANNELS:
        Fc = F_analytic_closed(E1, E2, 1.0)
        Fn = F_analytic_numeric(E1, E2, 1.0)
        err = abs(Fc-Fn)/Fc if Fc>0 else float("nan")
        print(f"  {n:4} {E1:>8.0f} {E2:>8.0f} {Fc:>14.6e} {Fn:>14.6e} {err:>7.5%}")
    print()

def print_cutoff_table():
    print("  Størmer cutoff table:")
    print(f"  {'Point':12} {'r [Re]':>7} {'λ [°]':>6} "
          f"{'Rc_max [GV]':>12} {'E_cut [MeV]':>12} {'T_geo':>8}")
    for tc in TEST_CASES:
        Rc  = Rc_max_GV(tc["r_Re"], tc["lam_deg"])
        Ec  = E_kin_from_R_GV(Rc)
        Tg  = T_geometric(tc["r_Re"])
        print(f"  {tc['label'][:12]:12} {tc['r_Re']:>7.1f} {tc['lam_deg']:>6.0f} "
              f"{Rc:>12.3f} {Ec:>12.0f} {Tg:>8.5f}")
    print()

def print_analytic_flux_table():
    print("  Analytic flux [m^-2 s^-1]  (closed form, T=T_geo):")
    hdr = f"  {'Channel':6} {'E1 MeV':>8} {'E2 MeV':>8}"
    for tc in TEST_CASES:
        hdr += f"  {tc['label'][:10]:>13}"
    print(hdr)
    for n, E1, E2, _ in ENERGY_CHANNELS:
        row = f"  {n:6} {E1:>8.0f} {E2:>8.0f}"
        for tc in TEST_CASES:
            row += f"  {F_analytic_closed(E1,E2,T_geometric(tc['r_Re'])):>13.4e}"
        print(row)
    row = f"  {'TOTAL':6} {EMIN_GRID:>8.0f} {EMAX_GRID:>8.0f}"
    for tc in TEST_CASES:
        row += f"  {F_analytic_closed(EMIN_GRID,EMAX_GRID,T_geometric(tc['r_Re'])):>13.4e}"
    print(row)
    print()

def print_summary(d_res, f_res, fch_res):
    W = 76
    print(); print("=" * W); print("FINAL SUMMARY"); print("=" * W)

    # --- density ---
    print("\n  DENSITY [m^-3]")
    print(f"  {'Case':<34} {'n_model':>12} {'n_analytic':>12} {'ratio':>8} {'Result':>8}")
    print("  " + "-" * 70)
    nd = sum(r["passed"] for r in d_res)
    for tc, r in zip(TEST_CASES, d_res):
        nm, na, rat, ok = (r.get("n_model", float("nan")),
                           r.get("n_analytic", float("nan")),
                           r.get("ratio", float("nan")),
                           r.get("passed", False))
        print(f"  {tc['label'][:34]:<34} {nm:>12.3e} {na:>12.3e} "
              f"{rat:>8.4f} {'PASS ✓' if ok else 'FAIL ✗':>8}")
    print(f"  Density: {nd}/{len(d_res)} passed")

    # --- total flux ---
    print("\n  TOTAL FLUX [m^-2 s^-1]")
    print(f"  {'Case':<34} {'F_model':>14} {'F_analytic':>14} {'ratio':>8} {'Result':>8}")
    print("  " + "-" * 74)
    nf = sum(r["passed"] for r in f_res)
    for tc, r in zip(TEST_CASES, f_res):
        Fm, Fa, ok = (r.get("F_py", float("nan")),
                      r.get("F_analytic", float("nan")),
                      r.get("passed", False))
        rat = Fm/Fa if Fa>0 else float("nan")
        print(f"  {tc['label'][:34]:<34} {Fm:>14.4e} {Fa:>14.4e} "
              f"{rat:>8.4f} {'PASS ✓' if ok else 'FAIL ✗':>8}")
    print(f"  Total flux: {nf}/{len(f_res)} passed")

    # --- per-channel ---
    print("\n  PER-CHANNEL FLUX  (PASS ✓ / FAIL ✗)")
    ch_names = [c[0] for c in ENERGY_CHANNELS]
    hdr = f"  {'Case':<34}"
    for cn in ch_names: hdr += f" {cn:>9}"
    print(hdr); print("  " + "-" * (34 + 10*len(ch_names)))
    nch_pass = nch_tot = 0
    for tc, fcr in zip(TEST_CASES, fch_res):
        row = f"  {tc['label'][:34]:<34}"
        for cn in ch_names:
            ok = fcr["channels"].get(cn, {}).get("passed", False)
            nch_pass += int(ok); nch_tot += 1
            row += f" {'PASS ✓' if ok else 'FAIL ✗':>9}"
        print(row)
    print(f"  Channel flux: {nch_pass}/{nch_tot} passed")

    grand     = nd + nf + nch_pass
    grand_tot = len(d_res) + len(f_res) + nch_tot
    print()
    print("=" * W)
    if grand == grand_tot:
        print(f"ALL {grand_tot} TESTS PASSED")
    else:
        print(f"PASSED {grand}/{grand_tot}  "
              f"({grand_tot-grand} FAILURE{'S' if grand_tot-grand>1 else ''})")
    print("=" * W)
    return grand_tot - grand

# =============================================================================
# SECTION 7 PLOTS
# =============================================================================

def make_plots(d_res, f_res, fch_res, out_dir="."):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("[plots] matplotlib not available — skipping"); return

    COLORS = ["#1f77b4", "#2ca02c", "#ff7f0e", "#d62728"]

    # ── Figure 1: T(E) per test case ──────────────────────────────────────
    fig, axes = plt.subplots(2, 2, figsize=(13, 9))
    axes = axes.ravel()
    for i, (tc, dr, ax) in enumerate(zip(TEST_CASES, d_res, axes)):
        Es, Ts = dr.get("E_sub", np.array([])), dr.get("T_sub", np.array([]))
        Tg     = dr.get("T_geo", float("nan"))
        if Es.size > 0:
            ax.semilogx(Es, Ts, "o-", color=COLORS[i], ms=3, lw=1.5, label="AMPS T(E)")
        ax.axhline(Tg, color="k", ls="--", lw=1.5, label=f"T_geo={Tg:.4f}")
        Rc = Rc_max_GV(tc["r_Re"], tc["lam_deg"])
        if Rc > 0:
            Ec = E_kin_from_R_GV(Rc)
            if EMIN_GRID < Ec < EMAX_GRID:
                ax.axvline(Ec, color="purple", ls="-.", lw=1.0, alpha=0.7,
                           label=f"Rc={Rc:.2f} GV")
        for j, (cn, E1, E2, _) in enumerate(ENERGY_CHANNELS):
            ax.axvspan(E1, E2, alpha=0.07, color=plt.cm.tab10(j/10), label=cn)
        verdict = "PASS ✓" if dr["passed"] else "FAIL ✗"
        ax.set_title(f"TC{i+1}: {verdict}  r={tc['r_Re']:.0f}Re λ={tc['lam_deg']:.0f}°",
                     fontsize=9)
        ax.set_xlabel("E [MeV]"); ax.set_ylabel("T"); ax.set_ylim(-0.05, 1.15)
        ax.legend(fontsize=6, loc="lower right"); ax.grid(True, alpha=0.3)
    fig.suptitle("T(E): solver vs analytic (DIPOLE)", fontsize=11)
    plt.tight_layout()
    p1 = os.path.join(out_dir, "test_density_T_comparison.png")
    plt.savefig(p1, dpi=150, bbox_inches="tight"); print(f"[plots] {p1}"); plt.close(fig)

    # ── Figure 2: density + total flux bar chart ───────────────────────────
    fig2, (ax_n, ax_F) = plt.subplots(1, 2, figsize=(13, 5))
    x = np.arange(len(TEST_CASES)); w = 0.35
    ax_n.bar(x-w/2, [r.get("n_model",0) for r in d_res], w, label="model",
             color=COLORS[0])
    ax_n.bar(x+w/2, [r.get("n_analytic",0) for r in d_res], w, label="analytic",
             color=COLORS[2], alpha=0.7)
    ax_n.set_yscale("log"); ax_n.set_xticks(x)
    ax_n.set_xticklabels([f"TC{i+1}" for i in range(len(TEST_CASES))])
    ax_n.set_ylabel("n  [m⁻³]"); ax_n.set_title("Number density"); ax_n.legend()
    ax_n.grid(True, alpha=0.3, axis="y")

    ax_F.bar(x-w/2, [r.get("F_py",0) for r in f_res], w, label="model",
             color=COLORS[1])
    ax_F.bar(x+w/2, [r.get("F_analytic",0) for r in f_res], w, label="analytic",
             color=COLORS[3], alpha=0.7)
    ax_F.set_yscale("log"); ax_F.set_xticks(x)
    ax_F.set_xticklabels([f"TC{i+1}" for i in range(len(TEST_CASES))])
    ax_F.set_ylabel("F_tot  [m⁻² s⁻¹]"); ax_F.set_title("Total flux"); ax_F.legend()
    ax_F.grid(True, alpha=0.3, axis="y")

    fig2.suptitle("Density and total flux: model vs analytic (DIPOLE)", fontsize=11)
    plt.tight_layout()
    p2 = os.path.join(out_dir, "test_density_n_comparison.png")
    plt.savefig(p2, dpi=150, bbox_inches="tight"); print(f"[plots] {p2}"); plt.close(fig2)

    # ── Figure 3: per-channel flux bar chart (one panel per TC) ───────────
    nTC = len(TEST_CASES)
    fig3, axes3 = plt.subplots(1, nTC, figsize=(4*nTC, 5))
    if nTC == 1: axes3 = [axes3]
    ch_names = [c[0] for c in ENERGY_CHANNELS]
    xch = np.arange(len(ch_names)); wch = 0.35
    for i, (tc, fcr, ax) in enumerate(zip(TEST_CASES, fch_res, axes3)):
        F_m = [fcr["channels"].get(cn,{}).get("F_py",0)       for cn in ch_names]
        F_a = [fcr["channels"].get(cn,{}).get("F_analytic",0) for cn in ch_names]
        ax.bar(xch-wch/2, F_m, wch, label="model",    color=COLORS[i])
        ax.bar(xch+wch/2, F_a, wch, label="analytic", color="gray", alpha=0.6)
        ax.set_yscale("log"); ax.set_xticks(xch)
        ax.set_xticklabels(ch_names, fontsize=8)
        if i == 0: ax.set_ylabel("F_ch  [m⁻² s⁻¹]")
        ok = "PASS ✓" if fcr["all_passed"] else "FAIL ✗"
        ax.set_title(f"TC{i+1}  r={tc['r_Re']:.0f}Re\n{ok}", fontsize=9)
        ax.legend(fontsize=7); ax.grid(True, alpha=0.3, axis="y")
    fig3.suptitle("Per-channel integral flux: model vs analytic (DIPOLE)", fontsize=11)
    plt.tight_layout()
    p3 = os.path.join(out_dir, "test_flux_channels.png")
    plt.savefig(p3, dpi=150, bbox_inches="tight"); print(f"[plots] {p3}"); plt.close(fig3)

    # ── Figure 4: relative flux error for above-cutoff channels ───────────
    fig4, ax4 = plt.subplots(figsize=(10, 4))
    for i, (tc, fcr) in enumerate(zip(TEST_CASES, fch_res)):
        pts = [(j, cr["rel_err_py"]*100)
               for j, (cn, _, _, _) in enumerate(ENERGY_CHANNELS)
               if (cr := fcr["channels"].get(cn, {})) and
               cr.get("fully_above_cutoff", False) and
               math.isfinite(cr.get("rel_err_py", float("nan")))]
        if pts:
            js, errs = zip(*pts)
            ax4.plot(js, errs, "o-", color=COLORS[i], label=f"TC{i+1}")
            for j, e in pts:
                ax4.annotate(ENERGY_CHANNELS[j][0], (j, e),
                             textcoords="offset points", xytext=(2,3), fontsize=7)
    ax4.axhline(5.0, color="red",  ls="--", lw=1.0, label="5% tol")
    ax4.axhline(2.0, color="gray", ls=":",  lw=1.0, label="2%")
    ax4.set_ylabel("|F_model − F_analytic| / F_analytic  [%]")
    ax4.set_title("Relative flux error — channels fully above Størmer cutoff")
    ax4.set_xticks(range(len(ENERGY_CHANNELS)))
    ax4.set_xticklabels([c[0] for c in ENERGY_CHANNELS])
    ax4.legend(fontsize=8); ax4.grid(True, alpha=0.3)
    plt.tight_layout()
    p4 = os.path.join(out_dir, "test_flux_relative_error.png")
    plt.savefig(p4, dpi=150, bbox_inches="tight"); print(f"[plots] {p4}"); plt.close(fig4)

# =============================================================================
# MAIN
# =============================================================================

def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir",      default=".",
                    help="Directory with AMPS output files  (default: '.')")
    ap.add_argument("--tol",      type=float, default=0.05,
                    help="Relative tolerance for exact tests  (default: 0.05)")
    ap.add_argument("--no-plots", action="store_true")
    ap.add_argument("-v", "--verbose", action="store_true", default=True)
    args = ap.parse_args()

    d = args.dir
    f_density  = os.path.join(d, "gridless_points_density.dat")
    f_spectrum = os.path.join(d, "gridless_points_spectrum.dat")
    f_flux     = os.path.join(d, "gridless_points_flux.dat")

    for fp, label in [(f_density, "density"), (f_spectrum, "spectrum")]:
        if not os.path.isfile(fp):
            print(f"ERROR: {label} file not found: {fp}\n"
                  "       Run AMPS with test_density_dipole_4pts.in first.")
            sys.exit(1)

    flux_present = os.path.isfile(f_flux)
    if not flux_present:
        print(f"NOTE: flux file not found ({f_flux})\n"
              "      Solver-file consistency check skipped; "
              "Python flux reconstruction will still run.")

    density_rows   = read_density_dat(f_density)
    spectrum_zones = read_spectrum_dat(f_spectrum)
    flux_rows      = read_flux_dat(f_flux) if flux_present else []

    print_header()
    print_cutoff_table()
    print_analytic_flux_table()

    print("=" * 76)
    print("DETAILED RESULTS")
    print("=" * 76)

    d_res = []; f_res = []; fch_res = []

    for i, tc in enumerate(TEST_CASES):
        print(f"\n{'─'*70}\n  {tc['label']}\n{'─'*70}")
        sz = spectrum_zones[i] if i < len(spectrum_zones) else {}
        fr = flux_rows[i]      if i < len(flux_rows)      else None

        d_res.append(  compare_density(tc, sz, args.tol, args.verbose))
        print()
        f_res.append(  compare_total_flux(tc, sz, fr, args.tol, args.verbose))
        print()
        fch_res.append(compare_flux_channels(tc, sz, fr, args.tol, args.verbose))

    n_fail = print_summary(d_res, f_res, fch_res)

    if not args.no_plots:
        make_plots(d_res, f_res, fch_res, out_dir=d)

    sys.exit(min(n_fail, 1))

if __name__ == "__main__":
    main()
