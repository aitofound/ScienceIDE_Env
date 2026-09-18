//======================================================================================
// AnisotropicSpectrum.h
//======================================================================================
//
// PURPOSE
// -------
// Evaluate the per-trajectory anisotropy weight factor  f_aniso = f_PAD * f_spatial
// used by the ANISOTROPIC branch of the DensityGridless density/spectrum solver.
//
// This file is self-contained: it depends only on EarthUtil::AnisotropyParam (from
// amps_param_parser.h) and the C++ standard library.
//
//======================================================================================
// PHYSICS BACKGROUND AND MOTIVATION
//======================================================================================
//
// 1. WHY THE ISOTROPIC BRANCH IS INSUFFICIENT
// --------------------------------------------
// The default isotropic density/spectrum solver computes a local differential intensity:
//
//   J_loc(E; x0) = T(E; x0) * J_b(E)               ... (1)
//
// where:
//   T(E; x0) = (1/N_dirs) * sum_k A_k(E; x0)       ... (2)
//
// is a direction-averaged transmissivity (A_k in {0,1} = allowed/forbidden for trajectory
// k), and J_b(E) is a single scalar boundary spectrum independent of direction.
//
// Equation (1) is exact only if the boundary distribution is *isotropic and uniform*:
// J_b is the same for every asymptotic arrival direction and at every point on the outer
// domain boundary. This is a reasonable assumption for background galactic cosmic rays (GCR)
// during quiet solar conditions but fails in the following scientifically important cases:
//
//   (a) Solar energetic particle (SEP) events, especially near the onset:
//       SEPs stream along the interplanetary magnetic field (IMF) from the acceleration
//       site. The arriving flux has a strongly *field-aligned* pitch-angle distribution
//       (PAD): particles arrive preferentially with their velocity vectors nearly parallel
//       or anti-parallel to the local IMF (|cos alpha| >> 0).  During large events the
//       anisotropy can exceed 100:1 relative to the perpendicular direction.  Equations
//       (1)-(2) would assign equal weight to a field-aligned allowed trajectory and a
//       cross-field allowed trajectory, overcounting the contribution of the latter and
//       undercounting the former.
//
//   (b) Radiation belt electrons with a "pancake" PAD:
//       Near the magnetic equator of a dipole-like field the trapped population preferentially
//       occupies pitch angles near 90 degrees (sin^n distribution). A solver that treats the
//       outer boundary injection as isotropic will misrepresent the equatorial filling factor.
//
//   (c) Bidirectional streaming distributions (e.g., field-aligned beams from opposite legs
//       of a closed magnetic loop; SEPs trapped between two acceleration sites during CME
//       passage). These produce |cos(alpha)|^n distributions symmetric about alpha = 90 deg.
//
//   (d) Spatial non-uniformity at the boundary:
//       During a major geomagnetic storm the flux of energetic particles at the dayside
//       magnetopause may differ from the nightside by an order of magnitude due to solar wind
//       ram pressure, sheath fields, or the CME arrival front geometry. Using a single J_b(E)
//       for the entire outer boundary introduces a bias whose sign depends on whether the
//       observation point preferentially "sees" the dayside or nightside boundary.
//
// 2. THE ANISOTROPIC EXTENSION
// ----------------------------
// The exact local differential intensity for an arbitrary boundary distribution is:
//
//   J_loc(E; x0) = (1/4*pi) * integral_S2  A(E, Omega; x0) * J_b(E, Omega_inf, x_inf) dOmega
//                                                                                         ... (3)
//
// where:
//   Omega        = arrival direction at x0 (solid angle element)
//   A(E, Omega)  = 1 if direction Omega is magnetically allowed, 0 otherwise
//   Omega_inf    = the asymptotic direction at the outer boundary for a particle that
//                  arrives from direction Omega at x0
//   x_inf        = the GSM position on the outer domain surface where the backtraced
//                  trajectory exits
//   J_b(E, Omega_inf, x_inf) = the boundary differential intensity at that exit point and
//                               asymptotic direction
//
// Discretising equation (3) over a uniform direction grid of N_dirs directions:
//
//   J_loc(E; x0) ~= (1/N_dirs) * sum_k  A_k * J_b(E, Omega_inf_k, x_inf_k)             ... (4)
//
// This is a Monte Carlo estimator for the integral (3): each allowed trajectory k
// contributes a sample of the boundary intensity in the direction that particle actually
// came from asymptotically, at the spatial location where it entered the domain.
//
// 3. FACTORED REPRESENTATION IMPLEMENTED HERE
// --------------------------------------------
// Computing a fully general J_b(E, Omega_inf, x_inf) requires a 5-dimensional
// tabulation (2 energy + 2 direction + 2 position degrees of freedom) which is not yet
// provided by the AMPS boundary/spectrum module.  Instead we use a separable
// approximation:
//
//   J_b(E, Omega_inf, x_inf) ~= J_b_iso(E) * f_PAD(cos_alpha(Omega_inf, x_inf)) * f_spatial(x_inf)
//                                                                                         ... (5)
//
// where:
//   cos_alpha = v_exit_hat . B_hat(x_inf)  is the cosine of the pitch angle at the exit
//               point, computed using the magnetic field B at x_inf
//   f_PAD     = pitch-angle distribution weight (dimensionless, >= 0)
//   f_spatial = spatial modulation factor       (dimensionless, >= 0)
//
// Substituting (5) into (4):
//
//   J_loc(E; x0) ~= J_b_iso(E) * (1/N_dirs) * sum_k  A_k * f_PAD(cos_alpha_k) * f_spatial(x_inf_k)
//                 = J_b_iso(E) * T_aniso(E; x0)                                          ... (6)
//
// So T_aniso(E; x0) is an *anisotropy-weighted transmissivity*:
//
//   T_aniso(E; x0) = (1/N_dirs) * sum_k  A_k * f_aniso_k                                ... (7)
//
// where f_aniso_k = f_PAD(cos_alpha_k) * f_spatial(x_inf_k) is what this module computes.
//
// The downstream density integral is then structurally identical to the isotropic case:
//
//   n_tot(x0) = 4*pi * integral_{Emin}^{Emax}  J_b_iso(E) * T_aniso(E; x0) / v(E) dE  ... (8)
//
// This is a key design advantage: only the weighting inside T changes; the outer
// integration framework remains untouched.
//
//======================================================================================
// NORMALIZATION CONVENTION
//======================================================================================
//
// Neither f_PAD nor f_spatial is pre-normalized to unity over the sphere.  For the
// reference case where all weights equal 1 (f_PAD = f_spatial = 1), equation (7)
// reduces to the isotropic transmissivity:
//
//   T_aniso = T_iso = N_allowed / N_dirs.
//
// This is the intended normalization: the isotropic and anisotropic solvers produce
// identical output when BA_PAD_MODEL = ISOTROPIC and BA_SPATIAL_MODEL = UNIFORM.
//
// For other models the *absolute value* of n_tot will differ from the isotropic result.
// This is physically correct: the anisotropy changes how much flux actually reaches x0.
// Users who want only the *relative* change (ratio to quiet time) should post-process.
//
//======================================================================================
// PAD MODELS
//======================================================================================
//
// BA_PAD_MODEL selects the functional form of f_PAD(cos_alpha).
//
// ISOTROPIC   :  f(alpha) = 1
//                Identity; reduces ANISOTROPIC branch to isotropic transmissivity.
//                Useful as a sanity check that both branches agree when this is set.
//
// SINALPHA_N  :  f(alpha) = sin^n(alpha) = (1 - cos^2(alpha))^(n/2)
//                Pancake distribution peaking at alpha = 90 deg (particles crossing
//                field lines perpendicularly).  Physically appropriate for:
//                  - Radiation belt electrons near the equatorial plane
//                  - Mirror-force-reflected populations filling equatorial loss cones
//                  - Any population produced by perpendicular heating
//                Limit n -> inf narrows the distribution toward exactly 90 deg.
//                Limit n -> 0 recovers isotropic (f = 1).
//
// COSALPHA_N  :  f(alpha) = |cos(alpha)|^n
//                Field-aligned streaming beam peaking at alpha = 0 and alpha = 180 deg.
//                Physically appropriate for:
//                  - SEP events at onset: focused transport along IMF
//                  - Loss cone particles precipitating into the atmosphere
//                  - Strahl electrons (unidirectional variant would require signed cos)
//                Limit n -> inf gives a pencil beam along the field.
//                Limit n -> 0 recovers isotropic.
//
// BIDIRECTIONAL: f(alpha) = |cos(alpha)|^n  (same math as COSALPHA_N)
//                A symmetric beam with equal intensity in the +B and -B directions.
//                Distinguishing this from COSALPHA_N by name documents that the source
//                population is genuinely bidirectional (e.g., particles trapped on a
//                closed magnetic loop, observed by Solar Orbiter during some CME events).
//                The |cos| factor automatically provides the correct symmetry.
//
// Exponent n is specified by BA_PAD_EXPONENT (default 2.0).
// Physical ranges: n >= 0.  Values 1, 2, 4, 8 are commonly used in SEP literature
// (Gaussian approximations to focused-transport steady-states).
//
//======================================================================================
// SPATIAL MODELS
//======================================================================================
//
// BA_SPATIAL_MODEL selects the functional form of f_spatial(x_inf).
// x_inf is in GSM coordinates [m].
//
// UNIFORM           :  f_spatial = 1.0  everywhere
//                      No spatial variation.  Default (backward-compatible with isotropic
//                      solver when BA_PAD_MODEL = ISOTROPIC is also set).
//
// DAYSIDE_NIGHTSIDE :  f_spatial = BA_DAYSIDE_FACTOR   if GSM X_inf > 0 (sunward/dayside)
//                                = BA_NIGHTSIDE_FACTOR  if GSM X_inf <= 0 (tailward/nightside)
//                      Step function at the noon-midnight meridian (GSM X = 0).
//                      Appropriate for:
//                        - CME arrival front impacting the dayside magnetopause
//                        - Substorm injection from the nightside plasma sheet
//                        - Day/night asymmetry in the solar wind particle access
//                      BA_DAYSIDE_FACTOR and BA_NIGHTSIDE_FACTOR are set independently.
//                      Setting one to zero models a one-sided boundary source.
//
//======================================================================================
// HOW TO EXTEND THIS MODULE WITH MORE COMPLEX BOUNDARY SPECTRUM REPRESENTATIONS
//======================================================================================
//
// The current factored model (equation 5) has two limitations:
//
//   (L1) f_PAD and f_spatial are not correlated: the same PAD is applied everywhere on the
//        outer boundary regardless of position.
//
//   (L2) f_spatial is a coarse step function.  In reality the boundary flux can vary
//        smoothly in local time, latitude, and with the orientation of the IMF.
//
// The following upgrade paths are possible without changing the call sites in DensityGridless:
//
//   PATH A — Tabulated PAD at the boundary
//   ----------------------------------------
//   Replace f_PAD with a 2D table in (cos_alpha, energy) evaluated at each exit point
//   (x_inf, Omega_inf) by interpolation into a pre-computed background distribution.
//   The signature of EvalAnisotropyFactor would grow to include the energy E_MeV, and
//   the AnisotropyParam struct would gain a table pointer. TraceAllowedSharedEx already
//   returns both x_exit and cos_alpha, so no change to the trajectory layer is needed.
//
//   PATH B — Full 5D boundary intensity table J_b(E, theta, phi, lon_gsm, lat_gsm)
//   --------------------------------------------------------------------------------
//   The ultimate generalisation is to abandon the factored form entirely and pass
//   (E, Omega_inf, x_inf) into a dedicated 5D lookup.  This requires:
//     (i)  An interpolation library callable as J_b_lookup(E, cos_th, phi, x, y, z).
//     (ii) EvalAnisotropyFactor is no longer the right entry point: DensityGridless
//          would call J_b_lookup directly (or a thin wrapper), and the return type of
//          ComputeT_atEnergy would change from a scalar weight to a per-direction
//          intensity contribution.
//     (iii) For MHD-coupled runs: the boundary table could be populated from a coupled
//          MHD output snapshot (e.g., LFM or BATS-R-US boundary state at the outer box
//          face) interpolated to the Tsyganenko-field outer domain.
//   This path matches the approach used in production radiation belt codes such as
//   VERB-3D / VERB-4D (Subbotin & Shprits 2009) and the Salammbo code (Beutier & Boscher
//   1995) which include a position and energy-dependent boundary condition at L=6--10.
//
//   PATH C — Monte Carlo boundary sampling with the interplanetary transport model
//   -------------------------------------------------------------------------------
//   For SEP events a higher-fidelity approach couples the gridless magnetospheric
//   backtracer to a 1D or 3D SEP transport model (e.g., PARADISE, SEPMOD, ENLIL+SEP):
//     (i)  The interplanetary transport model provides f(E, mu, t) at 1 AU.
//     (ii) For each allowed trajectory k, its asymptotic direction Omega_inf_k is
//          mapped from GSM to GSE to ecliptic coordinates and used to sample
//          f(E, mu = v_inf_k . B_IMF / |v_inf_k|, t).
//     (iii) That value replaces J_b_iso * f_PAD_k in equation (4), entirely bypassing
//          the factored model.
//   The TrajectoryExitState struct already stores x_exit_m and v_exit_unit, which are
//   the only quantities needed to compute the pitch angle relative to the IMF at 1 AU
//   (after coordinate transformation by the caller).
//
//   PATH D — Pitch-angle diffusion at the magnetopause
//   ---------------------------------------------------
//   The magnetopause is not a sharp transmission surface: pitch-angle scattering at the
//   boundary layer can randomize the distribution. A practical extension is to convolve
//   the outgoing PAD with a Gaussian kernel of width sigma_alpha before applying f_PAD:
//
//     f_PAD_scatter(cos_alpha) = integral f_PAD(cos_alpha') * G(cos_alpha - cos_alpha') d(cos_alpha')
//
//   This can be precomputed as a 1D table and passed in via AnisotropyParam.
//
//======================================================================================
// DIFFERENCE FROM OTHER MODELING APPROACHES
//======================================================================================
//
// 1. MAGNETOCOSMICS / PLANETOCOSMICS (Desorgher et al. 2005)
//    MAGNETOCOSMICS computes cutoff rigidities and asymptotic viewing directions but does
//    NOT integrate over energy to produce a flux or density.  It reports T(E; x0) as a
//    sky map. Our solver uses an equivalent T but then integrates J_b * T over E to give
//    n_tot, which MAGNETOCOSMICS does not do.  The anisotropic PAD weighting in this
//    module extends that sky map to a weighted sky map suitable for SEP flux prediction.
//
// 2. GLE analysis pipeline (Mishev, Usoskin et al. 2014-2021)
//    The GLE pipeline assigns Gaussian PAD weights to asymptotic directions of neutron
//    monitor stations in an *inversion* problem: given observed NM count rates, infer
//    the PAD parameters. Our module does the *forward* problem: given a prescribed PAD,
//    compute the local flux. The f_PAD functional forms are the same (|cos|^n for beams,
//    Gaussian approximation at large n), but our context is a spatial grid of observation
//    points rather than a network of NM stations.
//
// 3. Dartmouth geomagnetic cutoff code (Kress et al. 2004, 2010)
//    The Dartmouth code models SEP access using backtracing in MHD or T05/LFM fields to
//    produce cutoff surfaces in L-shell / energy space.  It does not compute a directional
//    flux-weighted transmissivity.  Our approach is complementary: we compute the flux
//    rather than the cutoff boundary.
//
// 4. Radiation belt diffusion codes (VERB, Salammbô, IRBEM-based tools)
//    Codes such as VERB-3D integrate pitch-angle and energy diffusion equations on a fixed
//    L-shell / pitch-angle grid (Subbotin & Shprits 2009; Beutier & Boscher 1995).  They
//    require a boundary condition at the outer L-shell as a function of pitch angle, which
//    is what this module provides from the perspective of a test particle entering from
//    outside. The key difference is that diffusion codes work in adiabatic-invariant
//    coordinates and are appropriate for the radiation belt interior; our module computes
//    the incoming flux at any point including outside the trapping region.
//
// 5. AE9/AP9 statistical models (O'Brien et al. 2013)
//    AE9/AP9 are empirical models of the trapped radiation environment built from
//    satellite measurements. They do not propagate source spectra through the
//    magnetosphere but instead parameterise the observed fluxes directly. Our module
//    is a first-principles complement: it predicts how a given interplanetary source
//    spectrum maps to an internal observation point, allowing event-specific forecasts
//    rather than statistical climatology.
//
//======================================================================================
// ALGORITHM STEP-BY-STEP (how DensityGridless uses this module)
//======================================================================================
//
// The following steps describe the full path for a single observation point x0 and
// energy E in ANISOTROPIC mode.
//
// Step 0: Startup validation
//   - ParseAmpsParamFile reads #DENSITY_SPECTRUM and #BOUNDARY_ANISOTROPY sections.
//   - DS_BOUNDARY_MODE = ANISOTROPIC is validated; AnisotropyParam is populated.
//   - InitGlobalSpectrumFromKeyValueMap prepares ::gSpectrum (the isotropic reference
//     spectrum J_b_iso(E)).
//
// Step 1: Direction grid construction (BuildDirGrid)
//   - Generate N_dirs = nZenith * nAz unit vectors uniformly distributed over the
//     upper hemisphere (equal spacing in cos(theta), uniform in azimuth).
//   - This grid approximates a Monte Carlo estimator for the directional integral (3).
//   - If DS_MAX_PARTICLES is set, deterministically subsample directions so that
//     nDir * nEnergyPoints <= DS_MAX_PARTICLES.
//
// Step 2: For each energy point E_i on the grid
//   Step 2a: Convert E_i from MeV to Joules: Ej = E_i * MEV_TO_J.
//   Step 2b: Convert to relativistic rigidity R [GV]:
//             p = gamma*m*v = sqrt(Ej^2/c^2 + 2*Ej*m0*c^2) / c
//             R = p*c / (|q| * 1e9)    [GV = 10^9 V]
//
// Step 3: For each direction d_k in the subsampled direction set
//   Step 3a: Launch a backtraced trajectory from x0 in direction -d_k (time reversal).
//   Step 3b: Integrate using StepParticleChecked (relativistic Boris / RK mover) with
//            adaptive time-stepping (0.15 rad / omega_c gyro limit + CFL travel limit).
//   Step 3c: Classify as ALLOWED (escaped outer box) or FORBIDDEN (hit inner sphere
//            or timed out).
//   Step 3d (ANISOTROPIC only): If ALLOWED, retrieve the exit state from
//            TraceAllowedSharedEx:
//              x_exit_m[3]    - GSM position of the domain boundary crossing [m]
//              v_exit_unit[3] - velocity unit vector at exit
//              cosAlpha       - cos(alpha) = v_exit . B_hat(x_exit)
//            (These are stored in TrajectoryExitState by the extended tracer.)
//
// Step 4: Accumulate the anisotropy-weighted transmissivity
//   For each ALLOWED trajectory k:
//     weight_k = EvalAnisotropyFactor(par, cos_alpha_k, x_exit_m_k)
//              = f_PAD(cos_alpha_k; BA_PAD_MODEL, BA_PAD_EXPONENT)
//              * f_spatial(x_exit_m_k; BA_SPATIAL_MODEL, ...)
//     weightSum += weight_k
//   T_aniso(E_i; x0) = weightSum / N_dirs
//
//   (ISOTROPIC would simply do: weightSum += 1 for each ALLOWED trajectory.)
//
// Step 5: Compute local spectrum at E_i
//   J_b_iso(E_i) = ::gSpectrum.GetSpectrum(Ej)    [m^-2 s^-1 sr^-1 J^-1]
//   J_loc(E_i)   = T_aniso(E_i) * J_b_iso(E_i)
//
// Step 6: Relativistic speed
//   gamma_i = 1 + Ej / (m0 * c^2)
//   v_i     = c * sqrt(1 - 1/gamma_i^2)
//
// Step 7: Density integrand
//   integrand_i = (4*pi / v_i) * J_loc(E_i)       [m^-3 J^-1]
//
// Step 8: After processing all energy points, integrate with the trapezoidal rule:
//   n_tot(x0) = Trapz( {E_i}, {integrand_i} )      [m^-3]
//
// Step 9 (parallel path): Workers repeat Steps 3-8 for their assigned observation
//   points and return {idx, density, T[]} to the rank-0 master for collection and
//   output.
//
//======================================================================================

#ifndef _AMPS_ANISOTROPIC_SPECTRUM_H_
#define _AMPS_ANISOTROPIC_SPECTRUM_H_

#include "util/amps_param_parser.h"

// Evaluate the combined anisotropy factor  f_aniso = f_PAD(cos_alpha) * f_spatial(x).
//
// Arguments:
//   par          : anisotropy parameters parsed from #BOUNDARY_ANISOTROPY
//   cos_alpha    : cosine of the pitch angle at the domain boundary exit point.
//                  cos(alpha) = v_exit_hat . B_hat(x_exit).  Range [-1, 1].
//   x_exit_m[3] : GSM position of the domain boundary crossing [m].
//
// Returns: non-negative weight to multiply J_b_iso(E) by for this trajectory.
//
// Throws: std::runtime_error for unknown model names (caught once at startup by
//         the parser validator; should not occur in normal operation).
double EvalAnisotropyFactor(const EarthUtil::AnisotropyParam& par,
                             double cos_alpha,
                             const double x_exit_m[3]);

#endif // _AMPS_ANISOTROPIC_SPECTRUM_H_
