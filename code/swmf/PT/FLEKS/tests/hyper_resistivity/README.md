# Hybrid PIC Hyper-Resistivity Test

Quantitative test of the fourth-order hyper-resistive term $-\eta_h \nabla^2 \mathbf{J} = -\frac{\eta_h}{4\pi} \nabla \times (\nabla^2 \mathbf{B})$ in the Hybrid PIC generalized Ohm's law.

## Setup

A frozen plasma without macroparticles (`#PARTICLES 0 0 0`) isolates the hyper-resistive term driving $\mathbf{B}$, as all other Ohm's law terms vanish when $\rho = 0$. A single circularly polarized transverse mode is seeded on a uniform guide field $B_{x0}$:

$$
\mathbf{B} = (B_{x0},\, B_1 \cos kx,\, B_1 \sin kx), \quad k = \frac{2\pi \cdot \text{waveMode}}{L_x}
$$

The transverse magnetic energy decays exponentially:

$$
E_b(t) - E_{b,\text{guide}} = A e^{-2\gamma t}
$$

with discrete damping rate:

$$
\gamma = \frac{\eta_h}{4\pi} \frac{4 \sin^2(\theta) \sin^2(\theta/2)}{\Delta x^4}, \quad \theta = k \Delta x
$$

which accounts for the collocated $2\Delta x$ curl and 3-point Laplacian stencils.

## Validation

`validate.py` verifies:
1. **Dissipation**: Magnetic energy $E_b$ decreases monotonically and remains finite.
2. **Exponential Decay**: Fitted residual is $< 10^{-6}$ of the amplitude.
3. **Damping Rate**: Measured rate matches the analytic prediction (including RK4 time discretization) within 0.5%.

## Running

```bash
python3 tests/validate_tests.py --test=hyper_resistivity
```
