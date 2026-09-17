# README — Analytic Density/Spectrum Test Cases (DIPOLE field)

## Overview

These test cases verify the gridless density/spectrum solver against closed-form
analytic predictions. They use a **pure magnetic dipole** field so that Størmer
theory provides exact cutoff rigidities. The test framework is:

```
./amps -mode gridless -i test_density_dipole_4pts.in
python3 test_density_analytic.py
```

---

## 1. What the solver computes

At each observation point **x₀**, the solver computes:

```
J_loc(E; x₀) = T(E; x₀) · J_b(E)

n_tot(x₀) = 4π ∫[Emin,Emax] T(E; x₀) · J_b(E) / v(E) dE
```

where
- **T(E; x₀)** = transmissivity = fraction of backtraced trajectories that
  escape the outer domain box (ALLOWED); range [0, 1]
- **J_b(E)** = boundary differential intensity (power-law input spectrum)
- **v(E)** = relativistic proton speed at kinetic energy E

---

## 2. Analytic formula for T — when is it exact?

### 2.1 Geometric shadow

From any interior point at radius *r*, Earth's inner boundary (loss sphere,
radius *r_inner* = 1.01 Re) subtends a cone of half-angle α:

```
sin α = r_inner / r
```

The fraction of the full sky that is *geometrically blocked* by Earth is:

```
Ω_blocked / 4π = (1 − cos α) / 2
```

Therefore, even without any magnetic field, the transmissivity is:

```
T_geo(r) = 1 − (1 − cos α)/2 = (1 + cos α)/2 = (1 + √(1 − (r_inner/r)²)) / 2
```

| r [Re] | T_geo   |
|--------|---------|
| 2.0    | 0.9316  |
| 5.0    | 0.9897  |
| 6.0    | 0.9929  |
| 8.0    | 0.9960  |

This is always present — Earth blocks a solid angle regardless of energy or
magnetic field.

### 2.2 Størmer cutoff (additional, energy-dependent)

In a dipole field, a proton at geomagnetic latitude λ and geocentric distance
*r* (in Re) has a **maximum Størmer cutoff rigidity** (horizontal-east incidence):

```
Rc_max(r, λ) = Cs · cos⁴λ / r²   [GV]

where Cs = (μ₀/4π · M_E / Re²) · c / 10⁹ = 59.6 GV · Re²
```

For the Earth dipole used in `DipoleInterface.h` (B_eq = 3.12×10⁻⁵ T):
```
Cs = 59.6 GV · Re²
```

For vertical incidence (straight up/down), the cutoff is 4× lower:
```
Rc_vert(r, λ) = Cs · cos⁴λ / (4 r²)  =  Rc_max / 4
```

At the geomagnetic **pole** (λ = 90°), `cos λ = 0` and **Rc = 0 for all
directions and all energies**. The poles are always fully transparent.

### 2.3 Key identity: when T = T_geo exactly

If all energies in [Emin, Emax] are above Rc_max at the test location, then
the Størmer forbidden zone is empty for every direction, and:

```
T(E; x₀) = T_geo(r)    for ALL E in [Emin, Emax]          (*)
```

This gives an **exact, parameter-free analytic prediction** for the density:

```
n_exact = T_geo · 4π ∫[Emin,Emax] J_b(E) / v(E) dE
```

For a power-law spectrum J_b(E) = J₀ · (E/E₀)^(−γ):

```
n_exact = T_geo · 4π · J₀ / E₀^(−γ) · I(Emin, Emax)

where I = ∫ E^(−γ) / v(E) dE   [computed by numerical quadrature]
```

---

## 3. Does Earth's presence affect the analytic estimate?

**Yes — in two distinct ways:**

### Effect 1: Geometric shadow (always present, modeled correctly)

The inner loss sphere (r = 1.01 Re) blocks a solid angle of 4π × (1 − T_geo).
This is automatically accounted for by the solver: trajectories that hit the
inner sphere are classified FORBIDDEN. The analytic formula includes this via
the T_geo factor.

*Example:* At r = 2 Re, T_geo = 0.932, meaning 6.8% of the sky is blocked
by Earth's disk even if there were no magnetic field at all.

### Effect 2: Geomagnetic shielding (energy-dependent, on top of shadow)

The dipole field additionally forbids trajectories that would otherwise escape,
for energies below the Størmer cutoff. This is captured as an additional factor
T_Størmer(E; x₀) ≤ 1, so the total is:

```
T_total ≈ T_geo · T_Størmer     (schematically — the solver computes this jointly)
```

For **TC1–TC3**, the test energies are chosen to be above Rc_max, so
T_Størmer = 1 and the analytic prediction is exact.

For **TC4**, T_Størmer ≪ 1 (all energies deeply below Rc_vert), so
n_model ≪ T_geo · n_free. This is tested as an upper-bound inequality only.

---

## 4. Test case table

| Case | Location (GSM)       | Energy range | Rc_max [GV] | T_geo  | Test type    |
|------|----------------------|--------------|-------------|--------|--------------|
| TC1  | (8 Re, 0, 0) equator | 2–20 GeV     | 0.93 GV     | 0.9960 | Exact ±5%    |
| TC2  | (0, 0, 5 Re) pole    | 50–500 MeV   | 0 (pole)    | 0.9897 | Exact ±5%    |
| TC3  | (6 Re, 0, 0) equator | 3–30 GeV     | 1.66 GV     | 0.9929 | Exact ±5%    |
| TC4  | (2 Re, 0, 0) equator | 10–500 MeV   | 14.9 GV     | 0.9316 | Upper bound  |

### TC1 — 8 Re equator: free-space above-cutoff

The test energy range starts at 2 GeV, well above Rc_max = 384 MeV. At this
distance, the geometric shadow is only 0.4% (T_geo = 0.9960). This is almost
a pure "free-space" test of the density integral with T ≈ 1. Any bias in the
trajectory classification or the energy integration would show up here.

**Expected density:** 1.923 × 10⁻³ m⁻³

### TC2 — Polar axis: Rc = 0 at all energies

At the magnetic north pole, λ = 90°, so cos λ = 0 and **Størmer Rc = 0
exactly**. Every direction from the pole leads to infinity (or to the loss
sphere). T = T_geo regardless of what energy is used. This test is sensitive
to any bug in the geomagnetic-latitude handling: if the code incorrectly
applies a nonzero cutoff at the pole, T will be less than T_geo.

Uses 50–500 MeV protons — a physically interesting range for radiation belt
access — to exercise the spectrum integration over a softer energy range.

**Expected density:** 1.775 × 10⁻¹ m⁻³

### TC3 — 6 Re equator: above-cutoff at intermediate distance

Closer-in than TC1. Rc_max = 965 MeV at 6 Re, so the test uses 3–30 GeV.
This tests the T_geo scaling with r: T_geo(6Re) = 0.9929 vs T_geo(8Re) = 0.9960.
The ~0.3% difference should be detectable if the solver has enough trajectory
directions.

**Expected density:** 1.264 × 10⁻³ m⁻³

### TC4 — 2 Re equator: deep Størmer shielding

At 2 Re equator, the vertical Størmer cutoff corresponds to 2.9 GeV protons.
For the test energy range 10–500 MeV, all rigidities (0.14–1.09 GV) are
well below Rc_vert = 3.73 GV. The Størmer forbidden zone covers the vast
majority of directions. The expected number density is dramatically suppressed:

```
n_model ≪ T_geo · n_free ≈ 1.81 m⁻³
```

No exact analytic prediction is made because the open solid-angle fraction
(T_Størmer) depends on the full directional structure of the Størmer forbidden
zone and has no simple closed form. The test only checks the upper bound.

This test **qualitatively verifies that the solver is correctly shielding particles
deep inside the magnetosphere** and does not over-estimate the density there.

---

## 5. Spectrum used

Power-law differential intensity:

```
J_b(E) = J₀ · (E / E₀)^(−γ)   [m⁻² s⁻¹ sr⁻¹ MeV⁻¹]

J₀ = 1.0 × 10⁴  m⁻² s⁻¹ sr⁻¹ MeV⁻¹   = 1 cm⁻² s⁻¹ sr⁻¹ MeV⁻¹
E₀ = 100 MeV
γ  = 2.0
```

This is a representative SEP-event spectrum. The γ = 2 slope means the spectral
integral over any decade of energy contributes roughly equal density. The
reference unit J₀ = 1 cm⁻² s⁻¹ sr⁻¹ MeV⁻¹ matches the energy range of
large ground-level events (GLEs).

---

## 6. Tolerance justification

The solver samples a finite number of directions per energy per point (typically
N_dirs = 576 = 24 × 24). The RMS statistical error in T is approximately:

```
σ_T / T ≈ √(T(1−T) / N_dirs) ≈ 1/√N_dirs ≈ 4%
```

The density integral further smooths these fluctuations (T(E) errors at
different energy bins partially cancel). A 5% relative tolerance is therefore
consistent with the expected numerical noise.

TC4 uses a factor-of-2 margin (n_model < 50% × T_geo × n_free) which is
extremely conservative given that Rc_vert / R(Emax) ≈ 3.4 at 500 MeV.

---

## 7. How to extend these tests

### Add intermediate shielding cases

To quantitatively test partial transmissivity (0 < T_Størmer < 1), compute
T(E) numerically using the Størmer formula integrated over the full sphere:

```
T_Stormer(E; r, λ) = (1/4π) ∫∫ A(θ,φ; R, r, λ) sin θ dθ dφ
```

where A = 1 if R > Rc(θ,φ; r, λ) and A = 0 otherwise. This can be done in
Python using the exact Størmer formula for each direction.

### Test the anisotropic boundary mode

Run the same four points with `-density-mode ANISOTROPIC` and compare:
- At TC1–TC3 (above cutoff): n_aniso = T_geo · 4π ∫ J_b(E)·<f_PAD>_allowed /v dE
  where <f_PAD>_allowed is the solid-angle average of f_PAD over allowed directions.
- For an isotropic PAD (BA_PAD_MODEL=ISOTROPIC), the result should be identical
  to the isotropic run — this is a regression test.

### Test with T96/T05

Run the same point set with T96/T05 fields and compare to TC1–TC3 predictions:
during quiet times (small Dst, solar wind nominal), the outer magnetosphere
(r > 6 Re) should agree with the dipole result to within ~10%, while the inner
magnetosphere (TC4, r = 2 Re) should show similar or stronger shielding.

---

## 8. Files

| File | Description |
|------|-------------|
| `test_density_dipole_4pts.in`  | AMPS input file — all 4 test points, DIPOLE field, energy channels |
| `test_density_analytic.py`     | Python comparison script — density, total flux, per-channel flux |
| `README_density_tests.md`      | This document |

Run sequence:
```bash
./amps -mode gridless -i test_density_dipole_4pts.in
python3 test_density_analytic.py --tol 0.05
```

Expected AMPS outputs:
- `gridless_points_density.dat`   — 4 rows, one per point
- `gridless_points_spectrum.dat`  — 4 ZONEs, one per point
- `gridless_points_flux.dat`      — 4 rows: F_tot + F_CH1 + F_CH2 + F_CH3 + F_CH4

---

## 9. Integral flux — physics, algorithm, and analytic validation

### 9.1 Definition: density vs integral flux

Both quantities are derived from the local differential intensity
J_loc(E; x0) = T(E; x0) · J_b(E), but they differ by the presence or
absence of a 1/v(E) weighting:

| Quantity | Formula | Units |
|----------|---------|-------|
| Number density | n(x0) = 4π ∫ J_loc(E)/v(E) dE | m⁻³ |
| Integral flux | F(x0; E1,E2) = 4π ∫_{E1}^{E2} J_loc(E) dE | m⁻² s⁻¹ |

The **1/v factor** in the density integral means that slow particles contribute
more density per unit flux — a 100 MeV proton (v ≈ 0.43 c) contributes
1/0.43 ≈ 2.3× more density per unit flux than a 10 GeV proton (v ≈ c).

Integral flux, by contrast, is a direct count of particles per unit area per
unit time and is what particle detectors with energy thresholds measure.

### 9.2 Energy channels

Four channels are defined in `#ENERGY_CHANNELS` of the input file:

| Channel | E1 [MeV] | E2 [MeV] | Notes |
|---------|----------|----------|-------|
| CH1 | 10 | 100 | Sub-cutoff at equatorial points; above-cutoff at pole |
| CH2 | 100 | 1000 | Spans the Størmer cutoff transition for TC1/TC3 |
| CH3 | 1000 | 10000 | All test points above cutoff → exact analytic prediction |
| CH4 | 10000 | 20000 | High energy, near top of solver grid |

### 9.3 Analytic closed form (power-law spectrum, γ = 2)

For J_b(E) = J0 · (E/E0)^{−γ} with γ ≠ 1, the integral flux has an
**exact closed form** (no numerical approximation needed):

```
F(E1, E2) = 4π · T · J0 · E0^γ / (γ−1) · (E1^{1−γ} − E2^{1−γ})
```

For γ = 2 (used in this test):

```
F(E1, E2) = 4π · T · J0 · E0² · (1/E1 − 1/E2)
```

This expression is **independent of the energy grid resolution** — it is a
pure analytic formula. This is the primary benchmark for flux validation and
makes the flux tests much cleaner than density tests (which require numerical
quadrature of the 1/v factor).

**Example** (T_geo(8Re) = 0.99600, CH3: E1=1000 MeV, E2=10000 MeV):

```
F_CH3 = 4π × 0.99600 × 10⁴ × 100² × (1/1000 − 1/10000)
      = 4π × 0.99600 × 10⁴ × 10⁴ × 9×10⁻⁴
      ≈ 1.126 × 10⁶  m⁻² s⁻¹
```

### 9.4 Numerical implementation

**FluxIntegrateTotal** (total flux over [Emin, Emax]):

```cpp
F = 4π · Σ_i  0.5 · (J_loc_i + J_loc_{i+1}) · ΔE_J
```

Trapezoidal rule on the solver energy grid (in Joules), exactly as for
density but without the 1/v factor.

**FluxIntegrateChannel** (channel flux over user-defined [E1, E2]):

1. Clip the channel to the grid: `lo = max(E1, Emin_grid)`, `hi = min(E2, Emax_grid)`.
   Return 0 if `lo ≥ hi`.
2. Build an augmented sub-grid:  
   `{lo} ∪ {E_i ∈ (lo, hi)} ∪ {hi}`  
   where T at `lo` and `hi` is obtained by **linear interpolation** from the
   two nearest grid nodes.
3. Apply the trapezoidal rule over J_loc = T · J_b on the augmented sub-grid.

**MPI design:** channel flux is computed on rank 0 from the T[] array received
from each worker — no extra MPI messages. Workers already send T[nE] along
with n_tot in the density protocol.

### 9.5 Test logic in test_density_analytic.py

For each test point and each channel:

| Channel status | Test criterion | Rationale |
|---------------|---------------|-----------|
| E1 > E_cut (fully above cutoff) | \|F_model − F_analytic\| / F_analytic < 5% | T = T_geo exactly; analytic formula is exact |
| Channel straddles/below cutoff | F_model ≤ 1.05 · F_analytic | T_Størmer ≤ 1, so model ≤ analytic upper bound |

**Consistency check:** the Python reconstruction of F_ch from the spectrum
zone should agree with the solver-written value in `gridless_points_flux.dat`
to < 0.1% (same algorithm, same T[], same spectrum). This catches any unit
conversion bugs in the C++ output writer.

### 9.6 Expected values

| Channel | TC1 (8Re eq.) | TC2 (5Re pole) | TC3 (6Re eq.) | TC4 (2Re eq.) |
|---------|--------------|---------------|--------------|--------------|
| CH1 | shielded | 1.119×10⁸ m⁻²s⁻¹ | shielded | deep shielding |
| CH2 | shielded | 1.119×10⁷ | shielded | deep shielding |
| CH3 | 1.126×10⁶ | 1.119×10⁶ | 1.122×10⁶ | shielded |
| CH4 | 6.258×10⁴ | 6.219×10⁴ | 6.238×10⁴ | shielded |
| TOTAL | 1.251×10⁸ | 1.244×10⁸ | 1.247×10⁸ | << 1.244×10⁸ |

"shielded" = channel is below or straddles the Størmer cutoff at that location;
the model output is expected to be less than the analytic upper bound.

CH3 provides the cleanest analytic comparison: all four test points have
energies in 1–10 GeV well above their respective cutoffs, so T = T_geo exactly
for all of them, and the 5% tolerance is an upper bound on sampling error only.

