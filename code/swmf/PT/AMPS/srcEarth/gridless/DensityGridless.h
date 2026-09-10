//======================================================================================
// DensityGridless.h
//======================================================================================
/**
 * \file DensityGridless.h
 *
 * Public interface for the GRIDLESS energetic-particle density + spectrum calculator.
 *
 * WHY THIS MODULE EXISTS
 * ----------------------
 * The gridless cutoff-rigidity tool determines, for a given location and direction, whether
 * a particle of a given rigidity can access the location from outside the magnetosphere
 * (ALLOWED) or is blocked/absorbed (FORBIDDEN). That is a *geometric/topological* classifier.
 *
 * For many applications we also need *physics outputs* such as:
 *   - local energetic-particle spectrum at a point inside the magnetosphere, and
 *   - local total number density integrated over an energy band.
 *
 * This module reuses exactly the same backtracing classifier as cutoff rigidity, but instead
 * of searching for a single cutoff rigidity, it evaluates the allowed fraction T(E) across
 * an energy grid and folds it with a boundary spectrum J_b(E).
 *
 * THEORY SUMMARY
 * --------------
 * For each observation point x0 and kinetic energy E:
 *   (1) Backtrace a set of directions and compute transmissivity:
 *         T(E;x0) = N_allowed / N_dirs
 *   (2) Local differential intensity (isotropic assumption):
 *         J_loc(E;x0) = T(E;x0) * J_b(E)
 *   (3) Total number density:
 *         n_tot(x0) = 4*pi * integral_{Emin}^{Emax} [ J_loc(E;x0) / v(E) ] dE
 *
 * where v(E) is the relativistic particle speed at kinetic energy E.
 *
 * INPUT CONTRACT
 * --------------
 * This solver requires:
 *   CALC_TARGET            DENSITY_SPECTRUM
 *   FIELD_EVAL_METHOD      GRIDLESS
 * and a #DENSITY_SPECTRUM section defining the energy grid and optional work caps.
 *
 * OUTPUT_DOMAIN modes supported:
 *   - POINTS : explicit observation points.
 *   - SHELLS : one or more spherical shells defined by altitude(s) and angular
 *              resolution (lon/lat grid).
 *
 * OUTPUT CONTRACT
 * ---------------
 * POINTS:
 *   Writes three Tecplot files (rank 0):
 *     - gridless_points_density.dat
 *         Variables: X_km Y_km Z_km N_m3 N_cm3
 *         Number density integrated over [DS_EMIN, DS_EMAX].
 *
 *     - gridless_points_spectrum.dat
 *         One ZONE per observation point.
 *         Variables: E_MeV  T  J_boundary_perMeV  J_local_perMeV
 *
 *     - gridless_points_flux.dat
 *         Variables: X_km  Y_km  Z_km  F_tot_m2s1  [F_NAME_m2s1 ...]
 *         F_tot = 4π ∫ T(E)·J_b(E) dE  over [DS_EMIN, DS_EMAX]  [m^-2 s^-1].
 *         Additional columns F_NAME_m2s1 are added for each user-defined channel
 *         from the #ENERGY_CHANNELS section of the input file.
 *         If #ENERGY_CHANNELS is absent, only F_tot is written.
 *
 *   KEY DISTINCTION: density vs flux
 *     n(x0)     = 4π ∫ J_loc(E)/v(E) dE   [m^-3]     (1/v weight; density)
 *     F_tot(x0) = 4π ∫ J_loc(E)     dE   [m^-2 s^-1] (no weight; flux)
 *   For ultra-relativistic particles (v≈c): F_tot ≈ n · c.
 *
 * SHELLS:
 *   Writes one Tecplot file per shell altitude A[km] (rank 0):
 *     - gridless_shell_Akm_density_channels.dat
 *   Each such file contains multiple Tecplot ZONEs:
 *     - ZONE 1: total density integrated over the full energy band
 *     - ZONE 2..: density contributions from individual energy channels
 *                (one zone per energy interval [E_i,E_{i+1}]).
 *
 * See DensityGridless.cpp for full details and derivations.
 */


#ifndef _SRC_EARTH_GRIDLESS_DENSITYGRIDLESS_H_
#define _SRC_EARTH_GRIDLESS_DENSITYGRIDLESS_H_

#include "util/amps_param_parser.h"

namespace Earth {
  namespace GridlessMode {
    // Returns 0 on success; throws std::runtime_error on invalid input.
    int RunDensityAndSpectrum(const EarthUtil::AmpsParam& p);
  }
}

#endif
