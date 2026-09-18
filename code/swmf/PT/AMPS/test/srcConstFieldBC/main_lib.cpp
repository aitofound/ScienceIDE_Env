/*
 * main.cpp
 *
 *  Created on: Jun 21, 2012
 *      Author: fougere and vtenishe
 */

/***************************************************************************************
 * Test driver for PIC with/without particles and constant field boundary conditions
 *
 * OVERVIEW
 * --------
 * This test can run in four modes controlled via the command line:
 *
 *   1) -particles
 *        • PIC with particles enabled.
 *        • Field boundary conditions are held constant.
 *        • Particle boundary condition is *specular reflection* at domain faces.
 *        • Initial fields: by default E = 0, B = 0 unless you also set a background.
 *
 *   2) -no-particles
 *        • Field-only (no particles).
 *        • Field boundary conditions are held constant.
 *        • Initial fields default to E = 0, B = 0 (unless -B/-E are provided).
 *
 *   3) -B [Bx By Bz]
 *        • Field-only (implies -no-particles).
 *        • Initialize B to a uniform vector; E = 0.
 *        • If the three numbers are omitted, defaults to B = (0, 1, 0).
 *
 *   4) -E [Ex Ey Ez]
 *        • Field-only (implies -no-particles).
 *        • Initialize E to a uniform vector; B = 0.
 *        • If the three numbers are omitted, defaults to E = (1, 0, 0).
 *
 * Additionally:
 *   -stencil-order=N
 *        • Sets the finite-difference stencil order used by divergence/curl/Poisson
 *          operators in this test (typical choices: 2,4,6,8). Default is 2.
 *
 * BOUNDARY CONDITIONS
 * -------------------
 * • Fields: “constant BC” — the field values at the domain boundary are held fixed to
 *   the values chosen by the mode (e.g., the uniform E or B you set). In practice this
 *   can be enforced via your project’s boundary callback or by reapplying the uniform
 *   values to halo layers after each step in field-only runs.
 * • Particles: “specular reflection” — when a particle hits a domain face, its velocity
 *   component normal to that face is flipped (tangential components are preserved).
 *
 * HOW SetIC() WORKS NOW
 * ---------------------
 * SetIC(cfg) zeros all fields, then applies the requested initial condition:
 *   • -particles: leaves E and B at your default (often zeros) unless your test adds a
 *     background; particles are (pre)populated later as usual.
 *   • -no-particles: leaves both fields zero.
 *   • -B: sets cell-centered B uniformly to (Bx,By,Bz), keeps corner E = 0.
 *   • -E: sets corner E uniformly to (Ex,Ey,Ez), keeps cell-centered B = 0.
 *
 * EXAMPLES
 * --------
 *   # PIC with particles, specular reflection, 4th-order stencil
 *   ./amps_test -particles -stencil-order=4
 *
 *   # Field-only, uniform B=(0,1,0), default 2nd-order
 *   ./amps_test -B
 *
 *   # Field-only, E=(0.2,0,0), 8th-order stencil
 *   ./amps_test -E 0.2 0.0 0.0 -stencil-order 8
 *
 *   # Field-only, all-zero fields, 6th-order stencil
 *   ./amps_test -no-particles -stencil-order=6
 *
 ***************************************************************************************/


#include "pic.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <time.h>
#include <algorithm>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctime>

#include "meshAMRcutcell.h"
#include "cCutBlockSet.h"
#include "meshAMRgeneric.h"

#include "../../srcInterface/LinearSystemCornerNode.h"
#include "linear_solver_wrapper_c.h"

//#include "PeriodicBCTest.dfn"

#if _CUDA_MODE_ == _ON_
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#endif

//for lapenta mover

#include "pic.h"
#include "Exosphere.dfn"
#include "Exosphere.h"



//==============================================================================
// Optional internal spherical boundary ("Enceladus" placeholder)
//------------------------------------------------------------------------------
// We use the built-in internal-boundary sphere implementation:
//   PIC::BC::InternalBoundary::Sphere
//
// For this test driver we keep the physics intentionally minimal:
//   * The sphere is a SOLID obstacle inside the domain.
//   * Particles initialized inside the sphere are rejected (vacuum interior).
//   * Particles that intersect the surface are deleted (fully absorbing).
//
// This gives a clean geometric placeholder for Enceladus that can later be
// extended with source processes (sputtering, thermal desorption), secondary
// emission, or surface charging.
//
// IMPORTANT ORDERING:
//   AMPS requires that ALL internal surfaces be registered BEFORE
//   PIC::Mesh::mesh->init() is called. If you register the sphere after that,
//   you will hit:
//     "all internal surface must be registered before initialization of the mesh"
//==============================================================================

namespace ConstFieldBC_InternalSphere {
  static bool   gEnabled = false;
  static double gCenter[3] = {0.0,0.0,0.0};
  static double gRadius = 0.0;
  static cInternalSphericalData* gSphere = NULL;

  // Surface interaction callback (absorbing surface)
  static int ParticleSphereInteraction_Absorb(int spec,long int ptr,double *x,double *v,double &dtTotal,void *NodeDataPointer,void *SphereDataPointer) {
    (void)spec; (void)ptr; (void)x; (void)v; (void)dtTotal; (void)NodeDataPointer; (void)SphereDataPointer;
    return _PARTICLE_DELETED_ON_THE_FACE_;
  }

  static inline bool Inside(const double x[3]) {
    const double dx = x[0]-gCenter[0];
    const double dy = x[1]-gCenter[1];
    const double dz = x[2]-gCenter[2];
    return (dx*dx + dy*dy + dz*dz) <= gRadius*gRadius;
  }
}

bool IsPointInsideInternalSphere(const double x[3]) {
  using namespace ConstFieldBC_InternalSphere;
  if (!gEnabled) return false;
  return Inside(x);
}

void InitInternalSphericalBoundary(const TestConfig& cfg) {
  using namespace ConstFieldBC_InternalSphere;

  gEnabled = false;
  gSphere = NULL;
  gRadius = 0.0;
  gCenter[0]=gCenter[1]=gCenter[2]=0.0;

  if (!cfg.use_sphere) return;

  // -------------------------------------------------------------------------
  // Compute default geometry from the finalized domain extents
  // -------------------------------------------------------------------------
  const double Lx = xmax[0]-xmin[0];
  const double Ly = xmax[1]-xmin[1];
  const double Lz = xmax[2]-xmin[2];
  const double Lmin = std::min(Lx,std::min(Ly,Lz));

  if (cfg.user_sphere_center) {
    gCenter[0]=cfg.sphere_center[0];
    gCenter[1]=cfg.sphere_center[1];
    gCenter[2]=cfg.sphere_center[2];
  }
  else {
    gCenter[0]=0.5*(xmin[0]+xmax[0]);
    gCenter[1]=0.5*(xmin[1]+xmax[1]);
    gCenter[2]=0.5*(xmin[2]+xmax[2]);
  }

  if (cfg.user_sphere_radius) gRadius = cfg.sphere_radius;
  else gRadius = 0.25*Lmin;

  if (!(gRadius>0.0)) {
    if (PIC::Mesh::mesh->ThisThread==0) {
      std::fprintf(stderr,"[ConstFieldBC] WARNING: sphere enabled but radius<=0; disabling internal sphere\n");
    }
    return;
  }

  // -------------------------------------------------------------------------
  // Register the internal boundary with AMPS
  // -------------------------------------------------------------------------
  // Reserve zero surface-sampling variables per species for now (no flux diagnostics).
  std::vector<long int> ReserveSamplingSpace(PIC::nTotalSpecies,0);

  // Surface mesh resolution of the sphere discretization (zenith, azimuth).
  // Increase if you later need finer per-element diagnostics.
  cInternalSphericalData::SetGeneralSurfaceMeshParameters(20,40);
  PIC::BC::InternalBoundary::Sphere::Init(ReserveSamplingSpace.data(),NULL);

  cInternalBoundaryConditionsDescriptor d = PIC::BC::InternalBoundary::Sphere::RegisterInternalSphere();
  gSphere = (cInternalSphericalData*)d.BoundaryElement;
  gSphere->SetSphereGeometricalParameters(gCenter,gRadius);

  // Use a uniform surface resolution target for cut-cell / surface intersection.
  gSphere->localResolution = BulletLocalResolution;
  gSphere->ParticleSphereInteraction = ParticleSphereInteraction_Absorb;

  gEnabled = true;

  if (PIC::Mesh::mesh->ThisThread==0) {
    std::printf("[ConstFieldBC] Internal sphere enabled: center=(%g,%g,%g), R=%g\n", gCenter[0],gCenter[1],gCenter[2],gRadius);
  }
}

//==============================================================================
// Optional initialization of reduced (aligned) velocity state at particle birth
//------------------------------------------------------------------------------
// See the detailed discussion in bc.cpp. The key points are:
//   - When _USE_PARTICLE_V_PARALLEL_NORM_ is ON, particles have dedicated storage
//     for V_parallel and V_normal (in addition to the full 3D velocity vector).
//   - For guiding-center / gyrokinetic / magnetic-moment workflows it is useful
//     to also compute the magnetic moment at birth:
//         mu = (gamma^2 m v_perp^2) / (2|B|)
//   - This test driver uses a spatially uniform background magnetic field B0
//     (cfg.B0 in solver units), so we can compute these values in the generic
//     PIC::ParticleBuffer::InitiateParticle() callback without x/node.
//==============================================================================
namespace VparVnormMu {
  double gUniformB0_no[3] = {0.0, 0.0, 0.0};

  void InitParticle(PIC::ParticleBuffer::byte* ParticleDataStart) {
    namespace PB = PIC::ParticleBuffer;

    double* v = PB::GetV(ParticleDataStart);
    const int spec = PB::GetI(ParticleDataStart);

    const double Bx = gUniformB0_no[0];
    const double By = gUniformB0_no[1];
    const double Bz = gUniformB0_no[2];
    const double absB = std::sqrt(Bx*Bx + By*By + Bz*Bz) + 1.0e-15;
    const double bx = Bx/absB, by = By/absB, bz = Bz/absB;

    const double vpar = v[0]*bx + v[1]*by + v[2]*bz;
    const double v2   = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
    const double vperp2 = std::max(0.0, v2 - vpar*vpar);

    if ((fabs(vpar)>10.0)||(vperp2>100.0)) {
	    double ee=0;
	    ee+=33;
    }

    PB::SetVParallel(vpar, ParticleDataStart);
    PB::SetVNormal(std::sqrt(vperp2), ParticleDataStart);

    // Only write magnetic moment if that storage is present.
    #if _USE_MAGNETIC_MOMENT_ == _PIC_MODE_ON_
    {
      double gamma2 = 1.0;
      #if _PIC_PARTICLE_MOVER__RELATIVITY_MODE_ == _PIC_MODE_ON_
      const double c_no = picunits::si2no_v(SpeedOfLight, PIC::Units::Factors);
      const double beta2 = v2/(c_no*c_no);
      if (beta2 >= 1.0) exit(__LINE__,__FILE__,"Error: v^2 >= c^2 in normalized units");
      gamma2 = 1.0/(1.0 - beta2);
      #endif

      const double m_SI_kg = PIC::MolecularData::GetMass(spec);
      const double m_no    = picunits::si2no_m(m_SI_kg, PIC::Units::Factors);
      const double mu_no = 0.5 * gamma2 * m_no * vperp2 / absB;
      PB::SetMagneticMoment(mu_no, ParticleDataStart);
    }
    #endif
  }
}



#include "main_lib.h"

// -----------------------------------------------------------------------------
// InitGlobalParticleWeight_TargetPPC()
//   Choose a global particle weight so that PrepopulateDomain() injects roughly
//   cfg.target_ppc macro-particles per cell *per species* for the uniform
//   solar-wind-like IC.
//
//   PrepopulateDomain() injects particles with:
//       npart ≈ NumberDensity * CellVolume / ParticleWeight
//   where npart is per species (ions and electrons are injected separately).
//
//   We estimate a representative cell volume using the MIN cell volume across
//   all MPI ranks (and skip periodic halo blocks) to avoid under-resolving the
//   smallest cells when AMR/nonuniform meshes are used.
//
//   NOTE: This routine uses the same normalization logic as PrepopulateDomain()
//         (rho_conv and the definition of NumberDensity_ref) to ensure the ppc
//         target matches the injection formula used by the test.
// -----------------------------------------------------------------------------
void InitGlobalParticleWeight_TargetPPC(const picunits::Factors& F,const TestConfig& cfg) {
  if (cfg.mode != TestConfig::Mode::WithParticles) return;

  const double Nppc_target = cfg.target_ppc;
  if (Nppc_target <= 0.0) return;

  // Species indices used by this test: ion=0, electron=1
  const int ionSpec=0, electronSpec=1;

  // Mass normalization consistent with PrepopulateDomain() in this driver.
  const double ionMass      = PIC::MolecularData::GetMass(ionSpec)/_AMU_;
  const double electronMass = PIC::MolecularData::GetMass(electronSpec)/_AMU_;

  // Ensure BlockTable is up to date.
  PIC::DomainBlockDecomposition::UpdateBlockTable();

  // Conservative representative cell volume: min cell volume over all ranks.
  double localMinCellVolume = 1.0e100;
  const int nBlock[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};

  for (int nLocalNode=0; nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks; nLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (node->Thread!=PIC::ThisThread) continue;

    // In periodic mode, skip the periodic halo blocks (blocks with missing neighbors).
    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;
      for (int iface=0; iface<6; iface++) {
        if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) { BoundaryBlock=true; break; }
      }
      if (BoundaryBlock==true) continue;
    }

    double CellVolume=1.0;
    for (int idim=0; idim<3; idim++) {
      const double dx=(node->xmax[idim]-node->xmin[idim])/(double)nBlock[idim];
      CellVolume*=dx;
    }
    if (CellVolume < localMinCellVolume) localMinCellVolume = CellVolume;
  }

  double globalMinCellVolume=0.0;
  MPI_Allreduce(&localMinCellVolume,&globalMinCellVolume,1,MPI_DOUBLE,MPI_MIN,MPI_GLOBAL_COMMUNICATOR);

  // If for some reason all local blocks were skipped (should not happen in normal runs),
  // fall back to a safe positive volume to avoid division by zero.
  if (!(globalMinCellVolume>0.0) || globalMinCellVolume>1.0e90) {
    globalMinCellVolume = 1.0;
  }

  // Reference number density derived from the configured mass density.
  // IMPORTANT: In this test driver, cfg.sw_* fields are finalized (including
  // any SI->normalized conversions) by FinalizeConfigUnits(cfg) in main.cpp.
  // Therefore, the injection formula uses cfg.sw_rho0/cfg.sw_p0 directly, with
  // no additional ad-hoc scaling.
  const double rho = cfg.sw_rho0;
  const double NumberDensity_ref = rho / (ionMass + electronMass);

  const double ParticleWeight_ref = NumberDensity_ref * globalMinCellVolume / Nppc_target;

  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(ionSpec,ParticleWeight_ref);
  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(electronSpec,ParticleWeight_ref);

  const double pw = EvaluateGlobalParticleWeightForTargetPPC(F, cfg);
  PIC::ParticleWeightTimeStep::GlobalParticleWeight[ionSpec] = pw;  // if using species-dependent global weight
  PIC::ParticleWeightTimeStep::GlobalParticleWeight[electronSpec] = pw;  // if using species-dependent global weight

  if (PIC::ThisThread==0) {
    std::printf("[ConstFieldBC] ParticleWeight set for ~%.1f ppc/spec: weight=%e, n_ref=%e, Vcell(min)=%e\n",
                Nppc_target, ParticleWeight_ref, NumberDensity_ref, globalMinCellVolume);
  }
}




//------------------------------------------------------------------------
picunits::Factors  FinalizeConfigUnits(TestConfig& cfg) {
  // This routine converts *physical* (SI) user inputs (if provided) into the
  // unit system expected by the compiled ECSIM field solver.
  //
  // The AMPS/ECSIM build can be configured either to store fields in SI units
  // (_PIC_FIELD_SOLVER_INPUT_UNIT_SI_) or in a normalized "code-unit" system
  // (_PIC_FIELD_SOLVER_INPUT_UNIT_NORM_). For the latter, we use the
  // normalization in pic_units_normalization.h:
  //   - choose normalization scales (lSI, uSI, mSI)
  //   - compute factors
  //   - map SI -> normalized for {rho, p, v, B, E}
  //
  // IMPORTANT:
  //   * This is a *driver-level* convenience. The test logic (SetIC,
  //     PrepopulateDomain) expects cfg.B0/E0 and cfg.sw_* already expressed in
  //     the solver's input units after this routine runs.
  //   * For the solar-wind option, the convective field is E = -u x B.

  //determine cfg.units_mSI_kg: src/pic/units/example_units.cpp 
  const double m_p=1.6726E-27; 
  const double q_p=1.6022E-19; 

  cfg.units_mSI_kg=1.0E7*cfg.units_lSI_m*pow(m_p/q_p,2) ; // [kg] 

  // Build normalization factors once (used only if the solver expects NORM units).
  // NOTE: picunits::build() takes a NormScalesSI struct (see pic_units_normalization.h).
  picunits::NormScalesSI norm_scales{cfg.units_lSI_m, cfg.units_uSI_mps, cfg.units_mSI_kg};
  picunits::Factors F = picunits::build(norm_scales);

  auto si_to_solver_B = [&](const double B_T[3], double B_out[3]) {
#if _PIC_FIELD_SOLVER_INPUT_UNIT_ == _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_
    picunits::si2no_B3(B_T, B_out, F);
#else
    // SI build: store Tesla directly
    for (int d=0; d<3; d++) B_out[d] = B_T[d];
#endif
  };

  auto si_to_solver_E = [&](const double E_Vm[3], double E_out[3]) {
#if _PIC_FIELD_SOLVER_INPUT_UNIT_ == _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_
    picunits::si2no_E3(E_Vm, E_out, F);
#else
    // SI build: store V/m directly
    for (int d=0; d<3; d++) E_out[d] = E_Vm[d];
#endif
  };

  auto si_to_solver_v = [&](const double u_mps[3], double u_out[3]) {
#if _PIC_FIELD_SOLVER_INPUT_UNIT_ == _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_
    picunits::si2no_v3(u_mps, u_out, F);
#else
    for (int d=0; d<3; d++) u_out[d] = u_mps[d];
#endif
  };

  auto si_to_solver_rho = [&](double rho_kgm3) -> double {
#if _PIC_FIELD_SOLVER_INPUT_UNIT_ == _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_
    return picunits::si2no_rho(rho_kgm3, F);
#else
    return rho_kgm3;
#endif
  };

  auto si_to_solver_p = [&](double p_Pa) -> double {
#if _PIC_FIELD_SOLVER_INPUT_UNIT_ == _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_
    return picunits::si2no_P(p_Pa, F);
#else
    return p_Pa;
#endif
  };

  // ---------------- Background fields (non-solar-wind mode too) ----------------
  if (cfg.userB_SI) {
    si_to_solver_B(cfg.B0_SI_T, cfg.B0);
    cfg.userB = true;
  }

  if (cfg.userE_SI) {
    si_to_solver_E(cfg.E0_SI_Vm, cfg.E0);
    cfg.userE = true;
  }

  // ---------------- Solar wind: convert physical IC to solver units ----------------
  if (cfg.mode == TestConfig::Mode::WithParticles) {
    // --- Velocity ---
    bool have_u_SI = false;
    double u_SI[3] = {0.0,0.0,0.0};
    if (cfg.sw_has_u_kms) {
      for (int d=0; d<3; d++) u_SI[d] = cfg.sw_u_kms[d] * 1.0e3; // km/s -> m/s
      have_u_SI = true;
    }
    else if (cfg.sw_has_u_mps) {
      for (int d=0; d<3; d++) u_SI[d] = cfg.sw_u_mps[d];
      have_u_SI = true;
    }

    if (have_u_SI) {
      si_to_solver_v(u_SI, cfg.sw_u0);
    }

    // --- Magnetic field ---
    // If the user provided SW B in nT, store it also as background B in SI so it
    // gets converted above (cfg.userB_SI).
    if (cfg.sw_has_BnT) {
      for (int d=0; d<3; d++) cfg.B0_SI_T[d] = cfg.sw_BnT[d] * 1.0e-9; // nT -> Tesla
      cfg.userB_SI = true;
      si_to_solver_B(cfg.B0_SI_T, cfg.B0);
      cfg.userB = true;
    }

    // --- Density & pressure ---
    // If n and T are provided in physical units, we build SI rho,p and convert.
    if (cfg.sw_has_ncm3) {
      const double kB_SI = 1.380649e-23; // J/K
      const double n_m3 = cfg.sw_n_cm3 * 1.0e6; // cm^-3 -> m^-3

      // NOTE: we keep the legacy assumption of one ion species (species=0) and
      //       quasi-neutral plasma with Ti=Te=T.
      const double mi_kg = PIC::MolecularData::GetMass(0);
      const double me_kg = PIC::MolecularData::GetMass(PIC::nTotalSpecies-1);

      const double rho_SI = n_m3 * (mi_kg + me_kg); // kg/m^3
      cfg.sw_rho0 = si_to_solver_rho(rho_SI);

      if (cfg.sw_has_TK) {
        const double T = cfg.sw_TK;
        const double p_SI = n_m3 * kB_SI * (T + T); // Pi+Pe with Ti=Te=T
        cfg.sw_p0 = si_to_solver_p(p_SI);
      }
    }

    // --- Electric field ---
    // Explicit physical E overrides E=-u×B.
    if (cfg.sw_has_EmVm || cfg.sw_has_EVm) {
      double E_SI[3] = {0.0,0.0,0.0};
      if (cfg.sw_has_EmVm) {
        for (int d=0; d<3; d++) E_SI[d] = cfg.sw_E_mVm[d] * 1.0e-3; // mV/m -> V/m
      }
      else {
        for (int d=0; d<3; d++) E_SI[d] = cfg.sw_E_Vm[d];
      }

      for (int d=0; d<3; d++) cfg.E0_SI_Vm[d] = E_SI[d];
      cfg.userE_SI = true;
      si_to_solver_E(cfg.E0_SI_Vm, cfg.E0);
      cfg.userE = true;
    }
    else {
      // If sw-evxb is unset, default to computing E=-u×B when particles are enabled.
      const bool do_evxb = (cfg.sw_evxb == 1) || (cfg.sw_evxb < 0);

      if (do_evxb && cfg.userB && (cfg.sw_has_u_kms || cfg.sw_has_u_mps || (cfg.sw_u0[0]!=0.0 || cfg.sw_u0[1]!=0.0 || cfg.sw_u0[2]!=0.0))) {
        // Compute in solver units (works for both SI and normalized builds as long as
        // u and B are in consistent units):  E = -u × B
        const double ux = cfg.sw_u0[0], uy = cfg.sw_u0[1], uz = cfg.sw_u0[2];
        const double Bx = cfg.B0[0],    By = cfg.B0[1],    Bz = cfg.B0[2];

        cfg.E0[0] = -(uy*Bz - uz*By);
        cfg.E0[1] = -(uz*Bx - ux*Bz);
        cfg.E0[2] = -(ux*By - uy*Bx);
        cfg.userE = true;
      }
    }
  }

  // Optional sanity print (rank 0 only) to help validate unit conversions.
  {
    int do_print = 1;
#ifdef MPI_ON
    int mpi_init = 0;
    MPI_Initialized(&mpi_init);
    if (mpi_init) {
      int rank = 0;
      MPI_Comm_rank(MPI_COMM_WORLD,&rank);
      do_print = (rank == 0);
    }
#endif
    if (do_print) {
#if _PIC_FIELD_SOLVER_INPUT_UNIT_ == _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_
      printf("[ConstFieldBC] ECSIM input units: NORM (code units). Using normalization scales: lSI=%g m, uSI=%g m/s, mSI=%g kg\n",
             cfg.units_lSI_m, cfg.units_uSI_mps, cfg.units_mSI_kg);
#else
      printf("[ConstFieldBC] ECSIM input units: SI. Background fields interpreted as SI (Tesla, V/m).\n");
#endif
    }
  }

  return F;
}

// Uniform setters used by SetIC()
// NOTE: These use the same buffer/offset accessors that your legacy SetIC() uses.
void SetUniformCornerE(const double E0[3]) {
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;

  for (auto* node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread];
       node; node = node->nextNodeThisThread) {
    if (!node->block) continue;

    char *offset;
    for (int k=0; k<=_BLOCK_CELLS_Z_; ++k)
    for (int j=0; j<=_BLOCK_CELLS_Y_; ++j)
    for (int i=0; i<=_BLOCK_CELLS_X_; ++i) {
      auto* cn = node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
      if (!cn) continue;
      offset = cn->GetAssociatedDataBufferPointer() + PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

      ((double*)(offset + CurrentEOffset))[ExOffsetIndex] = E0[0];
      ((double*)(offset + CurrentEOffset))[EyOffsetIndex] = E0[1];
      ((double*)(offset + CurrentEOffset))[EzOffsetIndex] = E0[2];

      ((double*)(offset + OffsetE_HalfTimeStep))[ExOffsetIndex] = E0[0];
      ((double*)(offset + OffsetE_HalfTimeStep))[EyOffsetIndex] = E0[1];
      ((double*)(offset + OffsetE_HalfTimeStep))[EzOffsetIndex] = E0[2];
    }
  }
}

void SetUniformCenterB(const double B0[3]) {
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;

  for (auto* node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread];
       node; node = node->nextNodeThisThread) {
    if (!node->block) continue;

    char *offset;
    for (int k=0; k<_BLOCK_CELLS_Z_; ++k)
    for (int j=0; j<_BLOCK_CELLS_Y_; ++j)
    for (int i=0; i<_BLOCK_CELLS_X_; ++i) {
      auto* cc = node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k));
      if (!cc) continue;
      offset = cc->GetAssociatedDataBufferPointer() + PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;

      ((double*)(offset + CurrentBOffset))[BxOffsetIndex] = B0[0];
      ((double*)(offset + CurrentBOffset))[ByOffsetIndex] = B0[1];
      ((double*)(offset + CurrentBOffset))[BzOffsetIndex] = B0[2];

      ((double*)(offset + PrevBOffset))[BxOffsetIndex] = B0[0];
      ((double*)(offset + PrevBOffset))[ByOffsetIndex] = B0[1];
      ((double*)(offset + PrevBOffset))[BzOffsetIndex] = B0[2];
    }
  }
}

// New SetIC based on CLI
void SetIC() {
  const double Z[3] = {0.0,0.0,0.0};
  // Zero fields
  SetUniformCornerE(Z);
  SetUniformCenterB(Z);


  // Apply chosen mode
  switch (cfg.mode) {
    case TestConfig::Mode::FieldOnlyB:
      SetUniformCenterB(cfg.B0);
      break;
    case TestConfig::Mode::FieldOnlyE:
      SetUniformCornerE(cfg.E0);
      break;
    case TestConfig::Mode::WithParticles:
      // Particle mode: keep fields uniform/constant if provided via -B/-E, otherwise leave as zero.
      if (cfg.userB) SetUniformCenterB(cfg.B0);
      if (cfg.userE) SetUniformCornerE(cfg.E0);
      break;
    case TestConfig::Mode::NoParticles:
      // Field-only run with no particles: honor user-specified backgrounds if provided.
      if (cfg.userB) SetUniformCenterB(cfg.B0);
      if (cfg.userE) SetUniformCornerE(cfg.E0);
      break;
    default:
      break;
  }

  PIC::FieldSolver::Electromagnetic::ECSIM::SyncE();
  PIC::FieldSolver::Electromagnetic::ECSIM::SyncB();
}

void CleanParticles(){
  
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;

  for (node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) if (node->block!=NULL) {
   
     long int *  FirstCellParticleTable=node->block->FirstCellParticleTable;
     if (FirstCellParticleTable==NULL) continue;
     for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
       for (int j=0;j<_BLOCK_CELLS_Y_;j++)  {
	 for (int i=0;i<_BLOCK_CELLS_X_;i++) {
	   long int * ptr=FirstCellParticleTable+(i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k));
	   while ((*ptr)!=-1) PIC::ParticleBuffer::DeleteParticle(*ptr,*ptr);
	 }   
       }
     }
     
  }

}


// ============================================================================
// PrepopulateDomain(F, cfg)  —  Drifting Maxwellian particle initialization
// ----------------------------------------------------------------------------
// This function mirrors the logic/pattern of PIC::InitialCondition::PrepopulateDomain()
// (see pic_initial_conditions.cpp), but drives the initial plasma state from
// TestConfig in *physical units* and converts velocities to the solver’s
// normalized units via picunits::si2no_v3().
//
// USER CLARIFICATIONS INCORPORATED
//   (1) cell->Measure is ALREADY NORMALIZED volume (dimensionless), i.e. it is
//       expressed in units of (L0)^DIM where L0 = F.No2SiL [m].
//   (2) Solar-wind initial state is provided as:
//         density     cfg.sw_n_cm3   [cm^-3]
//         bulk speed  cfg.sw_u_kms   [km/s] (or cfg.sw_u_mps [m/s])
//         temperature cfg.sw_TK      [K]
//       These are used when cfg.sw_has_* flags are set.
//
// WHAT “NumberDensity” MEANS HERE (important)
//   The legacy AMPS PrepopulateDomain() expects NumberDensity in the SAME units
//   as cell->Measure so that:
//       expected_macro_count = NumberDensity * cell->Measure / ParticleWeight
//   Since cell->Measure is normalized (dimensionless) and represents V_no, and
//   the physical cell volume is:
//       V_SI = V_no * (L0)^DIM  where L0 = F.No2SiL [m],
//   converting SI number density n_SI [1/m^3] into the legacy "solver density"
//   that multiplies cell->Measure is simply:
//       NumberDensity_solver = n_SI * (L0)^DIM
//   because:
//       n_SI * V_SI = n_SI * (V_no * L0^DIM) = (n_SI * L0^DIM) * V_no
//
// VELOCITY + TEMPERATURE
//   We sample a drifting Maxwellian in SI units:
//       v_SI = U_SI + sigma * N(0,1)  (each component)
//       sigma = sqrt(kB*T / m_species)
//   Then store v_no = si2no_v3(v_SI, F) into the particle buffer.
//
// SPECIES MASS USED FOR THERMAL SPREAD
//   We need m_species in kg for sigma. This code tries to obtain it from
//   PIC::MolecularData::GetMass(spec) and heuristically interprets whether it is
//   returned in kg or g. If that fails for your build, replace GetSpeciesMass_kg()
//   with the correct accessor for your codebase.
//
// ROUNDING
//   - cfg.sw_use_rounding == true  -> stochastic rounding (legacy behavior)
//   - cfg.sw_use_rounding == false -> floor() (deterministic)
//
// RETURN
//   Global total number of injected macroparticles (MPI-summed).
// ============================================================================
long int PrepopulateDomain(int spec,picunits::Factors F, const TestConfig& cfg) {
  // Only inject particles when requested
  if (cfg.mode != TestConfig::Mode::WithParticles) return 0;

  // Cache uniform B0 for particle birth initialization (solver units).
  #if _USE_PARTICLE_V_PARALLEL_NORM_ == _PIC_MODE_ON_
  VparVnormMu::gUniformB0_no[0] = cfg.B0[0];
  VparVnormMu::gUniformB0_no[1] = cfg.B0[1];
  VparVnormMu::gUniformB0_no[2] = cfg.B0[2];
  #endif

  if (!(F.No2SiL > 0.0 && F.Si2NoV > 0.0)) {
    throw std::invalid_argument("PrepopulateDomain: invalid Factors (No2SiL/Si2NoV).");
  }

  // Bulk velocity:
  //  - if sw_has_u_kms: km/s -> m/s -> normalize to v_no
  //  - else if sw_has_u_mps: m/s -> normalize to v_no
  //  - else: assume cfg.sw_u0 already in solver units (normalized), use directly
  double BulkVelocity_no[3] = {0.0,0.0,0.0};

  if (cfg.sw_has_u_kms || cfg.sw_has_u_mps) {
    double U_SI[3];
    if (cfg.sw_has_u_kms) {
      U_SI[0] = cfg.sw_u_kms[0] * 1.0e3;
      U_SI[1] = cfg.sw_u_kms[1] * 1.0e3;
      U_SI[2] = cfg.sw_u_kms[2] * 1.0e3;
    }
    else {
      U_SI[0] = cfg.sw_u_mps[0];
      U_SI[1] = cfg.sw_u_mps[1];
      U_SI[2] = cfg.sw_u_mps[2];
    }
    picunits::si2no_v3(U_SI,BulkVelocity_no,  F);
  }
  else {
    BulkVelocity_no[0] = cfg.sw_u0[0];
    BulkVelocity_no[1] = cfg.sw_u0[1];
    BulkVelocity_no[2] = cfg.sw_u0[2];
  }

  // Temperature in K (for thermal spread). If not provided, use 0K -> no spread.
  const double Temperature_K = (cfg.sw_has_TK ? cfg.sw_TK : 0.0);
  if (!(Temperature_K >= 0.0)) {
    throw std::invalid_argument("PrepopulateDomain: Temperature_K must be >= 0.");
  }

  // Thermal velocity sigma in SI [m/s]
  constexpr double kB_SI = 1.380649e-23; // J/K
  const double m_species_kg = PIC::MolecularData::GetMass(spec);

  const double sigma_SI =
    (Temperature_K > 0.0) ? std::sqrt(kB_SI * Temperature_K / m_species_kg) : 0.0;

  // Standard normal generator using Box–Muller and AMPS rnd()
  auto normal01 = []() -> double {
    double u1 = rnd();
    if (u1 < 1.0e-300) u1 = 1.0e-300;  // protect log(0)
    const double u2 = rnd();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * Pi * u2);
  };

  // --------------------------------------------------------------------------
  // Injection loop (copied structurally from PIC::InitialCondition::PrepopulateDomain)
  // --------------------------------------------------------------------------
  int iCell,jCell,kCell,idim;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node;
  PIC::Mesh::cDataCenterNode* cell;
  long int nd, nLocalInjectedParticles = 0, nGlobalInjectedParticles = 0;

#if DIM == 3
  static const int iCellMax=_BLOCK_CELLS_X_, jCellMax=_BLOCK_CELLS_Y_, kCellMax=_BLOCK_CELLS_Z_;
#elif DIM == 2
  const int iCellMax=_BLOCK_CELLS_X_, jCellMax=_BLOCK_CELLS_Y_, kCellMax=1;
#elif DIM == 1
  const int iCellMax=_BLOCK_CELLS_X_, jCellMax=1,               kCellMax=1;
#else
  exit(__LINE__,__FILE__,"Error: the option is not defined");
#endif

  // boundaries of the block and middle point of the cell
  double* xmin;
  double* xmax;
  double* xMiddle;

  double x[3], v_no[3];

  for (node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];
       node != NULL; node = node->nextNodeThisThread) {

    // Skip periodic "ghost" boundary blocks (legacy logic)
    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;
      for (int iface=0; iface<6; iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
        BoundaryBlock=true;
        break;
      }
      if (BoundaryBlock==true) continue;
    }

    // Local copy of the block's cells
    const int cellListLength = node->block->GetCenterNodeListLength();
    PIC::Mesh::cDataCenterNode* cellList[cellListLength];
    memcpy(cellList, node->block->GetCenterNodeList(), cellListLength*sizeof(PIC::Mesh::cDataCenterNode*));

    xmin = node->xmin;
    xmax = node->xmax;

    // Particle statistical weight (physical particles per macroparticle)
    double ParticleWeight;
#if _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
    ParticleWeight = PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
#else
    ParticleWeight = node->block->GetLocalParticleWeight(spec);
#endif
    if (!(ParticleWeight > 0.0)) {
      throw std::runtime_error("PrepopulateDomain: ParticleWeight must be > 0.");
    }

    for (kCell=0; kCell<kCellMax; kCell++)
    for (jCell=0; jCell<jCellMax; jCell++)
    for (iCell=0; iCell<iCellMax; iCell++) {

      nd = _getCenterNodeLocalNumber(iCell,jCell,kCell);
      cell = cellList[nd];
      xMiddle = cell->GetX();

      // Expected macro count in this cell (cell->Measure is normalized volume)
      const double n_SI_m3 = cfg.sw_n_cm3 * 1.0e6; // cm^-3 -> m^-3

      #if DIM == 3
      const double V_SI_m3 = cell->Measure * (F.No2SiL*F.No2SiL*F.No2SiL);
      #elif DIM == 2
      const double V_SI_m3 = cell->Measure * (F.No2SiL*F.No2SiL);
      #elif DIM == 1
      const double V_SI_m3 = cell->Measure * (F.No2SiL);
      #endif

      const double anpart = (n_SI_m3 * V_SI_m3) / ParticleWeight;      
      
      if (anpart <= 0.0) continue;

      long int npart = static_cast<long int>(anpart);

      // Rounding behavior
      if (cfg.sw_use_rounding) {
        if (rnd() < anpart - static_cast<double>(npart)) npart++;
      }
      // else: deterministic floor()

      // Optional min-particles behavior: TestConfig does not currently carry
      // ForceMinParticleNumber / limit; keep legacy behavior OFF by default.
      // If you later add cfg.forceMinParticleNumber, apply the same logic as the legacy
      // PrepopulateDomain: w = anpart/limit, npart=limit.

      // The per-particle sampler weight stored in particle buffer
      double w = 1.0;

      nLocalInjectedParticles += npart;

      while (npart-- > 0) {
        // Uniform position inside the cell (legacy sampling from block extents).
        // If an internal sphere is enabled, reject samples that fall inside it
        // (keeps the initial condition "vacuum" inside Enceladus).
        bool okPos=false;
        for (int attempt=0; attempt<32; ++attempt) {
          x[0] = xMiddle[0] + (xmax[0]-xmin[0])/_BLOCK_CELLS_X_*(rnd()-0.5);
#if DIM >= 2
          x[1] = xMiddle[1] + (xmax[1]-xmin[1])/_BLOCK_CELLS_Y_*(rnd()-0.5);
#else
          x[1] = 0.0;
#endif
#if DIM == 3
          x[2] = xMiddle[2] + (xmax[2]-xmin[2])/_BLOCK_CELLS_Z_*(rnd()-0.5);
#else
          x[2] = 0.0;
#endif
          if (!IsPointInsideInternalSphere(x)) { okPos=true; break; }
        }
        if (!okPos) continue;

        // Sample velocity:
        //   v_no = BulkVelocity_no + (sigma_no * N(0,1))
        // Compute sigma_no by converting sigma_SI with the same normalization:
        //   sigma_no = sigma_SI / U0 = sigma_SI * F.Si2NoV
        const double sigma_no = sigma_SI * F.Si2NoV;

	const double beta=sqrt(m_species_kg/(2.0*kB_SI * Temperature_K)); 

        for (idim=0; idim<3; idim++) {
          //v_no[idim] = BulkVelocity_no[idim] + (sigma_no > 0.0 ? sigma_no * normal01() : 0.0);
	  v_no[idim] = BulkVelocity_no[idim] + sqrt(-log(rnd()))/beta*sin(2.0*Pi*rnd()) * F.Si2NoV; 
        }

        // Particle birth:
        //   - store full 3D velocity v_no in the particle buffer
        //   - if _USE_PARTICLE_V_PARALLEL_NORM_ is enabled, additionally
        //     initialize V_parallel, V_normal and magnetic moment (if enabled)
        //     via the user-init callback.
        long int newptr; 
	
	newptr=PIC::ParticleBuffer::InitiateParticle(
          x, v_no, &w, &spec, NULL,
          _PIC_INIT_PARTICLE_MODE__ADD2LIST_,
          (void*)node,
          #if _USE_PARTICLE_V_PARALLEL_NORM_ == _PIC_MODE_ON_
          VparVnormMu::InitParticle 
          #else
          NULL
          #endif
        );

        double vp=PIC::ParticleBuffer::GetVParallel(newptr);
	if (fabs(vp)>5.0) {
  
		double vsi[3];
		for (int i=0;i<3;i++) vsi[i]=F.No2SiV*v_no[i]; 

		double za=0;
		za+=22;


	}

      }
    }
  }

  MPI_Allreduce(&nLocalInjectedParticles, &nGlobalInjectedParticles, 1, MPI_LONG, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);
  return nGlobalInjectedParticles;
}



double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
    double CellSize;
    double CharacteristicSpeed;
    double dt;


    CellSize=startNode->GetCharacteristicCellSize();
    //return 0.3*CellSize/CharacteristicSpeed;

    //return 0.05;
    return 1;
}


double BulletLocalResolution(double *x) {
  (void)x;

  // ---------------------------------------------------------------------------
  // Mesh-resolution callback used by PIC::Mesh::mesh->init().
  //
  // AMPS expects this callback to return a *target local cell size / resolution
  // scale* in the same coordinate units as the mesh. A smaller returned value
  // requests a finer grid; a larger value requests a coarser grid.
  //
  // Historically this test always returned the same constant value:
  //
  //   sqrt(3) + 0.1
  //
  // That behavior is preserved when g_GridResolutionMultiplier == 1.0.
  //
  // To make the new CLI option intuitive, we define:
  //
  //   effective_resolution = legacy_resolution / g_GridResolutionMultiplier
  //
  // Therefore:
  //   multiplier > 1  -> smaller target cell size -> finer mesh
  //   multiplier < 1  -> larger  target cell size -> coarser mesh
  //
  // NOTE:
  //   The old code contained some dead branches for uniform/nonuniform test modes,
  //   but the final returned value was always overwritten by sqrt(3)+0.1. The new
  //   implementation keeps the same legacy base value and applies the user-requested
  //   multiplier explicitly.
  // ---------------------------------------------------------------------------
  const double legacy_resolution = std::sqrt(3.0) + 0.1;

  double mult = g_GridResolutionMultiplier;
  if (!(mult > 0.0)) mult = 1.0;

  const double effective_resolution = legacy_resolution / mult;
  return effective_resolution;
}

#include <cmath>
#include <stdexcept>
#include <mpi.h>

// ============================================================================
// EvaluateGlobalParticleWeightForTargetPPC()
// ----------------------------------------------------------------------------
// Compute a *global* macroparticle statistical weight (physical particles per
// macroparticle) such that, for a uniform background number density, the run
// will inject ~cfg.target_ppc macroparticles per cell on average.
//
// INPUTS / ASSUMPTIONS
//   • Cell geometry:
//       - cell->Measure is already a *normalized* cell volume V_no (dimensionless),
//         i.e. V_SI = V_no * (L0)^DIM, where L0 = F.No2SiL [m].
//   • Density input:
//       - If cfg.sw_has_ncm3: use cfg.sw_n_cm3 [cm^-3]
//         n_SI_m3 = cfg.sw_n_cm3 * 1e6.
//       - Otherwise, you can either refuse (recommended) or fall back to some
//         solver-unit density. This implementation requires sw_has_ncm3.
//   • Target macroparticles per cell:
//       - cfg.target_ppc is the desired *average* number of macroparticles per cell.
//   • Uniform density assumed across the whole domain.
//
// DERIVATION
//   Let Ncells be the total number of cells (global).
//   Let Vtot_SI be the total physical domain volume (global).
//   Then total physical particles represented by density n_SI is:
//       Nphys_tot = n_SI * Vtot_SI.
//
//   If we want on average target_ppc macroparticles per cell, then the target
//   global macroparticle count is:
//       Nmacro_target = target_ppc * Ncells.
//
//   The statistical weight must satisfy:
//       Nmacro_target * ParticleWeight = Nphys_tot
//   so
//       ParticleWeight = Nphys_tot / Nmacro_target.
//
//   With this ParticleWeight, the expected macroparticles in a cell become:
//       anpart = (n_SI * Vcell_SI) / ParticleWeight
//            ~= target_ppc * (Vcell_SI / Vavg_SI)
//   i.e. average is target_ppc, with proportional variation if cell volumes vary.
//
// RETURN
//   ParticleWeight (global), in "physical particles per macroparticle" units,
//   consistent with the counting used in PrepopulateDomain.
// ============================================================================

double EvaluateGlobalParticleWeightForTargetPPC(const picunits::Factors& F,const TestConfig& cfg) { 
  if (!(F.No2SiL > 0.0)) throw std::invalid_argument("EvaluateParticleWeight: F.No2SiL must be > 0.");
  if (!(cfg.target_ppc > 0.0)) throw std::invalid_argument("EvaluateParticleWeight: cfg.target_ppc must be > 0.");

  if (!cfg.sw_has_ncm3) {
    throw std::invalid_argument("EvaluateParticleWeight: cfg.sw_has_ncm3 must be true (need physical density).");
  }

  const double n_SI_m3 = cfg.sw_n_cm3 * 1.0e6; // cm^-3 -> m^-3
  if (!(n_SI_m3 >= 0.0)) throw std::invalid_argument("EvaluateParticleWeight: n_SI_m3 must be >= 0.");

  // --- local totals ---
  int localCellCount = 0;
  double    localVolumeNo  = 0.0;  // sum of normalized cell volumes (dimensionless)

  // Count cells & sum normalized volumes over local blocks (same pattern as PrepopulateDomain)
  for (auto* node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];
       node != NULL; node = node->nextNodeThisThread) {

    // Match PrepopulateDomain periodic-boundary skipping to avoid double-counting
    if (_PIC_BC__PERIODIC_MODE_ == _PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;
      for (int iface=0; iface<6; iface++) {
        if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh) == NULL) { BoundaryBlock=true; break; }
      }
      if (BoundaryBlock) continue;
    }

    const int len = node->block->GetCenterNodeListLength();
    PIC::Mesh::cDataCenterNode** list = node->block->GetCenterNodeList();

    for (int i=0; i<len; ++i) {
      PIC::Mesh::cDataCenterNode* cell = list[i];

      if (cell!=NULL) {
        localVolumeNo += cell->Measure;  // Measure is V_no (already normalized)
	localCellCount++;
      }
    }
  }

  // --- global totals ---
  int globalCellCount = 0;
  double    globalVolumeNo  = 0.0;

  MPI_Allreduce(&localCellCount, &globalCellCount, 1, MPI_INT, MPI_SUM, MPI_GLOBAL_COMMUNICATOR);
  MPI_Allreduce(&localVolumeNo,  &globalVolumeNo,  1, MPI_DOUBLE,    MPI_SUM, MPI_GLOBAL_COMMUNICATOR);

  if (globalCellCount <= 0) throw std::runtime_error("EvaluateParticleWeight: globalCellCount <= 0.");
  if (!(globalVolumeNo > 0.0)) throw std::runtime_error("EvaluateParticleWeight: globalVolumeNo <= 0.");

  // Convert total normalized volume to SI volume using L0 = F.No2SiL [m]
#if DIM == 3
  const double Vtot_SI_m3 = globalVolumeNo * (F.No2SiL * F.No2SiL * F.No2SiL);
#elif DIM == 2
  const double Vtot_SI_m3 = globalVolumeNo * (F.No2SiL * F.No2SiL);
#elif DIM == 1
  const double Vtot_SI_m3 = globalVolumeNo * (F.No2SiL);
#else
  exit(__LINE__,__FILE__,"Error: DIM is not defined");
#endif

  const double Nphys_tot        = n_SI_m3 * Vtot_SI_m3;
  const double Nmacro_target    = cfg.target_ppc * static_cast<double>(globalCellCount);
  const double ParticleWeight   = Nphys_tot / Nmacro_target;

  if (!(ParticleWeight > 0.0)) throw std::runtime_error("EvaluateParticleWeight: computed ParticleWeight <= 0.");

  return ParticleWeight;
}

#include <cmath>
#include <limits>
#include <mpi.h>
#include "pic.h"

// ============================================================================
// EvaluateCFLTimeStepForSpecies()
// ----------------------------------------------------------------------------
// PURPOSE
//   Compute a stable explicit time step for particle advection based on a CFL
//   condition for a specific species:
//
//       dt_cell = CFL * h_cell / <|v|>_cell,spec
//
//   where
//     • CFL is a user-requested Courant number (0 < CFL <= 1 typically)
//     • h_cell is a characteristic cell size (here: min cell edge length)
//     • <|v|> is the mean particle speed magnitude for the requested species
//       within that cell
//
//   The function loops over all *local* cells, computes dt_cell where the cell
//   contains at least one particle of the requested species with nonzero mean
//   speed, and returns the *global* minimum dt (MPI_MIN across ranks).
//
// UNITS / CONSISTENCY
//   This routine works entirely in the solver’s native units:
//     • cell geometry uses node->xmin/xmax (solver length units)
//     • particle velocities are read from ParticleBuffer (solver velocity units)
//   Therefore the returned dt is in solver time units.
//   If you need SI seconds, multiply by picunits::Factors::No2SiT.
//
// WHAT IS MEAN SPEED?
//   We compute a statistically-weighted mean speed in each cell:
//
//       <|v|> = ( Σ w_p * |v_p| ) / ( Σ w_p )
//
//   where w_p is the physical-particle statistical weight represented by the
//   macroparticle. In AMPS this is typically:
//       w_p = LocalParticleWeight(spec) * IndividualStatWeightCorrection(p)
//
//   If you want the unweighted macro-average instead, set w_p = 1.0 below.
//
// EDGE CASES
//   • Cells with no particles of this species are ignored (dt = +∞).
//   • Cells with particles but <|v|> == 0 are ignored (dt = +∞).
//   • If there are no qualifying cells anywhere (no particles of this species),
//     the function returns 0.0.
//
// PERFORMANCE NOTE
//   This is an O(Nparticles) scan and is intended for test harnesses / diagnostics.
// ============================================================================

double EvaluateCFLTimeStepForSpecies(int spec, double CFL) {
  if (!(CFL > 0.0)) {
    throw std::invalid_argument("EvaluateCFLTimeStepForSpecies: CFL must be > 0");
  }

  double local_dt_min = std::numeric_limits<double>::infinity();

  // Loop over local blocks on this MPI rank/thread
  for (auto* node = PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];
       node != NULL; node = node->nextNodeThisThread) {

    // Skip periodic "ghost" boundary blocks (same convention as PrepopulateDomain)
    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;
      for (int iface=0; iface<6; iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
        BoundaryBlock=true;
        break;
      }
      if (BoundaryBlock) continue;
    }

    auto* block = node->block;
    long int* FirstCellParticleTable = block->FirstCellParticleTable;

    // Cell edge lengths for this block (uniform within block)
    double dx[3] = {1.0,1.0,1.0};
#if DIM >= 1
    dx[0] = (node->xmax[0] - node->xmin[0]) / _BLOCK_CELLS_X_;
#endif
#if DIM >= 2
    dx[1] = (node->xmax[1] - node->xmin[1]) / _BLOCK_CELLS_Y_;
#else
    dx[1] = std::numeric_limits<double>::infinity();
#endif
#if DIM == 3
    dx[2] = (node->xmax[2] - node->xmin[2]) / _BLOCK_CELLS_Z_;
#else
    dx[2] = std::numeric_limits<double>::infinity();
#endif

    const double h_cell = std::min(dx[0], std::min(dx[1], dx[2]));
    if (!(h_cell > 0.0)) continue;

    // Loop over all cells in block
#if DIM == 3
    for (int k=0; k<_BLOCK_CELLS_Z_; ++k)
    for (int j=0; j<_BLOCK_CELLS_Y_; ++j)
    for (int i=0; i<_BLOCK_CELLS_X_; ++i) {
      const int cellIndex = i + _BLOCK_CELLS_X_*(j + _BLOCK_CELLS_Y_*k);
#elif DIM == 2
    for (int j=0; j<_BLOCK_CELLS_Y_; ++j)
    for (int i=0; i<_BLOCK_CELLS_X_; ++i) {
      const int cellIndex = i + _BLOCK_CELLS_X_*j;
#elif DIM == 1
    for (int i=0; i<_BLOCK_CELLS_X_; ++i) {
      const int cellIndex = i;
#endif

      long int ptr = FirstCellParticleTable[cellIndex];
      if (ptr == -1) continue;

      // Weighted mean speed in this cell for this species
      double sum_w = 0.0;
      double sum_w_speed = 0.0;

      while (ptr != -1) {
        auto  pdata = PIC::ParticleBuffer::GetParticleDataPointer(ptr);

        const int pspec = PIC::ParticleBuffer::GetI(pdata);
        if (pspec == spec) {
          double v[3];
          PIC::ParticleBuffer::GetV(v, pdata);

          const double speed = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

          if (speed > 0.0) {
            // Statistical weight represented by this macroparticle
            double w = 1.0;

            // Recommended: physical-particle weight (matches sampling / moments)
#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
            w = PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
#else
            w = block->GetLocalParticleWeight(spec);
#endif
            w *= PIC::ParticleBuffer::GetIndividualStatWeightCorrection(pdata);

            // If you prefer pure macro-average, comment out the two lines above
            // and keep w = 1.0.

            sum_w       += w;
            sum_w_speed += w * speed;
          }
        }

        ptr = PIC::ParticleBuffer::GetNext(pdata);
      }

      if (sum_w > 0.0) {
        const double mean_speed = sum_w_speed / sum_w;
        if (mean_speed > 0.0) {
          const double dt_cell = CFL * h_cell / mean_speed;
          if (dt_cell < local_dt_min) local_dt_min = dt_cell;
        }
      }

#if DIM == 3
    }
#elif DIM == 2
    }
#elif DIM == 1
    }
#endif
  }

  // Global minimum across MPI ranks
  double global_dt_min = std::numeric_limits<double>::infinity();
  MPI_Allreduce(&local_dt_min, &global_dt_min, 1, MPI_DOUBLE, MPI_MIN, MPI_GLOBAL_COMMUNICATOR);

  if (!std::isfinite(global_dt_min)) return 0.0; // no particles of this species found
  return global_dt_min;
}

