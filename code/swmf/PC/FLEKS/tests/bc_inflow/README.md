# Inflow / Outflow Open-Boundary Hybrid Test

## Physics

A uniform magnetized hybrid plasma (kinetic ions + massless fluid electrons)
streams along `+x` through a 1D domain.  The `-x` face is an **inflow**
boundary (`BC::inflow`) and the `+x` face is an **outflow** boundary
(`BC::outflow`).

A perfectly uniform streaming plasma is the exact steady solution, so this test
validates that the inflow/outflow pair keeps a uniform state uniform:

* the upstream density `rhoS0` and bulk velocity `uxS0` are preserved at the
  inflow face,
* the guide field `Bx` stays uniform and unchanged,
* no spurious electric field develops,
* ion kinetic energy `Epart` and magnetic energy `Eb` stay finite and bounded.

## The `#INFLOW` command

This test uses the `#INFLOW` command to prescribe the upstream state at the
inflow face (in SI units, mirroring `#UNIFORMSTATE`):

```
#INFLOW
5.0          rho [amu/cc]
20.0         ux [km/s]
0.0          uy [km/s]
0.0          uz [km/s]
10000.0      T [K]
```

When an `inflow` face is present and this block is supplied:

1. **Field BC** — `Pic::apply_inflow_wall` (in `src/Pic.cpp`) fills the ghost
   cells with a zero-gradient copy of the adjacent edge cell for every
   component (ghost B = edge B, ghost E = edge E).  The upstream B field is set
   by `#UNIFORMSTATE`, not by `#INFLOW`, and is NOT pinned by this command; the
   upstream state is supplied by the particle injection below.

2. **Particle BC** — `Particles::inject_flux_at_inflow_faces` (in
   `src/Particles.cpp`) injects flux-weighted particles at the boundary face,
   sampled from a drifting Maxwellian with the prescribed upstream density,
   bulk velocity, and temperature (the `InflowVel` struct on `FluidInterface`),
   which supplies the upstream mass/current into the domain.

Outgoing particles that cross the inflow face outward are removed and tallied
(in `Particles::reflect_or_delete_particle`), matching the `absorb` behaviour.

Without an `#INFLOW` block, an `inflow` face falls back to the zero-gradient
ghost copy (the original open-boundary behaviour): `Pic::use_float` copies the
adjacent physical cell's field, and the particles are sampled from the live
fluid-interface state.

## Running

```bash
python3 tests/validate_tests.py --test=bc_inflow
```

## Validation

`validate.py` checks:

1. **Energy log** (`validate_log`): `Etot`/`Ee`/`Eb`/`Epart` finite; `Epart`
   bounded (open BCs inject/remove particles every step, so the ratio
   tolerance is loose); `Eb` within ~20% of its initial value (uniform `Bx`); `Ee`
   negligible (no spurious E build-up).
2. **Plot output** (`validate_plot`):
   * `<uxS0>` conserved across the domain,
   * guide field `Bx` uniform and unchanged,
   * inflow-side (first third of the domain) `rhoS0` preserved to ~10%
     (the inflow maintains the upstream state rather than draining),
   * no spurious mean `Ex`/`Ey`/`Ez`.
