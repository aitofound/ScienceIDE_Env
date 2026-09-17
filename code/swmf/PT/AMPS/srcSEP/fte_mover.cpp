
/*==============================================================================
 * SEP::ParticleMover_FocusedTransport_EventDriven
 *==============================================================================
 *
 * DESCRIPTION:
 *   Event-driven Monte Carlo solver for the Focused Transport Equation (FTE)
 *   in the heliosphere. This function advances energetic particles along
 *   magnetic field lines with proper treatment of:
 *   - Deterministic streaming along field lines
 *   - Adiabatic magnetic focusing/defocusing
 *   - Pitch angle scattering with Alfvén waves
 *   - Adiabatic cooling due to solar wind expansion
 *   - Relativistic wave-particle interactions
 *
 * PHYSICS MODEL:
 *   The Focused Transport Equation describes particle motion in the solar wind:
 *   ∂f/∂t + v·μ ∂f/∂s + (1-μ²)/(2L) v ∂f/∂μ = ∂/∂μ[D_μμ ∂f/∂μ] + source terms
 *
 *   where:
 *   - s: distance along magnetic field line
 *   - μ = v∥/v: pitch angle cosine
 *   - L = -|B|/(∂|B|/∂s): magnetic focusing length
 *   - D_μμ: pitch angle diffusion coefficient
 *
 * REFERENCE FRAME:
 *   - Simulation conducted in PLASMA REST FRAME
 *   - Particle velocities (vParallel, vNormal) given in plasma rest frame
 *   - Alfvén waves propagate at ±v_A relative to plasma
 *   - Wave-particle scattering uses relativistic frame transformations
 *
 * SCATTERING PHYSICS:
 *   Wave-particle resonance conditions:
 *   - Forward particles (μ > 0): scatter only with W- waves (v = -v_A)
 *   - Backward particles (μ < 0): scatter only with W+ waves (v = +v_A)
 *
 *   Scattering process:
 *   1. Transform particle velocity: Plasma frame → Alfvén wave frame
 *   2. Perform isotropic scattering in wave frame (preserve speed)
 *   3. Transform back: Wave frame → Plasma frame
 *   4. Calculate energy gain/loss from relativistic transformations
 *
 * RELATIVISTIC TRANSFORMATIONS:
 *   Uses exact Lorentz boost formulas for frame transformations:
 *
 *   Plasma → Wave frame:
 *   v'∥ = (v∥ - V_A) / (1 - v∥V_A/c²)
 *   v'⊥ = v⊥ / [γ_A(1 - v∥V_A/c²)]
 *
 *   Wave → Plasma frame:
 *   v∥ = (v'∥ + V_A) / (1 + v'∥V_A/c²)
 *   v⊥ = v'⊥ / [γ_A(1 + v'∥V_A/c²)]
 *
 *   where β_A = V_A/c, γ_A = 1/√(1-β_A²)
 *
 * EVENT-DRIVEN ALGORITHM:
 *   1. Calculate scattering time: t_scatter ~ -ln(r)/ν_scatter
 *   2. Advance particle deterministically until next event
 *   3. Apply magnetic focusing: dμ/dt = (1-μ²)v/(2L)
 *   4. Apply adiabatic cooling: dp/dt ∝ ∇·v_sw
 *   5. Handle scattering event with proper frame transformations
 *   6. Sample particle flux for wave coupling between events
 *   7. Repeat until time step completed or particle exits domain
 *
 * FLUX SAMPLING:
 *   - Samples AccumulateParticleFluxForWaveCoupling() between events
 *   - Records: time interval, start/end positions, distance traveled
 *   - Resets tracking variables after each scattering event
 *   - Ensures complete coverage of particle trajectory
 *
 * NUMERICAL FEATURES:
 *   - Adaptive time stepping based on physics time scales
 *   - Numerical stability checks for relativistic transformations
 *   - Speed limits to prevent superluminal velocities
 *   - Proper handling of pitch angle boundaries (μ ∈ [-1,1])
 *
 * INPUT PARAMETERS:
 *   ptr      - Particle buffer pointer
 *   dtTotal  - Total time step duration
 *   node     - AMR tree node containing particle
 *
 * RETURN VALUES:
 *   _PARTICLE_MOTION_FINISHED_ - Particle successfully advanced
 *   _PARTICLE_LEFT_THE_DOMAIN_ - Particle exited simulation domain
 *
 * DEPENDENCIES:
 *   - Field line data structure (magnetic field, plasma properties)
 *   - Alfvén wave amplitudes (W+, W-) at field line vertices
 *   - Mean free path calculation routines
 *   - Relativistic energy-momentum conversion functions
 *   - Transport coefficient calculation (adiabatic cooling)
 *
 * AUTHOR: Generated for SEP transport simulation
 * DATE: 2024
 *
 * NOTES:
 *   - Currently uses simple scattering frequency: ν = v/λ
 *   - Future: Can extend to sophisticated wave-dependent rates
 *   - Assumes mean free path >> Larmor radius (guiding center approximation)
 *   - Compatible with existing SEP simulation framework
 *==============================================================================*/

#include "sep.h"

int SEP::ParticleMover_FocusedTransport_EventDriven(long int ptr, double dtTotal, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  namespace PB = PIC::ParticleBuffer;
  namespace FL = PIC::FieldLine;

  PIC::ParticleBuffer::byte *ParticleData;
  double Speed, mu, vParallel, vNormal;
  double FieldLineCoord, xCartesian[3];
  int iFieldLine, spec;
  FL::cFieldLineSegment *Segment;

  static int ncall = 0;
  ncall++;

  ParticleData = PB::GetParticleDataPointer(ptr);

  FieldLineCoord = PB::GetFieldLineCoord(ParticleData);
  iFieldLine = PB::GetFieldLineId(ParticleData);
  spec = PB::GetI(ParticleData);

  // Velocity is in the frame moving with solar wind.
  //
  // A zero-speed or non-finite particle is not a valid SEP macro-particle for
  // the focused-transport mover: the pitch-angle cosine is defined as
  //     mu = v_parallel / |v|,
  // and both the scattering rate and resonant wave number require a finite
  // particle speed.  Previously this routine divided by |v| without checking
  // for |v|=0.  If an invalid particle reached this mover, NaNs could be
  // generated and later written back to the particle buffer as an apparently
  // zero velocity.  Abort with a detailed diagnostic instead of silently
  // corrupting the particle state; such a particle should be traced back to the
  // injection/source routine.
  vParallel = PB::GetVParallel(ParticleData);
  vNormal = PB::GetVNormal(ParticleData);

  if ((Segment = FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord)) == NULL) {
    exit(__LINE__, __FILE__, "Error: cannot find the segment");
  }

  double TimeCounter = 0.0;

  // ---------------------------------------------------------------------------
  // Robust velocity controls used by this event-driven focused-transport mover.
  // ---------------------------------------------------------------------------
  // The wave-particle coupling and adiabatic-cooling terms can occasionally try
  // to remove too much kinetic energy from a Monte-Carlo macro-particle in a
  // single time step.  If that happens, a particle speed can become extremely
  // small and later cause divisions by |v| when the pitch-angle cosine
  //     mu = v_parallel / |v|
  // is recomputed.  A speed that is mathematically zero is not meaningful for
  // the SEP focused-transport equation, because resonance, scattering, and
  // focusing all require a finite particle speed.
  //
  // We therefore impose a very small absolute floor of 1 m/s.  This is far below
  // energetic-particle speeds and is intended only as a numerical safety floor.
  // It prevents the mover from writing exactly zero velocity to the particle
  // buffer, while still allowing the debugger to identify genuinely non-finite
  // states.  Non-finite values are not repaired, because they indicate a real
  // numerical error that should be traced to the previous operation.
  const double MinMoverSpeed = 1.0; // [m/s] absolute numerical floor for |v|

  // Relativistic upper speed guard.  Several parts of this mover convert
  // between speed and momentum.  In relativistic mode the mapping
  //
  //     p = gamma m v,      gamma = 1/sqrt(1-v^2/c^2),
  //
  // is singular at v=c.  The debugger showed a particle reaching exactly
  // SpeedOfLight before the call to Speed2Momentum(), which makes gamma and
  // the momentum infinite.  The physical solution should approach c only
  // asymptotically, so any finite Monte-Carlo particle must be kept strictly
  // subluminal.  The margin below c is intentionally tiny: it prevents the
  // singular conversion while not imposing an artificial 0.99c cap on high
  // energy SEP particles.
  const double MaxRelativisticBeta = 1.0 - 1.0e-12;
  const double MaxMoverSpeed = MaxRelativisticBeta * SpeedOfLight;

  // Limit for a single adiabatic momentum update.  The physical cooling/heating
  // factor is p_new=p_old*exp(dLogP).  If dLogP is very large, exp(dLogP) can
  // overflow and contaminate the velocity with Inf/NaN; if it is very negative,
  // the particle can be driven almost to rest in one substep.  The limiter is a
  // local explicit-step stabilizer.  It does not remove the cooling physics; it
  // only prevents a single event-driven substep from changing the momentum by
  // more than about 5%.  Repeated substeps still accumulate the full effect.
  const double MaxAbsDLogPPerSubstep = 5.0e-2;

  // ---------------------------------------------------------------------------
  // Mean-free-path safety limits used only by this event-driven mover.
  // ---------------------------------------------------------------------------
  // The focused-transport event scheduler uses the scattering frequency
  //     nu = v / lambda,
  // where lambda is returned by the selected mean-free-path model.  If that model
  // returns NaN, Inf, or a non-positive value, the event-time sampler is poisoned
  // and the next particle update can produce NaN velocities.  Such a value can be
  // caused by a particle outside the intended range of the parameterization, by a
  // vanishing magnetic field, by a zero/underflowed speed, or by a singular power
  // law inside a particular lambda model.
  //
  // The mover should not die because one macro-particle sits outside the validity
  // range of the scattering prescription.  The least intrusive fallback is a very
  // large mean free path, which simply makes the particle nearly ballistic for
  // this substep.  This avoids introducing artificial strong scattering while
  // preserving particle transport and allowing debug diagnostics to identify the
  // problematic state.
  const double MinAbsBForMeanFreePath = 1.0e-20; // [T] avoid B=0 singularities
  const double MinMeanFreePath        = 1.0e3;  // [m] lower numerical lambda bound
  const double MaxMeanFreePath        = 1.0e15; // [m] weak-scattering fallback

  auto AbortInvalidVelocity = [&](const char* location) {
    char msg[2048];
    sprintf(msg,
        "Invalid particle velocity in ParticleMover_FocusedTransport_EventDriven at %s. "
        "ptr=%ld, spec=%d, iFieldLine=%d, FieldLineCoord=%e, TimeCounter=%e, dtTotal=%e, "
        "vParallel=%e m/s, vNormal=%e m/s, |v|=%e m/s, mu=%e. "
        "The focused-transport mover requires finite |v|>0 because mu=v_parallel/|v| "
        "and the resonant wave number depend on the particle speed.  A non-finite "
        "velocity should be traced to the immediately preceding operation in this mover "
        "or to particle/turbulence energy redistribution before the mover was called.",
        location,ptr,spec,iFieldLine,FieldLineCoord,TimeCounter,dtTotal,
        vParallel,vNormal,Speed,mu);
    exit(__LINE__,__FILE__,msg);
  };

  auto ClampPitchAngle = [&]() {
    // Roundoff after focusing or Lorentz transformations can push |mu| slightly
    // above unity.  The FTE equations require -1<=mu<=1; keep a tiny margin away
    // from the singular endpoints when the value is used for sqrt(1-mu^2).
    if (mu >  1.0 - 1.0e-12) mu =  1.0 - 1.0e-12;
    if (mu < -1.0 + 1.0e-12) mu = -1.0 + 1.0e-12;
  };

  auto ValidateOrFloorSpeedMu = [&](const char* location) {
    // This helper is deliberately called after every operation that can modify
    // Speed or mu.  It separates three cases:
    //   (1) non-finite values: abort immediately with context; this is a true bug;
    //   (2) finite but too-small speed: apply the 1 m/s numerical floor;
    //   (3) finite but luminal/superluminal speed: cap below c.
    //
    // The lower floor can occur if coupling/cooling removes nearly all particle
    // kinetic energy.  The upper cap is equally important in relativistic mode:
    // Lorentz transformations and roundoff can push a particle to v=c exactly,
    // where Speed2Momentum() is singular.  Keeping Speed strictly below c makes
    // subsequent speed<->momentum conversions well defined.
    if (!isfinite(Speed) || !isfinite(mu)) {
      AbortInvalidVelocity(location);
    }

    if (Speed < MinMoverSpeed) {
      static long int nSpeedFloorWarnings = 0;
      if ((PIC::ThisThread == 0) && (nSpeedFloorWarnings < 20)) {
        std::cerr << "WARNING: ParticleMover_FocusedTransport_EventDriven applied "
                  << "the 1 m/s speed floor at " << location
                  << ": ptr=" << ptr
                  << ", spec=" << spec
                  << ", field_line=" << iFieldLine
                  << ", coord=" << FieldLineCoord
                  << ", old_speed=" << Speed
                  << ", mu=" << mu << std::endl;
        nSpeedFloorWarnings++;
      }
      Speed = MinMoverSpeed;
    }

    if (Speed > MaxMoverSpeed) {
      static long int nSpeedCapWarnings = 0;
      if ((PIC::ThisThread == 0) && (nSpeedCapWarnings < 20)) {
        std::cerr << "WARNING: ParticleMover_FocusedTransport_EventDriven capped "
                  << "a luminal/superluminal speed at " << location
                  << ": ptr=" << ptr
                  << ", spec=" << spec
                  << ", field_line=" << iFieldLine
                  << ", coord=" << FieldLineCoord
                  << ", old_speed=" << Speed
                  << ", max_allowed=" << MaxMoverSpeed
                  << ", beta_max=" << MaxRelativisticBeta
                  << ", mu=" << mu << std::endl;
        nSpeedCapWarnings++;
      }
      Speed = MaxMoverSpeed;
    }

    ClampPitchAngle();
  };

  auto MomentumFromSafeSpeed = [&](double speed_in, const char* location) -> double {
    // Convert a particle speed to momentum only after enforcing the same lower
    // and upper speed bounds used by the mover.  This wrapper exists because the
    // relativistic conversion p=gamma*m*v is singular at v=c.  Calling the raw
    // Relativistic::Speed2Momentum() with Speed==SpeedOfLight was the failure
    // mode seen in the debugger.
    double speed_safe = speed_in;

    if (!isfinite(speed_safe)) {
      Speed = speed_safe;
      AbortInvalidVelocity(location);
    }

    if (speed_safe < MinMoverSpeed) speed_safe = MinMoverSpeed;
    if (speed_safe > MaxMoverSpeed) speed_safe = MaxMoverSpeed;

    const double mass = PIC::MolecularData::GetMass(spec);
    double p_safe = 0.0;

    switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
    case _PIC_MODE_OFF_:
      p_safe = speed_safe * mass;
      break;
    case _PIC_MODE_ON_:
      p_safe = Relativistic::Speed2Momentum(speed_safe, mass);
      break;
    }

    if (!isfinite(p_safe) || p_safe < 0.0) {
      Speed = speed_safe;
      AbortInvalidVelocity(location);
    }

    return p_safe;
  };

  auto SafeSpeedFromMomentum = [&](double p_in, const char* location) -> double {
    // Convert momentum back to speed with explicit positivity, finiteness, and
    // subluminal checks.  In relativistic mode the stable expression
    //
    //     beta = p / sqrt(p^2 + (mc)^2)
    //
    // is used instead of relying solely on helper routines that may not guard
    // against overflowing intermediate values.  The result is then passed through
    // the same [MinMoverSpeed, MaxMoverSpeed] interval used everywhere else in
    // the mover.
    if (!isfinite(p_in) || p_in < 0.0) {
      Speed = p_in;
      AbortInvalidVelocity(location);
    }

    const double mass = PIC::MolecularData::GetMass(spec);
    double speed_out = 0.0;

    switch (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_) {
    case _PIC_MODE_OFF_:
      speed_out = p_in / mass;
      break;
    case _PIC_MODE_ON_:
      {
        const double mc = mass * SpeedOfLight;
        const double denom = std::hypot(p_in, mc);
        if (!isfinite(denom) || denom <= 0.0) {
          Speed = p_in;
          AbortInvalidVelocity(location);
        }
        speed_out = SpeedOfLight * (p_in / denom);
      }
      break;
    }

    if (!isfinite(speed_out)) {
      Speed = speed_out;
      AbortInvalidVelocity(location);
    }

    if (speed_out < MinMoverSpeed) speed_out = MinMoverSpeed;
    if (speed_out > MaxMoverSpeed) speed_out = MaxMoverSpeed;

    return speed_out;
  };

  // Calculate initial particle properties.
  Speed = sqrt(vNormal * vNormal + vParallel * vParallel);
  if (!isfinite(vParallel) || !isfinite(vNormal) || !isfinite(Speed)) {
    AbortInvalidVelocity("initial velocity read from particle buffer");
  }

  // Apply the same speed/mu guard immediately after reading the particle.  A
  // particle below the numerical speed floor is kept alive but assigned a finite
  // speed before mu is evaluated, preventing the initial mu=v_parallel/|v|
  // division from producing NaNs.
  if (Speed < MinMoverSpeed) Speed = MinMoverSpeed;
  if (Speed > MaxMoverSpeed) Speed = MaxMoverSpeed;
  mu = vParallel / Speed;
  ValidateOrFloorSpeedMu("initial pitch-angle calculation");

  // Initialize tracking variables for flux sampling
  double s_segment_start = FieldLineCoord;  // Start position for current segment
  double segment_traversed_path = 0.0;      // Accumulated path in current segment

  // Helper function to sample particle flux with proper parameters
  auto SampleParticleFlux = [&](double s_start, double s_end, double traversed_path) {
    if (SEP::AlfvenTurbulence_Kolmogorov::ActiveFlag && fabs(traversed_path) > 1.0e-20) {
      SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::AccumulateParticleFluxForWaveCoupling(
        iFieldLine,              // Field line index
        ptr,                     // Particle index
        dtTotal,                 // Time step
        vParallel,              // Parallel velocity component
        vNormal,                // Normal (gyration) velocity component
        s_start,                // Start position along field line
        s_end,                  // End position along field line
        traversed_path          // Signed parallel path length
      );
    }
  };

  // Helper function to reset flux tracking variables
  auto ResetFluxTracking = [&]() {
    s_segment_start = FieldLineCoord;
    segment_traversed_path = 0.0;
  };

  // Helper function to calculate the raw mean free path from the selected model.
  //
  // This routine deliberately contains only the physics/parameterization switch.
  // It does not decide how to recover from invalid input or invalid output.  All
  // numerical protection is centralized in SafeCalculateMeanFreePath() below so
  // that every mean-free-path model is treated consistently by the event-driven
  // mover.
  auto CalculateMeanFreePath = [&](int spec, double rHelio, double Speed, double AbsB) {
    double MeanFreePath, dxx, energy;

    switch (SEP::Scattering::MeanFreePathMode) {
    case SEP::Scattering::MeanFreePathMode_QLT:
      MeanFreePath = QLT::calculateMeanFreePath(rHelio, Speed);
      break;
    case SEP::Scattering::MeanFreePathMode_QLT1:
      MeanFreePath = QLT1::calculateMeanFreePath(rHelio, Speed, AbsB);
      break;
    case SEP::Scattering::MeanFreePathMode_Tenishev2005AIAA:
      energy = Relativistic::Speed2E(Speed, PIC::MolecularData::GetMass(spec));
      MeanFreePath = SEP::Scattering::Tenishev2005AIAA::lambda0 *
        pow(energy/GeV2J, SEP::Scattering::Tenishev2005AIAA::alpha) *
        pow(rHelio/_AU_, SEP::Scattering::Tenishev2005AIAA::beta);
      break;
    case SEP::Scattering::MeanFreePathMode_Chen2024AA:
      energy = Relativistic::Speed2E(Speed, PIC::MolecularData::GetMass(spec));
      dxx = SEP::Diffusion::Chen2024AA::GetDxx(rHelio, energy);
      MeanFreePath = 3.0 * dxx / Speed; // Eq. 15, Liu-2024-arXiv
      break;
    default:
      exit(__LINE__, __FILE__, "Error: the option is unknown");
    }

    return MeanFreePath;
  };

  auto SafeCalculateMeanFreePath = [&](int spec, double rHelio, double Speed, double AbsB,
                                       const char* location) {
    // This wrapper is intentionally conservative.  A bad lambda should not be
    // propagated into the event sampler where nu=v/lambda is used to sample the
    // next scattering time.  Instead, invalid inputs/outputs are converted into a
    // weak-scattering fallback.  That choice is physically safer than assigning a
    // tiny lambda, because a tiny lambda would force artificial rapid scattering.
    //
    // Non-finite particle velocity is still considered a true mover error and is
    // handled by ValidateOrFloorSpeedMu() before this wrapper is called.
    static long int nMeanFreePathWarnings = 0;

    auto WarnAndReturnFallback = [&](const char* reason, double raw_lambda) {
      if ((PIC::ThisThread == 0) && (nMeanFreePathWarnings < 50)) {
        std::cerr
          << "WARNING: ParticleMover_FocusedTransport_EventDriven uses a weak-scattering "
          << "fallback mean free path at " << location
          << ": reason=" << reason
          << ", raw_lambda=" << raw_lambda
          << " m, fallback_lambda=" << MaxMeanFreePath
          << " m, ptr=" << ptr
          << ", spec=" << spec
          << ", field_line=" << iFieldLine
          << ", coord=" << FieldLineCoord
          << ", r=" << rHelio
          << " m, speed=" << Speed
          << " m/s, |B|=" << AbsB
          << " T, TimeCounter=" << TimeCounter
          << ", dtTotal=" << dtTotal
          << std::endl;
        nMeanFreePathWarnings++;
      }

      return MaxMeanFreePath;
    };

    // Validate the inputs that commonly appear in denominators or power laws in
    // the mean-free-path models.  If any of these checks fail, the lambda model is
    // outside its numerical validity range for this particle/substep.
    if (!isfinite(rHelio) || rHelio <= 0.0) {
      return WarnAndReturnFallback("invalid heliocentric distance", NAN);
    }

    if (!isfinite(Speed) || Speed < MinMoverSpeed) {
      return WarnAndReturnFallback("invalid or below-floor particle speed", NAN);
    }

    if (!isfinite(AbsB) || AbsB < MinAbsBForMeanFreePath) {
      return WarnAndReturnFallback("invalid or too-small magnetic field", NAN);
    }

    double lambda = CalculateMeanFreePath(spec, rHelio, Speed, AbsB);

    if (!isfinite(lambda) || lambda <= 0.0) {
      return WarnAndReturnFallback("mean-free-path model returned invalid lambda", lambda);
    }

    // Clamp extreme but finite values.  The lower bound prevents an unrealistically
    // small lambda from producing a scattering time that is much shorter than all
    // other scales in the explicit event mover.  The upper bound is effectively
    // ballistic for SEP applications and also avoids Inf when computing v/lambda.
    if (lambda < MinMeanFreePath) {
      if ((PIC::ThisThread == 0) && (nMeanFreePathWarnings < 50)) {
        std::cerr
          << "WARNING: ParticleMover_FocusedTransport_EventDriven clamps a very small "
          << "mean free path at " << location
          << ": raw_lambda=" << lambda
          << " m, clamped_lambda=" << MinMeanFreePath
          << " m, ptr=" << ptr
          << ", speed=" << Speed
          << " m/s, r=" << rHelio
          << " m, |B|=" << AbsB << " T" << std::endl;
        nMeanFreePathWarnings++;
      }
      lambda = MinMeanFreePath;
    }

    if (lambda > MaxMeanFreePath) {
      lambda = MaxMeanFreePath;
    }

    return lambda;
  };

  // Helper function to calculate scattering frequency
  auto CalculateScatteringFrequency = [&](double Speed, double MeanFreePath, double mu,
                                          double W_plus, double W_minus,
                                          double& Nu_scattering, bool& scatter_with_Wplus) -> void {
    // For now, use simple mean free path approach without wave energy density.
    // The event-driven algorithm samples scattering events from
    //     nu = v/lambda.
    // Every input to this expression must be finite and positive.  If the safe
    // mean-free-path wrapper had to return a very large fallback lambda, this
    // formula naturally produces an almost-zero scattering frequency and the
    // particle streams ballistically through the current substep.
    Nu_scattering = 0.0;
    scatter_with_Wplus = false;

    if (!isfinite(Speed) || Speed < MinMoverSpeed ||
        !isfinite(MeanFreePath) || MeanFreePath <= 0.0 ||
        !isfinite(mu)) {
      return;
    }

    const double nu = Speed / MeanFreePath;
    if (!isfinite(nu) || nu <= 0.0) return;

    // Determine which wave branch can cause scattering based on particle direction
    if (mu > 0.0 ) {
      // Outward moving particle - can scatter with inward wave (W-)
      Nu_scattering = nu;
      scatter_with_Wplus = false;
    } else if (mu < 0.0 ) {
      // Inward moving particle - can scatter with outward wave (W+)
      Nu_scattering = nu;
      scatter_with_Wplus = true;
    }
    // else: No scattering possible - values remain 0.0 and false
  };

  // Helper function to calculate adiabatic focusing parameter L
  auto CalculateFocusingLength = [&](FL::cFieldLineSegment* currentSegment, double currentFieldLineCoord, int fieldLineId) {
    double B[3], B0[3], B1[3], AbsBDeriv, AbsB, L;

    FL::FieldLinesAll[fieldLineId].GetMagneticField(B0, (int)currentFieldLineCoord);
    FL::FieldLinesAll[fieldLineId].GetMagneticField(B, currentFieldLineCoord);
    FL::FieldLinesAll[fieldLineId].GetMagneticField(B1, (int)currentFieldLineCoord + 1 - 1E-7);

    AbsB = sqrt(B[0]*B[0] + B[1]*B[1] + B[2]*B[2]);
    AbsBDeriv = (sqrt(B1[0]*B1[0] + B1[1]*B1[1] + B1[2]*B1[2]) -
                 sqrt(B0[0]*B0[0] + B0[1]*B0[1] + B0[2]*B0[2])) /
                FL::FieldLinesAll[fieldLineId].GetSegmentLength(currentFieldLineCoord);

    L = -AbsB / AbsBDeriv; // Focusing length scale

    return L;
  };

  // Event-driven Monte Carlo loop for Focused Transport Equation
  while (TimeCounter < dtTotal) {
    // Get current position and magnetic field
    double x[3], rHelio;
    Segment->GetCartesian(x, FieldLineCoord);
    rHelio = Vector3D::Length(x);

    double AbsB;
    switch (_PIC_COUPLER_MODE_) {
    case _PIC_COUPLER_MODE__SWMF_:
      AbsB = SEP::FieldLineData::GetAbsB(FieldLineCoord, Segment, iFieldLine);
      break;
    default:
      AbsB = SEP::ParkerSpiral::GetAbsB(rHelio);
    }

    // Calculate transport coefficients for Focused Transport Equation.
    //
    // Validate the particle state immediately before the mean-free-path model is
    // evaluated.  If Speed had been corrupted by a previous substep, the raw lambda
    // routines could return NaN and obscure the true source of the error.  The
    // validation helper floors finite but very small speeds and aborts on non-finite
    // values, keeping the diagnostic location close to the operation that exposed
    // the problem.
    ValidateOrFloorSpeedMu("before mean-free-path evaluation");

    double MeanFreePath = SafeCalculateMeanFreePath(
        spec, rHelio, Speed, AbsB,
        "main event-driven transport loop");

    double L = CalculateFocusingLength(Segment, FieldLineCoord, iFieldLine);

    // Store mean free path if sampling is active
    if ((SEP::Offset::MeanFreePath != -1) && (SEP::Sampling::MeanFreePath::active_flag == true)) {
      *((double*)(ParticleData + SEP::Offset::MeanFreePath)) = MeanFreePath;
    }

    // Calculate wave-specific scattering rates
    // Get Alfven wave amplitudes at current location
    double W[2]; // W[0] = W+, W[1] = W-
    FL::cFieldLineVertex* VertexBegin = Segment->GetBegin();
    FL::cFieldLineVertex* VertexEnd = Segment->GetEnd();
    double *W0 = VertexBegin->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);
    double *W1 = VertexEnd->GetDatum_ptr(FL::DatumAtVertexPlasmaWaves);

    // Interpolate wave amplitudes.  These vertex data are present when the
    // field-line/SWMF plasma-wave datums are available.  The present
    // event-driven scattering-frequency helper does not yet use the wave
    // amplitudes quantitatively, but keep the interpolation robust so this
    // mover can also be used in tests without SWMF/AWSoM wave datums.
    double w1 = fmod(FieldLineCoord, 1);
    double w0 = 1.0 - w1;
    if ((W0 != NULL) && (W1 != NULL)) {
      W[0] = w0 * W0[0] + w1 * W1[0]; // W+ (outward wave)
      W[1] = w0 * W0[1] + w1 * W1[1]; // W- (inward wave)
    }
    else {
      W[0] = 0.0;
      W[1] = 0.0;
    }

    // Calculate scattering frequency using the lambda function
    double Nu_scattering;
    bool scatter_with_Wplus;
    CalculateScatteringFrequency(Speed, MeanFreePath, mu, W[0], W[1], Nu_scattering, scatter_with_Wplus);
    bool can_scatter = (Nu_scattering > 0.0);

    // Calculate characteristic times for different processes
    double tau_scattering = (can_scatter && Nu_scattering > 0.0) ? 1.0 / Nu_scattering : 1e20;
    double tau_focusing = (L > 0.0 && fabs(mu) < 1.0) ? fabs(L) / (Speed * sqrt(1.0 - mu*mu)) : 1e20;
    double tau_remaining = dtTotal - TimeCounter;

    // Sample time to next scattering event (exponential distribution).
    //
    // Guard the random number because log(0) is -Inf and log(r>=1) can produce a
    // zero/negative event time.  Both cases are rare but can happen with finite
    // precision random number generators.  If the sampled number is outside the
    // open interval (0,1), suppress scattering for this substep rather than
    // propagating an invalid event time.
    double t_scattering = 1.0e20;
    if (can_scatter) {
      const double xi_scatter = rnd();
      if (isfinite(xi_scatter) && xi_scatter > 0.0 && xi_scatter < 1.0 &&
          isfinite(tau_scattering) && tau_scattering > 0.0) {
        t_scattering = -tau_scattering * log(xi_scatter);
      }
      else {
        t_scattering = 1.0e20;
      }
    }

    // Determine the time to advance (minimum of scattering time and remaining time).
    // A random number generator can, in principle, return a value very close to
    // one, making -log(rnd()) nearly zero.  A zero event time would leave the
    // particle unmoved and could stall the while loop.  Guard that corner case
    // and skip the scattering event for this substep if the sampled interval is
    // numerically zero.
    double dt_event = min(t_scattering, tau_remaining);

    // Determine if scattering occurs during this time step.
    bool scattering_occurs = (t_scattering <= tau_remaining) && can_scatter;

    if (!isfinite(dt_event) || dt_event <= 0.0) {
      if (tau_remaining <= 0.0) break;
      dt_event = tau_remaining;
      scattering_occurs = false;
    }

    // For Focused Transport: particles move along field lines with velocity v*mu
    // No stochastic spatial displacement - only deterministic motion along field lines
    double ds_parallel = vParallel * dt_event; // Motion along magnetic field line

    // Adiabatic focusing: change in mu due to magnetic field gradient
    // dmu/dt = (1-mu²)/(2L) * v for adiabatic focusing
    double dmu_focusing = 0.0;
    if (isfinite(L) && fabs(L) > 1.0e-20) {
      dmu_focusing = (1.0 - mu*mu) / (2.0 * L) * Speed * dt_event;
    }

    // Update pitch angle due to focusing (before potential scattering)
    mu += dmu_focusing;

    // Apply pitch angle limits.  Use the common validation helper immediately
    // after the focusing update because this is the first place in the substep
    // where mu is modified.  The helper also protects against accidental NaNs
    // from an ill-conditioned focusing length.
    ClampPitchAngle();
    ValidateOrFloorSpeedMu("after magnetic focusing update");

    segment_traversed_path += ds_parallel;

    // Reset tracking variables for next step
    ResetFluxTracking();

    // Move particle along field line
    FieldLineCoord = FL::FieldLinesAll[iFieldLine].move(FieldLineCoord, ds_parallel, Segment);

    // SAMPLE FLUX AFTER EACH MOVEMENT STEP
    SampleParticleFlux(s_segment_start, FieldLineCoord,ds_parallel);


    if (Segment == NULL) {
      // Particle left the simulation domain
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }

    // Update particle position
    Segment->GetCartesian(x, FieldLineCoord);
    rHelio = Vector3D::Length(x);

    // Apply adiabatic cooling if enabled
    if (SEP::AccountAdiabaticCoolingFlag == true) {
      // Get transport coefficients including adiabatic cooling
      double dP, dLogP, dmu_transport, vSolarWindParallel;
      SEP::GetTransportCoefficients(dP, dLogP, dmu_transport, Speed, mu, Segment,
                                    FieldLineCoord, dt_event, iFieldLine, vSolarWindParallel);

      if (isfinite(dLogP) && isfinite(dmu_transport)) {
        // Apply momentum change due to adiabatic cooling.  The conservative
        // variable in this part of the FTE is momentum; the transport coefficient
        // routine returns dLogP such that p_new=p_old*exp(dLogP).
        // The debugger showed particles reaching exactly the speed of light
        // before this conversion.  Do not call the raw speed->momentum routine
        // until Speed has been made strictly subluminal.
        ValidateOrFloorSpeedMu("before speed-to-momentum conversion in adiabatic cooling");
        double p = MomentumFromSafeSpeed(Speed,
            "invalid momentum before adiabatic cooling update");

        // Explicit-step limiter for the logarithmic momentum update.  Without this
        // guard, a locally large solar-wind divergence can make exp(dLogP) overflow
        // or can drive p almost to zero in a single event-driven substep.  Either
        // case later produces invalid velocities.  Limiting dLogP to +/-0.05 keeps
        // each substep to a controlled momentum change while preserving the same
        // sign and allowing the accumulated effect through multiple substeps.
        double dLogP_limited = dLogP;
        if (dLogP_limited >  MaxAbsDLogPPerSubstep) dLogP_limited =  MaxAbsDLogPPerSubstep;
        if (dLogP_limited < -MaxAbsDLogPPerSubstep) dLogP_limited = -MaxAbsDLogPPerSubstep;

        const double cooling_factor = exp(dLogP_limited);
        if (!isfinite(cooling_factor) || cooling_factor <= 0.0) {
          AbortInvalidVelocity("invalid exp(dLogP) in adiabatic cooling update");
        }

        // Apply momentum change: p_new = p_old * exp(dLogP_limited).
        p *= cooling_factor;

        // Convert back to speed with a stable momentum->speed formula and the
        // same strict subluminal cap.  This avoids returning Speed==c for very
        // large but finite momenta.
        Speed = SafeSpeedFromMomentum(p,
            "invalid speed after adiabatic-cooling momentum update");
        ValidateOrFloorSpeedMu("after adiabatic-cooling momentum update");

        // Add transport coefficient contribution to pitch angle change.  This
        // includes effects from solar-wind velocity gradients.  Validate again
        // because dmu_transport can be large in strongly compressed regions.
        mu += dmu_transport;
        ValidateOrFloorSpeedMu("after adiabatic-cooling pitch-angle update");
      }
    }

    // Handle scattering event with proper frame transformations.
    if (scattering_occurs) {
      // The particle velocity components stored in AMPS are measured in the
      // local plasma/solar-wind frame (see the comment at the top of this
      // routine).  The older implementation first composed an approximate
      // wave velocity in a lab frame and then transformed a plasma-frame
      // particle velocity using that lab-frame wave velocity.  That mixed
      // frames and can generate extremely small or non-finite particle speeds
      // for special pitch angles.  Here the scattering is done consistently in
      // the local plasma frame:
      //   1. plasma frame  -> Alfven-wave frame with V = +/- v_A;
      //   2. isotropize the pitch angle at fixed speed in the wave frame;
      //   3. wave frame    -> plasma frame with the inverse Lorentz transform.
      //
      // The manager-based wave-particle coupling is handled outside the mover
      // from the accumulated streaming arrays G_+(k) and G_-(k).  Therefore this
      // scattering event changes the particle direction/speed according to the
      // wave-frame kinematics but does NOT directly subtract/add energy from the
      // CellIntegratedWaveEnergy datum.  Direct updates inside this mover would
      // double count the coupling when --particle-coupling on is used and would
      // also bypass the wave-number-resolved E_\pm(k_j) bins.

      const double Speed_before_scatter = Speed;
      const double mu_before_scatter = mu;

      // Recompute vertex pointers and interpolation weights at the CURRENT
      // particle position.  The particle may have crossed into another segment
      // during FL::move(), so using the pre-move VertexBegin/VertexEnd pointers
      // can sample B and density from the wrong segment.
      FL::cFieldLineVertex* ScatterVertexBegin = Segment->GetBegin();
      FL::cFieldLineVertex* ScatterVertexEnd   = Segment->GetEnd();
      double scatter_w1 = fmod(FieldLineCoord, 1.0);
      double scatter_w0 = 1.0 - scatter_w1;

      double *B0 = ScatterVertexBegin->GetDatum_ptr(FL::DatumAtVertexMagneticField);
      double *B1 = ScatterVertexEnd->GetDatum_ptr(FL::DatumAtVertexMagneticField);

      bool valid_scatter_state = true;
      double B[3] = {0.0,0.0,0.0};
      double AbsB_scatter = 0.0;

      if ((B0 == NULL) || (B1 == NULL)) {
        valid_scatter_state = false;
      }
      else {
        for (int idim = 0; idim < 3; idim++) {
          B[idim] = scatter_w0 * B0[idim] + scatter_w1 * B1[idim];
        }
        AbsB_scatter = Vector3D::Length(B);
        if (!isfinite(AbsB_scatter) || AbsB_scatter <= 0.0) valid_scatter_state = false;
      }

      double PlasmaDensity0 = 0.0, PlasmaDensity1 = 0.0, PlasmaMassDensity = 0.0;
      if (valid_scatter_state) {
        ScatterVertexBegin->GetDatum(FL::DatumAtVertexPlasmaDensity, &PlasmaDensity0);
        ScatterVertexEnd->GetDatum(FL::DatumAtVertexPlasmaDensity, &PlasmaDensity1);

        // Vertex density is a number density.  Convert to mass density using the
        // same mean atomic mass convention used elsewhere in the SEP/SWMF
        // coupling.  The Alfven velocity is undefined for non-positive density;
        // skip the scattering event rather than producing NaNs.
        PlasmaMassDensity = (scatter_w0 * PlasmaDensity0 + scatter_w1 * PlasmaDensity1) *
                            PIC::CPLR::SWMF::MeanPlasmaAtomicMass;
        if (!isfinite(PlasmaMassDensity) || PlasmaMassDensity <= 0.0) valid_scatter_state = false;
      }

      if (valid_scatter_state) {
        double vAlfven = AbsB_scatter / sqrt(VacuumPermeability * PlasmaMassDensity);
        if (!isfinite(vAlfven) || fabs(vAlfven) >= 0.999999*SpeedOfLight) {
          valid_scatter_state = false;
        }

        if (valid_scatter_state) {
          // Select the Alfven-wave branch that caused the scattering.  W+ waves
          // propagate in the positive field-line direction and W- waves in the
          // negative direction in the plasma frame.
          const double v_wave_plasma_frame = scatter_with_Wplus ? +vAlfven : -vAlfven;

          const double beta_wave = v_wave_plasma_frame / SpeedOfLight;
          const double gamma_wave = 1.0 / sqrt(1.0 - beta_wave*beta_wave);

          // Current particle components in the plasma frame.
          const double v_parallel_plasma = Speed * mu;
          const double perp_arg = std::max(0.0,1.0 - mu*mu);
          const double v_perp_plasma = Speed * sqrt(perp_arg);
          const double beta_parallel_plasma = v_parallel_plasma / SpeedOfLight;

          // Plasma -> wave-frame Lorentz transform.  The perpendicular transform
          // contains the same denominator as the parallel transform.  The older
          // code omitted this denominator and later multiplied by gamma in the
          // inverse step; that is not the correct Lorentz velocity transform and
          // can create nonphysical speeds.
          const double denom_to_wave = 1.0 - beta_parallel_plasma*beta_wave;
          if (!isfinite(denom_to_wave) || fabs(denom_to_wave) <= 1.0e-14) {
            valid_scatter_state = false;
          }
          else {
            double beta_parallel_wave = (beta_parallel_plasma - beta_wave) / denom_to_wave;
            if (beta_parallel_wave >  0.999999999999) beta_parallel_wave =  0.999999999999;
            if (beta_parallel_wave < -0.999999999999) beta_parallel_wave = -0.999999999999;

            double v_parallel_wave = beta_parallel_wave * SpeedOfLight;
            double v_perp_wave = v_perp_plasma / (gamma_wave * denom_to_wave);
            double Speed_wave = sqrt(v_parallel_wave*v_parallel_wave + v_perp_wave*v_perp_wave);

            // If the particle is exactly co-moving with the selected Alfven
            // frame, the wave-frame speed can be numerically zero.  In that
            // degenerate case an isotropic scattering direction is undefined;
            // skip the scattering event and keep the pre-scatter velocity.
            if (!isfinite(Speed_wave) || Speed_wave <= MinMoverSpeed) {
              valid_scatter_state = false;
            }
            else {
              // Isotropize the direction in the wave frame while preserving the
              // speed in that frame.
              double mu_wave = -1.0 + 2.0 * rnd();
              if (mu_wave >  1.0) mu_wave =  1.0;
              if (mu_wave < -1.0) mu_wave = -1.0;

              v_parallel_wave = Speed_wave * mu_wave;
              v_perp_wave = Speed_wave * sqrt(std::max(0.0,1.0 - mu_wave*mu_wave));
              beta_parallel_wave = v_parallel_wave / SpeedOfLight;

              // Wave frame -> plasma frame inverse Lorentz transform.
              const double denom_to_plasma = 1.0 + beta_parallel_wave*beta_wave;
              if (!isfinite(denom_to_plasma) || fabs(denom_to_plasma) <= 1.0e-14) {
                valid_scatter_state = false;
              }
              else {
                double beta_parallel_plasma_new = (beta_parallel_wave + beta_wave) / denom_to_plasma;
                if (beta_parallel_plasma_new >  0.999999999999) beta_parallel_plasma_new =  0.999999999999;
                if (beta_parallel_plasma_new < -0.999999999999) beta_parallel_plasma_new = -0.999999999999;

                double v_parallel_plasma_new = beta_parallel_plasma_new * SpeedOfLight;
                double v_perp_plasma_new = v_perp_wave / (gamma_wave * denom_to_plasma);

                double beta_perp_plasma_new = v_perp_plasma_new / SpeedOfLight;
                double beta2_new = beta_parallel_plasma_new*beta_parallel_plasma_new +
                                   beta_perp_plasma_new*beta_perp_plasma_new;

                if (!isfinite(beta2_new) || beta2_new < 0.0) {
                  valid_scatter_state = false;
                }
                else {
                  // The inverse Lorentz transformation is algebraically
                  // subluminal, but finite precision can still make
                  // beta_parallel^2+beta_perp^2 slightly exceed one for very
                  // energetic particles.  Rather than allowing Speed==c to
                  // reach the next Speed2Momentum() call, rescale the velocity
                  // vector to the largest allowed subluminal value while
                  // preserving its direction in velocity space.
                  if (beta2_new > MaxRelativisticBeta*MaxRelativisticBeta) {
                    const double beta_norm = sqrt(beta2_new);
                    const double beta_scale = MaxRelativisticBeta / beta_norm;
                    beta_parallel_plasma_new *= beta_scale;
                    beta_perp_plasma_new     *= beta_scale;
                    v_parallel_plasma_new = beta_parallel_plasma_new * SpeedOfLight;
                    v_perp_plasma_new     = beta_perp_plasma_new     * SpeedOfLight;
                    beta2_new = MaxRelativisticBeta*MaxRelativisticBeta;
                  }

                  double Speed_new = SpeedOfLight * sqrt(beta2_new);

                  if (!isfinite(Speed_new) || Speed_new <= MinMoverSpeed) {
                    valid_scatter_state = false;
                  }
                  else {
                    // A scattering event is elastic in the selected Alfven-wave
                    // frame, but it can change the particle energy in the
                    // plasma frame.  Very large single-event energy jumps are
                    // numerically dangerous and can push particles to c in one
                    // step.  Limit the post-scatter momentum change to the same
                    // logarithmic increment used for adiabatic updates.  This is
                    // a local substep limiter: repeated scattering events can
                    // still accumulate the physical energy exchange, but no
                    // single event can create a singular speed-to-momentum
                    // conversion.
                    const double p_before_scatter = MomentumFromSafeSpeed(
                        Speed_before_scatter,
                        "invalid pre-scatter momentum in wave-frame scattering");
                    const double p_after_scatter = MomentumFromSafeSpeed(
                        Speed_new,
                        "invalid post-scatter momentum in wave-frame scattering");

                    if ((p_before_scatter > 0.0) && (p_after_scatter > 0.0)) {
                      double dLogP_scatter = log(p_after_scatter / p_before_scatter);
                      if (isfinite(dLogP_scatter)) {
                        double dLogP_scatter_limited = dLogP_scatter;
                        if (dLogP_scatter_limited >  MaxAbsDLogPPerSubstep) dLogP_scatter_limited =  MaxAbsDLogPPerSubstep;
                        if (dLogP_scatter_limited < -MaxAbsDLogPPerSubstep) dLogP_scatter_limited = -MaxAbsDLogPPerSubstep;

                        if (dLogP_scatter_limited != dLogP_scatter) {
                          const double p_limited = p_before_scatter * exp(dLogP_scatter_limited);
                          const double Speed_limited = SafeSpeedFromMomentum(
                              p_limited,
                              "invalid speed after limiting wave-frame scattering momentum jump");

                          const double scale_v = Speed_limited / Speed_new;
                          v_parallel_plasma_new *= scale_v;
                          v_perp_plasma_new     *= scale_v;
                          Speed_new = Speed_limited;
                        }
                      }
                      else {
                        valid_scatter_state = false;
                      }
                    }

                    Speed = Speed_new;
                    mu = v_parallel_plasma_new / Speed;
                    if (!isfinite(mu)) valid_scatter_state = false;
                  }
                }
              }
            }
          }
        }
      }

      if (!valid_scatter_state) {
        // Degenerate or invalid local wave/plasma state.  Do not write a zero or
        // NaN velocity into the particle buffer.  Keep the pre-scatter velocity
        // and continue the deterministic transport.  This is safer than deleting
        // the particle because the failure can be caused by missing plasma-wave
        // vertex data in a diagnostic run rather than by a bad particle source.
        Speed = Speed_before_scatter;
        mu = mu_before_scatter;
      }

      // Apply pitch-angle and speed safeguards after the scattering transform or
      // fallback.  A failed local wave/plasma transform restores the pre-scatter
      // values above; this call verifies that the restored or transformed state is
      // still finite and above the 1 m/s floor before the next deterministic step.
      ValidateOrFloorSpeedMu("after wave-frame scattering/fallback");
    }

    // Update velocity components based on the new pitch angle and speed.
    // NOTE: These will be used for the next iteration flux sampling.  Keep the
    // square-root argument non-negative even after roundoff in the Lorentz
    // transforms or focusing update.
    ValidateOrFloorSpeedMu("before updating velocity components after event step");

    vParallel = mu * Speed;
    vNormal = sqrt(std::max(0.0,1.0 - mu*mu)) * Speed;

    if (!isfinite(vParallel) || !isfinite(vNormal)) {
      Speed = sqrt(vParallel*vParallel + vNormal*vNormal);
      AbortInvalidVelocity("non-finite velocity components after event step");
    }

    // Recompute and enforce both speed bounds after component reconstruction.
    // This keeps the values actually carried into the next substep consistent
    // with the floor and the strict subluminal cap.  When the upper cap is
    // applied, scale both velocity components by the same factor so that the
    // pitch-angle direction is preserved.
    Speed = sqrt(vParallel*vParallel + vNormal*vNormal);
    if (Speed < MinMoverSpeed) {
      Speed = MinMoverSpeed;
      vParallel = mu * Speed;
      vNormal = sqrt(std::max(0.0,1.0 - mu*mu)) * Speed;
    }
    else if (Speed > MaxMoverSpeed) {
      const double scale_v = MaxMoverSpeed / Speed;
      vParallel *= scale_v;
      vNormal   *= scale_v;
      Speed = MaxMoverSpeed;
      mu = vParallel / Speed;
      ValidateOrFloorSpeedMu("after component reconstruction speed cap");
    }

    // Update perpendicular scattering if enabled (for field line spreading)
    if (SEP::Offset::RadialLocation != -1) {
      double radial_pos = *((double*)(ParticleData + SEP::Offset::RadialLocation));
      double dr, theta = PiTimes2 * rnd();
      double sin_theta = sin(theta);
      double D_perp;

      // Apply perpendicular diffusion
      D_perp = QLT1::calculatePerpendicularDiffusion(rHelio, Speed, AbsB);
      dr = sqrt(2.0 * D_perp * dt_event) * Vector3D::Distribution::Normal();
      radial_pos = sqrt(radial_pos*radial_pos + dr*dr + 2.0*radial_pos*dr*sin_theta);

      *((double*)(ParticleData + SEP::Offset::RadialLocation)) = radial_pos;
    }

    TimeCounter += dt_event;
  }

  // SAMPLE FINAL FLUX BEFORE UPDATING PARTICLE PROPERTIES (CRITICAL!)
  // This captures any remaining segment that wasn't sampled due to scattering reset
  SampleParticleFlux(s_segment_start, FieldLineCoord, segment_traversed_path);

  // Set the new values of the normal and parallel particle velocities.
  // Final guard: never write a zero/NaN velocity back to the particle buffer.
  Speed = sqrt(vParallel*vParallel + vNormal*vNormal);
  if (!isfinite(vParallel) || !isfinite(vNormal) || !isfinite(Speed)) {
    AbortInvalidVelocity("final velocity write-back");
  }

  // Last-resort write-back protection.  The focused-transport solver should have
  // maintained |v|>=1 m/s throughout the step; this final check guarantees that
  // no zero-speed particle is written to the AMPS particle buffer even if roundoff
  // in the component reconstruction made the speed slightly smaller.
  if (Speed < MinMoverSpeed) {
    Speed = MinMoverSpeed;
    ValidateOrFloorSpeedMu("final velocity write-back speed floor");
    vParallel = mu * Speed;
    vNormal = sqrt(std::max(0.0,1.0 - mu*mu)) * Speed;
  }
  else if (Speed > MaxMoverSpeed) {
    // Absolute last line of defense before the AMPS particle buffer is updated:
    // never write a luminal/superluminal velocity.  This is intentionally placed
    // at the final write-back because other diagnostics and sampling routines may
    // expose a bad value before it is stored permanently.
    const double scale_v = MaxMoverSpeed / Speed;
    vParallel *= scale_v;
    vNormal   *= scale_v;
    Speed = MaxMoverSpeed;
    mu = vParallel / Speed;
    ValidateOrFloorSpeedMu("final velocity write-back speed cap");
  }

  PB::SetVParallel(vParallel, ParticleData);
  PB::SetVNormal(vNormal, ParticleData);

  // Set the new particle coordinate
  PB::SetFieldLineCoord(FieldLineCoord, ParticleData);

  // Attach the particle to the temporary list
  switch (_PIC_PARTICLE_LIST_ATTACHING_) {
  case _PIC_PARTICLE_LIST_ATTACHING_NODE_:
    exit(__LINE__, __FILE__, "Error: the function was developed for the case _PIC_PARTICLE_LIST_ATTACHING_==_PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_");
    break;
  case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_:
    {
      long int temp = Segment->tempFirstParticleIndex.exchange(ptr);
      PIC::ParticleBuffer::SetNext(temp, ParticleData);
      PIC::ParticleBuffer::SetPrev(-1, ParticleData);

      if (temp != -1) PIC::ParticleBuffer::SetPrev(ptr, temp);
    }
    break;
  default:
    exit(__LINE__, __FILE__, "Error: the option is unknown");
  }

  return _PARTICLE_MOTION_FINISHED_;
}
