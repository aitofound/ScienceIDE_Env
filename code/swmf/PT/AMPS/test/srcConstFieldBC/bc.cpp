//==============================================================================
// InjectBoundaryParticles()
//------------------------------------------------------------------------------
// Inject drifting-Maxwellian plasma particles through *external* block faces,
// using AMPS Maxwellian flux + sampler, assuming uniform upstream (n, U, T)
// identical to your prepopulation parameters.
//
// KEY POINT (your note):
//   We do NOT iterate boundary-adjacent cells. The injection flux returned by
//   CalculateInjectionRate_MaxwellianDistribution() depends only on (n,U,T) and
//   the face normal, so it is constant over a *block face* for uniform inflow.
//   Therefore we inject directly into the block with a single Poisson process
//   per external face.
//
// UNITS (SI build):
//   - NumberDensity:  [1/m^3]
//   - BulkVelocity:   [m/s]
//   - Temperature:    [K]
//   - Flux returned:  [1/(m^2 s)]  (or [1/(m^(DIM-1) s)] in general DIM)
//   - InjectMaxwellianDistribution() samples v in [m/s]
//
// NORMAL CONVENTION:
//   ExternalNormal points OUT of the block/domain.
//   Inflow corresponds to v · ExternalNormal < 0.
//   We pass ExternalNormal unchanged to AMPS routines.
//   To place particles slightly inside, shift x by -eps*ExternalNormal.
//
// POSITION SAMPLING:
//   Uniform over the full face using the face coordinate frame:
//     x = x0 + r0*e0 + r1*e1  (with r0,r1 ~ U[0,1])
//   In DIM=2, one of (e0,e1) will be tangent and the other ~0;
//   in DIM=1 both are ~0 and x=x0.
//
// PARTICLE WEIGHT:
//   ParticleWeight is physical particles per macroparticle, consistent with
//   your prepopulation. The macroparticle arrival rate is:
//     Rate_macro = Flux * FaceMeasure / ParticleWeight
//
// RETURN:
//   Global number of injected macroparticles (MPI_SUM).
//==============================================================================

#include "cli.h"
#include "pic.h"
#include "../pic/units/pic_units_normalization.h"

int InjectBoundaryParticles(const picunits::Factors& F,
                                 const TestConfig& cfg,
                                 int spec,
                                 double dt_no) {
  if (cfg.mode != TestConfig::Mode::WithParticles) return 0;
  if (!(dt_no > 0.0)) return 0;

  if (!cfg.sw_has_ncm3) {
    exit(__LINE__, __FILE__,
         "InjectBoundaryParticles: cfg.sw_has_ncm3 must be true (need upstream number density).");
  }
  if (!cfg.sw_has_TK) {
    exit(__LINE__, __FILE__,
         "InjectBoundaryParticles: cfg.sw_has_TK must be true (need upstream temperature).");
  }
  if (!(F.No2SiT > 0.0 && F.No2SiL > 0.0 && F.Si2NoV > 0.0)) {
    exit(__LINE__, __FILE__,
         "InjectBoundaryParticles: invalid Factors (No2SiT/No2SiL/Si2NoV must be >0).");
  }

  static int ncall=0;

  // Cache uniform B0 for particle birth initialization (solver units).
  // This is used only when _USE_PARTICLE_V_PARALLEL_NORM_ is enabled.
  #if _USE_PARTICLE_V_PARALLEL_NORM_ == _PIC_MODE_ON_
  VparVnormMu::gUniformB0_no[0] = cfg.B0[0];
  VparVnormMu::gUniformB0_no[1] = cfg.B0[1];
  VparVnormMu::gUniformB0_no[2] = cfg.B0[2];
  #endif
  

  // ---- helpers (lambdas) ----------------------------------------------------
  auto get_bulk_velocity_SI = [&]() -> std::array<double,3> {
    std::array<double,3> U = {0.0, 0.0, 0.0};

    if (cfg.sw_has_u_kms) {
      U[0] = cfg.sw_u_kms[0] * 1.0e3;
      U[1] = cfg.sw_u_kms[1] * 1.0e3;
      U[2] = cfg.sw_u_kms[2] * 1.0e3;
    }
    else if (cfg.sw_has_u_mps) {
      U[0] = cfg.sw_u_mps[0];
      U[1] = cfg.sw_u_mps[1];
      U[2] = cfg.sw_u_mps[2];
    }
    else {
      // cfg.sw_u0 is in solver units -> SI using No2SiV = 1/Si2NoV
      const double No2SiV = 1.0 / F.Si2NoV;
      U[0] = cfg.sw_u0[0] * No2SiV;
      U[1] = cfg.sw_u0[1] * No2SiV;
      U[2] = cfg.sw_u0[2] * No2SiV;
    }

    return U;
  };

  auto si2no_v3_local = [&](double* v_si, double* v_no) {
    picunits::si2no_v3(v_si, v_no, F);
  };

  // Face "measure" scaling: mesh returns L^(DIM-1) in mesh units.
  // Convert to SI by multiplying by No2SiL^(DIM-1).
  auto face_measure_scale_SI = [&]() -> double {
    if (DIM == 3) return F.No2SiL * F.No2SiL; // m^2 per (mesh L^2)
    if (DIM == 2) return F.No2SiL;            // m   per (mesh L)
    return 1.0;                                         // DIM==1: measure is dimensionless here
  };

  // ---- upstream inflow params (SI) ------------------------------------------
  const double n_SI_m3 = cfg.sw_n_cm3 * 1.0e6;  // cm^-3 -> m^-3
  const double T_K     = cfg.sw_TK;
  const auto   U_SI    = get_bulk_velocity_SI();
  const double dt_SI   = dt_no * F.No2SiT;

  int localInjected = 0;

  // ---- loop over local blocks -----------------------------------------------
  for (auto* node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];
       node != NULL; node = node->nextNodeThisThread) {

    // Particle weight (physical particles per macroparticle)
    const double ParticleWeight = node->block->GetLocalParticleWeight(spec);
    if (!(ParticleWeight > 0.0)) continue;

    // Small inward offset to avoid particles lying exactly on the face.
    // Use a tiny fraction of a cell size in mesh units.
    double h_cell = (node->xmax[0] - node->xmin[0]) / _BLOCK_CELLS_X_;
#if DIM >= 2
    h_cell = std::min(h_cell, (node->xmax[1] - node->xmin[1]) / _BLOCK_CELLS_Y_);
#endif
#if DIM == 3
    h_cell = std::min(h_cell, (node->xmax[2] - node->xmin[2]) / _BLOCK_CELLS_Z_);
#endif
    const double eps = 1.0e-6 * h_cell;

    // ---- loop faces: inject only on true domain boundary faces --------------
    for (int nface = 0; nface < 2*DIM; ++nface) {
      if (node->GetNeibFace(nface, 0, 0, PIC::Mesh::mesh) != NULL) continue;

      // Outward unit normal (must point outside the block/domain)
      double ExternalNormal[3] = {0.0, 0.0, 0.0};
      node->GetExternalNormal(ExternalNormal, nface);

      // Maxwellian inflow flux through this face (SI): [1/(m^(DIM-1) s)]
      const double Flux =
        PIC::BC::CalculateInjectionRate_MaxwellianDistribution(
          n_SI_m3, T_K, U_SI.data(), ExternalNormal, spec);

      if (!(Flux > 0.0)) continue;

      // Total face measure in SI:
      //   DIM=3: area [m^2]
      //   DIM=2: length [m]
      //   DIM=1: "measure" [1] (conceptually unit cross-section)
      const double FaceMeasure_mesh = node->GetBlockFaceSurfaceArea(nface);
      const double FaceMeasure_SI   = FaceMeasure_mesh * face_measure_scale_SI();

      if (!(FaceMeasure_SI > 0.0)) continue;

      // Macroparticle arrival rate for the whole face: [1/s]
      const double RateMacro = Flux * FaceMeasure_SI / ParticleWeight;
      if (!(RateMacro > 0.0)) continue;

      // Coordinate frame spanning the entire face:
      // x = x0 + r0*e0 + r1*e1 covers the full face uniformly for r0,r1 in [0,1].
      double x0[3], e0[3], e1[3];
      PIC::Mesh::mesh->GetBlockFaceCoordinateFrame_3D(x0, e0, e1, nface, node);

      // Poisson arrivals on this face over dt_SI.
      double t = 0.0;
      while ((t += -std::log(rnd()) / RateMacro) < dt_SI) {

        // --- Sample a uniform point over the full face -----------------------
        const double r0 = rnd();
        const double r1 = rnd();

        double x[3] = {0.0, 0.0, 0.0};
        for (int idim = 0; idim < DIM; ++idim) {
          x[idim] = x0[idim] + r0*e0[idim] + r1*e1[idim];
        }

        // Nudge inward: inward direction is -ExternalNormal
        for (int idim = 0; idim < DIM; ++idim) {
          x[idim] -= eps * ExternalNormal[idim];
        }

        // --- Sample inflow velocity in SI [m/s] consistent with the same flux -
        //     InjectMaxwellianDistribution enforces v·ExternalNormal < 0.
        double v_SI_sample[3] = {0.0, 0.0, 0.0};
        double ExternalNormalLocal[3] = {ExternalNormal[0], ExternalNormal[1], ExternalNormal[2]};

        const int weightMode = _PIC_DISTRIBUTION_WEIGHT_CORRECTION_MODE__NO_WEIGHT_CORRECTION_;
        const double corr =
          PIC::Distribution::InjectMaxwellianDistribution(
            v_SI_sample, U_SI.data(), T_K, ExternalNormalLocal, spec, weightMode);

        // Optional debug safety (enable temporarily if you’re unsure about sign conventions):
        // const double vdotn = v_SI_sample[0]*ExternalNormal[0] + v_SI_sample[1]*ExternalNormal[1] + v_SI_sample[2]*ExternalNormal[2];
        // if (vdotn >= 0.0) exit(__LINE__,__FILE__,"InjectBoundaryParticles: sampled velocity is not incoming (vdotn>=0)");

        // Convert SI velocity to solver (normalized) velocity for storage
        double v_no[3];
        si2no_v3_local(v_SI_sample, v_no);

        // Per-particle correction factor (==1 for NO_WEIGHT_CORRECTION)
        double w = corr;
        int specLocal = spec;

	ncall++;

        // Particle birth:
        //   - always store full 3D velocity v_no in the particle buffer
        //   - if _USE_PARTICLE_V_PARALLEL_NORM_ is enabled, additionally
        //     initialize V_parallel, V_normal and magnetic moment (if enabled)
        //     BEFORE the particle mover is called (InitMode==MOVE).
        PIC::ParticleBuffer::InitiateParticle(
          x, v_no, &w, &specLocal,
          NULL,
          _PIC_INIT_PARTICLE_MODE__MOVE_,
          (void*)node,
          #if _USE_PARTICLE_V_PARALLEL_NORM_ == _PIC_MODE_ON_
          VparVnormMu::InitParticle
          #else
          NULL
          #endif
        );

//  int np=PIC::Debugger::GetParticleNumberInLists(true);
//  int npb=PIC::ParticleBuffer::GetAllPartNum();
///  if (np!=npb) exit(__LINE__,__FILE__);


        ++localInjected;
      } // arrivals while t<dt
    } // face loop
  } // block loop


  //Commit thread-safe temp per-cell particle lists into the main per-cell lists 
  PIC::Mover::CommitTempParticleMovingListsToFirstCellParticleTable(); 

  // Exchange injected particles across MPI ranks so each rank owns what lies in its subdomain
  PIC::Parallel::ExchangeParticleData();

  int np=PIC::Debugger::GetParticleNumberInLists(true);
  int npb=PIC::ParticleBuffer::GetAllPartNum();
  if (np!=npb) exit(__LINE__,__FILE__);



  // Exchange injected particles across MPI ranks so each rank owns what lies in its subdomain
  // Return global injected count
  int globalInjected = 0;
  MPI_Allreduce(&localInjected, &globalInjected, 1, MPI_INT, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);


   np=PIC::Debugger::GetParticleNumberInLists(true);
   npb=PIC::ParticleBuffer::GetAllPartNum();
  if (np!=npb) exit(__LINE__,__FILE__);

  return globalInjected;
}

