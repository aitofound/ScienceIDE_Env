#!/usr/bin/env python3
"""
test_density_aniso.py
=====================
Compare AMPS gridless anisotropic density/spectrum output against analytic or
semi-analytic predictions, for four test configurations:

  RUN A — Regression (ISOTROPIC PAD):
    n_aniso must equal n_iso to within sampling noise (~1%).

  RUN B — DAYSIDE_NIGHTSIDE spatial asymmetry:
    Observation point on Y-axis (dusk). By symmetry:
      n_aniso / n_iso = (f_dayside + f_nightside) / 2  [EXACT ANALYTIC]

  RUN C — SINALPHA_N n=2 PAD (perpendicular / pancake):
    Semi-analytic reference via straight-line trajectory + dipole B at exit.

  RUN D — COSALPHA_N n=2 PAD (field-aligned / SEP beam):
    Semi-analytic reference, same method.

USAGE
-----
  Run AMPS four times, then:

    python3 test_density_aniso.py \\
        --dir-regression  <path_to_run_A_output> \\
        --dir-spatial     <path_to_run_B_output> \\
        --dir-sinalpha    <path_to_run_C_output> \\
        --dir-cosalpha    <path_to_run_D_output> \\
        [--dir-isotropic  <path_to_isotropic_baseline>]  \\
        [--tol-exact   0.05]  \\
        [--tol-semianalytic 0.10]  \\
        [--no-plots]

  If --dir-isotropic is provided, the isotropic n values from TC1/TC3 are used
  as the denominator for RUN A; otherwise the analytic T_geo formula is used.

ANALYTIC BACKGROUND
-------------------
The solver computes for each allowed trajectory k at (x0, E):

  weight_k = f_PAD(cos_alpha_k) * f_spatial(x_exit_k)
  cos_alpha_k = v_exit_k . B_hat(x_exit_k)           [pitch angle at exit]

  T_aniso(E; x0) = (1/N_dirs) * sum_k A_k * weight_k
  J_loc(E; x0)   = J_b_iso(E) * T_aniso(E; x0)
  n_tot(x0)      = 4pi * integral T_aniso(E) * J_b(E) / v(E) dE

For locations where ALL energies are above Rc_max (so A_k depends only on the
geometric shadow), the expected ratio is:

  n_aniso / n_iso = <f_PAD * f_spatial>_allowed
                  = (1 / N_allowed) * sum_{k: A_k=1} f_PAD(cos_alpha_k) * f_spatial(x_exit_k)

This is computed by the semi_analytic_ratio() function below using:
  - straight-line ray tracing to find x_exit (valid when rL >> domain)
  - exact dipole B field at x_exit
  - exact f_PAD formula

STRAIGHT-LINE APPROXIMATION VALIDITY
--------------------------------------
The approximation requires the Larmor radius rL >> domain size (9 Re):

  Location       E_min   rL_min   Domain   Valid?
  8Re equator    5 GeV    50 Re    9 Re     YES
  8Re pole       5 GeV    25 Re    9 Re     YES
  6Re equator    5 GeV    21 Re    9 Re     YES (marginally)

All test energies are 5–50 GeV, so the approximation holds.
The reference for 6Re equator has ~5% systematic uncertainty.

TOLERANCE RATIONALE
-------------------
  RUN A (ISOTROPIC PAD regression): 2%
    Only statistical error from direction sampling (~1%), no physics approximation.
  RUN B (DAYSIDE/NIGHTSIDE spatial): 5%
    Analytic prediction is exact; 5% allows for directional-sampling statistics.
  RUN C/D (SINALPHA/COSALPHA PAD): 10%
    Straight-line approximation error ~5% (see above) + sampling error ~5%.
    6Re equator: tolerance is 15% because rL/domain ≈ 2.3 is marginal.
"""

import sys
import os
import math
import argparse
import numpy as np
from pathlib import Path

# ---------------------------------------------------------------------------
# Physical constants (must match DipoleInterface.h and DensityGridless.cpp)
# ---------------------------------------------------------------------------
C_LIGHT  = 299792458.0
QE       = 1.602176634e-19
AMU      = 1.66053906660e-27
MP_KG    = 1.007276466621 * AMU
MP_MEV   = MP_KG * C_LIGHT**2 / (1e6 * QE)   # ~938.272 MeV
RE_M     = 6371.2e3                            # m
RE_KM    = 6371.2                              # km
MU0_4PI  = 1e-7                               # T·m/A
B_EQ_RE  = 3.12e-5                            # T  (from DipoleInterface.h)
M_E      = B_EQ_RE * RE_M**3 / MU0_4PI        # A·m²
CS_GV    = MU0_4PI * M_E / RE_M**2 * C_LIGHT / 1e9  # 59.6 GV·Re²

# Domain and inner sphere (must match .in files)
BOX_RE       = 9.0
R_INNER_RE   = 1.01
R_INNER_KM   = R_INNER_RE * RE_KM

# Boundary spectrum (must match #SPECTRUM in all .in files)
J0_SPEC = 1.0e4   # m^-2 s^-1 sr^-1 MeV^-1
E0_SPEC = 100.0   # MeV
GAMMA   = 2.0

# Energy range used in all anisotropic .in files
EMIN_ANISO = 5000.0    # MeV
EMAX_ANISO = 50000.0   # MeV

# Energy channels (same definition as isotropic test; see #ENERGY_CHANNELS in .in files)
# Tuple: (name, E1_MeV, E2_MeV)
ANISO_CHANNELS = [
    ("CH1",    10.0,    100.0),
    ("CH2",   100.0,   1000.0),
    ("CH3",  1000.0,  10000.0),
    ("CH4", 10000.0,  20000.0),
]

# ---------------------------------------------------------------------------
# Test case definitions for PAD runs (A, C, D)
# ---------------------------------------------------------------------------
PAD_TEST_POINTS = [
    {
        "label":  "EQ8",
        "desc":   "8Re equator",
        "x_km":   8.0 * RE_KM,
        "y_km":   0.0,
        "z_km":   0.0,
        "r_Re":   8.0,
        "lam_deg": 0.0,
        "tol_pad": 0.10,
    },
    {
        "label":  "PL8",
        "desc":   "8Re magnetic pole",
        "x_km":   0.0,
        "y_km":   0.0,
        "z_km":   8.0 * RE_KM,
        "r_Re":   8.0,
        "lam_deg": 90.0,
        "tol_pad": 0.10,
    },
    {
        "label":  "EQ6",
        "desc":   "6Re equator",
        "x_km":   6.0 * RE_KM,
        "y_km":   0.0,
        "z_km":   0.0,
        "r_Re":   6.0,
        "lam_deg": 0.0,
        "tol_pad": 0.15,   # rL/domain ≈ 2.3 at 5 GeV, marginal approximation
    },
]

SPATIAL_TEST_POINT = {
    "label":  "DUSK",
    "desc":   "8Re dusk meridian",
    "x_km":   0.0,
    "y_km":   8.0 * RE_KM,
    "z_km":   0.0,
    "r_Re":   8.0,
    "f_day":  2.0,
    "f_night": 0.5,
}

# ---------------------------------------------------------------------------
# Kinematic helpers
# ---------------------------------------------------------------------------

def v_from_E_MeV(E_MeV: float) -> float:
    """Relativistic proton speed [m/s]."""
    g = 1.0 + E_MeV / MP_MEV
    return C_LIGHT * math.sqrt(max(0.0, 1.0 - 1.0/g**2))

def Jb(E_MeV: float) -> float:
    """Power-law boundary spectrum [m^-2 s^-1 sr^-1 MeV^-1]."""
    return J0_SPEC * (E_MeV / E0_SPEC) ** (-GAMMA)

def T_geometric(r_Re: float) -> float:
    """Geometric transmissivity (shadow of inner sphere only)."""
    s = R_INNER_RE / r_Re
    return (1.0 + math.sqrt(max(0.0, 1.0 - s**2))) / 2.0

def Rc_max_GV(r_Re: float, lam_deg: float) -> float:
    c = math.cos(math.radians(lam_deg))
    return CS_GV * c**4 / r_Re**2

def n_from_T(T_of_E_fn, Emin=EMIN_ANISO, Emax=EMAX_ANISO, N=2000) -> float:
    """
    n = 4pi * integral[Emin,Emax] T(E) * Jb(E) / v(E) dE
    T_of_E_fn: callable E_MeV -> float
    """
    Es = np.logspace(np.log10(Emin), np.log10(Emax), N)
    T_vals = np.array([T_of_E_fn(e) for e in Es])
    Jb_vals = J0_SPEC * (Es / E0_SPEC) ** (-GAMMA)
    vs = np.array([v_from_E_MeV(e) for e in Es])
    return 4.0 * math.pi * float(np.trapezoid(T_vals * Jb_vals / vs, Es))

def n_isotropic_analytic(r_Re: float, Emin=EMIN_ANISO, Emax=EMAX_ANISO) -> float:
    """n_iso = T_geo * 4pi * integral Jb/v dE  (above-cutoff limit)."""
    Tg = T_geometric(r_Re)
    return n_from_T(lambda E: Tg, Emin, Emax)

# ---------------------------------------------------------------------------
# Dipole field and ray geometry
# ---------------------------------------------------------------------------

def dipole_B(x_m: np.ndarray) -> np.ndarray:
    """Dipole B [T] at x_m [m], moment along +Z."""
    x, y, z = x_m
    r2 = float(x*x + y*y + z*z)
    if r2 < 1e-30:
        return np.zeros(3)
    r = math.sqrt(r2)
    r5 = r2*r2*r
    mdotr = M_E * z
    return np.array([
        MU0_4PI * 3.0 * x * mdotr / r5,
        MU0_4PI * 3.0 * y * mdotr / r5,
        MU0_4PI * (3.0 * z * mdotr / r5 - M_E / (r2*r)),
    ])

def dipole_Bhat(x_m: np.ndarray) -> np.ndarray:
    B = dipole_B(x_m)
    Bmag = np.linalg.norm(B)
    return B / Bmag if Bmag > 1e-30 else np.array([0.0, 0.0, 1.0])

def ray_box_exit(x0_m: np.ndarray, vhat: np.ndarray, box_m: float) -> np.ndarray:
    """Return exit point of ray x0 + t*vhat on the axis-aligned box ±box_m."""
    t_exit = np.inf
    for i in range(3):
        if abs(vhat[i]) > 1e-15:
            t1 = ( box_m - x0_m[i]) / vhat[i]
            t2 = (-box_m - x0_m[i]) / vhat[i]
            t_pos = max(t1, t2)
            if t_pos > 0:
                t_exit = min(t_exit, t_pos)
    return x0_m + t_exit * vhat

def hits_inner_sphere(x0_m: np.ndarray, vhat: np.ndarray, r_in_m: float) -> bool:
    """True if the forward ray intersects the inner sphere."""
    b = -float(np.dot(x0_m, vhat))
    if b < 0:
        return False
    d2 = float(np.dot(x0_m, x0_m)) - b*b
    if d2 > r_in_m**2:
        return False
    disc = r_in_m**2 - d2
    t_hit = b - math.sqrt(disc)
    return t_hit > 0.0

# ---------------------------------------------------------------------------
# PAD models
# ---------------------------------------------------------------------------

def f_PAD(cos_alpha: float, model: str, n_exp: float = 2.0) -> float:
    """Pitch-angle distribution weight (from AnisotropicSpectrum.cpp)."""
    if model == "ISOTROPIC":
        return 1.0
    if model == "SINALPHA_N":
        return max(0.0, 1.0 - cos_alpha**2) ** (n_exp / 2.0)
    if model == "COSALPHA_N":
        return abs(cos_alpha) ** n_exp
    if model == "BIDIRECTIONAL":
        return abs(cos_alpha) ** n_exp
    raise ValueError(f"Unknown PAD model: {model!r}")

def f_spatial(x_exit_m: np.ndarray, model: str,
              f_day: float = 1.0, f_night: float = 1.0) -> float:
    """Spatial boundary weight."""
    if model == "UNIFORM":
        return 1.0
    if model == "DAYSIDE_NIGHTSIDE":
        return f_day if x_exit_m[0] > 0 else f_night
    raise ValueError(f"Unknown spatial model: {model!r}")

# ---------------------------------------------------------------------------
# Semi-analytic reference: <f_PAD * f_spatial>_allowed
# ---------------------------------------------------------------------------

def semi_analytic_ratio(x0_Re, pad_model: str, pad_n: float,
                        spatial_model: str = "UNIFORM",
                        f_day: float = 1.0, f_night: float = 1.0,
                        N_theta: int = 200, N_phi: int = 400) -> dict:
    """
    Compute the reference ratio n_aniso/n_iso using straight-line trajectories
    and the exact dipole B field at the domain boundary exit point.

    Returns dict with keys:
      T_geo_ref    : geometric transmissivity (fraction of sky not blocked by Earth)
      T_aniso_ref  : anisotropy-weighted transmissivity
      ratio        : T_aniso_ref / T_geo_ref  =  n_aniso / n_iso

    Valid when Larmor radius >> domain size (see module docstring).
    """
    x0_m    = np.array(x0_Re, dtype=float) * RE_M
    r_in_m  = R_INNER_RE * RE_M
    box_m   = BOX_RE * RE_M

    n_total  = 0
    n_geo    = 0
    T_aniso_sum = 0.0

    cos_thetas = np.linspace(-1.0 + 1.0/N_theta, 1.0 - 1.0/N_theta, N_theta)
    phis       = np.linspace(0.0, 2*math.pi*(1.0 - 1.0/N_phi), N_phi)

    for ct in cos_thetas:
        st = math.sqrt(max(0.0, 1.0 - ct*ct))
        for phi in phis:
            vhat = np.array([st*math.cos(phi), st*math.sin(phi), ct])
            n_total += 1

            if hits_inner_sphere(x0_m, vhat, r_in_m):
                continue  # FORBIDDEN by inner sphere (Earth)

            n_geo += 1

            # Exit position on domain box
            x_exit_m = ray_box_exit(x0_m, vhat, box_m)

            # Pitch angle cosine at exit: cos(alpha) = v_hat . B_hat(x_exit)
            Bhat = dipole_Bhat(x_exit_m)
            cos_alpha = float(np.dot(vhat, Bhat))

            # Weights
            fp = f_PAD(cos_alpha, pad_model, pad_n)
            fs = f_spatial(x_exit_m, spatial_model, f_day, f_night)
            T_aniso_sum += fp * fs

    T_geo   = n_geo / n_total if n_total > 0 else 0.0
    T_aniso = T_aniso_sum / n_total if n_total > 0 else 0.0
    ratio   = T_aniso / T_geo if T_geo > 0 else float("nan")
    return {"T_geo_ref": T_geo, "T_aniso_ref": T_aniso, "ratio": ratio}

# ---------------------------------------------------------------------------
# File readers (shared with test_density_analytic.py format)
# ---------------------------------------------------------------------------

def read_spectrum_dat(filepath: str) -> list:
    """Parse gridless_points_spectrum.dat. Returns list of zone dicts."""
    zones = []
    current = None
    var_names = []

    with open(filepath) as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("!") or s.startswith("#"):
                continue
            if s.upper().startswith("VARIABLES"):
                parts = s.split("=", 1)[1]
                var_names = [v.strip().strip('"') for v in parts.split()]
                continue
            if s.upper().startswith("ZONE"):
                if current:
                    zones.append({k: np.array(v) for k, v in current.items()})
                current = {v: [] for v in var_names}
                continue
            if current is not None:
                try:
                    vals = [float(v) for v in s.split()]
                    if len(vals) == len(var_names):
                        for v, x in zip(var_names, vals):
                            current[v].append(x)
                except ValueError:
                    pass

    if current:
        zones.append({k: np.array(v) for k, v in current.items()})
    return zones


def read_density_dat(filepath: str) -> list:
    """Parse gridless_points_density.dat. Returns list of row dicts."""
    rows = []
    var_names = []
    in_zone = False
    with open(filepath) as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("!") or s.startswith("#"):
                continue
            if s.upper().startswith("VARIABLES"):
                parts = s.split("=", 1)[1]
                var_names = [v.strip().strip('"') for v in parts.split()]
                continue
            if s.upper().startswith("ZONE"):
                in_zone = True
                continue
            if in_zone:
                try:
                    vals = [float(v) for v in s.split()]
                    if len(vals) == len(var_names):
                        rows.append(dict(zip(var_names, vals)))
                except ValueError:
                    pass
    return rows


def compute_n_from_spectrum_zone(zone: dict,
                                  Emin: float = EMIN_ANISO,
                                  Emax: float = EMAX_ANISO) -> float:
    """
    Integrate n = 4pi * integral T(E)*Jb(E)/v(E) dE from a spectrum zone.
    Uses the T(E) column from the zone and the analytic Jb formula.
    """
    E_arr = zone.get("E_MeV", np.array([]))
    T_arr = zone.get("T", np.array([]))
    if E_arr.size == 0:
        return float("nan")
    mask = (E_arr >= Emin) & (E_arr <= Emax)
    if mask.sum() < 2:
        return float("nan")
    E_sub = E_arr[mask]
    T_sub = T_arr[mask]
    Jb_sub = J0_SPEC * (E_sub / E0_SPEC) ** (-GAMMA)
    vs_sub = np.array([v_from_E_MeV(e) for e in E_sub])
    return 4.0 * math.pi * float(np.trapezoid(T_sub * Jb_sub / vs_sub, E_sub))

# ---------------------------------------------------------------------------
# Flux helpers
# ---------------------------------------------------------------------------

def read_flux_dat(filepath: str) -> list:
    """
    Parse gridless_points_flux.dat.
    Returns a list of row dicts; each row has keys:
      X_km, Y_km, Z_km, F_tot_m2s1, [F_CH1_m2s1, F_CH2_m2s1, ...]
    """
    rows = []
    var_names = []
    in_zone = False
    with open(filepath) as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("!") or s.startswith("AUXDATA"):
                continue
            if s.upper().startswith("VARIABLES"):
                parts = s.split("=", 1)[1]
                var_names = [v.strip().strip('"') for v in parts.split()]
                continue
            if s.upper().startswith("ZONE"):
                in_zone = True
                continue
            if in_zone:
                try:
                    vals = [float(v) for v in s.split()]
                    if len(vals) == len(var_names):
                        rows.append(dict(zip(var_names, vals)))
                except ValueError:
                    pass
    return rows


def F_closed(E1_MeV: float, E2_MeV: float, T_eff: float) -> float:
    """
    Closed-form omnidirectional integral flux for a power-law spectrum
    J_b(E) = J0*(E/E0)^(-GAMMA) with GAMMA=2:

      F = 4π · T_eff · J0 · E0² · (1/E1 - 1/E2)     [m^-2 s^-1]
    """
    if E2_MeV <= E1_MeV or T_eff == 0.0:
        return 0.0
    return 4.0 * math.pi * T_eff * J0_SPEC * E0_SPEC**2 * (1.0/E1_MeV - 1.0/E2_MeV)


def F_closed_clipped(E1_MeV: float, E2_MeV: float, T_eff: float,
                     Emin_grid: float = EMIN_ANISO,
                     Emax_grid: float = EMAX_ANISO) -> float:
    """
    Expected channel flux as produced by FluxIntegrateChannel in DensityGridless.cpp.

    The C++ code clips the channel [E1, E2] to the solver energy grid [Emin_grid,
    Emax_grid] before integrating.  For channels entirely outside the grid the
    return value is 0.0.

    For the anisotropic test the grid is [5000, 50000] MeV, so:
      CH1 (10-100 MeV)   -> 0          (below grid)
      CH2 (100-1000 MeV) -> 0          (below grid)
      CH3 (1-10 GeV)     -> F([5000, 10000], T_eff)
      CH4 (10-20 GeV)    -> F([10000, 20000], T_eff)
    """
    lo = max(E1_MeV, Emin_grid)
    hi = min(E2_MeV, Emax_grid)
    if lo >= hi:
        return 0.0
    return F_closed(lo, hi, T_eff)


def compare_flux_channels_aniso(
        run_dir: str,
        run_label: str,
        T_aniso_ref: float,        # expected effective transmissivity
        tol: float = 0.05,
        verbose: bool = True) -> list:
    """
    Read gridless_points_flux.dat from run_dir and compare per-channel flux against
    the analytic closed-form.

    For the aniso test cases the solver grid is [5-50 GeV]:
      - CH1, CH2 are entirely below the grid -> C++ returns 0; we verify that.
      - CH3 (1-10 GeV) clips to [5-10 GeV]; CH4 (10-20 GeV) is fully in the grid.

    T_aniso_ref is the effective transmissivity (= T_aniso from semi_analytic_ratio,
    or T_geo for Run A regression).  Since the PAD/spatial weights don't depend on
    energy (rL >> domain), F_ch_aniso = F_closed_clipped(E1,E2, T_aniso_ref).

    Returns a list of result dicts, one per channel per test point.
    """
    flux_file = os.path.join(run_dir, "gridless_points_flux.dat")
    if not os.path.isfile(flux_file):
        if verbose:
            print(f"  [flux] {run_label}: gridless_points_flux.dat not found — skipping flux test")
        return []

    rows = read_flux_dat(flux_file)
    if not rows:
        if verbose:
            print(f"  [flux] {run_label}: no rows in flux file — skipping")
        return []

    results = []
    if verbose:
        print(f"  [flux] {run_label}: comparing {len(ANISO_CHANNELS)} channels "
              f"× {len(rows)} points  (T_aniso_ref={T_aniso_ref:.5f})")

    for row_idx, row in enumerate(rows):
        for ch_name, E1, E2 in ANISO_CHANNELS:
            col = f"F_{ch_name}_m2s1"
            if col not in row:
                continue
            F_model = row[col]
            F_ref   = F_closed_clipped(E1, E2, T_aniso_ref)

            if F_ref == 0.0:
                # Expect zero — check absolute value is negligible
                tiny = 1.0   # 1 m^-2 s^-1 threshold for "zero"
                passed = abs(F_model) < tiny
                rel_err = float("nan")
                status  = "PASS" if passed else "FAIL"
                if verbose:
                    print(f"    [{run_label} pt{row_idx} {ch_name}]  F_model={F_model:.3e}"
                          f"  F_ref=0 (below grid)  >>> {status} <<<")
            else:
                rel_err = abs(F_model - F_ref) / F_ref
                passed  = rel_err < tol
                status  = "PASS" if passed else "FAIL"
                if verbose:
                    print(f"    [{run_label} pt{row_idx} {ch_name}]  "
                          f"F_model={F_model:.4e}  F_ref={F_ref:.4e}  "
                          f"err={rel_err:.2%}  tol={tol:.0%}  >>> {status} <<<")

            results.append({
                "run":       run_label,
                "point_idx": row_idx,
                "channel":   ch_name,
                "E1":        E1,
                "E2":        E2,
                "F_model":   F_model,
                "F_ref":     F_ref,
                "rel_err":   rel_err,
                "passed":    passed,
                "is_zero_expected": F_ref == 0.0,
            })
    return results


# ---------------------------------------------------------------------------
# Individual test runners
# ---------------------------------------------------------------------------

def run_test_A_regression(run_dir: str, tol: float = 0.02) -> list:
    """
    RUN A: ISOTROPIC PAD regression.
    n_aniso should equal n_iso = T_geo * n_free within tol.
    """
    spec_file = os.path.join(run_dir, "gridless_points_spectrum.dat")
    if not os.path.isfile(spec_file):
        print(f"[RUN A] ERROR: file not found: {spec_file}")
        return []

    zones = read_spectrum_dat(spec_file)
    results = []

    for i, (tc, zone) in enumerate(zip(PAD_TEST_POINTS, zones)):
        n_model    = compute_n_from_spectrum_zone(zone)
        n_analytic = n_isotropic_analytic(tc["r_Re"])
        Tg         = T_geometric(tc["r_Re"])
        T_mean     = float(np.mean(zone.get("T", [float("nan")])))

        rel_err = abs(n_model - n_analytic) / n_analytic if n_analytic > 0 else float("nan")
        passed  = rel_err < tol
        status  = "PASS" if passed else "FAIL"

        print(f"  [A-{tc['label']}] {tc['desc']}")
        print(f"    n_analytic (T_geo×n_free) = {n_analytic:.4e} m^-3  (T_geo={Tg:.5f})")
        print(f"    n_model (ISOTROPIC PAD)   = {n_model:.4e} m^-3  (T_mean={T_mean:.5f})")
        print(f"    rel_err = {rel_err:.2%}  tol={tol:.0%}  >>> {status} <<<")
        print()

        results.append({"run": "A", "label": tc["label"], "passed": passed,
                        "n_model": n_model, "n_analytic": n_analytic,
                        "rel_err": rel_err, "T_mean": T_mean, "T_geo": Tg})
    return results


def run_test_A_flux(run_dir: str, tol: float = 0.02) -> list:
    """
    RUN A flux: ISOTROPIC PAD → T_aniso_ref = T_geo for each point.
    For CH3 and CH4 (in-grid channels) F_aniso must equal F_iso within tol.
    For CH1 and CH2 (below grid) the solver returns 0; verified as zero.
    """
    print("  [RUN A flux] Comparing per-channel flux against analytic reference ...")
    all_flux = []
    for i, tc in enumerate(PAD_TEST_POINTS):
        Tg = T_geometric(tc["r_Re"])
        flux_file = os.path.join(run_dir, "gridless_points_flux.dat")
        if not os.path.isfile(flux_file):
            print(f"    gridless_points_flux.dat not found in {run_dir}")
            return []
        rows = read_flux_dat(flux_file)
        if i >= len(rows):
            continue
        row = rows[i]
        for ch_name, E1, E2 in ANISO_CHANNELS:
            col = f"F_{ch_name}_m2s1"
            if col not in row:
                continue
            F_model = row[col]
            F_ref   = F_closed_clipped(E1, E2, Tg)
            if F_ref == 0.0:
                passed  = abs(F_model) < 1.0
                rel_err = float("nan")
            else:
                rel_err = abs(F_model - F_ref) / F_ref
                passed  = rel_err < tol
            st = "PASS" if passed else "FAIL"
            print(f"    [A-{tc['label']} {ch_name}]  "
                  + (f"F_model={F_model:.4e}  F_ref={F_ref:.4e}  err={rel_err:.2%}  >>> {st} <<<"
                     if F_ref > 0 else f"F_model={F_model:.3e}  F_ref=0 (below grid)  >>> {st} <<<"))
            all_flux.append({
                "run": "A-flux", "label": tc["label"], "channel": ch_name,
                "E1": E1, "E2": E2, "F_model": F_model, "F_ref": F_ref,
                "rel_err": rel_err, "passed": passed, "is_zero_expected": F_ref == 0.0,
            })
    print()
    return all_flux


def run_test_B_spatial(run_dir: str, tol: float = 0.05) -> list:
    """
    RUN B: DAYSIDE_NIGHTSIDE spatial.
    n_aniso / n_iso = (f_day + f_night) / 2  [EXACT by symmetry].
    """
    spec_file = os.path.join(run_dir, "gridless_points_spectrum.dat")
    if not os.path.isfile(spec_file):
        print(f"[RUN B] ERROR: file not found: {spec_file}")
        return []

    zones = read_spectrum_dat(spec_file)
    if not zones:
        print("[RUN B] ERROR: no zones found in spectrum file")
        return []

    tc = SPATIAL_TEST_POINT
    zone = zones[0]
    n_aniso_model = compute_n_from_spectrum_zone(zone)
    n_iso_analytic = n_isotropic_analytic(tc["r_Re"])   # T_geo * n_free (UNIFORM spatial)
    ratio_exact   = (tc["f_day"] + tc["f_night"]) / 2.0
    n_expected    = ratio_exact * n_iso_analytic
    T_mean        = float(np.mean(zone.get("T", [float("nan")])))

    rel_err = abs(n_aniso_model - n_expected) / n_expected if n_expected > 0 else float("nan")
    passed  = rel_err < tol
    status  = "PASS" if passed else "FAIL"

    print(f"  [B-{tc['label']}] {tc['desc']}")
    print(f"    Exact analytic ratio = (f_day+f_night)/2 = ({tc['f_day']}+{tc['f_night']})/2 = {ratio_exact:.4f}")
    print(f"    n_iso_analytic       = {n_iso_analytic:.4e} m^-3")
    print(f"    n_expected (aniso)   = {n_expected:.4e} m^-3  (= ratio × n_iso)")
    print(f"    n_model              = {n_aniso_model:.4e} m^-3")
    print(f"    rel_err = {rel_err:.2%}  tol={tol:.0%}  >>> {status} <<<")
    print()

    return [{"run": "B", "label": tc["label"], "passed": passed,
             "n_model": n_aniso_model, "n_analytic": n_expected,
             "rel_err": rel_err, "ratio_exact": ratio_exact,
             "n_iso_analytic": n_iso_analytic}]


def run_test_B_flux(run_dir: str, tol: float = 0.05) -> list:
    """
    RUN B flux: DAYSIDE_NIGHTSIDE spatial, DUSK point.
    Since PAD=ISOTROPIC and T is energy-independent, flux ratio = (f_day+f_night)/2
    exactly as for density.  T_aniso_ref = T_geo * ratio_exact.
    """
    print("  [RUN B flux] Comparing per-channel flux (dusk point) ...")
    tc   = SPATIAL_TEST_POINT
    Tg   = T_geometric(tc["r_Re"])
    ratio_exact   = (tc["f_day"] + tc["f_night"]) / 2.0
    T_aniso_ref   = Tg * ratio_exact

    flux_file = os.path.join(run_dir, "gridless_points_flux.dat")
    if not os.path.isfile(flux_file):
        print(f"    gridless_points_flux.dat not found in {run_dir}")
        return []
    rows = read_flux_dat(flux_file)
    if not rows:
        return []
    row = rows[0]

    all_flux = []
    for ch_name, E1, E2 in ANISO_CHANNELS:
        col = f"F_{ch_name}_m2s1"
        if col not in row:
            continue
        F_model = row[col]
        F_ref   = F_closed_clipped(E1, E2, T_aniso_ref)
        if F_ref == 0.0:
            passed  = abs(F_model) < 1.0
            rel_err = float("nan")
        else:
            rel_err = abs(F_model - F_ref) / F_ref
            passed  = rel_err < tol
        st = "PASS" if passed else "FAIL"
        print(f"    [B-DUSK {ch_name}]  "
              + (f"F_model={F_model:.4e}  F_ref={F_ref:.4e}  err={rel_err:.2%}  >>> {st} <<<"
                 if F_ref > 0 else f"F_model={F_model:.3e}  F_ref=0 (below grid)  >>> {st} <<<"))
        all_flux.append({
            "run": "B-flux", "label": "DUSK", "channel": ch_name,
            "E1": E1, "E2": E2, "F_model": F_model, "F_ref": F_ref,
            "rel_err": rel_err, "passed": passed, "is_zero_expected": F_ref == 0.0,
        })
    print()
    return all_flux


def run_test_PAD(run_dir: str, pad_model: str, pad_n: float,
                 tol_override: dict = None,
                 recompute_refs: bool = False,
                 N_theta: int = 200, N_phi: int = 400) -> list:
    """
    RUN C or D: PAD anisotropy (SINALPHA_N or COSALPHA_N).
    Compares n_model / n_iso_analytic to semi-analytic reference ratio.
    """
    run_label = "C" if "SIN" in pad_model else "D"
    spec_file = os.path.join(run_dir, "gridless_points_spectrum.dat")
    if not os.path.isfile(spec_file):
        print(f"[RUN {run_label}] ERROR: file not found: {spec_file}")
        return []

    zones = read_spectrum_dat(spec_file)
    results = []

    for i, (tc, zone) in enumerate(zip(PAD_TEST_POINTS, zones)):
        tol = (tol_override or {}).get(tc["label"], tc["tol_pad"])

        # Compute or look up semi-analytic reference
        print(f"  [{run_label}-{tc['label']}] {tc['desc']}  ({pad_model} n={pad_n})")
        print(f"    Computing semi-analytic reference (N={N_theta}×{N_phi}) ...", end=" ", flush=True)

        x0_Re = [tc["x_km"]/RE_KM, tc["y_km"]/RE_KM, tc["z_km"]/RE_KM]
        ref = semi_analytic_ratio(x0_Re, pad_model, pad_n,
                                   N_theta=N_theta, N_phi=N_phi)
        print(f"done.")

        ratio_ref   = ref["ratio"]
        Tg          = ref["T_geo_ref"]
        n_iso_ref   = n_isotropic_analytic(tc["r_Re"])
        n_expected  = ratio_ref * n_iso_ref

        n_model = compute_n_from_spectrum_zone(zone)
        T_mean  = float(np.mean(zone.get("T", [float("nan")])))

        # Ratio from model output
        ratio_model = n_model / n_iso_ref if n_iso_ref > 0 else float("nan")

        rel_err = abs(ratio_model - ratio_ref) / ratio_ref if ratio_ref > 0 else float("nan")
        passed  = rel_err < tol
        status  = "PASS" if passed else "FAIL"

        print(f"    Semi-analytic reference:")
        print(f"      T_geo_ref   = {Tg:.5f}")
        print(f"      T_aniso_ref = {ref['T_aniso_ref']:.5f}")
        print(f"      ratio_ref   = {ratio_ref:.5f}  (n_aniso/n_iso)")
        print(f"    n_iso_ref     = {n_iso_ref:.4e} m^-3  (T_geo × n_free)")
        print(f"    n_expected    = {n_expected:.4e} m^-3  (ratio_ref × n_iso_ref)")
        print(f"    n_model       = {n_model:.4e} m^-3")
        print(f"    ratio_model   = {ratio_model:.5f}  vs  ratio_ref={ratio_ref:.5f}")
        print(f"    rel_err (ratio) = {rel_err:.2%}  tol={tol:.0%}  >>> {status} <<<")
        print()

        results.append({
            "run": run_label, "label": tc["label"], "passed": passed,
            "n_model": n_model, "n_analytic": n_expected,
            "ratio_model": ratio_model, "ratio_ref": ratio_ref,
            "rel_err": rel_err, "T_geo": Tg, "T_mean": T_mean,
            "pad_model": pad_model,
        })

    return results


def run_test_PAD_flux(run_dir: str, pad_model: str, pad_n: float,
                      tol: float = 0.10,
                      N_theta: int = 200, N_phi: int = 400) -> list:
    """
    RUN C/D flux: PAD anisotropy.
    For energies well above the Størmer cutoff, T is energy-independent, so:
      F_ch_aniso = T_aniso_ref * F_closed_clipped(E1, E2, 1.0)
                 = F_closed_clipped(E1, E2, T_aniso_ref)

    T_aniso_ref is taken from semi_analytic_ratio() for each test point.
    """
    run_label = "C" if "SIN" in pad_model else "D"
    flux_file = os.path.join(run_dir, "gridless_points_flux.dat")
    if not os.path.isfile(flux_file):
        print(f"  [{run_label} flux] gridless_points_flux.dat not found — skipping")
        return []
    rows = read_flux_dat(flux_file)
    if not rows:
        return []

    print(f"  [{run_label} flux] Comparing per-channel flux ({pad_model} n={pad_n}) ...")
    all_flux = []

    for i, tc in enumerate(PAD_TEST_POINTS):
        if i >= len(rows):
            continue
        row = rows[i]
        x0_Re = [tc["x_km"]/RE_KM, tc["y_km"]/RE_KM, tc["z_km"]/RE_KM]
        ref = semi_analytic_ratio(x0_Re, pad_model, pad_n, N_theta=N_theta, N_phi=N_phi)
        T_aniso_ref = ref["T_aniso_ref"]

        for ch_name, E1, E2 in ANISO_CHANNELS:
            col = f"F_{ch_name}_m2s1"
            if col not in row:
                continue
            F_model = row[col]
            F_ref   = F_closed_clipped(E1, E2, T_aniso_ref)
            if F_ref == 0.0:
                passed  = abs(F_model) < 1.0
                rel_err = float("nan")
            else:
                rel_err = abs(F_model - F_ref) / F_ref
                passed  = rel_err < tol
            st = "PASS" if passed else "FAIL"
            print(f"    [{run_label}-{tc['label']} {ch_name}]  "
                  + (f"F_model={F_model:.4e}  F_ref={F_ref:.4e}  err={rel_err:.2%}  >>> {st} <<<"
                     if F_ref > 0 else f"F_model={F_model:.3e}  F_ref=0 (below grid)  >>> {st} <<<"))
            all_flux.append({
                "run": f"{run_label}-flux", "label": tc["label"], "channel": ch_name,
                "E1": E1, "E2": E2, "F_model": F_model, "F_ref": F_ref,
                "rel_err": rel_err, "passed": passed, "is_zero_expected": F_ref == 0.0,
            })
    print()
    return all_flux

def print_summary(all_results: list, all_flux: list = None):
    n_pass = sum(1 for r in all_results if r.get("passed", False))
    n_total = len(all_results)
    print()
    print("=" * 72)
    print("SUMMARY — DENSITY")
    print("-" * 72)
    print(f"  {'Run':6}  {'Label':8}  {'n_model':>12}  {'n_analytic':>12}  "
          f"{'rel_err':>8}  {'Result':>6}")
    print("-" * 72)
    for r in all_results:
        nm  = f"{r.get('n_model', float('nan')):.3e}"
        na  = f"{r.get('n_analytic', float('nan')):.3e}"
        err = f"{r.get('rel_err', float('nan')):.1%}"
        st  = "PASS" if r.get("passed") else "FAIL"
        print(f"  {r.get('run','?'):6}  {r.get('label','?'):8}  {nm:>12}  {na:>12}  "
              f"{err:>8}  {st:>6}")
    print("-" * 72)
    print(f"  Passed: {n_pass}/{n_total}")

    if all_flux:
        flux_active = [r for r in all_flux if not r.get("is_zero_expected", False)]
        flux_zero   = [r for r in all_flux if r.get("is_zero_expected", False)]
        fp = sum(1 for r in all_flux if r.get("passed", False))
        ft = len(all_flux)
        print()
        print("SUMMARY — FLUX CHANNELS")
        print("-" * 72)
        print(f"  {'Run':8}  {'Label':6}  {'Ch':5}  {'F_model':>12}  "
              f"{'F_ref':>12}  {'rel_err':>8}  {'Result':>6}")
        print("-" * 72)
        for r in flux_active:
            fm  = f"{r.get('F_model', float('nan')):.4e}"
            fr  = f"{r.get('F_ref',   float('nan')):.4e}"
            err = f"{r.get('rel_err', float('nan')):.1%}"
            st  = "PASS" if r.get("passed") else "FAIL"
            print(f"  {r.get('run','?'):8}  {r.get('label','?'):6}  "
                  f"{r.get('channel','?'):5}  {fm:>12}  {fr:>12}  {err:>8}  {st:>6}")
        if flux_zero:
            n_zero_pass = sum(1 for r in flux_zero if r.get("passed", False))
            print(f"  (Below-grid channels: {n_zero_pass}/{len(flux_zero)} correctly return 0)")
        print("-" * 72)
        print(f"  Passed: {fp}/{ft}")

    n_total_all = n_total + (len(all_flux) if all_flux else 0)
    n_pass_all  = n_pass  + (sum(1 for r in all_flux if r.get("passed")) if all_flux else 0)
    print()
    print("=" * 72)
    if n_pass_all == n_total_all:
        print("ALL TESTS PASSED")
    else:
        print(f"WARNING: {n_total_all - n_pass_all} TEST(S) FAILED")
    print("=" * 72)


# ---------------------------------------------------------------------------
# Optional plots
# ---------------------------------------------------------------------------

def make_plots(all_results: list, zones_by_run: dict, out_dir: str = ".",
               all_flux: list = None):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("[plots] matplotlib not available — skipping")
        return

    # ----- Plot 1: T(E) per point per run -----
    point_labels = [tc["label"] for tc in PAD_TEST_POINTS]
    run_keys     = sorted(zones_by_run.keys())
    n_runs       = len(run_keys)
    n_pts        = len(PAD_TEST_POINTS)

    fig, axes = plt.subplots(n_runs, n_pts, figsize=(4*n_pts, 3*n_runs),
                              sharey=False, squeeze=False)

    colors = {"A": "#1f77b4", "B": "#2ca02c", "C": "#ff7f0e", "D": "#d62728"}
    for ri, run_key in enumerate(run_keys):
        zones = zones_by_run[run_key]
        for ci, zone in enumerate(zones[:n_pts]):
            ax = axes[ri, ci]
            E_arr = zone.get("E_MeV", np.array([]))
            T_arr = zone.get("T",     np.array([]))
            if E_arr.size > 0:
                ax.semilogx(E_arr, T_arr, "o-", ms=3, lw=1.5,
                            color=colors.get(run_key, "gray"), label=f"Run {run_key}")
            tc = PAD_TEST_POINTS[ci]
            Tg = T_geometric(tc["r_Re"])
            ax.axhline(Tg, color="k", ls="--", lw=1.0, label=f"T_geo={Tg:.3f}")
            ax.set_xlabel("E [MeV]")
            ax.set_ylabel("T")
            ax.set_title(f"Run {run_key} — {tc['desc']}", fontsize=8)
            ax.legend(fontsize=6)
            ax.grid(True, alpha=0.3)
            ax.set_xlim(EMIN_ANISO*0.8, EMAX_ANISO*1.2)

    fig.suptitle("T(E) per run and point — anisotropic boundary", fontsize=10)
    plt.tight_layout()
    out1 = os.path.join(out_dir, "test_aniso_T_curves.png")
    plt.savefig(out1, dpi=150, bbox_inches="tight")
    print(f"[plots] Saved: {out1}")
    plt.close(fig)

    # ----- Plot 2: ratio n_aniso / n_iso per run per point -----
    pad_results = [r for r in all_results if r.get("run") in ("C", "D")]
    if pad_results:
        labels_C = [r["label"] for r in pad_results if r["run"] == "C"]
        ratio_C  = [r.get("ratio_model", float("nan")) for r in pad_results if r["run"] == "C"]
        ratio_C_ref = [r.get("ratio_ref",   float("nan")) for r in pad_results if r["run"] == "C"]
        labels_D = [r["label"] for r in pad_results if r["run"] == "D"]
        ratio_D  = [r.get("ratio_model", float("nan")) for r in pad_results if r["run"] == "D"]
        ratio_D_ref = [r.get("ratio_ref",   float("nan")) for r in pad_results if r["run"] == "D"]

        fig2, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4))
        for ax, labels, ratios, refs, title in [
            (ax1, labels_C, ratio_C, ratio_C_ref, "SINALPHA_N n=2"),
            (ax2, labels_D, ratio_D, ratio_D_ref, "COSALPHA_N n=2"),
        ]:
            x = np.arange(len(labels))
            ax.bar(x - 0.2, ratios, 0.35, label="AMPS model", color="#1f77b4")
            ax.bar(x + 0.2, refs,   0.35, label="Semi-analytic ref", color="#ff7f0e", alpha=0.7)
            ax.axhline(1.0, color="k", ls="--", lw=0.8, label="Isotropic (ratio=1)")
            ax.set_xticks(x)
            ax.set_xticklabels(labels)
            ax.set_ylabel("n_aniso / n_iso")
            ax.set_ylim(0, 1.2)
            ax.set_title(f"PAD: {title}")
            ax.legend(fontsize=8)
            ax.grid(True, alpha=0.3, axis="y")

        fig2.suptitle("Anisotropic/isotropic density ratio — model vs reference", fontsize=10)
        plt.tight_layout()
        out2 = os.path.join(out_dir, "test_aniso_ratio_comparison.png")
        plt.savefig(out2, dpi=150, bbox_inches="tight")
        print(f"[plots] Saved: {out2}")
        plt.close(fig2)

    # ----- Plot 3: per-channel flux model vs reference -----
    if all_flux:
        active_flux = [r for r in all_flux if not r.get("is_zero_expected", False)]
        if active_flux:
            # Group by (run, label) pair
            groups = {}
            for r in active_flux:
                key = f"{r['run']}\n{r['label']}"
                groups.setdefault(key, {})
                groups[key][r["channel"]] = (r["F_model"], r["F_ref"])

            ch_names = [c for c, _, _ in ANISO_CHANNELS
                        if any(c in groups[k] for k in groups)]
            n_groups = len(groups)
            n_ch     = len(ch_names)
            if n_groups > 0 and n_ch > 0:
                fig3, ax = plt.subplots(figsize=(max(6, 2*n_groups), 4))
                x = np.arange(n_groups)
                width = 0.8 / (2 * n_ch)
                for ci, ch in enumerate(ch_names):
                    F_models = [groups[k].get(ch, (float("nan"),float("nan")))[0]
                                for k in groups]
                    F_refs   = [groups[k].get(ch, (float("nan"),float("nan")))[1]
                                for k in groups]
                    offset_m = (2*ci - n_ch + 0.5) * width
                    offset_r = offset_m + width
                    ax.bar(x + offset_m, F_models, width*0.9,
                           label=f"{ch} model", alpha=0.85)
                    ax.bar(x + offset_r, F_refs, width*0.9,
                           label=f"{ch} ref", alpha=0.55, hatch="//")
                ax.set_xticks(x)
                ax.set_xticklabels(list(groups.keys()), fontsize=7)
                ax.set_ylabel("Integral flux [m⁻² s⁻¹]")
                ax.set_yscale("log")
                ax.legend(fontsize=7, ncol=2)
                ax.set_title("Per-channel integral flux: model vs analytic reference", fontsize=9)
                ax.grid(True, alpha=0.3, axis="y")
                plt.tight_layout()
                out3 = os.path.join(out_dir, "test_aniso_flux_channels.png")
                plt.savefig(out3, dpi=150, bbox_inches="tight")
                print(f"[plots] Saved: {out3}")
                plt.close(fig3)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dir-regression", default=None,
                        help="Directory with RUN A output (ISOTROPIC PAD regression)")
    parser.add_argument("--dir-spatial",   default=None,
                        help="Directory with RUN B output (DAYSIDE_NIGHTSIDE spatial)")
    parser.add_argument("--dir-sinalpha",  default=None,
                        help="Directory with RUN C output (SINALPHA_N n=2)")
    parser.add_argument("--dir-cosalpha",  default=None,
                        help="Directory with RUN D output (COSALPHA_N n=2)")
    parser.add_argument("--tol-exact",       type=float, default=0.02,
                        help="Tolerance for RUN A (default 0.02 = 2%%)")
    parser.add_argument("--tol-spatial",     type=float, default=0.05,
                        help="Tolerance for RUN B (default 0.05 = 5%%)")
    parser.add_argument("--tol-semianalytic",type=float, default=None,
                        help="Global tolerance override for RUN C/D (default: per-point)")
    parser.add_argument("--fast-refs", action="store_true",
                        help="Use low-resolution reference computation (N=80×160, faster)")
    parser.add_argument("--no-plots", action="store_true",
                        help="Skip matplotlib figures")
    args = parser.parse_args()

    ref_N_theta = 80  if args.fast_refs else 200
    ref_N_phi   = 160 if args.fast_refs else 400

    if args.tol_semianalytic:
        tol_override = {tc["label"]: args.tol_semianalytic for tc in PAD_TEST_POINTS}
    else:
        tol_override = None

    all_results = []
    all_flux    = []
    zones_by_run = {}

    print()
    print("=" * 72)
    print("AMPS DENSITY/SPECTRUM — ANISOTROPIC BOUNDARY SPECTRUM TEST SUITE")
    print("DIPOLE FIELD  |  Power-law J0=1e4, E0=100MeV, gamma=2")
    print("Energy range: 5–50 GeV  |  Larmor radius >> 9 Re domain")
    print("=" * 72)
    print()

    # RUN A
    if args.dir_regression:
        print("─" * 72)
        print("RUN A — Regression: ISOTROPIC PAD (ANISOTROPIC mode = ISOTROPIC mode)")
        print(f"  Input file  : test_aniso_regression.in")
        print(f"  Output dir  : {args.dir_regression}")
        print(f"  Tolerance   : {args.tol_exact:.0%}")
        print()
        res_A = run_test_A_regression(args.dir_regression, tol=args.tol_exact)
        all_results.extend(res_A)
        all_flux.extend(run_test_A_flux(args.dir_regression, tol=args.tol_exact))
        if not args.no_plots and args.dir_regression:
            sf = os.path.join(args.dir_regression, "gridless_points_spectrum.dat")
            if os.path.isfile(sf):
                zones_by_run["A"] = read_spectrum_dat(sf)

    # RUN B
    if args.dir_spatial:
        print("─" * 72)
        print("RUN B — DAYSIDE_NIGHTSIDE spatial (exact analytic: ratio=1.25)")
        print(f"  Input file  : test_aniso_spatial.in")
        print(f"  Output dir  : {args.dir_spatial}")
        print(f"  Tolerance   : {args.tol_spatial:.0%}")
        print(f"  f_day={SPATIAL_TEST_POINT['f_day']}  f_night={SPATIAL_TEST_POINT['f_night']}  "
              f"ratio_exact={(SPATIAL_TEST_POINT['f_day']+SPATIAL_TEST_POINT['f_night'])/2:.3f}")
        print()
        res_B = run_test_B_spatial(args.dir_spatial, tol=args.tol_spatial)
        all_results.extend(res_B)
        all_flux.extend(run_test_B_flux(args.dir_spatial, tol=args.tol_spatial))

    # RUN C
    if args.dir_sinalpha:
        print("─" * 72)
        print("RUN C — PAD: SINALPHA_N n=2 (perpendicular / pancake distribution)")
        print(f"  Input file  : test_aniso_sinalpha.in")
        print(f"  Output dir  : {args.dir_sinalpha}")
        print(f"  Reference   : straight-line + dipole B  (N={ref_N_theta}×{ref_N_phi})")
        print()
        res_C = run_test_PAD(args.dir_sinalpha, "SINALPHA_N", 2.0,
                              tol_override=tol_override,
                              N_theta=ref_N_theta, N_phi=ref_N_phi)
        all_results.extend(res_C)
        all_flux.extend(run_test_PAD_flux(args.dir_sinalpha, "SINALPHA_N", 2.0,
                                           tol=args.tol_semianalytic or 0.10,
                                           N_theta=ref_N_theta, N_phi=ref_N_phi))
        if not args.no_plots and args.dir_sinalpha:
            sf = os.path.join(args.dir_sinalpha, "gridless_points_spectrum.dat")
            if os.path.isfile(sf):
                zones_by_run["C"] = read_spectrum_dat(sf)

    # RUN D
    if args.dir_cosalpha:
        print("─" * 72)
        print("RUN D — PAD: COSALPHA_N n=2 (field-aligned / SEP beam distribution)")
        print(f"  Input file  : test_aniso_cosalpha.in")
        print(f"  Output dir  : {args.dir_cosalpha}")
        print(f"  Reference   : straight-line + dipole B  (N={ref_N_theta}×{ref_N_phi})")
        print()
        res_D = run_test_PAD(args.dir_cosalpha, "COSALPHA_N", 2.0,
                              tol_override=tol_override,
                              N_theta=ref_N_theta, N_phi=ref_N_phi)
        all_results.extend(res_D)
        all_flux.extend(run_test_PAD_flux(args.dir_cosalpha, "COSALPHA_N", 2.0,
                                           tol=args.tol_semianalytic or 0.10,
                                           N_theta=ref_N_theta, N_phi=ref_N_phi))
        if not args.no_plots and args.dir_cosalpha:
            sf = os.path.join(args.dir_cosalpha, "gridless_points_spectrum.dat")
            if os.path.isfile(sf):
                zones_by_run["D"] = read_spectrum_dat(sf)

    if not all_results:
        print("No run directories provided. Printing reference table only.\n")
        print_reference_table(ref_N_theta, ref_N_phi)
        sys.exit(0)

    print_summary(all_results, all_flux if all_flux else None)

    if not args.no_plots and zones_by_run:
        out_dir = (args.dir_regression or args.dir_sinalpha
                   or args.dir_cosalpha or ".")
        make_plots(all_results, zones_by_run, out_dir=out_dir,
                   all_flux=all_flux if all_flux else None)

    n_failed = sum(1 for r in all_results if not r.get("passed", False))
    n_failed += sum(1 for r in all_flux   if not r.get("passed", False))
    sys.exit(n_failed)


def print_reference_table(N_theta=200, N_phi=400):
    """Print the semi-analytic reference table without any AMPS output files."""
    print("Semi-analytic reference ratios n_aniso / n_iso")
    print(f"(straight-line + dipole B, N={N_theta}×{N_phi})\n")
    print(f"  {'Point':20} {'ISOTROPIC':>12} {'SINALPHA_N':>12} {'COSALPHA_N':>12} {'SIN\u00b2+COS\u00b2':>10}")
    print("  " + "-"*68)
    for tc in PAD_TEST_POINTS:
        x0_Re = [tc["x_km"]/RE_KM, tc["y_km"]/RE_KM, tc["z_km"]/RE_KM]
        r_iso  = semi_analytic_ratio(x0_Re, "ISOTROPIC",  2.0, N_theta=N_theta, N_phi=N_phi)
        r_sin  = semi_analytic_ratio(x0_Re, "SINALPHA_N", 2.0, N_theta=N_theta, N_phi=N_phi)
        r_cos  = semi_analytic_ratio(x0_Re, "COSALPHA_N", 2.0, N_theta=N_theta, N_phi=N_phi)
        chk    = r_sin["ratio"] + r_cos["ratio"]
        print(f"  {tc['desc']:20} {r_iso['ratio']:>12.5f} {r_sin['ratio']:>12.5f} "
              f"{r_cos['ratio']:>12.5f} {chk:>8.5f}")
    print()
    print("  Note: SINALPHA\u00b2 + COSALPHA\u00b2 = 1.000 (because sin\u00b2\u03b1+cos\u00b2\u03b1=1 and")
    print("  the average is linear), which is a useful internal consistency check.")
    print()
    print(f"  Dusk point (0,8Re,0), DAYSIDE_NIGHTSIDE (f_day=2.0, f_night=0.5):")
    print(f"    Exact ratio = (2.0 + 0.5)/2 = 1.250  [no approximation needed]")


if __name__ == "__main__":
    main()
