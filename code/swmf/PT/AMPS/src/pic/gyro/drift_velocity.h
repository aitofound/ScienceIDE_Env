#ifndef _PIC_GYROKINETIC_DRIFT_VELOCITY_H_
#define _PIC_GYROKINETIC_DRIFT_VELOCITY_H_

/*
  ==========================================================================================
  Gyrokinetic / guiding-center drift velocity helper (Elkington-2002-JASTP style)
  ==========================================================================================

  Purpose
  -------
  Provide a compact, reuse-friendly implementation of the *gyro-averaged* perpendicular
  drift velocity used by the guiding-center / drift-kinetic particle model:

      v_drift = v_{E×B} + v_{∇B} + v_{curv}

  computed using a *reduced particle state*:
      - vParallel : scalar parallel velocity,  v_parallel = v · b
      - vNorm     : scalar perpendicular speed magnitude, |v_perp|
      - spec      : species id (used to obtain mass/charge and apply normalization)

  This function is intended for:
      * guiding-center (GC) movers (electrons in ECSIM, etc.)
      * diagnostics (output of drift speed, drift Mach number, etc.)
      * hybrid models that need a drift-kinetic estimate of particle perpendicular motion

  It is NOT intended to:
      * reconstruct gyrophase-resolved velocity components
      * model unmagnetized motion near B≈0 (the routine will safely return zero drifts there)

  Key design choices
  ------------------
  (1) Reduced-state interface:
      The drift expressions depend on the particle only through p_parallel and μ:
          p_parallel = m * vParallel   (non-relativistic)
          μ = (1/2) * m * vNorm^2 / B
      Therefore, the perpendicular direction of v_perp (gyrophase) is not needed.

  (2) Field access and interpolation stencil contract:
      The routine uses the *current* CPLR interpolation stencil
          PIC::InterpolationRoutines::CellCentered::StencilTable
      to evaluate the E×B drift as a weighted average of per-stencil-cell drifts:
          Σ w_i (E_i×B_i)/|B_i|^2
      This matches the historical implementation in PIC::CPLR::GetDriftVelocity().

      IMPORTANT: The caller MUST initialize the stencil for the position of interest by calling:
          PIC::CPLR::InitInterpolationStencil(x,node)
      immediately before calling PIC::GYROKINETIC::GetDriftVelocity().
      If the stencil is stale, the drift corresponds to the wrong location.

  (3) Units and normalization:
      AMPS stores species mass/charge tables in SI units. When the field solver delivers
      E and B in normalized (dimensionless) units
          _PIC_FIELD_SOLVER_INPUT_UNIT_ == _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_
      this routine converts mass and charge SI→normalized using picunits::si2no_m/q and
      PIC::Units::Factors, mirroring the approach used in the Boris mover for ECSIM.

      The drift expressions are written for the same Lorentz-force convention used by the
      SI-form Boris pusher in AMPS:
          dv/dt = (q/m) (E + v×B)
      (i.e., no explicit 1/c factor). If your force convention differs, the E×B term must
      be adjusted accordingly.

  (4) Relativity:
      This helper is deliberately NON-RELATIVISTIC (γ=1) to match the intended GC-electron
      use case you described. If relativistic support is needed later, γ should be computed as
          γ = 1 / sqrt(1 - v^2/c^2),  v^2 = vParallel^2 + vNorm^2,
      and used consistently in μ, p_parallel, and drift prefactors.

  API
  ---
      void PIC::GYROKINETIC::GetDriftVelocity(
          double *vDrift,   // output: drift velocity vector (3)
          double vParallel, // input: scalar v_parallel = v·b
          double vNorm,     // input: scalar |v_perp|
          int    spec       // input: species id
      );

  Output
  ------
  vDrift[0..2] contains the perpendicular drift velocity. If B is zero/invalid or any
  intermediate calculation produces non-finite values, the routine returns vDrift = 0.

  Threading
  ---------
  In HYBRID mode (OpenMP), the function uses the per-thread stencil from StencilTable.

  ==========================================================================================
*/

namespace PIC {
namespace GYROKINETIC {
  void GetDriftVelocity(double *vDrift,double vParallel,double vNorm,int spec);
}
}

#endif

