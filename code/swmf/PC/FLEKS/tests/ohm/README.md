# Hybrid PIC Full Generalized Ohm's Law Test

Quantitative test of the complete Hybrid PIC (kinetic ions, massless fluid electrons) field solver exercising all four terms in the generalized Ohm's law:

$$
\mathbf{E} = -\mathbf{u}_i \times \mathbf{B} + \frac{\mathbf{J} \times \mathbf{B}}{e n_e} + \eta \mathbf{J} - \frac{\nabla P_e}{e n_e}
$$

1. **Convection**: $-\mathbf{u}_i \times \mathbf{B}$ (ion velocity).
2. **Hall**: $(\mathbf{J} \times \mathbf{B}) / (e n_e)$ (sub-cycled via `#BSUBCYCLE`).
3. **Resistive**: $\eta \mathbf{J}$ (enabled by `#RESISTIVITY > 0`).
4. **Electron pressure gradient**: $-\nabla P_e / (e n_e)$ (enabled by `#ELECTRONTEMPERATURE > 0`).

## Setup

The test initializes a 1D periodic plasma with a circularly polarized transverse whistler wave (`#TESTCASE HybridWave`) on a uniform guide field $B_{x0}$ along $x$.

Ion-scale normalization is used ($l_\text{norm} \approx d_i$, $u_\text{norm} \approx v_A$), with 2000 particles per cell for kinetic ions. Unlike [`whistler`](../whistler/README.md), which isolates the Hall term, this test activates both finite resistivity ($\eta = 2.0 \times 10^9\ \text{m}^2/\text{s}$) and electron temperature ($T_e = 20\ \text{eV}$) to verify coupled stability and resistive dissipation.

## Validation

`validate.py` verifies:
1. **Field & Particle Energy Stability**: Reuses shared hybrid validation (`tests/_shared/hybrid.py`) to confirm finite energies and stable field advance.
2. **Seeded Mode Preservation**: Verifies dominant $n = 1$ mode in transverse magnetic field perturbations.
3. **Resistive Damping Bound**: Asserts that late/early transverse amplitude growth $\max |B_\perp|$ remains below $1.9\times$ (measured $\approx 1.65\times$ with active resistivity vs. $\approx 2.16\times$ without resistivity).

## Running

```bash
python3 tests/validate_tests.py --test=ohm
```
