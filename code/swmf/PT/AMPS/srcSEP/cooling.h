#include <iostream>
#include <cmath>

#ifndef _COOLING_FACTOR_PARKER_
#define _COOLING_FACTOR_PARKER_

/*
 * Namespace: COOLING_FACTOR_PARKER
 * This namespace contains functions for calculating the adiabatic cooling factor
 * for Solar Energetic Particles (SEPs) in the heliosphere, based on the Parker
 * transport equation. The adiabatic cooling accounts for the energy loss of SEPs
 * due to the expansion of the solar wind.
 *
 * The radial distance from the Sun (r) is the heliocentric distance in meters.
 *
 * References:
 * - Parker, E. N. (1965). "The passage of energetic charged particles through interplanetary space."
 *   Planetary and Space Science, 13(1), 9-49.
 * - Schlickeiser, R. (2002). *Cosmic Ray Astrophysics*. Springer.
 * - Zhang, M. (2000). "Adiabatic cooling of solar energetic particles."
 *   The Astrophysical Journal, 541(2), 428-436.
 */

namespace COOLING_FACTOR_PARKER {

    // Physical constants
    const double c = 2.99792458e8;      // Speed of light in m/s
    const double e = 1.602176634e-19;   // Elementary charge in C
    const double mp = 1.67262192369e-27; // Proton mass in kg
    const double AU = 1.495978707e11;   // Astronomical Unit in meters

    /*
     * Function: calculateAdiabaticCooling
     * Calculates the new momentum of a particle after a time step due to adiabatic cooling
     * in the expanding solar wind.
     *
     * Inputs:
     * - p_old: Particle's initial momentum in kg·m/s (must be > 0)
     * - r: Heliocentric distance from the Sun in meters (must be > 0)
     * - dt: Time step in seconds (must be > 0)
     * - V_sw: Solar wind speed in m/s (typical values between 400e3 and 800e3 m/s)
     *
     * Output:
     * - p_new: Particle's new momentum in kg·m/s after adiabatic cooling
     *
     * Assumptions:
     * - The solar wind is radial and has a constant speed V_sw.
     * - The divergence of the solar wind velocity is given by ∇·V_sw = 2 V_sw / r.
     * - Uses non-relativistic approximation for momentum update (valid for v << c).
     * - The radial distance r is the heliocentric distance from the Sun.
     *
     * References:
     * - Parker, E. N. (1965). "The passage of energetic charged particles through interplanetary space."
     *   Planetary and Space Science, 13(1), 9-49.
     */
    extern double calculateAdiabaticCooling(double p_old, double r, double dt, double V_sw);
} // namespace COOLING_FACTOR_PARKER

#endif
