# README — Anisotropic Density/Spectrum Test Cases (DIPOLE field)

## Overview

These test cases verify the **anisotropic boundary spectrum branch** of the
gridless density/spectrum solver. They complement the isotropic tests in
`README_density_tests.md` and test the same physical pipeline at the same
observation points, but with a direction-dependent and spatially non-uniform
boundary condition.

Four runs are executed, each isolating a single physical effect:

| Run | Input file                    | What is tested                        | Prediction type    |
|-----|-------------------------------|---------------------------------------|--------------------|
| A   | `test_aniso_regression.in`    | ISOTROPIC PAD — code path regression  | Exact: n_A = n_iso |
| B   | `test_aniso_spatial.in`       | DAYSIDE_NIGHTSIDE spatial asymmetry   | Exact: ratio = 1.25|
| C   | `test_aniso_sinalpha.in`      | SINALPHA_N n=2 (perpendicular/pancake)| Semi-analytic      |
| D   | `test_aniso_cosalpha.in`      | COSALPHA_N n=2 (field-aligned beam)   | Semi-analytic      |

Run sequence:
```bash
./amps -mode gridless -i test_aniso_regression.in   # outputs to run_A/
./amps -mode gridless -i test_aniso_spatial.in      # outputs to run_B/
./amps -mode gridless -i test_aniso_sinalpha.in     # outputs to run_C/
./amps -mode gridless -i test_aniso_cosalpha.in     # outputs to run_D/

python3 test_density_aniso.py \
    --dir-regression run_A \
    --dir-spatial    run_B \
    --dir-sinalpha   run_C \
    --dir-cosalpha   run_D
```

---

## 1. What the anisotropic solver computes

The isotropic solver assigns each allowed trajectory unit weight:
```
T_iso(E; x₀) = (1/N_dirs) · Σ_k A_k
```

The anisotropic branch replaces unit weight with a direction- and
position-dependent weight:
```
T_aniso(E; x₀) = (1/N_dirs) · Σ_k A_k · f_PAD(cos α_k) · f_spatial(x_exit,k)
```

where:
- **A_k** = 1 if trajectory k is ALLOWED (escapes outer box), 0 otherwise
- **cos α_k** = **v**_exit,k · **B̂**(x_exit,k) is the **pitch-angle cosine** at
  the domain boundary crossing, with **v**_exit,k the unit velocity at exit and
  **B̂** the dipole field unit vector at the exit point
- **x_exit,k** = GSM position where trajectory k crosses the outer domain box
- **f_PAD(cos α)** = pitch-angle distribution weight (see §3)
- **f_spatial(x_exit)** = spatial boundary weight (see §4)

The local spectrum and number density follow as in the isotropic case:
```
J_loc(E; x₀)   = J_b_iso(E) · T_aniso(E; x₀)

n_tot(x₀) = 4π ∫[Emin,Emax] T_aniso(E; x₀) · J_b_iso(E) / v(E) dE
```

The physical interpretation is that the boundary differential intensity is
**not** uniform over directions and positions, but factored as:
```
J_b(E, Ω, x) = J_b_iso(E) · f_PAD(cos α) · f_spatial(x)
```
The isotropic case is recovered when f_PAD = 1 and f_spatial = 1 everywhere.

---

## 2. Analytic framework: when is the ratio n_aniso / n_iso tractable?

### 2.1 Above-cutoff condition

For observation points where all test energies are above the maximum Størmer
cutoff Rc_max (see `README_density_tests.md` §2.2), the allowed set {A_k = 1}
depends only on the geometric shadow of the inner sphere — not on energy. In
that regime:

```
n_aniso / n_iso = <f_PAD · f_spatial>_allowed
```

where the average is over all geometrically-allowed directions uniformly:
```
<f>_allowed = (1/N_allowed) · Σ_{k: A_k=1} f_PAD(cos α_k) · f_spatial(x_exit,k)
```

This ratio is **energy-independent** (when A_k is energy-independent), so the
density ratio equals the T_aniso-weighted transmissivity ratio at any energy in
the above-cutoff range. In particular:

```
n_aniso / n_iso = T_aniso / T_geo
```

All test energies are chosen to be **5–50 GeV**, well above Rc_max at every
test point (see §6 for the cutoff table). The ratio can therefore be computed
from solid-angle integrals alone, with no reference to particle dynamics.

### 2.2 Semi-analytic computation

The reference ratio is computed by `test_density_aniso.py` using:

1. Sample N_θ × N_φ = 200 × 400 = 80,000 directions uniformly in solid angle.
2. For each direction **v̂**:
   - Skip if it intersects the inner sphere (FORBIDDEN by geometric shadow).
   - Find the exit point x_exit on the domain box via ray-box intersection.
   - Evaluate the exact analytic dipole **B**(x_exit) and compute **B̂**.
   - Compute cos α = **v̂** · **B̂**(x_exit).
   - Accumulate weight f_PAD(cos α) · f_spatial(x_exit).
3. Divide total weight by N_allowed to get T_aniso / T_geo.

This is referred to as the **straight-line + dipole-B** approximation. It is
exact in the limit that the particle trajectory from x₀ to x_exit is a straight
line — which holds when the Larmor radius is much larger than the domain.

---

## 3. Pitch-angle distribution (PAD) models

The pitch angle α is defined as the angle between the particle velocity and the
local magnetic field direction. All PAD models are normalized to f = 1 in the
isotropic limit.

### 3.1 ISOTROPIC  — f(α) = 1

No directional preference. Reduces the anisotropic branch to an identity; used
for the regression test (Run A).

### 3.2 SINALPHA_N  — f(α) = sinⁿ(α) = (1 − cos²α)^(n/2)

Perpendicular-peaked distribution. Maximum at α = 90° (particles drifting
across field lines); zero at α = 0° and α = 180° (no field-aligned particles).
This models:
- Radiation belt electrons / protons with a typical pancake PAD
- Loss-cone distributions (empty at small α, full near 90°)

For n = 2: f = sin²α = 1 − cos²α.

### 3.3 COSALPHA_N  — f(α) = |cos α|ⁿ

Field-aligned beam. Maximum at α = 0° and 180° (streaming along field);
zero at α = 90°. This models:
- Solar energetic particle (SEP) events streaming along the interplanetary
  magnetic field from the Sun, entering the magnetosphere at the polar cap
- Field-aligned electron beams from reconnection
- Strahl distributions in the solar wind

For n = 2: f = cos²α.

### 3.4 BIDIRECTIONAL  — f(α) = |cos α|ⁿ

Identical formula to COSALPHA_N but interpreted as a bidirectional (symmetric)
streaming distribution, i.e., equal flux from both hemispheres along **B**.
Not used in the current test set; included here for completeness.

### 3.5 Identity relation

Because sin²α + cos²α = 1 at every pitch angle, and the expectation is linear:
```
<sin²α>_allowed + <cos²α>_allowed = 1
```

Therefore:
```
ratio_SINALPHA2 + ratio_COSALPHA2 = 1.000    (exactly)
```

This is a **built-in self-consistency check** built into `test_density_aniso.py`.
The reference table printed by the script verifies this to 5 decimal places.
Any numerical error in the reference computation or a bug in the f_PAD
implementation would break this identity.

---

## 4. Spatial model

### 4.1 UNIFORM  — f_spatial = 1

Boundary flux is the same everywhere on the outer box. Used for Runs A, C, D.

### 4.2 DAYSIDE_NIGHTSIDE  — f_spatial = f_day or f_night

```
f_spatial(x_exit) = BA_DAYSIDE_FACTOR   if  x_exit_GSM > 0  (sunward)
                  = BA_NIGHTSIDE_FACTOR  if  x_exit_GSM ≤ 0  (anti-sunward)
```

Used in Run B with f_day = 2.0, f_night = 0.5.

Physical motivation: during solar energetic particle (SEP) events, the flux
arriving at the dayside magnetopause (where the interplanetary field connects)
can differ significantly from the nightside. Boberg, Tylka et al. (1995) showed
day/night asymmetries of factors 2–4 in October 1989 SEP events.

---

## 5. Exact analytic prediction for Run B (DAYSIDE_NIGHTSIDE)

The observation point for Run B is placed at **(0, 8 Re, 0)** — on the GSM
Y-axis (dusk meridian). This location has a precise geometric symmetry with
respect to the GSM X–Z plane:

For any allowed direction **v̂** from this point that exits at x_exit with
x_GSM > 0 (dayside), there exists a mirror direction **v̂'** (reflected across
the Y–Z plane) that exits at x'_exit with x_GSM < 0 (nightside). Both **v̂**
and **v̂'** subtend the same solid angle element and are equally likely to be
ALLOWED (neither preferentially hits the inner sphere from a Y-axis point).

Therefore, exactly half the allowed solid angle exits on the dayside and half
on the nightside:

```
<f_spatial>_allowed = ½ · f_day + ½ · f_night = (f_day + f_night) / 2
```

With f_day = 2.0 and f_night = 0.5:
```
<f_spatial>_allowed = (2.0 + 0.5) / 2 = 1.25    [EXACT, no approximation]
```

And:
```
n_aniso / n_iso = 1.25    [EXACT ANALYTIC]
```

This prediction requires **no** straight-line approximation, no knowledge of
the dipole field, and no energy-dependent Størmer computation. It follows
entirely from the left-right symmetry of the domain geometry. It is therefore
an exact test of the spatial weighting implementation, independent of the PAD
code and independent of the trajectory integration accuracy.

---

## 6. Observation points and Larmor radius analysis

All PAD tests use three observation points:

| Label | Location (GSM) | r [Re] | λ_geo | T_geo  | Rc_max at 5 GeV |
|-------|---------------|--------|-------|--------|-----------------|
| EQ8   | (8Re, 0, 0)   | 8.0    | 0°    | 0.9960 | Rc_max = 0.93 GV → 384 MeV ≪ 5 GeV ✓ |
| PL8   | (0, 0, 8Re)   | 8.0    | 90°   | 0.9950 | Rc_max = 0 (pole) ✓ |
| EQ6   | (6Re, 0, 0)   | 6.0    | 0°    | 0.9929 | Rc_max = 1.66 GV → 965 MeV ≪ 5 GeV ✓ |

The spatial test uses one point:

| Label | Location (GSM)  | r [Re] | T_geo  | Note |
|-------|-----------------|--------|--------|------|
| DUSK  | (0, 8Re, 0)     | 8.0    | 0.9960 | Dusk meridian; exact dayside/nightside symmetry |

### Why 5–50 GeV?

The straight-line approximation for the reference computation requires the
proton Larmor radius rL to be much larger than the domain size (9 Re):

```
rL = p / (|q| B)    [m]

For a proton at rigidity R [GV]:  rL = R·10⁹ [V·s/m] / B [T]
```

The Larmor radius at each test point at 5 GeV:

| Location         | B [T]       | rL at 5 GeV [Re] | rL / domain |
|------------------|-------------|-------------------|-------------|
| EQ8 (8Re, eq.)   | 6.1×10⁻⁸ T  | 50 Re             | 5.6×         |
| PL8 (8Re, pole)  | 1.2×10⁻⁷ T  | 25 Re             | 2.8×         |
| EQ6 (6Re, eq.)   | 1.4×10⁻⁷ T  | 21 Re             | 2.3×         |

The equatorial B scales as B_eq/r³ = 3.12×10⁻⁵/r³ T; the polar B is twice
that at the same r. At 5 GeV, rL/domain ≥ 2.3 at all points. By 50 GeV,
rL > 20× the domain at every point.

The original TC2 location in the isotropic tests (5 Re pole) was **not used**
for the anisotropic PAD tests because at 5 Re the polar B = 5.0×10⁻⁷ T, giving
rL ≈ 6 Re at 5 GeV — too close to the 9 Re domain for the straight-line
approximation to be reliable. Moving to 8 Re reduces the polar B by (5/8)³ ≈
2.4×, giving rL ≈ 25 Re at 5 GeV.

---

## 7. Semi-analytic reference values

Reference ratios R = n_aniso / n_iso computed by straight-line + dipole-B
(N_θ = 200, N_φ = 400, i.e., 80,000 directions):

| Location | ISOTROPIC | SINALPHA_N n=2 | COSALPHA_N n=2 | SIN² + COS² |
|----------|-----------|----------------|----------------|-------------|
| EQ8 (8Re equator) | 1.00000 | 0.78924 | 0.21076 | **1.00000** |
| PL8 (8Re pole)    | 1.00000 | 0.30231 | 0.69769 | **1.00000** |
| EQ6 (6Re equator) | 1.00000 | 0.71271 | 0.28729 | **1.00000** |

The SIN² + COS² = 1.000 column is the sin²α + cos²α = 1 identity check (§3.5).

### Physical interpretation

**Equator (EQ8, EQ6):** The dipole field at the equatorial domain boundary
exit points is predominantly in the Z-direction (along the dipole axis). Most
trajectories from an equatorial observation point that reach the domain
boundary have an exit velocity with a large equatorial (X or Y) component,
making a substantial angle with the Z-directed field. This gives large sin²α
and small cos²α — equatorial points are dominated by perpendicular particles
and depleted of field-aligned ones.

**Pole (PL8):** Trajectories from a polar observation point exit the domain
mainly through the top and bottom faces of the box (which is close to the
±Z faces). These exits are in the polar region where the dipole field is also
predominantly along Z. The exit velocity is nearly parallel to the field, so
cos²α is large and sin²α is small. The pole is dominated by field-aligned
particles.

This pole-equator contrast is the fundamental reason that field-aligned SEP
beams (COSALPHA_N) have enhanced polar access (PL8 ratio = 0.70) and
suppressed equatorial access (EQ8 ratio = 0.21).

Absolute densities (5–50 GeV, J₀ = 1×10⁴ m⁻² s⁻¹ sr⁻¹ MeV⁻¹, γ = 2):

| Location | n_iso [m⁻³]   | n_SINALPHA [m⁻³] | n_COSALPHA [m⁻³] |
|----------|---------------|------------------|------------------|
| EQ8      | 7.553×10⁻⁴    | 5.961×10⁻⁴       | 1.592×10⁻⁴       |
| PL8      | 7.553×10⁻⁴    | 2.283×10⁻⁴       | 5.270×10⁻⁴       |
| EQ6      | 7.529×10⁻⁴    | 5.366×10⁻⁴       | 2.163×10⁻⁴       |

Note: the lower absolute values compared to the isotropic test (which uses 2–20 GeV or
50–500 MeV depending on the point) reflect the smaller energy integration range and the
γ = 2 spectrum falling off as E⁻². The ratios, not the absolute values, are what the
PAD tests primarily verify.

---

## 8. Does Earth's presence affect the anisotropic analytic estimate?

The same two effects from the isotropic README apply here, plus a third
specific to the anisotropic case.

### Effect 1: Geometric shadow (always present)

The inner sphere blocks a cone of solid angle, exactly as in the isotropic
case. T_geo(8Re) = 0.996 at both the equatorial and polar test points, so the
correction is at the 0.4% level and does not significantly affect the ratio.

### Effect 2: Geomagnetic shielding (not present at these energies)

All test energies (5–50 GeV) are above Rc_max at every test point, so the
Størmer forbidden zone is empty and A_k depends only on the geometric shadow.
This is the reason 5–50 GeV was chosen: to decouple the PAD test from the
shielding test.

If lower energies were used at an equatorial point, some directions would
become FORBIDDEN due to geomagnetic shielding. This would change the shape of
the allowed solid angle, and hence change <f_PAD>_allowed — making the
reference harder to compute and polluting the PAD test with shielding effects.

### Effect 3: Non-uniform field at exit points (anisotropic-only)

The dipole field **B**(x_exit) varies in direction and magnitude across the
domain boundary. Trajectories that exit through the top/bottom faces (near the
poles of the box) emerge into a field that is predominantly along Z, so
cos α ≈ ±1. Trajectories exiting through the equatorial faces (±X, ±Y) emerge
into a mostly equatorial field, so cos α ≈ 0.

This means that **the value of f_PAD(cos α) at the exit is determined by
where on the domain box the trajectory exits** — and therefore by the direction
of travel from x₀. The geometry of the dipole field at the boundary (not just
at the observation point) enters the reference computation. This is correctly
captured by the straight-line + dipole-B method in the reference script.

---

## 9. Test-by-test description

### Run A — Regression: ISOTROPIC PAD

**Purpose:** Verify that activating the anisotropic code path with an
ISOTROPIC PAD reproduces the isotropic result exactly. This tests that the
new branch has no bugs that change the output when the weight function is
identically 1.

**Prediction:** n_aniso = n_iso = T_geo · n_free for every point.

**Tolerance:** 2%. This is tighter than the other runs because no physical
approximation is involved — only directional-sampling noise (~1%) matters.

Any failure here indicates a coding bug in the anisotropic code path itself,
not a physics issue.

### Run B — Exact spatial asymmetry

**Purpose:** Verify the DAYSIDE_NIGHTSIDE spatial weight implementation using
an observation point on the GSM Y-axis where the exact analytic ratio is known.

**Prediction:** n_aniso / n_iso = (f_day + f_night) / 2 = 1.25 exactly.

**Why exact:** The dusk-meridian point (0, 8Re, 0) has perfect left-right
symmetry across the GSM X-Z plane. Every dayside exit direction has a mirror
nightside exit with equal probability, so the spatial average is exactly the
arithmetic mean of the two factors. This is independent of the PAD model,
the field model, and the trajectory integration accuracy.

**Tolerance:** 5%. Statistical noise from direction sampling is the only error
source; 5% is generous for N_dirs ~ 576.

### Run C — SINALPHA_N n=2 (perpendicular/pancake PAD)

**Purpose:** Test the SINALPHA_N PAD model against the semi-analytic reference
ratio computed by straight-line + dipole-B.

**Physical context:** A sin²α distribution is the canonical form for
radiation belt trapped particles and for SEP events observed near the equatorial
plane (equatorial mirroring particles). The distribution is zero at α = 0° and
180° (no field-aligned streaming) and maximum at α = 90°.

**Expected ratios:** EQ8 = 0.789, PL8 = 0.302, EQ6 = 0.713.
The equatorial points receive most of the weight because equatorial
trajectories exit through regions where B is nearly parallel to Z, making
most exit velocities (which are predominantly radial or tangential) nearly
perpendicular to B — large sin²α.

**Tolerance:** 10% for EQ8 and PL8 (rL/domain ≥ 2.8), 15% for EQ6
(rL/domain ≈ 2.3 at 5 GeV — marginal straight-line approximation).

### Run D — COSALPHA_N n=2 (field-aligned beam)

**Purpose:** Test the COSALPHA_N PAD model. This is the complement of Run C.

**Physical context:** |cos α|² is the standard form for a field-aligned beam
such as an SEP event streaming along the IMF. The distribution peaks at α = 0°
and 180° (parallel and anti-parallel to **B**) and is zero at α = 90°.

**Expected ratios:** EQ8 = 0.211, PL8 = 0.698, EQ6 = 0.287.
The polar point PL8 has much higher access than the equatorial points
because field-aligned trajectories connect directly to the polar domain
boundary, where the exit field is approximately along **B**.

**Self-consistency check:** ratio_C + ratio_D = 1.000 at every point (from
sin²α + cos²α = 1). If this check fails after the AMPS run, there is a
systematic error in either the PAD weight implementation or the reference
computation.

---

## 10. Spectrum used

Same power-law as in the isotropic tests:
```
J_b(E) = J₀ · (E / E₀)^(−γ)   [m⁻² s⁻¹ sr⁻¹ MeV⁻¹]

J₀ = 1.0 × 10⁴  m⁻² s⁻¹ sr⁻¹ MeV⁻¹
E₀ = 100 MeV
γ  = 2.0
```

The energy range 5–50 GeV was chosen for the PAD tests specifically to ensure
rL >> domain (§6), not for astrophysical relevance. In a production SEP run
one would use 10–500 MeV for the energetically significant range, but that
would require a more detailed reference computation (Størmer-corrected solid
angle fraction, not tractable analytically for the PAD-weighted case).

---

## 11. Tolerance justification

### Run A (regression)
The only error is directional-sampling noise in T_aniso:
```
σ_T / T ≈ 1/√N_dirs ≈ 4%  for N_dirs ~ 576
```
The density integral is a sum over N_E energy bins. Sampling errors at
different bins are independent, so the relative error in n is reduced to
roughly 4%/√N_E ≈ 1% for N_E = 40. The 2% tolerance is therefore comfortable.

### Run B (exact spatial)
The prediction is exact by symmetry; the only error is direction sampling.
The 5% tolerance matches the isotropic test tolerance.

### Runs C and D (semi-analytic PAD)
Two error sources accumulate:

1. **Straight-line approximation error.** The actual AMPS trajectory curves
   under the magnetic force. The reference assumes a straight path to the exit
   point. The fractional error in the exit position scales roughly as:
   ```
   δx / domain ~ (domain / rL)²
   ```
   For EQ6 at 5 GeV (rL ≈ 21 Re, domain = 9 Re):
   ```
   δx / domain ~ (9/21)² ≈ 0.18
   ```
   This translates to a systematic error in cos α at the exit of a few
   percent, giving a systematic error in the ratio of up to ~5% at EQ6.
   For EQ8 and PL8 (rL ≥ 25 Re at 5 GeV), the error is ~(9/25)² ≈ 0.13.

2. **Direction-sampling noise** in both the AMPS run and the reference.
   The reference uses N = 80,000 directions; AMPS uses ~576 per energy bin.
   The AMPS sampling dominates at ~4%.

Combined in quadrature: ~√(5² + 4²) ≈ 6–7%. Tolerances of 10% (EQ8, PL8)
and 15% (EQ6) give approximately 1.5–2.5σ margin.

---

## 12. How to extend these tests

### Add BIDIRECTIONAL PAD

Run AMPS with `BA_PAD_MODEL BIDIRECTIONAL` and n = 2. Because the formula is
identical to COSALPHA_N, the output should match Run D to within sampling noise
(~2%). This tests the parser and model-dispatch code without requiring a new
reference computation.

### Test different exponents n

The sin²+cos²=1 identity generalises: for SINALPHA_N (exponent n) and
COSALPHA_N (same n), the sum of the ratios equals:
```
<sinⁿα>_allowed + <cosⁿα>_allowed ≠ 1    (for n ≠ 2)
```
So the identity check is specific to n = 2. For other n, use the reference
script with `pad_n` set accordingly. Physically interesting values:
- n = 1: softer distributions (intermediate isotropy)
- n = 4: sharper beams (extreme solar events or loss-cone edges)
- n = 0: reduces to ISOTROPIC (useful additional regression)

### Test combined PAD + spatial

Run with COSALPHA_N + DAYSIDE_NIGHTSIDE simultaneously. At the dusk-meridian
point, the exact ratio is:
```
n_aniso / n_iso = (f_day · <cos²α>_day + f_night · <cos²α>_night)
                / <cos²α>_UNIFORM
```
where <cos²α>_day and <cos²α>_night are the average cos²α restricted to
dayside and nightside exit points respectively. These can be computed by the
reference script with minor modifications.

### Compare with and without geomagnetic shielding

Run the same PAD tests at lower energy (e.g., 50–500 MeV at EQ8 where
Rc_max = 384 MeV), where some trajectories are Størmer-forbidden. In this
regime, the ratio n_aniso / n_iso will differ from the above-cutoff reference
because the forbidden directions change the shape of the allowed solid angle —
and the weighting of that solid angle by f_PAD. This is not analytically
tractable but can be compared between different field models (DIPOLE vs T96
quiet-time) to check physical consistency.

---

## 13. Files

| File | Description |
|------|-------------|
| `test_aniso_regression.in`  | Run A: ANISOTROPIC mode, ISOTROPIC PAD, 3 points |
| `test_aniso_spatial.in`     | Run B: ANISOTROPIC mode, DAYSIDE_NIGHTSIDE, 1 dusk point |
| `test_aniso_sinalpha.in`    | Run C: ANISOTROPIC mode, SINALPHA_N n=2, 3 points |
| `test_aniso_cosalpha.in`    | Run D: ANISOTROPIC mode, COSALPHA_N n=2, 3 points |
| `test_density_aniso.py`     | Reference computation + comparison script |
| `README_aniso_density_tests.md` | This document |

See also:
- `README_density_tests.md` — isotropic tests (TC1–TC4), Størmer cutoff physics
- `test_density_analytic.py` — isotropic comparison script
