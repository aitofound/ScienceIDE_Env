#include <iostream>
#include <cmath>

#include "cooling.h"

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
    double calculateAdiabaticCooling(double p_old, double r, double dt, double V_sw) {
        // Validate inputs
        if (p_old <= 0) {
            std::cerr << "Error: Particle momentum must be positive." << std::endl;
            return -1.0;
        }
        if (r <= 0) {
            std::cerr << "Error: Heliocentric distance must be positive." << std::endl;
            return -1.0;
        }
        if (dt <= 0) {
            std::cerr << "Error: Time step must be positive." << std::endl;
            return -1.0;
        }
        if (V_sw <= 0) {
            std::cerr << "Error: Solar wind speed must be positive." << std::endl;
            return -1.0;
        }

        // Avoid division by zero near the Sun by setting a minimum heliocentric distance
        const double r_min = 1e7; // Minimum distance (e.g., 10,000 km) in meters
        if (r < r_min) {
            r = r_min;
        }

        // Compute divergence of solar wind velocity: ∇·V_sw = 2 V_sw / r
        double divergence_Vsw = (2.0 * V_sw) / r; // Units: 1/s

        // Rate of change of momentum due to adiabatic cooling: dp/dt = - (1/3) p * ∇·V_sw
        double dp_dt = - (1.0 / 3.0) * p_old * divergence_Vsw; // Units: kg·m/s²

        // Update momentum: p_new = p_old + dp/dt * dt
        double p_new = p_old + dp_dt * dt; // Units: kg·m/s

        // Ensure that momentum does not become negative
        if (p_new < 0) {
            p_new = 0.0;
        }

        return p_new;
    }

} // namespace COOLING_FACTOR_PARKER


