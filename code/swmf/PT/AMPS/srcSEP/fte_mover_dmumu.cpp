/*==============================================================================
 * SEP::ParticleMover_FocusedTransport_WaveScattering
 *==============================================================================
 *
 * DESCRIPTION:
 *   Focused Transport Equation solver with Alfvén wave scattering.
 *   Based on SEP::ParticleMover_FocusedTransport_EventDriven but modified to:
 *   - Scatter particles at every iteration using ScatterStepProton()
 *   - Extract wave energy density from segment data
 *   - Update wave energy after each scattering event
 *   - Use fixed time step calculated once outside the loop
 *
 * PHYSICS:
 *   - Deterministic streaming along field lines
 *   - Magnetic focusing/defocusing
 *   - Alfvén wave scattering at every time step
 *   - Energy exchange between particles and waves
 *   - Adiabatic cooling (if enabled)
 *
 * WAVE COUPLING:
 *   - Forward particles (μ > 0) scatter with W- waves
 *   - Backward particles (μ < 0) scatter with W+ waves
 *   - Energy gained by particle = Energy lost by waves
 *
 *==============================================================================*/

#include "sep.h"


int SEP::ParticleMover_FocusedTransport_WaveScattering(
    long int ptr, double dtTotal,
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node
) {
    namespace PB = PIC::ParticleBuffer;
    namespace FL = PIC::FieldLine;

    // Wave-branch indexing inside the cell-integrated energy array
    const int BranchPlus  = 0; // W+
    const int BranchMinus = 1; // W-

    // ---- Particle state ----
    PB::byte *ParticleData = PB::GetParticleDataPointer(ptr);
    double FieldLineCoord = PB::GetFieldLineCoord(ParticleData);
    int iFieldLine = PB::GetFieldLineId(ParticleData);
    int spec = PB::GetI(ParticleData);

    // Initial velocity components (in plasma rest frame)
    double vParallel = PB::GetVParallel(ParticleData);
    double vNormal = PB::GetVNormal(ParticleData);

    auto Segment = FL::FieldLinesAll[iFieldLine].GetSegment(FieldLineCoord);
    if (Segment == NULL) {
        exit(__LINE__, __FILE__, "Error: cannot find the segment");
    }

    // Calculate initial particle properties
    double Speed = sqrt(vNormal * vNormal + vParallel * vParallel);
    double mu = (Speed > 0.0) ? vParallel / Speed : 0.0;



    // Decompose velocity into parallel/normal (plasma frame)
    auto clamp_mu = [](double vmu) {
        return std::max(-1.0, std::min(1.0, vmu));
    };

    // ---------------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------------

    // Extract wave energy densities [J/m^3] for W+ and W- from a segment
    auto GetWaveEnergyDensities = [&](FL::cFieldLineSegment* seg,
                                      double& W_plus_density,
                                      double& W_minus_density) -> bool {
        double* wave_data =
            seg->GetDatum_ptr(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
        if (!wave_data) return false;

        // wave_data[0], wave_data[1] are cell-integrated energies [J]
        const double E_plus_total  = wave_data[0];
        const double E_minus_total = wave_data[1];

        // Segment volume [m^3]
        const double volume = SEP::FieldLine::GetSegmentVolume(seg, iFieldLine);
        if (volume <= 0.0) return false;

        W_plus_density  = E_plus_total  / volume; // [J/m^3]
        W_minus_density = E_minus_total / volume; // [J/m^3]
        return true;
    };

    // Update wave energy in the segment by particle energy change [J]
    auto UpdateWaveEnergy = [&](FL::cFieldLineSegment* seg,
                                double energyChange,
                                int AlfvenWaveBranch) -> void {
        double* wave_data =
            seg->GetDatum_ptr(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
        if (!wave_data) return;

        // Particle statistical weight
        const double stat_weight =
            PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec] *
            PB::GetIndividualStatWeightCorrection(ParticleData);

        // Particle gains -> waves lose; particle loses -> waves gain
        const double dWave = -stat_weight * energyChange;

        if (AlfvenWaveBranch == BranchPlus)  wave_data[0] += dWave;
        if (AlfvenWaveBranch == BranchMinus) wave_data[1] += dWave;

        // Keep non-negative
        wave_data[0] = std::max(0.0, wave_data[0]);
        wave_data[1] = std::max(0.0, wave_data[1]);
    };

    // Dμμ for Kolmogorov spectrum; inputs W± as *energy densities* [J/m^3],
    // converts internally to B^2 units via δB^2 = 2 μ0 W
    auto CalculateTotalDiffusionCoefficient = [&](
        double v, double mu_local, double Babs, double VA,
        double W_plus_density, double W_minus_density,
        double kmin, double kmax
    ) -> double {
        if (!(kmin > 0.0 && kmax > kmin) || Babs <= 0.0) return 0.0;

        // Convert to B^2 units [T^2]
        const double W_plus_B2  = 2.0 * VacuumPermeability * W_plus_density;
        const double W_minus_B2 = 2.0 * VacuumPermeability * W_minus_density;

        const double c     = SpeedOfLight;
        const double v2c2  = (v*v)/(c*c);
        const double gamma = 1.0 / std::sqrt(std::max(1.0 - v2c2, 1e-30));
        const double q     = PIC::MolecularData::GetElectricCharge(spec);
        const double m     = PIC::MolecularData::GetMass(spec);
        const double Omega = (q * Babs) / (gamma * m);

        // Power-law normalization for W^s(k) = C_s k^{-5/3}
        auto kolmo_C = [&](double Wtot_B2) -> double {
            const double p = -2.0/3.0; // integral exponent shift
            const double denom = std::pow(kmin, p) - std::pow(kmax, p);
            if (denom == 0.0) return 0.0;
            return (2.0/3.0) * Wtot_B2 / denom;
        };

        const double C_plus  = kolmo_C(W_plus_B2);
        const double C_minus = kolmo_C(W_minus_B2);

        auto Dmumu_branch = [&](int s, double C_s) -> double {
            if (C_s <= 0.0) return 0.0;

            // Resonance gate and resonant k
            const double gate = s*VA - v*mu_local;
            if (gate <= 0.0) return 0.0;

            const double kres = Omega / (gamma * gate);
            if (kres < kmin || kres > kmax) return 0.0;

            const double Wk    = C_s * std::pow(kres, -5.0/3.0);
            const double pref  = (M_PI * 0.5) * (Omega*Omega) / (Babs*Babs);
            const double denom = std::abs(v*mu_local - s*VA);

            return pref * (1.0 - mu_local*mu_local) * (Wk / denom);
        };

        return Dmumu_branch(+1, C_plus) + Dmumu_branch(-1, C_minus);
    };

    // Timestep computation with μ-diffusion limiter and local k-band
    auto CalculateTimeStep = [&](double currentSpeed,
                                 double currentMu,
                                 FL::cFieldLineSegment* currentSegment) -> double {

        // --- Focusing scale ---
        double Bm[3], Bm0[3], Bm1[3];
        FL::FieldLinesAll[iFieldLine].GetMagneticField(Bm0, (int)FieldLineCoord);
        FL::FieldLinesAll[iFieldLine].GetMagneticField(Bm,  FieldLineCoord);
        FL::FieldLinesAll[iFieldLine].GetMagneticField(Bm1, (int)FieldLineCoord + 1 - 1e-7);

        const double AbsB   = std::sqrt(Bm[0]*Bm[0] + Bm[1]*Bm[1] + Bm[2]*Bm[2]);
        const double AbsB0  = std::sqrt(Bm0[0]*Bm0[0] + Bm0[1]*Bm0[1] + Bm0[2]*Bm0[2]);
        const double AbsB1  = std::sqrt(Bm1[0]*Bm1[0] + Bm1[1]*Bm1[1] + Bm1[2]*Bm1[2]);
        const double dBds   = (AbsB1 - AbsB0) /
                              FL::FieldLinesAll[iFieldLine].GetSegmentLength(FieldLineCoord);
        const double Lfocus = (std::fabs(dBds) > 1e-20) ? -AbsB / dBds : 1e20;

        const double dt_focusing =
            (std::fabs(Lfocus) > 0.0 && std::fabs(currentMu) < 0.99)
                ? 0.1 * std::fabs(Lfocus) /
                      (currentSpeed * std::sqrt(1.0 - currentMu*currentMu))
                : 1e20;

        // --- Streaming scale ---
        const double dt_streaming = 0.1 * currentSegment->GetLength() / currentSpeed;

        // --- μ-diffusion limiter (RMS step in μ) ---
        double dt_mu_diffusion = 1e20, dt_diffusion_span = 1e20;
        double Wp_ed = 0.0, Wm_ed = 0.0;
        if (GetWaveEnergyDensities(currentSegment, Wp_ed, Wm_ed)) {
            // Interpolate B and density for V_A
            double B0v[3] = {0,0,0}, B1v[3] = {0,0,0};
            double AbsBi  = AbsB;
            double vA     = 0.0;

            FL::cFieldLineVertex* vb = currentSegment->GetBegin();
            FL::cFieldLineVertex* ve = currentSegment->GetEnd();
            if (vb && ve) {
                double *Bvb = vb->GetDatum_ptr(FL::DatumAtVertexMagneticField);
                double *Bve = ve->GetDatum_ptr(FL::DatumAtVertexMagneticField);
                if (Bvb && Bve) {
                    const double w1 = std::fmod(FieldLineCoord, 1.0);
                    const double w0 = 1.0 - w1;
                    double Bi[3] = { w0*Bvb[0] + w1*Bve[0],
                                     w0*Bvb[1] + w1*Bve[1],
                                     w0*Bvb[2] + w1*Bve[2] };
                    AbsBi = Vector3D::Length(Bi);

                    double rho0, rho1;
                    vb->GetDatum(FL::DatumAtVertexPlasmaDensity, &rho0);
                    ve->GetDatum(FL::DatumAtVertexPlasmaDensity, &rho1);
                    const double rho = (w0*rho0 + w1*rho1) * PIC::CPLR::SWMF::MeanPlasmaAtomicMass;
                    if (rho > 0.0) vA = AbsBi / std::sqrt(VacuumPermeability * rho);
                }
            }

            // Local k-band from turbulence model
            const double kmin_loc =
                SEP::AlfvenTurbulence_Kolmogorov::GetKmin(FieldLineCoord, iFieldLine);
            const double kmax_loc =
                SEP::AlfvenTurbulence_Kolmogorov::GetKmax(FieldLineCoord, iFieldLine);

            const double Dtot =
                CalculateTotalDiffusionCoefficient(currentSpeed, currentMu, AbsBi, vA,
                                                   Wp_ed, Wm_ed, kmin_loc, kmax_loc);

            if (Dtot > 1e-30) {
                const double dmu_target = 0.10; // target RMS Δμ per step
                const double safety     = 0.40; // CFL-like factor
                dt_mu_diffusion = safety * (dmu_target*dmu_target) / (2.0 * Dtot);
            }

            // Optional: classic span-based guard ~ (1-μ^2)/Dμμ
//            if (Dtot > 1e-30 && std::fabs(currentMu) < 0.99) {
//                dt_diffusion_span = 0.1 * (1.0 - currentMu*currentMu) / Dtot;
//            }
        }

        // Final dt choice + clamps
        double dt_calc = std::min({dt_focusing, dt_streaming,
                                   dt_mu_diffusion, dt_diffusion_span});
        dt_calc = std::max(dt_calc, 1.0e-4); // ≥ 0.1 ms
        dt_calc = std::min(dt_calc, 10.0);   // ≤ 10 s
        return dt_calc;
    };

    // ---------------------------------------------------------------------
    // Transport loop with adaptive dt (recomputed each substep)
    // ---------------------------------------------------------------------
    double TimeCounter = 0.0;

    while (TimeCounter < dtTotal) {
        // Recompute stable dt with current state
        const double dt_current =
            std::min(CalculateTimeStep(Speed, mu, Segment), dtTotal - TimeCounter);

        // --- Interpolate |B| and V_A at current location for scattering ---
        double AbsB = 0.0, vAlfven = 0.0;
        {
            FL::cFieldLineVertex* vb = Segment->GetBegin();
            FL::cFieldLineVertex* ve = Segment->GetEnd();
            if (vb && ve) {
                double *Bvb = vb->GetDatum_ptr(FL::DatumAtVertexMagneticField);
                double *Bve = ve->GetDatum_ptr(FL::DatumAtVertexMagneticField);
                if (Bvb && Bve) {
                    const double w1 = std::fmod(FieldLineCoord, 1.0);
                    const double w0 = 1.0 - w1;
                    double Bv[3] = { w0*Bvb[0] + w1*Bve[0],
                                     w0*Bvb[1] + w1*Bve[1],
                                     w0*Bvb[2] + w1*Bve[2] };
                    AbsB = Vector3D::Length(Bv);

                    double rho0, rho1;
                    vb->GetDatum(FL::DatumAtVertexPlasmaDensity, &rho0);
                    ve->GetDatum(FL::DatumAtVertexPlasmaDensity, &rho1);
                    const double rho = (w0*rho0 + w1*rho1) * PIC::CPLR::SWMF::MeanPlasmaAtomicMass;
                    if (rho > 0.0) vAlfven = AbsB / std::sqrt(VacuumPermeability * rho);
                }
            }
        }

        // --- Local wave energy densities [J/m^3] ---
        double W_plus_density  = 0.0;
        double W_minus_density = 0.0;
        GetWaveEnergyDensities(Segment, W_plus_density, W_minus_density);

        // Convert once to B^2 units [T^2] for the scatter step
        const double Wplus_B2  = 2.0 * VacuumPermeability * W_plus_density;
        const double Wminus_B2 = 2.0 * VacuumPermeability * W_minus_density;

        // Local k-band from turbulence model
        const double kmin_loc =
            SEP::AlfvenTurbulence_Kolmogorov::GetKmin(FieldLineCoord, iFieldLine);
        const double kmax_loc =
            SEP::AlfvenTurbulence_Kolmogorov::GetKmax(FieldLineCoord, iFieldLine);

        // Physical constants
        const double q = PIC::MolecularData::GetElectricCharge(spec);
        const double m = PIC::MolecularData::GetMass(spec);
        const double c = SpeedOfLight;

        // Pre-kick gamma for energy bookkeeping
        const double v_old     = std::hypot(vParallel, vNormal);
        const double gamma_old = 1.0 / std::sqrt(std::max(1.0 - (v_old*v_old)/(c*c), 1e-30));

        // -----------------------------
        // Wave scattering (one step)
        // -----------------------------
        auto scatterResult = SEP::AlfvenTurbulence_Kolmogorov::ScatterStepProton(
            vParallel, vNormal,     // plasma-frame components
            AbsB, vAlfven,          // |B| and V_A
            Wplus_B2, Wminus_B2,    // W± in B^2 units
            kmin_loc, kmax_loc,     // local k-band
            dt_current,
            q, m, c
        );

        // Apply scattered kinematics
        vParallel = scatterResult.vpar_new;
        vNormal   = scatterResult.vperp_new;
        mu        = clamp_mu(scatterResult.mu_new);

        // Energy exchange with waves (in plasma frame)
        const double energyChange = (scatterResult.gamma_new - gamma_old) * m * c * c;

	if (SEP::AlfvenTurbulence_Kolmogorov::ParticleCouplingMode==true) {
          if (scatterResult.scattered && scatterResult.branch != 0 && std::fabs(energyChange) > 0.0) {
            const int branchIdx = (scatterResult.branch == +1) ? BranchPlus : BranchMinus; // +1→W+, −1→W−
            UpdateWaveEnergy(Segment, energyChange, branchIdx);
	  }
        }

        // -----------------------------
        // Deterministic transport
        //   - parallel streaming
        //   - adiabatic focusing (explicit)
	//   - adiabatic cooling
        // -----------------------------

        // Parallel streaming distance along the field line during dt_current
        const double ds_parallel = vParallel * dt_current;

    // -----------------------------
    // ADIABATIC COOLING/HEATING
    //   dp/dt = -(p/3) ∇·V_sw
    //   p' = p * exp( - (∇·V) Δt / 3 )
    //   γ' = sqrt(1 + (p'/mc)^2),  v' = p'/(γ' m)
    //   (scale v_parallel and v_perp proportionally)
    // -----------------------------
   
// C++11: typed lambda (no generic lambdas yet)
const auto clamp_scalar = [](double x, double lo, double hi) -> double {
  return x < lo ? lo : (x > hi ? hi : x);
};

    {
      double r_here=0.0, drds=0.0;
      if (Segment->GetRadiusAndDrds(FieldLineCoord,r_here, drds)) {
        double n_loc=0.0, V_loc=0.0, divV_loc=0.0;
if (SEP::SW1DAdapter::QueryAtRadius(r_here, n_loc, V_loc, divV_loc, /*applyClamp=*/true)) {
  // dp/dt = -(p/3) ∇·V_sw  → stable exponential update
  const double vmag   = std::hypot(vParallel, vNormal);
  const double gamma0 = 1.0 / std::sqrt(std::max(1.0 - (vmag*vmag)/(SpeedOfLight*SpeedOfLight), 1e-30));
  const double m      = PIC::MolecularData::GetMass(spec);
  const double p0     = gamma0 * m * vmag;

  double dlnp = -(divV_loc * dt_current) / 3.0;
  dlnp = clamp_scalar(dlnp, -50.0, 50.0);
  const double p1     = p0 * std::exp(dlnp);
  const double pmc    = p1 / (m*SpeedOfLight);
  const double gamma1 = std::sqrt(1.0 + pmc*pmc);
  const double v1     = (gamma1>0.0) ? (p1/(gamma1*m)) : 0.0;

  const double scale  = (vmag>0.0) ? (v1/vmag) : 0.0;
  if (std::isfinite(scale) && scale>=0.0) {
    vParallel *= scale;
    vNormal   *= scale;
  }
}
}}

        // Adiabatic focusing — simple explicit step using local focusing length
        {
            double Bf[3], Bf0[3], Bf1[3];
            FL::FieldLinesAll[iFieldLine].GetMagneticField(Bf0, (int)FieldLineCoord);
            FL::FieldLinesAll[iFieldLine].GetMagneticField(Bf,  FieldLineCoord);
            FL::FieldLinesAll[iFieldLine].GetMagneticField(Bf1, (int)FieldLineCoord + 1 - 1e-7);

            const double AbsBf  = std::sqrt(Bf[0]*Bf[0] + Bf[1]*Bf[1] + Bf[2]*Bf[2]);
            const double AbsBf0 = std::sqrt(Bf0[0]*Bf0[0] + Bf0[1]*Bf0[1] + Bf0[2]*Bf0[2]);
            const double AbsBf1 = std::sqrt(Bf1[0]*Bf1[0] + Bf1[1]*Bf1[1] + Bf1[2]*Bf1[2]);

            const double dBds_f = (AbsBf1 - AbsBf0) /
                                  FL::FieldLinesAll[iFieldLine].GetSegmentLength(FieldLineCoord);
            const double Lf     = (std::fabs(dBds_f) > 1e-20) ? -AbsBf / dBds_f : 1e20;

            if (std::isfinite(Lf) && std::fabs(Lf) > 0.0) {
                // Keep the same focusing form you used previously (explicit, conservative)
                mu += (1.0 - mu*mu) * (vNormal / std::max(1e-30, Speed)) * (dt_current / Lf);
                mu  = clamp_mu(mu);
            }
        }

        // Move particle along field line
        FieldLineCoord = FL::FieldLinesAll[iFieldLine].move(FieldLineCoord, ds_parallel, Segment);

    if (Segment == NULL) {
      // Particle left the simulation domain
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }


        // Update speed and store back
        Speed = std::hypot(vParallel, vNormal);
        PB::SetVParallel(vParallel, ParticleData);
        PB::SetVNormal(vNormal, ParticleData);
        PB::SetFieldLineCoord(FieldLineCoord, ParticleData);

        TimeCounter += dt_current;
    }

    // Attach particle to the segment-local list (AMR-friendly)
    switch (_PIC_PARTICLE_LIST_ATTACHING_) {
    case _PIC_PARTICLE_LIST_ATTACHING_FL_SEGMENT_: {
        long int temp = Segment->tempFirstParticleIndex.exchange(ptr);
        PB::SetNext(temp, ParticleData);
        PB::SetPrev(-1, ParticleData);
        if (temp != -1) PB::SetPrev(ptr, temp);
    } break;
    default:
        exit(__LINE__, __FILE__, "Error: unknown particle attaching mode");
    }

    return _PARTICLE_MOTION_FINISHED_;
}
