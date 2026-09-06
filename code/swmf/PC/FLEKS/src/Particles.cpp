#include <cstdlib>

#include <AMReX_ParReduce.H>

#include "InitialCondition.h"
#include "Morton.h"
#include "Particles.h"
#include "SWMFInterface.h"
#include "Timer.h"
#include "Utility.h"

using namespace amrex;

namespace {

ParticlesInfo make_io_particles_info() {
  ParticlesInfo info;
  info.nPartPerCell = IntVect(AMREX_D_DECL(-1, -1, -1));
  return info;
}

} // namespace

//==========================================================
template <int NStructReal, int NStructInt>
Particles<NStructReal, NStructInt>::Particles(
    Grid* gridIn, FluidInterface* const fluidIn, TimeCtr* const tcIn,
    const int speciesIDIn, const Real chargeIn, const Real massIn,
    const ParticlesInfo& pInfo, const PartMode pModeIn,
    const InitialCondition* icIn)
    : AmrParticleContainer<NStructReal, NStructInt>(gridIn),
      grid(gridIn),
      fi(fluidIn),
      tc(tcIn),
      pMode(pModeIn),
      speciesID(speciesIDIn),
      charge(chargeIn),
      mass(massIn),
      nPartPerCell(pInfo.nPartPerCell),
      ic_(icIn) {

  isParticleLocationRandom = pInfo.isParticleLocationRandom;
  isPPVconstant = pInfo.isPPVconstant;
  doPreSplitting = pInfo.doPreSplitting;
  fastMerge = pInfo.fastMerge;
  mergeLight = pInfo.mergeLight;
  nPartCombine = pInfo.nPartCombine;
  nPartNew = pInfo.nPartNew;
  nMergeTry = pInfo.nMergeTry;
  mergeThresholdDistance = pInfo.mergeThresholdDistance;
  velBinBufferSize = pInfo.velBinBufferSize;
  mergeRatioMax = pInfo.mergeRatioMax;
  pLevRatio = pInfo.pLevRatio;
  mergePartRatioMax = pInfo.mergePartRatioMax;
  if (fi)
    vacuum = pInfo.vacuumIO * cProtonMassSI * 1e6 * fi->get_Si2NoRho();
  else
    vacuum = pInfo.vacuumIO;
  ionOH = pInfo.ionOH;
  bc = pInfo.particle_bc(speciesID);
  supID = pInfo.initial_sup_id(speciesID);
  do_tiling = true;

  qom = charge / mass;
  qomSign = qom >= 0 ? 1 : -1;

  plo.resize(n_lev_max());
  phi.resize(n_lev_max());
  dx.resize(n_lev_max());
  invDx.resize(n_lev_max());
  invVol.resize(n_lev_max());

  for (int iLev = 0; iLev < n_lev_max(); iLev++) {
    for (int i = 0; i < nDim; ++i) {
      tile_size[i] = 1;
      plo[iLev][i] = Geom(iLev).ProbLo(i);
      phi[iLev][i] = Geom(iLev).ProbHi(i);
      dx[iLev][i] = Geom(iLev).CellSize(i);
      invDx[iLev][i] = Geom(iLev).InvCellSize(i);
    }
    invVol[iLev] = invDx[iLev].product();
  }

  isFake2D = (nDim == 3) &&
             (Geom(0).Domain().bigEnd(iz_) == Geom(0).Domain().smallEnd(iz_));

  // The following line is used to avoid an MPI bug (feature?) on Frontera. It
  // should be removed after the bug being fixed.
  SetUseUnlink(false);
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::outflow_bc(const MFIter& mfi,
                                                    const IntVect ijkGst,
                                                    const IntVect ijkPhy) {
  const int iLev = 0;

  ParticleTileType& pGst = get_particle_tile(iLev, mfi, ijkGst);

  ParticleTileType& pPhy = get_particle_tile(iLev, mfi, ijkPhy);

  AoS& phyParts = pPhy.GetArrayOfStructs();

  RealVect dxshift;
  for (int i = 0; i < nDim; ++i) {
    dxshift[i] = Geom(iLev).CellSize(i) * (ijkGst[i] - ijkPhy[i]);
  }

  Vector<ParticleType> pList;
  for (const auto& p : phyParts) {
    IntVect iv = Index(p, iLev);
    // Q: Why do we need to check if the physical domain contains the particle?
    // A: Even if tiling with tile_size=1 is used, it seems the ghost cells
    // still share the the same tile with a physical cell. Therefore, we need to
    // make sure a particle in a "physical tile" is actually inside the physical
    // domain.
    if (mfi.validbox().contains(IntVect(iv))) {
      ParticleType pNew = p;
      set_ids(pNew);
      for (int i = 0; i < nDim; ++i) {
        pNew.pos(i) = p.pos(i) + dxshift[i];
      }

      pList.push_back(pNew);
    }
  }

  // Q: Why do not push the new particles into pGst inside previous loop?
  // A: Sometimes, if not always, pPhy and pGst share the same tile. Previous
  // for-loop loops through al particles in pPhy. If we push the new particles
  // into pGst, which is the same as pPhy sometimes, the loop behavior is not
  // well defined.
  for (auto& p : pList) {
    pGst.push_back(p);
  }
}

//==========================================================
/**
 * @brief Adds particles to a specific cell in the grid.
 *
 * This function adds particles to a specific cell in the grid at a given level
 * (`iLev`). It initializes the particles based on the provided fluid interface,
 * user-defined velocities, and other parameters.
 *
 * @tparam NStructReal Number of real components in the particle structure.
 * @tparam NStructInt Number of integer components in the particle structure.
 * @param iLev The level at which to add the particles.
 * @param mfi The MultiFab iterator for the current tile.
 * @param ijk The cell index where particles are to be added.
 * @param interface Pointer to the fluid interface used for initializing
 * particles.
 * @param doVacuumLimit Flag indicating whether to apply vacuum limit.
 * @param ppc Particles per cell.
 * @param tpVel User-defined velocity for initializing test particles.
 * @param dt Time step for density change rate.
 */
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::add_particles_cell(
    const int iLev, const MFIter& mfi, IntVect ijk,
    const FluidInterface* interface, bool doVacuumLimit, IntVect ppc,
    const Vel& tpVel, Real dt) {

  // If true, initialize the test particles with user defined velocities instead
  // of from fluid.
  bool userState = (tpVel.tag == speciesID);

  // If dt>0, it suggests the 'density' obtained from interface is actually the
  // density changing rate.
  if (dt <= 0)
    dt = 1;

  IntVect nPPC = nPartPerCell;
  if (!(ppc == 0)) {
    nPPC = ppc;
  }

  if (nPPC == 0)
    return;

  if (isTargetPPCDefined && !isFake2D) {
    const auto tppc = target_PPC(iLev)[mfi].array();
    for (int i = 0; i < nDim; ++i) {
      if (nDim > 2) {
        nPPC[i] = cbrt(tppc(ijk));
      } else {
        nPPC[i] = sqrt(tppc(ijk));
      }
    }
  }
  set_random_seed(iLev, ijk, nPPC);

  const Real vol = dx[iLev].product();
  const int npcel = product(nPPC);

  const Real vol2Npcel = qomSign * vol / npcel;

  ParticleTileType& particles = get_particle_tile(iLev, mfi, ijk);

  int icount = 0;
  // Loop over particles inside grid cell i, j, k

  const int kmax = nDim > 2 ? nPPC[iz_] : 1;

  for (int ii = 0; ii < nPPC[ix_]; ++ii)
    for (int jj = 0; jj < nPPC[iy_]; ++jj)
      for (int kk = 0; kk < kmax; ++kk) {
        RealVect xyz, xyz0;

        IntVect ijk0 = { AMREX_D_DECL(ii, jj, kk) };

        for (int iDim = 0; iDim < nDim; iDim++) {
          xyz[iDim] = (ijk0[iDim] + randNum()) * (dx[iLev][iDim] / nPPC[iDim]) +
                      ijk[iDim] * dx[iLev][iDim] + plo[iLev][iDim];

          // If the particle weight is sampled in a random location, the sum of
          // particle mass is NOT the same as the integral of the grid density.
          // It is more convenient for debugging if mass is exactly conserved.
          // For a production run, it makes little difference.
          xyz0[iDim] = (ijk0[iDim] + 0.5) * (dx[iLev][iDim] / nPPC[iDim]) +
                       ijk[iDim] * dx[iLev][iDim] + plo[iLev][iDim];
        }

        if (nDim == 2) {
          // For comparison with the 3D case only.
          randNum();
        }

        if (!isParticleLocationRandom) {
          xyz = xyz0;
        }

        const Real nDens =
            (userState && tpVel.nDens >= 0.0)
                ? tpVel.nDens
                : interface->get_number_density(mfi, xyz0, speciesID, iLev);

        if (doVacuumLimit && nDens * dt < vacuum)
          continue;

        Real q = vol2Npcel * nDens;

        // Per-particle weight modification (e.g. ion-acoustic-wave density
        // perturbation). Routed through the InitialCondition plugin; runs
        // before the q != 0 guard so an IC can suppress a particle.
        if (ic_ && ic_->modifies_weights()) {
          ParticleICState pics;
          pics.iLev = iLev;
          pics.iSpec = speciesID;
          pics.iCount = icount;
          pics.nPerCell = npcel;
          pics.x = xyz[0];
          pics.y = xyz[1];
          pics.z = xyz[2];
          pics.q = q;
          ic_->modify_particle_weight(pics);
          q = pics.q;
        }

        if (q != 0) {
          Real u, v, w;
          Real rand1 = randNum();
          Real rand2 = randNum();
          Real rand3 = randNum();
          Real rand4 = randNum();

          Real uth = (userState ? tpVel.vth : -1);

          if (!is_neutral() && interface->get_UseAnisoP() &&
              (speciesID > 0 || interface->get_useElectronFluid())) {
            interface->set_particle_uth_aniso(iLev, mfi, xyz, &u, &v, &w, rand1,
                                              rand2, rand3, rand4, speciesID,
                                              uth, uth);
          } else {
            interface->set_particle_uth_iso(iLev, mfi, xyz, &u, &v, &w, rand1,
                                            rand2, rand3, rand4, speciesID,
                                            uth);
          }

          // Per-particle THERMAL-velocity override (e.g. a bi-Maxwellian with
          // distinct T_perp / T_par for the proton-cyclotron anisotropy
          // instability). Routed through the InitialCondition plugin; runs
          // AFTER the isotropic thermal velocity is sampled and BEFORE the bulk
          // velocity is added, so the IC may fully replace the sampled thermal
          // velocity with its own anisotropic draw.
          if (ic_ && ic_->modifies_thermal_velocity()) {
            ParticleICState pics;
            pics.iLev = iLev;
            pics.iSpec = speciesID;
            pics.iCount = icount;
            pics.nPerCell = npcel;
            pics.x = xyz[0];
            pics.y = xyz[1];
            pics.z = xyz[2];
            pics.uThermal = u;
            pics.vThermal = v;
            pics.wThermal = w;
            pics.rand[0] = rand1;
            pics.rand[1] = rand2;
            pics.rand[2] = rand3;
            pics.rand[3] = rand4;
            ic_->modify_particle_thermal_velocity(pics);
            u = pics.uThermal;
            v = pics.vThermal;
            w = pics.wThermal;
          }

          Real uBulk = userState ? tpVel.vx
                                 : interface->get_ux(mfi, xyz, speciesID, iLev);
          Real vBulk = userState ? tpVel.vy
                                 : interface->get_uy(mfi, xyz, speciesID, iLev);
          Real wBulk = userState ? tpVel.vz
                                 : interface->get_uz(mfi, xyz, speciesID, iLev);

          // Per-particle velocity / weight modification (e.g. beam bulk
          // override, or the hybrid-wave Alfven velocity kick). Routed through
          // the InitialCondition plugin; runs after the bulk velocity is
          // computed and before it is added to the thermal velocity.
          if (ic_ && ic_->modifies_velocities()) {
            ParticleICState pics;
            pics.iLev = iLev;
            pics.iSpec = speciesID;
            pics.iCount = icount;
            pics.nPerCell = npcel;
            pics.x = xyz[0];
            pics.y = xyz[1];
            pics.z = xyz[2];
            pics.uBulk = uBulk;
            pics.vBulk = vBulk;
            pics.wBulk = wBulk;
            pics.qScale = 1.0;
            pics.charge = charge;
            pics.temperature =
                interface->get_uth_iso(mfi, xyz, speciesID, iLev);
            pics.temperature *= pics.temperature * mass;
            ic_->modify_particle_velocity(pics);
            uBulk = pics.uBulk;
            vBulk = pics.vBulk;
            wBulk = pics.wBulk;
            q *= pics.qScale;
          }
          // Wave boundary: add the velocity kick to particles in a wave cell.
          if (waveVelocityKick) {
            const int loX = mfi.validbox().smallEnd(ix_);
            const int hiX = mfi.validbox().bigEnd(ix_);
            const int loY = mfi.validbox().smallEnd(iy_);
            const int hiY = mfi.validbox().bigEnd(iy_);
            const int loZ = mfi.validbox().smallEnd(iz_);
            const int hiZ = mfi.validbox().bigEnd(iz_);
            // Driven by the field-side wave faces (set from Pic::bcField),
            // not by a particle-side spelling.
            const bool onWaveX = (isWaveFace[0] && ijk[ix_] < loX) ||
                                 (isWaveFace[1] && ijk[ix_] > hiX);
            const bool onWaveY = (isWaveFace[2] && ijk[iy_] < loY) ||
                                 (isWaveFace[3] && ijk[iy_] > hiY);
            const bool onWaveZ = (isWaveFace[4] && ijk[iz_] < loZ) ||
                                 (isWaveFace[5] && ijk[iz_] > hiZ);
            if (onWaveX || onWaveY || onWaveZ) {
              const Real ppos[3] = { xyz[0], xyz[1], xyz[2] };
              const Real tNow = tc ? tc->get_time() : 0.0;
              Real dvx = 0, dvy = 0, dvz = 0;
              waveVelocityKick(ppos, tNow, dvx, dvy, dvz);
              uBulk += dvx;
              vBulk += dvy;
              wBulk += dvz;
            }
          }
          u += uBulk;
          v += vBulk;
          w += wBulk;

          auto p = make_particle();
          set_ids(p);
          for (int iDim = 0; iDim < nDim; iDim++) {
            p.pos(iDim) = xyz[iDim];
          }
          p.rdata(iup_) = u;
          p.rdata(ivp_) = v;
          p.rdata(iwp_) = w;
          // Convert 'density changing rate' to 'density' if necessary.
          p.rdata(iqp_) = q * dt;

          if (NStructInt > iRecordCount_) {
            // For test particle only.
            p.idata(iRecordCount_) = 0;
          }

          particles.push_back(p);

          icount++;
        }
      }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::add_particles_source(
    const FluidInterface* interface, const FluidInterface* const stateOH,
    Real dt, IntVect ppc, const bool doSelectRegion, const bool adaptivePPC) {
  timing_func("Pts::add_particles_source");

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (MFIter mfi = MakeMFIter(iLev, false); mfi.isValid(); ++mfi) {
      const Box& tile_box = mfi.validbox();
      const auto lo = lbound(tile_box);
      const auto hi = ubound(tile_box);

      for (int k = lo.z; k <= hi.z; ++k)
        for (int j = lo.y; j <= hi.y; ++j)
          for (int i = lo.x; i <= hi.x; ++i) {
            const auto& status = cell_status(iLev)[mfi].array();
            if (bit::is_refined(status(i, j, k)))
              continue;

            IntVect ijk = { AMREX_D_DECL(i, j, k) };

            bool doAdd = true;
#ifdef _PT_COMPONENT_
            if (stateOH && doSelectRegion) {
              const int iRegion =
                  stateOH->get_neu_source_region(mfi, ijk, iLev);
              doAdd = (iRegion == speciesID);
            }
#endif
            if (doAdd) {
              if (adaptivePPC) {
                // Adjust ppc so that the weight of the
                // source particles is not too small.
                const int initPPC = product(nPartPerCell);
                const int sourcePPC = product(ppc);

                Real rho = fi->get_number_density(mfi, ijk, speciesID, iLev);
                Real rhoSource =
                    interface->get_number_density(mfi, ijk, speciesID, iLev);
                if (dt > 0)
                  rhoSource *= dt;

                Real avgInitW = rho / initPPC;
                Real avgSourceW = rhoSource / sourcePPC;

                Real targetSourceW = avgInitW * 0.1;

                if (avgSourceW < targetSourceW) {
                  Real ratio = pow(avgSourceW / targetSourceW, 1.0 / nDim);
                  for (int iDim = 0; iDim < nDim; iDim++) {
                    ppc[iDim] = std::max(1, int(ppc[iDim] * ratio));
                  }
                }
              }

              add_particles_cell(iLev, mfi, ijk, interface, false, ppc, Vel(),
                                 dt);
            }
          }
    }
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::add_particles_domain() {
  timing_func("Pts::add_particles_domain");
  int iLevMax = 0;
  if (tc->get_cycle() == 0) {
    iLevMax = n_lev() - 1;
  }
  for (int iLev = 0; iLev <= iLevMax; iLev++) {
    for (MFIter mfi = MakeMFIter(iLev, false); mfi.isValid(); ++mfi) {

      const auto& status = cell_status(iLev)[mfi].array();
      ParallelFor(mfi.validbox(), [&](int i, int j, int k) noexcept {
        IntVect ijk = { AMREX_D_DECL(i, j, k) };
        if (bit::is_new(status(ijk)) && !bit::is_refined(status(ijk))) {
          add_particles_cell(iLev, mfi, ijk, fi, true);
        }
      });
    }
  }
}

//==========================================================
// Return true if ghost-cell particle injection should be skipped for this BC.
// - outflow: Ghost particles would be folded into edge cells in
//   sum_moments_cell_centered(), causing double counting.
// - inflow: Flux is injected at the physical face by
//   inject_flux_at_inflow_faces().
//==========================================================
static bool particle_bc_no_injection(const ParticleBC::Type type) {
  switch (type) {
    case ParticleBC::inflow:
    case ParticleBC::outflow:
    case ParticleBC::vacuum:
    case ParticleBC::reflect:
    case ParticleBC::absorb:
      return true;
    case ParticleBC::coupled:
    case ParticleBC::periodic:
      return false;
    default:
      return true;
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::inject_particles_at_boundary() {
  timing_func("Pts::inject_particles_at_boundary");

  // Only inject nGstInject layers.
  const int nGstInject = 1;

  // Only launch particles to the base grid boundary cells. The particle moments
  // of the domain edge nodes can be corrected by calling
  // interp_from_coarse_to_fine_for_domain_edge() in order from coarest level to
  // finest level.
  int iLev = 0;

  for (MFIter mfi = MakeMFIter(iLev, false); mfi.isValid(); ++mfi) {
    const auto& status = cell_status(iLev)[mfi].array();
    const Box& bx = mfi.validbox();
    const IntVect bxLo = bx.smallEnd();
    const IntVect bxHi = bx.bigEnd();

    Box bxGst = bx;
    for (int iDim = 0; iDim < fi->get_fluid_dimension(); iDim++) {
      bxGst.grow(iDim, nGstInject);
    }

    ParallelFor(bxGst, [&](int i, int j, int k) noexcept {
      IntVect ijk = { AMREX_D_DECL(i, j, k) };
      IntVect ijksrc;
      if (do_inject_particles_for_this_cell(bx, status, ijk, ijksrc)) {
        // A ghost cell can lie beyond more than one face (a corner); it is
        // skipped when ANY of those faces must not carry a ghost-cell
        // population -- see particle_bc_no_injection() above.
        bool skip = false;
        for (int d = 0; d < nDim && !skip; ++d) {
          if (ijk[d] < bxLo[d])
            skip = particle_bc_no_injection(
                static_cast<ParticleBC::Type>(bc.face(d, 0)));
          else if (ijk[d] > bxHi[d])
            skip = particle_bc_no_injection(
                static_cast<ParticleBC::Type>(bc.face(d, 1)));
        }
        if (skip)
          return;

        // Seed ghost cell from prescribed #INFLOW state if defined, else fall
        // back to the fluid interface.
        Vel inflowVel;
        if (fi->get_inflow_defined()) {
          const auto* iv = fi->get_inflow_vel(speciesID);
          if (iv) {
            inflowVel.tag = speciesID; // enable the userState override path
            inflowVel.nDens = iv->nDens;
            inflowVel.vth = iv->vth;
            inflowVel.vx = iv->ux;
            inflowVel.vy = iv->uy;
            inflowVel.vz = iv->uz;
          }
        }
        add_particles_cell(iLev, mfi, ijk, fi, true, IntVect(), inflowVel, -1);
      }
    });
  }
}

//==========================================================
namespace {
constexpr amrex::Real injPI = 3.14159265358979323846264338328;
constexpr amrex::Real sqpi = 1.77245385090551602729816748334;

// Draw a standard normal variate from two uniforms (Box-Muller).
inline amrex::Real inj_gaussian(amrex::Real r1, amrex::Real r2) {
  const amrex::Real rr = std::sqrt(-2.0 * std::log(std::max(r1, 1e-300)));
  return rr * std::cos(2.0 * injPI * r2);
}

// Mean inward flux of a drifting Maxwellian through a boundary face, in
// units of n * vtherm (vtherm = sqrt(2) * sigma, sigma = 1-D thermal std):
//   g(vd) = [ exp(-vd^2)/sqrt(pi) + vd * erfc(-vd) ] / 2
// with vd = (inward drift speed) / vtherm. Using erfc(-vd) == 1 + erf(vd)
// avoids catastrophic cancellation for negative outward drift (vd < 0).
inline amrex::Real inj_mean_inward_flux(amrex::Real vd) {
  return 0.5 * (std::exp(-vd * vd) / sqpi + vd * std::erfc(-vd));
}

// Fast speed sampler for the flux-weighted half-space Maxwellian
//   f(w) \propto w * exp(-(w - vd)^2), w >= 0.
// Precomputes face-level constants and a 64-point LUT once per face to
// eliminate costly 60-step bisections per particle, converging in 2-3
// Newton-Raphson steps.
struct InflowSpeedSampler {
  amrex::Real vd{ 0.0 };
  amrex::Real e0{ 1.0 };
  amrex::Real erfc_mvd{ 1.0 };
  amrex::Real twoZ{ 1.0 };
  amrex::Real wHi{ 8.0 };

  static constexpr int LUT_SIZE = 64;
  amrex::Real lut[LUT_SIZE + 1];

  void init(amrex::Real vd_in) {
    vd = vd_in;
    e0 = std::exp(-vd * vd);
    erfc_mvd = std::erfc(-vd);
    twoZ = e0 + vd * sqpi * erfc_mvd;
    wHi = std::max(vd, 0.0) + 8.0;

    lut[0] = 0.0;
    amrex::Real wLo = 0.0;
    for (int k = 1; k < LUT_SIZE; ++k) {
      const amrex::Real rTarget = static_cast<amrex::Real>(k) / LUT_SIZE;
      const amrex::Real target = 0.5 * rTarget * twoZ;
      amrex::Real lo = wLo;
      amrex::Real hi = wHi;
      for (int it = 0; it < 30; ++it) {
        amrex::Real mid = 0.5 * (lo + hi);
        amrex::Real Fmid = 0.5 * ((e0 - std::exp(-(mid - vd) * (mid - vd))) +
                                  vd * sqpi * (erfc_mvd - std::erfc(mid - vd)));
        if (Fmid < target)
          lo = mid;
        else
          hi = mid;
      }
      lut[k] = 0.5 * (lo + hi);
      wLo = lut[k];
    }
    lut[LUT_SIZE] = wHi;
  }

  inline amrex::Real draw(amrex::Real r) const {
    const amrex::Real target = 0.5 * r * twoZ;
    const amrex::Real rIdx = r * LUT_SIZE;
    const int idx = std::min(std::max(static_cast<int>(rIdx), 0), LUT_SIZE - 1);
    const amrex::Real frac = rIdx - idx;
    amrex::Real w = lut[idx] + frac * (lut[idx + 1] - lut[idx]);

    // 2-3 Newton-Raphson polish iterations
    for (int it = 0; it < 3; ++it) {
      const amrex::Real diff = w - vd;
      const amrex::Real exp_term = std::exp(-diff * diff);
      const amrex::Real Fw =
          0.5 * ((e0 - exp_term) + vd * sqpi * (erfc_mvd - std::erfc(diff)));
      const amrex::Real dF = w * exp_term;
      if (std::abs(dF) < 1e-300)
        break;
      amrex::Real wNew = w - (Fw - target) / dF;
      if (wNew < 0.0 || wNew > wHi)
        wNew = 0.5 * (std::max(static_cast<amrex::Real>(0.0), w) + wHi);
      if (std::abs(wNew - w) < 1e-13 * (1.0 + w))
        break;
      w = wNew;
    }
    return w;
  }
};
} // namespace

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::inject_flux_at_inflow_faces(Real dt) {
  timing_func("Pts::inject_flux_at_inflow_faces");

  if (dt <= 0)
    return;

  // Any inflow face for this species?
  bool hasInflow = false;
  for (int iDim = 0; iDim < nDim && !hasInflow; ++iDim)
    hasInflow = (bc.lo[iDim] == ParticleBC::inflow) ||
                (bc.hi[iDim] == ParticleBC::inflow);
  if (!hasInflow)
    return;

  // Prescribed upstream state (#INFLOW, code units).  vth is the 1-D
  // thermal std sigma = sqrt(kT/m); the Maxwellian is written with
  // vtherm = sqrt(2)*sigma.
  const auto* inv =
      fi->get_inflow_defined() ? fi->get_inflow_vel(speciesID) : nullptr;
  if (inv == nullptr || inv->nDens <= 0)
    return;

  const Real nDens = inv->nDens;
  const Real sigma = inv->vth;
  const Real vtherm = std::sqrt(2.0) * sigma;
  const Real uIn[3] = { inv->ux, inv->uy, inv->uz };

  // Macroparticle weight identical to the interior cell particles
  // (add_particles_cell convention): qp = qomSign * n * V / nppc.
  const Real q = qomSign * dx[0].product() * nDens / product(nPartPerCell);
  const int nppc = product(nPartPerCell);

  // Inject on the base level only (same policy as
  // inject_particles_at_boundary).
  const int iLev = 0;
  const auto& geom = Geom(iLev);
  const Box& domain = geom.Domain();
  const Real* probLo = geom.ProbLo();
  const Real* probHi = geom.ProbHi();
  const Real* cellSize = geom.CellSize();

  // Cache speed samplers for each inflow face (iDim, side)
  InflowSpeedSampler speedSamplers[3][2];
  bool samplerInit[3][2] = { { false, false },
                             { false, false },
                             { false, false } };

  for (MFIter mfi = MakeMFIter(iLev, false); mfi.isValid(); ++mfi) {
    const Box& bx = mfi.validbox();
    ParticleTileType& particles = get_particle_tile(iLev, mfi);

    for (int iDim = 0; iDim < nDim; ++iDim) {
      const int t1 = (iDim + 1) % nDim; // first transverse grid direction
      const int t2 = (nDim > 2) ? (iDim + 2) % nDim
                                : 0;     // second transverse grid direction
      const int trans1 = (iDim + 1) % 3; // first transverse velocity direction
      const int trans2 = (iDim + 2) % 3; // second transverse velocity direction

      for (int side = 0; side < 2; ++side) {
        const bool isHi = (side == 1);
        const int faceBc = isHi ? bc.hi[iDim] : bc.lo[iDim];
        if (faceBc != ParticleBC::inflow)
          continue;

        // This tile must touch the global domain edge on this face.
        const int domEdge = isHi ? domain.bigEnd(iDim) : domain.smallEnd(iDim);
        const int tileEdge = isHi ? bx.bigEnd(iDim) : bx.smallEnd(iDim);
        if (tileEdge != domEdge)
          continue;

        const Real dxn = cellSize[iDim];
        // Inward-pointing direction in the outward-normal coordinate:
        // hi face -> -1 (inward is -d), lo face -> +1.
        const Real inward = isHi ? -1.0 : 1.0;

        // Inward drift speed = bulk velocity dotted with the inward unit
        // normal (uOut * inward; > 0 when plasma flows into the domain).
        const Real uOut = uIn[iDim];
        const Real vd = (sigma > 0) ? (uOut * inward / vtherm) : 1.0e30;

        if (vtherm > 0 && !samplerInit[iDim][side]) {
          speedSamplers[iDim][side].init(vd);
          samplerInit[iDim][side] = true;
        }

        // Mean influx per boundary-transverse cell per step, in
        // macroparticles (Hybrid-VPIC shock deck, bright() accumulator):
        //   dn = nppc * vtherm * g(vd) * dt / dx
        const Real fluxRate =
            (vtherm > 0) ? nppc * vtherm * inj_mean_inward_flux(vd) * dt / dxn
                         : nppc * (uOut * inward) * dt / dxn;
        if (fluxRate <= 0.0)
          continue;

        // Transverse cell range for this tile's face
        const int lo1 = bx.smallEnd(t1), hi1 = bx.bigEnd(t1);
        const int lo2 = (nDim > 2) ? bx.smallEnd(t2) : 0;
        const int hi2 = (nDim > 2) ? bx.bigEnd(t2) : 0;
        const int numCells = (hi1 - lo1 + 1) * (hi2 - lo2 + 1);

        // Tile-face accumulator: packs (iLev, iDim, side, tileLocalIndex).
        // Accumulating over the tile face eliminates the artificial coherent
        // "pulsing sheets" and replaces 10^4 map lookups per step with 1
        // scalar.
        const int64_t tileKey =
            ((((int64_t)iLev * 3 + iDim) * 2 + side) << 40) |
            (((int64_t)mfi.index()) << 16) |
            (int64_t)(mfi.LocalTileIndex() & 0xFFFF);
        Real& acc = injectFluxAcc[tileKey];
        acc += numCells * fluxRate;
        int nInject = static_cast<int>(acc);
        if (nInject <= 0) {
          if (acc < 0)
            acc = 0;
          continue;
        }
        acc -= nInject;

        const Real span1 = (hi1 - lo1 + 1) * cellSize[t1];
        const Real base1 =
            probLo[t1] + (lo1 - domain.smallEnd(t1)) * cellSize[t1];
        const Real span2 = (nDim > 2) ? (hi2 - lo2 + 1) * cellSize[t2] : 0.0;
        const Real base2 =
            (nDim > 2) ? probLo[t2] + (lo2 - domain.smallEnd(t2)) * cellSize[t2]
                       : 0.0;
        const Real xFace = isHi ? probHi[iDim] : probLo[iDim];

        for (int np = 0; np < nInject; ++np) {
          // Velocity: inward normal speed from speed sampler; transverse
          // components from paired Box-Muller. All 3 velocity components
          // are sampled so 2D3V (AMREX_SPACEDIM=2) has correct out-of-plane
          // velocity vz and thermal pressure.
          Real wIn;
          if (vtherm > 0) {
            wIn = vtherm * speedSamplers[iDim][side].draw(randNum());
          } else {
            wIn = uOut * inward; // cold beam
          }

          // Fractional ingress advancement: particles crossed the face at
          // random times t' in [0, dt], so at dt they have penetrated
          // distance wIn * dt * randNum(). Bound strictly inside the cell.
          const Real pDist =
              (wIn > 0)
                  ? std::min(1.0e-3 * dxn + wIn * dt * randNum(), 0.999 * dxn)
                  : 1.0e-3 * dxn;

          // Position: on the boundary face, nudged into the cell according to
          // sub-step ingress distance; uniform in the transverse directions.
          RealVect pos;
          pos[iDim] = xFace + inward * pDist;
          pos[t1] = base1 + randNum() * span1;
          if (nDim > 2) {
            pos[t2] = base2 + randNum() * span2;
          }

          Real vel[3] = { 0.0, 0.0, 0.0 };
          vel[iDim] = inward * wIn;
          if (sigma > 0) {
            const Real r1 = randNum();
            const Real r2 = randNum();
            const Real R = std::sqrt(-2.0 * std::log(std::max(r1, 1e-300)));
            const Real phi = 2.0 * injPI * r2;
            vel[trans1] = uIn[trans1] + sigma * (R * std::cos(phi));
            vel[trans2] = uIn[trans2] + sigma * (R * std::sin(phi));
          } else {
            vel[trans1] = uIn[trans1];
            vel[trans2] = uIn[trans2];
          }

          auto p = make_particle();
          set_ids(p);
          for (int d = 0; d < nDim; ++d)
            p.pos(d) = pos[d];
          p.rdata(iup_) = vel[ix_];
          p.rdata(ivp_) = vel[iy_];
          p.rdata(iwp_) = vel[iz_];
          p.rdata(iqp_) = q;

          if (NStructInt > iRecordCount_) {
            p.idata(iRecordCount_) = 0;
          }

          particles.push_back(p);
        }
      }
    }
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::accumulate_mass_matrix_contribution(
    int iLev, const IntVect& loIdx, const RealVect& dShift, Real qp,
    Array4<RealCMM> const& mmArr) {

  Real weights_IIID[2][2][2][nDim3];
  //----- Mass matrix calculation begin--------------
  const Real xi0 = dShift[ix_] * dx[iLev][ix_];
  const Real eta0 = dShift[iy_] * dx[iLev][iy_];
  const Real zeta0 = nDim > 2 ? dShift[iz_] * dx[iLev][iz_] : 0;
  const Real xi1 = dx[iLev][ix_] - xi0;
  const Real eta1 = dx[iLev][iy_] - eta0;
  const Real zeta1 = nDim > 2 ? dx[iLev][iz_] - zeta0 : 1;

  weights_IIID[1][1][1][ix_] = eta0 * zeta0 * invVol[iLev];
  weights_IIID[1][1][1][iy_] = xi0 * zeta0 * invVol[iLev];
  weights_IIID[1][1][1][iz_] = xi0 * eta0 * invVol[iLev];

  // xi0*eta0*zeta1*invVol[iLev];
  weights_IIID[1][1][0][ix_] = eta0 * zeta1 * invVol[iLev];
  weights_IIID[1][1][0][iy_] = xi0 * zeta1 * invVol[iLev];
  weights_IIID[1][1][0][iz_] = -xi0 * eta0 * invVol[iLev];

  // xi0*eta1*zeta0*invVol[iLev];
  weights_IIID[1][0][1][ix_] = eta1 * zeta0 * invVol[iLev];
  weights_IIID[1][0][1][iy_] = -xi0 * zeta0 * invVol[iLev];
  weights_IIID[1][0][1][iz_] = xi0 * eta1 * invVol[iLev];

  // xi0*eta1*zeta1*invVol[iLev];
  weights_IIID[1][0][0][ix_] = eta1 * zeta1 * invVol[iLev];
  weights_IIID[1][0][0][iy_] = -xi0 * zeta1 * invVol[iLev];
  weights_IIID[1][0][0][iz_] = -xi0 * eta1 * invVol[iLev];

  // xi1*eta0*zeta0*invVol[iLev];
  weights_IIID[0][1][1][ix_] = -eta0 * zeta0 * invVol[iLev];
  weights_IIID[0][1][1][iy_] = xi1 * zeta0 * invVol[iLev];
  weights_IIID[0][1][1][iz_] = xi1 * eta0 * invVol[iLev];

  // xi1*eta0*zeta1*invVol[iLev];
  weights_IIID[0][1][0][ix_] = -eta0 * zeta1 * invVol[iLev];
  weights_IIID[0][1][0][iy_] = xi1 * zeta1 * invVol[iLev];
  weights_IIID[0][1][0][iz_] = -xi1 * eta0 * invVol[iLev];

  // xi1*eta1*zeta0*invVol[iLev];
  weights_IIID[0][0][1][ix_] = -eta1 * zeta0 * invVol[iLev];
  weights_IIID[0][0][1][iy_] = -xi1 * zeta0 * invVol[iLev];
  weights_IIID[0][0][1][iz_] = xi1 * eta1 * invVol[iLev];

  // xi1*eta1*zeta1*invVol[iLev];
  weights_IIID[0][0][0][ix_] = -eta1 * zeta1 * invVol[iLev];
  weights_IIID[0][0][0][iy_] = -xi1 * zeta1 * invVol[iLev];
  weights_IIID[0][0][0][iz_] = -xi1 * eta1 * invVol[iLev];

  const int iMin = loIdx[ix_];
  const int jMin = loIdx[iy_];
  const int kMin = nDim > 2 ? loIdx[iz_] : 0;
  const int iMax = iMin + 1;
  const int jMax = jMin + 1;
  const int kMax = nDim > 2 ? kMin + 1 : 0;

  const Real coef = fabs(qp) * invVol[iLev];
  Real wg_D[nDim3];
  for (int k1 = kMin; k1 <= kMax; k1++)
    for (int j1 = jMin; j1 <= jMax; j1++)
      for (int i1 = iMin; i1 <= iMax; i1++) {

        for (int iDim = 0; iDim < nDim; iDim++) {
          wg_D[iDim] =
              coef * weights_IIID[i1 - iMin][j1 - jMin][k1 - kMin][iDim];
        }

        auto& data = mmArr(i1, j1, k1);
        // Real weights[27] = { 0 };
        for (int i2 = iMin; i2 <= iMax; i2++) {
          int ip = i2 - i1 + 1;
          const int gp0 = ip * 9;
          for (int j2 = jMin; j2 <= jMax; j2++) {
            int jp = j2 - j1 + 1;
            const int gp1 = gp0 + jp * nDim3;
            for (int k2 = kMin; k2 <= kMax; k2++) {
              const Real(&wg1_D)[nDim3] =
                  weights_IIID[i2 - iMin][j2 - jMin][k2 - kMin];

              // const int kp = k2 - k1 + 1;
              const int gp = gp1 + k2 - k1 + 1;
              for (int iDim = 0; iDim < nDim; iDim++) {
                data[gp] += wg_D[iDim] * wg1_D[iDim];
              }
            }
          }
        }
      }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::sum_to_center(
    MultiFab& netChargeMF, UMultiFab<RealCMM>& centerMM, bool doNetChargeOnly,
    int iLev) {
  timing_func("Pts::sum_to_center");

  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    Array4<Real> const& chargeArr = netChargeMF[pti].array();
    Array4<RealCMM> const& mmArr = centerMM[pti].array();
    const AoS& particles = pti.GetArrayOfStructs();

    const Dim3 lo = init_dim3(0);
    const Dim3 hi = init_dim3(1);

    for (const auto& p : particles) {
      /*
      Q: Why do not check p.id() < 0?
      A: IDs of ghost cell particles are set to -1 inside
      divE_correct_position(), but these particles should be take into account
      here.
      */

      // Print() << "particle = " << p << std::endl;

      const Real qp = p.rdata(iqp_);

      //-----calculate interpolate coef begin-------------
      IntVect loIdx;
      RealVect dShift;
      find_cell_index(p.pos(), Geom(iLev).ProbLo(), Geom(iLev).InvCellSize(),
                      loIdx, dShift);
      Real coef[2][2][2];
      linear_interpolation_coef(dShift, coef);
      //-----calculate interpolate coef end-------------

      const Real cTmp = qp * invVol[iLev];
      for (int kk = lo.z; kk <= hi.z; ++kk)
        for (int jj = lo.y; jj <= hi.y; ++jj)
          for (int ii = lo.x; ii <= hi.x; ++ii) {
            const IntVect ijk = { AMREX_D_DECL(loIdx[ix_] + ii, loIdx[iy_] + jj,
                                               loIdx[iz_] + kk) };
            chargeArr(ijk) += coef[ii][jj][kk] * cTmp;
          }

      if (!doNetChargeOnly) {
        accumulate_mass_matrix_contribution(iLev, loIdx, dShift, qp, mmArr);
      } // if doChargeOnly

    } // for p
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::sum_to_center_amr(
    MultiFab& netChargeMF, MultiFab& jc, MultiFab& jf,
    UMultiFab<RealCMM>& centerMM, bool doNetChargeOnly, int iLev) {
  timing_func("Pts::sum_to_center");

  int finer_level = iLev + 1;
  int coarser_level = iLev - 1;
  if (iLev == 0) {
    coarser_level = iLev;
  }
  if (iLev == (n_lev() - 1)) {
    finer_level = iLev;
  }
  for (int nLev = finer_level; nLev >= coarser_level; nLev--) {
    if (iLev == nLev) {
      for (PIter pti(*this, nLev); pti.isValid(); ++pti) {
        Array4<Real> const& chargeArr = netChargeMF[pti].array();

        Array4<RealCMM> const& mmArr = centerMM[pti].array();
        const Array4<int const>& status = cell_status(nLev)[pti].array();
        const AoS& particles = pti.GetArrayOfStructs();
        const Dim3 lo = init_dim3(0);
        const Dim3 hi = init_dim3(1);
        for (const auto& p : particles) {
          const Real qp = p.rdata(iqp_);
          IntVect loIdx;
          RealVect dShift;
          IntVect realIdx;
          RealVect tmprv;
          find_cell_index_exp(p.pos(), Geom(nLev).ProbLo(),
                              Geom(nLev).InvCellSize(), realIdx, tmprv);
          if (nLev == iLev || (bit::is_refined_neighbour(status(realIdx)) ||
                               bit::is_lev_edge(status(realIdx)))) {
            find_cell_index(p.pos(), Geom(iLev).ProbLo(),
                            Geom(iLev).InvCellSize(), loIdx, dShift);
            Real coef[2][2][2];
            linear_interpolation_coef(dShift, coef);
            const Real cTmp = qp * invVol[iLev];
            for (int kk = lo.z; kk <= hi.z; ++kk)
              for (int jj = lo.y; jj <= hi.y; ++jj)
                for (int ii = lo.x; ii <= hi.x; ++ii) {
                  const IntVect ijk = { AMREX_D_DECL(
                      loIdx[ix_] + ii, loIdx[iy_] + jj, loIdx[iz_] + kk) };
                  chargeArr(ijk) += coef[ii][jj][kk] * cTmp;
                }
          }

          bool skipParticle = false;
          if (n_lev() > 1) {
            skipParticle = skip_particle_for_dive_cleaning(p.pos(), Geom(iLev),
                                                           iLev, status);
          }
          if (!doNetChargeOnly && !skipParticle) {
            accumulate_mass_matrix_contribution(iLev, loIdx, dShift, qp, mmArr);
          }
        }
      }
    }
    if (nLev > iLev) {
      for (PIter pti(*this, nLev); pti.isValid(); ++pti) {
        Array4<Real> const& chargeArr = jf[pti].array();
        const Array4<int const>& status = cell_status(nLev)[pti].array();
        const AoS& particles = pti.GetArrayOfStructs();
        const Dim3 lo = init_dim3(0);
        const Dim3 hi = init_dim3(1);
        for (const auto& p : particles) {
          const Real qp = p.rdata(iqp_);
          IntVect loIdx;
          RealVect dShift;
          IntVect realIdx;
          RealVect tmprv;
          find_cell_index_exp(p.pos(), Geom(nLev).ProbLo(),
                              Geom(nLev).InvCellSize(), realIdx, tmprv);
          if (nLev == iLev || (bit::is_refined_neighbour(status(realIdx)) ||
                               bit::is_lev_edge(status(realIdx)))) {
            find_cell_index(p.pos(), Geom(iLev).ProbLo(),
                            Geom(iLev).InvCellSize(), loIdx, dShift);
            Real coef[2][2][2];
            linear_interpolation_coef(dShift, coef);
            const Real cTmp = qp * invVol[iLev];
            for (int kk = lo.z; kk <= hi.z; ++kk)
              for (int jj = lo.y; jj <= hi.y; ++jj)
                for (int ii = lo.x; ii <= hi.x; ++ii) {
                  const IntVect ijk = { AMREX_D_DECL(
                      loIdx[ix_] + ii, loIdx[iy_] + jj, loIdx[iz_] + kk) };

                  chargeArr(ijk) += coef[ii][jj][kk] * cTmp;
                }
          }
        }
      }
    }
    if (nLev < iLev) {
      for (PIter pti(*this, nLev); pti.isValid(); ++pti) {
        Array4<Real> const& chargeArr = jc[pti].array();
        const Array4<int const>& status = cell_status(nLev)[pti].array();
        const AoS& particles = pti.GetArrayOfStructs();
        const Dim3 lo = init_dim3(0);
        const Dim3 hi = init_dim3(1);
        for (const auto& p : particles) {
          const Real qp = p.rdata(iqp_);
          IntVect loIdx;
          RealVect dShift;
          IntVect realIdx;
          RealVect tmprv;
          find_cell_index_exp(p.pos(), Geom(nLev).ProbLo(),
                              Geom(nLev).InvCellSize(), realIdx, tmprv);
          if (nLev == iLev || (bit::is_refined_neighbour(status(realIdx)) ||
                               bit::is_lev_edge(status(realIdx)))) {
            find_cell_index(p.pos(), Geom(iLev).ProbLo(),
                            Geom(iLev).InvCellSize(), loIdx, dShift);
            Real coef[2][2][2];
            linear_interpolation_coef(dShift, coef);
            const Real cTmp = qp * invVol[iLev];
            for (int kk = lo.z; kk <= hi.z; ++kk)
              for (int jj = lo.y; jj <= hi.y; ++jj)
                for (int ii = lo.x; ii <= hi.x; ++ii) {
                  const IntVect ijk = { AMREX_D_DECL(
                      loIdx[ix_] + ii, loIdx[iy_] + jj, loIdx[iz_] + kk) };

                  chargeArr(ijk) += coef[ii][jj][kk] * cTmp;
                }
          }
        }
      }
    }
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
std::array<Real, 5> Particles<NStructReal, NStructInt>::total_moments(
    bool localOnly) {
  timing_func("Pts::total_moments");

  std::array<Real, 5> sum = { 0, 0, 0, 0, 0 };

  for (int i = 0; i < 5; ++i)
    sum[i] = 0;

  const int iLev = 0;
  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    const AoS& particles = pti.GetArrayOfStructs();
    for (const auto& p : particles) {
      if (p.id() < 0)
        continue;

      const Real up = p.rdata(iup_);
      const Real vp = p.rdata(ivp_);
      const Real wp = p.rdata(iwp_);
      const Real qp = p.rdata(iqp_);

      sum[0] += qp;
      sum[1] += qp * up;
      sum[2] += qp * vp;
      sum[3] += qp * wp;
      sum[4] += 0.5 * qp * (up * up + vp * vp + wp * wp);
    }
  }

  for (int i = 0; i < 5; ++i)
    sum[i] *= qomSign * get_mass();

  if (!localOnly) {
    ParallelDescriptor::ReduceRealSum(sum.data(), sum.size(),
                                      ParallelDescriptor::IOProcessorNumber());
  }

  return sum;
}

//==========================================================
template <int NStructReal, int NStructInt>
Real Particles<NStructReal, NStructInt>::sum_moments(
    Vector<MultiFab>& momentsMF, Vector<MultiFab>& nodeBMF, Real dt) {
  timing_func("Pts::sum_moments");

  Real energy = 0;
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    timing_func("Pts::sum_moments_node_deposit");
    momentsMF[iLev].setVal(0.0);
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      Array4<Real> const& momentsArr = momentsMF[iLev][pti].array();

      const AoS& particles = pti.GetArrayOfStructs();

      const Dim3 lo = init_dim3(0);
      const Dim3 hi = init_dim3(1);

      // Print() << "iLev = " << iLev << std::endl;
      for (const auto& p : particles) {
        if (p.id() < 0)
          continue;

        // Print() << "p = " << p << std::endl;
        const Real up = p.rdata(iup_);
        const Real vp = p.rdata(ivp_);
        const Real wp = p.rdata(iwp_);
        const Real qp = p.rdata(iqp_);

        //-----calculate interpolate coef begin-------------
        IntVect loIdx;
        RealVect dShift;
        find_node_index(p.pos(), Geom(iLev).ProbLo(), Geom(iLev).InvCellSize(),
                        loIdx, dShift);
        Real coef[2][2][2];
        linear_interpolation_coef(dShift, coef);
        //-----calculate interpolate coef end-------------

        //-------nodePlasma begin---------
        Real pMoments[nMoments];

        pMoments[iNum_] = 1;
        pMoments[iRho_] = qp;

        {
          const Real mx = qp * up;
          const Real my = qp * vp;
          const Real mz = qp * wp;
          pMoments[iMx_] = mx;
          pMoments[iMy_] = my;
          pMoments[iMz_] = mz;

          pMoments[iPxx_] = mx * up;
          pMoments[iPyy_] = my * vp;
          pMoments[iPzz_] = mz * wp;

          pMoments[iPxy_] = mx * vp;
          pMoments[iPxz_] = mx * wp;
          pMoments[iPyz_] = my * wp;
        }

        for (int iVar = 0; iVar < nMoments; iVar++)
          for (int kk = lo.z; kk <= hi.z; ++kk)
            for (int jj = lo.y; jj <= hi.y; ++jj)
              for (int ii = lo.x; ii <= hi.x; ++ii) {
                const IntVect ijk = { AMREX_D_DECL(
                    loIdx[ix_] + ii, loIdx[iy_] + jj, loIdx[iz_] + kk) };
                momentsArr(ijk, iVar) += coef[ii][jj][kk] * pMoments[iVar];
              }

        //-------nodePlasma end---------

        energy += qp * (up * up + vp * vp + wp * wp);
      } // for p
    }

    // Exclude the number density.
    momentsMF[iLev].mult(invVol[iLev], 0, nMoments - 1,
                         momentsMF[iLev].nGrow());

    //----- Mirror boundary condition for the node-centred deposit ------------
    // A node-centred CIC deposit at a non-periodic wall leaves the boundary
    // node with only ~half the charge of an interior node: with the node at the
    // domain face, a particle in the edge cell deposits its weight between that
    // edge node and the next interior node, and NO particle lies on the
    // exterior side to contribute the complementary weight.  This is why, in
    // full-PIC mode, the reflecting-wall node of the 1D shock test showed rhoS0
    // ~ 6.25 instead of ~12.5, while the hybrid path (cell-centred deposit +
    // sum_moments_cell_centered) did not.
    //
    // The physically correct boundary condition for a reflecting wall (and an
    // open-inflow face, whose injected particles live just inside the edge
    // cell) is MIRROR SYMMETRY across the boundary node: the ghost node
    // must carry the same moment value as the edge node (Neumann mirror).  We
    // therefore copy the edge node into the first ghost node and fold the ghost
    // back into the edge node (equivalent to doubling the edge node), then zero
    // the ghost layer so SumBoundary does not re-use it.  We apply this ONLY at
    // reflect / inflow faces (`conducting` is a field-only type; on the
    // particle side it is mapped to `reflect` at parse time).  At coupled
    // (MHD-AEPIC), outflow, vacuum, absorb, and periodic faces the
    // ghost layer may hold real injected/leaving particles, so we leave it
    // untouched to avoid double counting.  Periodic directions are skipped
    // (SumBoundary wraps them).
    // NOTE: the fold runs BEFORE SumBoundary on purpose so a transverse ghost
    // target that lies in a tile-neighbour's valid region is combined exactly
    // once.
    if (!Geom(iLev).isAllPeriodic() && momentsMF[iLev].nGrow() > 0) {
      const Box& dom = Geom(iLev).Domain();
      const int nCompMF = momentsMF[iLev].nComp();
      for (MFIter mfi(momentsMF[iLev]); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        Array4<Real> const& arr = momentsMF[iLev][mfi].array();

        for (int iDim = 0; iDim < nDim; ++iDim) {
          if (Geom(iLev).isPeriodic(iDim))
            continue;

          for (int side = 0; side < 2; ++side) {
            const bool isLo = (side == 0);
            const int faceBc = isLo ? bc.lo[iDim] : bc.hi[iDim];
            // A particle face can no longer be `conducting`: that is a
            // field-only type, and on the particle side it is mapped to
            // `reflect` at parse time.  The fold set is therefore
            // reflect + inflow.
            const bool doFold =
                (faceBc == ParticleBC::reflect || faceBc == ParticleBC::inflow);
            if (!doFold)
              continue;

            // NOTE: momentsMF is NODE-centred (nGrids = convert(cGrids,
            // nodeVector)), whose domain box extends one node beyond the
            // cell-centred Geometry box on the hi side.  The edge NODE is
            // dom.smallEnd (lo) or dom.bigEnd+1 (hi); using dom.bigEnd would
            // hit the second-last node and skip the real edge tile.
            const int domEdge =
                isLo ? dom.smallEnd(iDim) : dom.bigEnd(iDim) + 1;
            const int tileEdge = isLo ? bx.smallEnd(iDim) : bx.bigEnd(iDim);
            if (tileEdge != domEdge)
              continue;

            // Strip of the first ghost-node layer just outside the domain,
            // spanning the full transverse extent of this FAB (valid +
            // transverse ghosts).  Mirror the edge node into it, fold it into
            // the edge node (doubling), then clear the ghost.
            const Box& fbx = mfi.fabbox();
            IntVect gs = fbx.smallEnd();
            IntVect ge = fbx.bigEnd();
            gs[iDim] = domEdge + (isLo ? -1 : 1);
            ge[iDim] = gs[iDim];
            const Box strip(gs, ge);

            const bool isReflect = (faceBc == ParticleBC::reflect);

            ParallelFor(strip, nCompMF,
                        [=] AMREX_GPU_DEVICE(int i, int j, int k, int c) {
                          // Odd parity components relative to iDim under
                          // specular reflection: normal momentum and
                          // off-diagonal normal-shear stresses.
                          bool isOdd = false;
                          if (iDim == 0) {
                            isOdd = (c == iMx_ || c == iPxy_ || c == iPxz_);
                          } else if (iDim == 1) {
                            isOdd = (c == iMy_ || c == iPxy_ || c == iPyz_);
                          } else if (iDim == 2) {
                            isOdd = (c == iMz_ || c == iPxz_ || c == iPyz_);
                          }

#if (AMREX_SPACEDIM == 2)
                          int ei = i, ej = j;
                          if (iDim == 0)
                            ei = domEdge;
                          if (iDim == 1)
                            ej = domEdge;
                          const Real val = arr(ei, ej, 0, c);
                          if (isReflect && isOdd) {
                            arr(ei, ej, 0, c) = 0.0;
                          } else {
                            arr(ei, ej, 0, c) += val; // fold mirror back -> 2x
                          }
                          arr(i, j, 0, c) = 0.0; // zero the ghost layer
#elif (AMREX_SPACEDIM == 3)
                  int ei = i, ej = j, ek = k;
                  if (iDim == 0) ei = domEdge;
                  if (iDim == 1) ej = domEdge;
                  if (iDim == 2) ek = domEdge;
                  const Real val = arr(ei, ej, ek, c);
                  if (isReflect && isOdd) {
                    arr(ei, ej, ek, c) = 0.0;
                  } else {
                    arr(ei, ej, ek, c) += val; // fold mirror back -> 2x
                  }
                  arr(i, j, k, c) = 0.0; // zero the ghost layer
#endif
                        });
          }
        }
      }
    }

    momentsMF[iLev].SumBoundary(Geom(iLev).periodicity());
  }

  for (int iLev = n_lev() - 2; iLev >= 0; iLev--) {
    timing_func("Pts::sum_moments_coarse_fine_interface");
    sum_two_lev_interface_node(
        momentsMF[iLev], momentsMF[iLev + 1], 0, momentsMF[iLev].nComp(),
        get_ref_ratio(iLev), Geom(iLev), Geom(iLev + 1), node_status(iLev + 1));
  }

  // Correct domain edge nodes
  for (int iLev = 0; iLev < n_lev() - 1; iLev++) {
    timing_func("Pts::sum_moments_domain_edge_correction");
    interp_from_coarse_to_fine_for_domain_edge(
        momentsMF[iLev], momentsMF[iLev + 1], 0, momentsMF[iLev].nComp(),
        get_ref_ratio(iLev), Geom(iLev), Geom(iLev + 1), node_status(iLev + 1));
  }

  energy *= 0.5 * qomSign * get_mass();

  return energy;
}

//==========================================================
// Cell-centred moment deposit. The raw rho / momentum / pressure-tensor moments
// are scattered to the cell-centred momentsMF (centerPlasma[iSpecies]) with a
// plain cell-centred trilinear scatter (find_cell_index + linear weights).
template <int NStructReal, int NStructInt>
Real Particles<NStructReal, NStructInt>::sum_moments_cell_centered(
    Vector<MultiFab>& momentsMF) {
  timing_func("Pts::sum_moments_cell_centered");

  Real energy = 0;
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    momentsMF[iLev].setVal(0.0);
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      Array4<Real> const& momentsArr = momentsMF[iLev][pti].array();

      const AoS& particles = pti.GetArrayOfStructs();

      const Dim3 lo = init_dim3(0);
      const Dim3 hi = init_dim3(1);

      for (const auto& p : particles) {
        if (p.id() < 0)
          continue;

        const Real up = p.rdata(iup_);
        const Real vp = p.rdata(ivp_);
        const Real wp = p.rdata(iwp_);
        const Real qp = p.rdata(iqp_);

        //-----calculate interpolate coef begin-------------
        IntVect loIdx;
        RealVect dShift;
        // Cell-centred deposit: find the containing cell (find_cell_index) and
        // interpolate between its centre and the next cell centre (trilinear).
        find_cell_index(p.pos(), Geom(iLev).ProbLo(), Geom(iLev).InvCellSize(),
                        loIdx, dShift);
        Real coef[2][2][2];
        linear_interpolation_coef(dShift, coef);
        //-----calculate interpolate coef end-------------

        //-------cell-centred moments begin---------
        Real pMoments[nMoments];

        pMoments[iNum_] = 1;
        pMoments[iRho_] = qp;

        {
          const Real mx = qp * up;
          const Real my = qp * vp;
          const Real mz = qp * wp;
          pMoments[iMx_] = mx;
          pMoments[iMy_] = my;
          pMoments[iMz_] = mz;

          pMoments[iPxx_] = mx * up;
          pMoments[iPyy_] = my * vp;
          pMoments[iPzz_] = mz * wp;

          pMoments[iPxy_] = mx * vp;
          pMoments[iPxz_] = mx * wp;
          pMoments[iPyz_] = my * wp;
        }

        for (int iVar = 0; iVar < nMoments; iVar++)
          for (int kk = lo.z; kk <= hi.z; ++kk)
            for (int jj = lo.y; jj <= hi.y; ++jj)
              for (int ii = lo.x; ii <= hi.x; ++ii) {
                const IntVect ijk = { AMREX_D_DECL(
                    loIdx[ix_] + ii, loIdx[iy_] + jj, loIdx[iz_] + kk) };
                momentsArr(ijk, iVar) += coef[ii][jj][kk] * pMoments[iVar];
              }
        //-------cell-centred moments end---------

        energy += qp * (up * up + vp * vp + wp * wp);
      } // for p
    }

    // Exclude the number density.
    momentsMF[iLev].mult(invVol[iLev], 0, nMoments - 1,
                         momentsMF[iLev].nGrow());

    // Fold first ghost layer back into edge cells at non-periodic domain
    // boundaries to conserve charge from outer half-cell CIC deposits.
    // Executed before SumBoundary so transverse ghost tails are correctly
    // combined across tile boundaries.
    if (!Geom(iLev).isAllPeriodic() && momentsMF[iLev].nGrow() > 0) {
      const Box& dom = Geom(iLev).Domain();
      const int nCompMF = momentsMF[iLev].nComp();
      for (MFIter mfi(momentsMF[iLev]); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        Array4<Real> const& arr = momentsMF[iLev][mfi].array();

        for (int iDim = 0; iDim < nDim; ++iDim) {
          if (Geom(iLev).isPeriodic(iDim))
            continue;

          for (int side = 0; side < 2; ++side) {
            const bool isLo = (side == 0);
            const int domEdge = isLo ? dom.smallEnd(iDim) : dom.bigEnd(iDim);
            if ((isLo ? bx.smallEnd(iDim) : bx.bigEnd(iDim)) != domEdge)
              continue;

            const Box& fbx = mfi.fabbox();
            IntVect gs = fbx.smallEnd();
            IntVect ge = fbx.bigEnd();
            gs[iDim] = domEdge + (isLo ? -1 : 1);
            ge[iDim] = gs[iDim];
            const Box strip(gs, ge);

            const int di = (iDim == 0) ? (isLo ? 1 : -1) : 0;
            const int dj = (iDim == 1) ? (isLo ? 1 : -1) : 0;
            const int dk = (iDim == 2) ? (isLo ? 1 : -1) : 0;

            ParallelFor(strip, nCompMF,
                        [=] AMREX_GPU_DEVICE(int i, int j, int k, int c) {
                          arr(i + di, j + dj, k + dk, c) += arr(i, j, k, c);
                          arr(i, j, k, c) = 0.0;
                        });
          }
        }
      }
    }

    momentsMF[iLev].SumBoundary(Geom(iLev).periodicity());
  }

  // Cell-centred coarse-fine interface for AMR. Unlike the node-centred path,
  // we only need the fine→coarse summation (average_down + Add) for
  // cell-centred moments. The coarse→fine overwrite of level-edge cells is NOT
  // used here because it discards valid fine-level particle deposits (the fine
  // level-edge cells contain real particles), causing worse energy growth.
  for (int iLev = n_lev() - 2; iLev >= 0; iLev--) {
    timing_func("Pts::sum_moments_coarse_fine_interface_cell");
    sum_fine_to_coarse_lev_bny_cell(momentsMF[iLev], momentsMF[iLev + 1], 0,
                                    momentsMF[iLev].nComp(),
                                    get_ref_ratio(iLev));
  }

  energy *= 0.5 * qomSign * get_mass();

  return energy;
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::calc_mass_matrix(
    UMultiFab<RealMM>& nodeMM, MultiFab& jHat, MultiFab& nodeBMF,
    MultiFab& u0MF, Real dt, int iLev, bool solveInCoMov) {
  timing_func("Pts::calc_mass_matrix");

  Real qdto2mc = charge / mass * 0.5 * dt;

  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    Array4<Real const> const& nodeBArr = nodeBMF[pti].array();
    Array4<Real> const& jArr = jHat[pti].array();
    Array4<RealMM> const& mmArr = nodeMM[pti].array();

    Array4<Real const> const& u0Arr = u0MF[pti].array();

    const AoS& particles = pti.GetArrayOfStructs();

    const Dim3 lo = init_dim3(0);
    const Dim3 hi = init_dim3(1);

    for (const auto& p : particles) {
      if (p.id() < 0)
        continue;

      const Real up = p.rdata(iup_);
      const Real vp = p.rdata(ivp_);
      const Real wp = p.rdata(iwp_);
      const Real qp = p.rdata(iqp_);

      //-----calculate interpolation coef begin-------------
      IntVect loIdx;
      RealVect dShift;

      find_node_index(p.pos(), Geom(iLev).ProbLo(), Geom(iLev).InvCellSize(),
                      loIdx, dShift);

      Real coef[2][2][2];
      linear_interpolation_coef(dShift, coef);
      //-----calculate interpolate coef end-------------

      //----- Mass matrix calculation begin--------------
      Real u0[3] = { 0, 0, 0 };
      Real bp[3] = { 0, 0, 0 };

      for (int kk = lo.z; kk <= hi.z; ++kk)
        for (int jj = lo.y; jj <= hi.y; ++jj)
          for (int ii = lo.x; ii <= hi.x; ++ii) {
            const IntVect ijk = { AMREX_D_DECL(loIdx[ix_] + ii, loIdx[iy_] + jj,
                                               loIdx[iz_] + kk) };
            for (int iDim = 0; iDim < nDim3; iDim++) {
              bp[iDim] += nodeBArr(ijk, iDim) * coef[ii][jj][kk];

              if (solveInCoMov)
                u0[iDim] += u0Arr(ijk, iDim) * coef[ii][jj][kk];
            }
          }

      const Real omx = qdto2mc * bp[ix_];
      const Real omy = qdto2mc * bp[iy_];
      const Real omz = qdto2mc * bp[iz_];

      // end interpolation
      const Real omx2 = omx * omx;
      const Real omy2 = omy * omy;
      const Real omz2 = omz * omz;
      const Real omxomy = omx * omy;
      const Real omxomz = omx * omz;
      const Real omyomz = omy * omz;
      const Real omsq = omx2 + omy2 + omz2;
      const Real denom = 1.0 / (1.0 + omsq);

      const Real c0 = denom * invVol[iLev] * qp * qdto2mc;

      Real alpha[9];
      alpha[0] = (1 + omx2) * c0;
      alpha[1] = (omz + omxomy) * c0;
      alpha[2] = (-omy + omxomz) * c0;
      alpha[3] = (-omz + omxomy) * c0;
      alpha[4] = (1 + omy2) * c0;
      alpha[5] = (omx + omyomz) * c0;
      alpha[6] = (omy + omxomz) * c0;
      alpha[7] = (-omx + omyomz) * c0;
      alpha[8] = (1 + omz2) * c0;

      {
        // jHat
        Real currents[3];

        const Real up1 = up - u0[0];
        const Real vp1 = vp - u0[1];
        const Real wp1 = wp - u0[2];

        const Real udotOm1 = up1 * omx + vp1 * omy + wp1 * omz;

        {
          const Real coef1 = denom * qp;
          currents[ix_] =
              (up1 + (vp1 * omz - wp1 * omy + udotOm1 * omx)) * coef1;
          currents[iy_] =
              (vp1 + (wp1 * omx - up1 * omz + udotOm1 * omy)) * coef1;
          currents[iz_] =
              (wp1 + (up1 * omy - vp1 * omx + udotOm1 * omz)) * coef1;
        }

        for (int iVar = 0; iVar < 3; iVar++)
          for (int kk = lo.z; kk <= hi.z; ++kk)
            for (int jj = lo.y; jj <= hi.y; ++jj)
              for (int ii = lo.x; ii <= hi.x; ++ii) {
                IntVect ijk = { AMREX_D_DECL(loIdx[ix_] + ii, loIdx[iy_] + jj,
                                             loIdx[iz_] + kk) };
                jArr(ijk, iVar) += coef[ii][jj][kk] * currents[iVar];
              }
      }

      const int iMin = loIdx[ix_];
      const int jMin = loIdx[iy_];
      const int kMin = nDim > 2 ? loIdx[iz_] : 0;
      const int iMax = iMin + 1;
      const int jMax = jMin + 1;
      const int kMax = nDim > 2 ? kMin + 1 : 0;

      for (int k1 = kMin; k1 <= kMax; k1++)
        for (int j1 = jMin; j1 <= jMax; j1++)
          for (int i1 = iMin; i1 <= iMax; i1++) {
            const Real wg = coef[i1 - iMin][j1 - jMin][k1 - kMin];
            auto& data0 = mmArr(i1, j1, k1);
            for (int k2 = kMin; k2 <= kMax; k2++) {
              const int kp = k2 - k1 + 1;
              if (kp > 0) {
                for (int j2 = jMin; j2 <= jMax; j2++) {
                  const int jp = j2 - j1 + 1;
                  for (int i2 = iMin; i2 <= iMax; i2++) {
                    const Real weight =
                        wg * coef[i2 - iMin][j2 - jMin][k2 - kMin];
                    const int idx0 = kp * 81 + jp * 27 + (i2 - i1 + 1) * 9;

                    Real* const data = &(data0[idx0]);
                    for (int idx = 0; idx < 9; idx++) {
                      data[idx] += alpha[idx] * weight;
                    }
                  } // k2

                } // j2
              } // if (ip > 0)
            } // i2
          } // k1

      //----- Mass matrix calculation end--------------

    } // for p
  }

  for (MFIter mfi(nodeMM); mfi.isValid(); ++mfi) {
    // Finalize the mass matrix calculation.
    const Box box = mfi.validbox();
    const auto lo = lbound(box);
    const auto hi = ubound(box);

    Array4<RealMM> const& mmArr = nodeMM[mfi].array();

    // We only need the mass matrix on the physical nodes. But the first layer
    // of the ghost nodes may contributes to the physical nodes below (ghost
    // node constributes as a sender). So, we need the '-1' and '+1' staff.
    const int iMin = lo.x - 1, jMin = lo.y - 1, kMin = nDim > 2 ? lo.z - 1 : 0;
    const int iMax = hi.x + 1, jMax = hi.y + 1, kMax = nDim > 2 ? hi.z + 1 : 0;

    int gps, gpr; // gp_send, gp_receive
    for (int k1 = kMin; k1 <= kMax; k1++)
      for (int j1 = jMin; j1 <= jMax; j1++)
        for (int i1 = iMin; i1 <= iMax; i1++) {
          const int kp = 2;
          const int kr = nDim > 2 ? k1 + kp - 1 : 0;
          if (kr > kMax || kr < kMin)
            continue;
          auto& datas0 = mmArr(i1, j1, k1);
          for (int jp = 0; jp < 3; jp++) {
            const int jr = j1 + jp - 1;
            if (jr > jMax || jr < jMin)
              continue;
            const int jpr = 2 - jp;
            for (int ip = 0; ip < 3; ip++) {
              const int ir = i1 + ip - 1;
              if (ir > iMax || ir < iMin)
                continue;
              const int ipr = 2 - ip;
              gpr = jpr * 3 + ipr;
              gps = 18 + jp * 3 + ip; // gps = kp*9+jp*3+kp

              Real* const datar = &(mmArr(ir, jr, kr)[gpr * 9]);
              const Real* const datas = &(datas0[gps * 9]);
              for (int idx = 0; idx < 9; idx++) {
                datar[idx] = datas[idx];
              } // idx
            } // kp
          } // jp
        } // k1
  }
}
//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::calc_mass_matrix_amr(
    UMultiFab<RealMM>& nodeMM,
    amrex::Vector<amrex::Vector<UMultiFab<RealMM> > >& nmmc,
    amrex::Vector<UMultiFab<RealMM> >& nmmf, MultiFab& jHat,
    amrex::Vector<amrex::Vector<amrex::MultiFab> >& jhc,
    amrex::Vector<amrex::MultiFab>& jhf, MultiFab& nodeBMF, MultiFab& u0MF,
    Real dt, int iLev, bool solveInCoMov,
    amrex::Vector<amrex::iMultiFab>& cellstatus) {
  timing_func("Pts::calc_mass_matrix");

  Real qdto2mc = charge / mass * 0.5 * dt;

  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    Array4<Real const> const& nodeBArr = nodeBMF[pti].array();
    Array4<Real> const& jArr = jHat[pti].array();
    Array4<RealMM> const& mmArr = nodeMM[pti].array();
    Array4<Real const> const& u0Arr = u0MF[pti].array();
    const Array4<int const>& status = cellstatus[iLev][pti].array();
    Box bx = pti.tilebox();
    IntVect ibx = bx.smallEnd();
    bool refinedneighbour = false;
    if (bit::is_refined_neighbour(status(ibx))) {
      refinedneighbour = true;
    }
    amrex::Vector<Array4<Real> > jArrt;
    amrex::Vector<Array4<RealMM> > mmArrt;
    if (iLev > 0) {
      for (int i = 0; i < iLev; i++) {
        jArrt.push_back(jhc[iLev][i][pti].array());
        mmArrt.push_back(nmmc[iLev][i][pti].array());
      }
    }
    jArrt.push_back(jArr);
    mmArrt.push_back(mmArr);

    if (refinedneighbour) {
      jArrt.push_back(jhf[iLev][pti].array());
      mmArrt.push_back(nmmf[iLev][pti].array());
    }

    amrex::Vector<IntVect> loIdx;
    amrex::Vector<RealVect> dShift;
    const int nCoef = iLev + 1 + refinedneighbour;
    loIdx.resize(nCoef);
    dShift.resize(nCoef);
    using InterpolationCoef = Real[2][2][2];
    auto coef = std::make_unique<InterpolationCoef[]>(nCoef);

    const AoS& particles = pti.GetArrayOfStructs();

    const Dim3 lo = init_dim3(0);
    const Dim3 hi = init_dim3(1);

    for (const auto& p : particles) {
      if (p.id() < 0)
        continue;

      // Print()<<"p = "<<p<<std::endl;
      const Real up = p.rdata(iup_);
      const Real vp = p.rdata(ivp_);
      const Real wp = p.rdata(iwp_);
      const Real qp = p.rdata(iqp_);

      //-----calculate interpolate coef begin-------------
      for (int i = 0; i < nCoef; i++) {
        find_node_index(p.pos(), Geom(i).ProbLo(), Geom(i).InvCellSize(),
                        loIdx[i], dShift[i]);

        linear_interpolation_coef(dShift[i], coef[i]);
      }

      //-----calculate interpolate coef end-------------

      //----- Mass matrix calculation begin--------------
      Real u0[3] = { 0, 0, 0 };
      Real bp[3] = { 0, 0, 0 };

      for (int kk = lo.z; kk <= hi.z; ++kk)
        for (int jj = lo.y; jj <= hi.y; ++jj)
          for (int ii = lo.x; ii <= hi.x; ++ii) {
            const IntVect ijk = { AMREX_D_DECL(loIdx[iLev][ix_] + ii,
                                               loIdx[iLev][iy_] + jj,
                                               loIdx[iLev][iz_] + kk) };
            for (int iDim = 0; iDim < nDim3; iDim++) {
              bp[iDim] += nodeBArr(ijk, iDim) * coef[iLev][ii][jj][kk];

              if (solveInCoMov)
                u0[iDim] += u0Arr(ijk, iDim) * coef[iLev][ii][jj][kk];
            }
          }

      const Real omx = qdto2mc * bp[ix_];
      const Real omy = qdto2mc * bp[iy_];
      const Real omz = qdto2mc * bp[iz_];

      // end interpolation
      const Real omx2 = omx * omx;
      const Real omy2 = omy * omy;
      const Real omz2 = omz * omz;
      const Real omxomy = omx * omy;
      const Real omxomz = omx * omz;
      const Real omyomz = omy * omz;
      const Real omsq = omx2 + omy2 + omz2;
      const Real denom = 1.0 / (1.0 + omsq);

      const Real c0 = denom * invVol[iLev] * qp * qdto2mc;

      Real alpha[9];
      alpha[0] = (1 + omx2) * c0;
      alpha[1] = (omz + omxomy) * c0;
      alpha[2] = (-omy + omxomz) * c0;
      alpha[3] = (-omz + omxomy) * c0;
      alpha[4] = (1 + omy2) * c0;
      alpha[5] = (omx + omyomz) * c0;
      alpha[6] = (omy + omxomz) * c0;
      alpha[7] = (-omx + omyomz) * c0;
      alpha[8] = (1 + omz2) * c0;

      // jHat
      Real currents[3];

      const Real up1 = up - u0[0];
      const Real vp1 = vp - u0[1];
      const Real wp1 = wp - u0[2];

      const Real udotOm1 = up1 * omx + vp1 * omy + wp1 * omz;

      const Real coef1 = denom * qp;
      currents[ix_] = (up1 + (vp1 * omz - wp1 * omy + udotOm1 * omx)) * coef1;
      currents[iy_] = (vp1 + (wp1 * omx - up1 * omz + udotOm1 * omy)) * coef1;
      currents[iz_] = (wp1 + (up1 * omy - vp1 * omx + udotOm1 * omz)) * coef1;

      for (int iVar = 0; iVar < 3; iVar++)
        for (int kk = lo.z; kk <= hi.z; ++kk)
          for (int jj = lo.y; jj <= hi.y; ++jj)
            for (int ii = lo.x; ii <= hi.x; ++ii) {
              for (int i = 0; i < nCoef; i++) {
                IntVect ijk = { AMREX_D_DECL(loIdx[i][ix_] + ii,
                                             loIdx[i][iy_] + jj,
                                             loIdx[i][iz_] + kk) };
                jArrt[i](ijk, iVar) += coef[i][ii][jj][kk] * currents[iVar];
              }
            }

      for (int i = 0; i < nCoef; i++) {
        const int iMin = loIdx[i][ix_];
        const int jMin = loIdx[i][iy_];
        const int kMin = nDim > 2 ? loIdx[i][iz_] : 0;
        const int iMax = iMin + 1;
        const int jMax = jMin + 1;
        const int kMax = nDim > 2 ? kMin + 1 : 0;

        for (int k1 = kMin; k1 <= kMax; k1++)
          for (int j1 = jMin; j1 <= jMax; j1++)
            for (int i1 = iMin; i1 <= iMax; i1++) {
              const Real wg = coef[i][i1 - iMin][j1 - jMin][k1 - kMin];
              auto& data0 = mmArrt[i](i1, j1, k1);
              for (int k2 = kMin; k2 <= kMax; k2++) {
                const int kp = k2 - k1 + 1;
                // if (kp > 0)
                if (true) {
                  for (int j2 = jMin; j2 <= jMax; j2++) {
                    const int jp = j2 - j1 + 1;
                    for (int i2 = iMin; i2 <= iMax; i2++) {
                      const Real weight =
                          wg * coef[i][i2 - iMin][j2 - jMin][k2 - kMin];
                      const int idx0 = kp * 81 + jp * 27 + (i2 - i1 + 1) * 9;

                      Real* const data = &(data0[idx0]);
                      for (int idx = 0; idx < 9; idx++) {
                        data[idx] += alpha[idx] * weight;
                      }
                    } // k2

                  } // j2
                } // if (ip > 0)
              } // i2
            } // k1
      }
    } // for p
  }

  // for (MFIter mfi(nodeMM); mfi.isValid(); ++mfi) {
  //   // Finalize the mass matrix calculation.
  //   const Box box = mfi.validbox();
  //   const auto lo = lbound(box);
  //   const auto hi = ubound(box);

  //   Array4<RealMM> const& mmArr = nodeMM[mfi].array();

  //   // We only need the mass matrix on the physical nodes. But the first
  //   // layer
  //   // of the ghost nodes may contributes to the physical nodes below (ghost
  //   // node constributes as a sender). So, we need the '-1' and '+1' staff.
  //   const int iMin = lo.x - 1, jMin = lo.y - 1, kMin = nDim > 2 ? lo.z - 1 :
  //   0; const int iMax = hi.x + 1, jMax = hi.y + 1, kMax = nDim > 2 ? hi.z + 1
  //   : 0;

  //   int gps, gpr; // gp_send, gp_receive
  //   for (int k1 = kMin; k1 <= kMax; k1++)
  //     for (int j1 = jMin; j1 <= jMax; j1++)
  //       for (int i1 = iMin; i1 <= iMax; i1++) {
  //         const int kp = 2;
  //         const int kr = nDim > 2 ? k1 + kp - 1 : 0;
  //         if (kr > kMax || kr < kMin)
  //           continue;
  //         auto& datas0 = mmArr(i1, j1, k1);
  //         for (int jp = 0; jp < 3; jp++) {
  //           const int jr = j1 + jp - 1;
  //           if (jr > jMax || jr < jMin)
  //             continue;
  //           const int jpr = 2 - jp;
  //           for (int ip = 0; ip < 3; ip++) {
  //             const int ir = i1 + ip - 1;
  //             if (ir > iMax || ir < iMin)
  //               continue;
  //             const int ipr = 2 - ip;
  //             gpr = jpr * 3 + ipr;
  //             gps = 18 + jp * 3 + ip; // gps = kp*9+jp*3+kp

  //             Real* const datar = &(mmArr(ir, jr, kr)[gpr * 9]);
  //             const Real* const datas = &(datas0[gps * 9]);
  //             for (int idx = 0; idx < 9; idx++) {
  //               datar[idx] = datas[idx];
  //             } // idx
  //           } // kp
  //         } // jp
  //       } // k1
  // }
}
//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::calc_jhat(MultiFab& jHat,
                                                   MultiFab& nodeBMF, Real dt) {
  timing_func("Pts::calc_jhat");

  Real qdto2mc = charge / mass * 0.5 * dt;

  const int iLev = 0;
  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    Array4<Real const> const& nodeBArr = nodeBMF[pti].array();
    Array4<Real> const& jArr = jHat[pti].array();

    const AoS& particles = pti.GetArrayOfStructs();

    for (const auto& p : particles) {
      if (p.id() < 0)
        continue;

      // Print()<<"p = "<<p<<std::endl;
      const Real up = p.rdata(iup_);
      const Real vp = p.rdata(ivp_);
      const Real wp = p.rdata(iwp_);
      const Real qp = p.rdata(iqp_);

      //-----calculate interpolate coef begin-------------
      IntVect loIdx;
      RealVect dShift;

      find_node_index(p.pos(), Geom(iLev).ProbLo(), Geom(iLev).InvCellSize(),
                      loIdx, dShift);

      Real coef[2][2][2];
      linear_interpolation_coef(dShift, coef);
      //-----calculate interpolate coef end-------------

      Real Bxl = 0, Byl = 0, Bzl = 0; // should be bp[3];

      for (int kk = 0; kk < 2; ++kk)
        for (int jj = 0; jj < 2; ++jj)
          for (int ii = 0; ii < 2; ++ii) {
            Bxl += nodeBArr(loIdx[ix_] + ii, loIdx[iy_] + jj, loIdx[iz_] + kk,
                            ix_) *
                   coef[ii][jj][kk];
            Byl += nodeBArr(loIdx[ix_] + ii, loIdx[iy_] + jj, loIdx[iz_] + kk,
                            iy_) *
                   coef[ii][jj][kk];
            Bzl += nodeBArr(loIdx[ix_] + ii, loIdx[iy_] + jj, loIdx[iz_] + kk,
                            iz_) *
                   coef[ii][jj][kk];
          }

      const Real omx = qdto2mc * Bxl;
      const Real omy = qdto2mc * Byl;
      const Real omz = qdto2mc * Bzl;

      // end interpolation
      const Real omsq = (omx * omx + omy * omy + omz * omz);
      const Real denom = 1.0 / (1.0 + omsq);
      const Real udotOm = up * omx + vp * omy + wp * omz;

      {
        // jHat
        Real currents[3];

        {
          const Real coef1 = denom * qp;
          currents[ix_] = (up + (vp * omz - wp * omy + udotOm * omx)) * coef1;
          currents[iy_] = (vp + (wp * omx - up * omz + udotOm * omy)) * coef1;
          currents[iz_] = (wp + (up * omy - vp * omx + udotOm * omz)) * coef1;
        }

        for (int iVar = 0; iVar < nDim; iVar++)
          for (int kk = 0; kk < 2; ++kk)
            for (int jj = 0; jj < 2; ++jj)
              for (int ii = 0; ii < 2; ++ii) {
                jArr(loIdx[ix_] + ii, loIdx[iy_] + jj, loIdx[iz_] + kk, iVar) +=
                    coef[ii][jj][kk] * currents[iVar];
              }
      }

    } // for p
  }
}

//==========================================================
// Apply mirror BC for node-centred current at reflect and inflow domain faces.
// For reflect faces, specular symmetry zeroes normal current and doubles
// tangential current. For inflow faces, all components double to compensate
// for half-space node weighting.
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::apply_jhat_mirror(MultiFab& jHat,
                                                           int iLev) {
  if (!Geom(iLev).isAllPeriodic() && jHat.nGrow() > 0) {
    const Box& dom = Geom(iLev).Domain();
    const int nCompJ = jHat.nComp();
    for (MFIter mfi(jHat); mfi.isValid(); ++mfi) {
      const Box& bx = mfi.validbox();
      Array4<Real> const& jArr = jHat[mfi].array();

      for (int iDim = 0; iDim < nDim; ++iDim) {
        if (Geom(iLev).isPeriodic(iDim))
          continue;

        for (int side = 0; side < 2; ++side) {
          const bool isLo = (side == 0);
          const int faceBc = isLo ? bc.lo[iDim] : bc.hi[iDim];
          if (faceBc != ParticleBC::reflect && faceBc != ParticleBC::inflow)
            continue;

          // Upper boundary node is dom.bigEnd + 1 due to node-centering.
          const int domEdge = isLo ? dom.smallEnd(iDim) : dom.bigEnd(iDim) + 1;
          if ((isLo ? bx.smallEnd(iDim) : bx.bigEnd(iDim)) != domEdge)
            continue;

          const Box& fbx = mfi.fabbox();
          IntVect gs = fbx.smallEnd();
          IntVect ge = fbx.bigEnd();
          gs[iDim] = domEdge + (isLo ? -1 : 1);
          ge[iDim] = gs[iDim];
          const Box strip(gs, ge);

          const bool isReflect = (faceBc == ParticleBC::reflect);
          const int di = (iDim == 0) ? (isLo ? 1 : -1) : 0;
          const int dj = (iDim == 1) ? (isLo ? 1 : -1) : 0;
          const int dk = (iDim == 2) ? (isLo ? 1 : -1) : 0;

          ParallelFor(strip, nCompJ,
                      [=] AMREX_GPU_DEVICE(int i, int j, int k, int c) {
                        const int ei = i + di, ej = j + dj, ek = k + dk;
                        if (isReflect && c == iDim) {
                          jArr(ei, ej, ek, c) = 0.0;
                        } else {
                          jArr(ei, ej, ek, c) *= 2.0;
                        }
                        jArr(i, j, k, c) = 0.0;
                      });
        }
      }
    }
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
Real Particles<NStructReal, NStructInt>::calc_max_thermal_velocity(
    MultiFab& momentsMF) {

  constexpr Real c1over3 = 1. / 3;
  auto const& ma = momentsMF.const_arrays();

  Real uthMax = ParReduce(TypeList<ReduceOpMax>{}, TypeList<Real>{}, momentsMF,
                          IntVect(0), // zero ghost cells
                          [=] AMREX_GPU_DEVICE(int nb, int i, int j, int k)
                              noexcept -> GpuTuple<Real> {
                                Array4<Real const> const& arr = ma[nb];
                                Real rho = arr(i, j, k, iRho_);
                                if (rho == 0)
                                  return 0.0;
                                Real p =
                                    (arr(i, j, k, iPxx_) + arr(i, j, k, iPyy_) +
                                     arr(i, j, k, iPzz_)) *
                                    c1over3;
                                Real uth = sqrt(p / rho);
                                return uth;
                              });

  return uthMax;
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::convert_to_fluid_moments(
    Vector<MultiFab>& momentsMF) {

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    MultiFab tmpMF(momentsMF[iLev], make_alias, iRho_, iPyz_ - iRho_ + 1);
    tmpMF.mult(qomSign * get_mass(), tmpMF.nGrow());

    for (MFIter mfi(momentsMF[iLev]); mfi.isValid(); ++mfi) {
      FArrayBox& fab = momentsMF[iLev][mfi];
      const Box& box = mfi.fabbox();
      const Array4<Real>& arr = fab.array();

      const auto lo = lbound(box);
      const auto hi = ubound(box);

      for (int k = lo.z; k <= hi.z; ++k)
        for (int j = lo.y; j <= hi.y; ++j)
          for (int i = lo.x; i <= hi.x; ++i) {
            const Real rho = arr(i, j, k, iRho_);
            if (rho > 0) {
              const Real ux = arr(i, j, k, iUx_) / rho;
              const Real uy = arr(i, j, k, iUy_) / rho;
              const Real uz = arr(i, j, k, iUz_) / rho;
              arr(i, j, k, iPxx_) = arr(i, j, k, iPxx_) - rho * ux * ux;
              arr(i, j, k, iPyy_) = arr(i, j, k, iPyy_) - rho * uy * uy;
              arr(i, j, k, iPzz_) = arr(i, j, k, iPzz_) - rho * uz * uz;

              arr(i, j, k, iPxy_) = arr(i, j, k, iPxy_) - rho * ux * uy;
              arr(i, j, k, iPxz_) = arr(i, j, k, iPxz_) - rho * ux * uz;
              arr(i, j, k, iPyz_) = arr(i, j, k, iPyz_) - rho * uy * uz;
            }
          }
    }
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::update_position_to_half_stage(
    const MultiFab& nodeEMF, const MultiFab& nodeBMF, Real dt) {
  timing_func("Pts::update_position_to_half_stage");

  Real dtLoc = 0.5 * dt;

  const int iLev = 0;
  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    AoS& particles = pti.GetArrayOfStructs();

    const Box& bx = cell_status(iLev)[pti].box();
    const Array4<int const>& status = cell_status(iLev)[pti].array();

    const IntVect lowCorner = bx.smallEnd();
    const IntVect highCorner = bx.bigEnd();

    for (auto& p : particles) {
      if (p.id() < 0)
        continue;

      for (int iDim = 0; iDim < nDim; iDim++) {
        p.pos(iDim) += p.rdata(iup_ + iDim) * dtLoc;
      }

      // Mark for deletion
      if (reflect_or_delete_particle(p, status, lowCorner, highCorner, iLev)) {
        p.id() = -1;
      }
    } // for p
  } // for pti

  redistribute_particles();
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::mover(const Vector<MultiFab>& nodeE,
                                               const Vector<MultiFab>& nodeB,
                                               const Vector<MultiFab>& eBg,
                                               const Vector<MultiFab>& uBg,
                                               Real dt, Real dtNext) {
  if (is_neutral()) {
    neutral_mover(dt);
  } else {
    charged_particle_mover(nodeE, nodeB, eBg, uBg, dt, dtNext);
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::mover_cell_centered(
    const Vector<MultiFab>& centerE, const Vector<MultiFab>& centerB,
    const Vector<MultiFab>& eBg, const Vector<MultiFab>& uBg, Real dt,
    Real dtNext) {
  if (is_neutral()) {
    neutral_mover(dt);
  } else {
    charged_particle_mover_cell_centered(centerE, centerB, eBg, uBg, dt,
                                         dtNext);
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::charged_particle_mover(
    const Vector<MultiFab>& nodeE, const Vector<MultiFab>& nodeB,
    const Vector<MultiFab>& eBg, const Vector<MultiFab>& uBg, Real dt,
    Real dtNext) {
  timing_func("Pts::charged_particle_mover");

  const Real qdto2mc = charge / mass * 0.5 * dt;
  Real dtLoc = 0.5 * (dt + dtNext);

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      const Array4<Real const>& nodeEArr = nodeE[iLev][pti].array();
      const Array4<Real const>& nodeBArr = nodeB[iLev][pti].array();

      const Box& bx = cell_status(iLev)[pti].box();
      const Array4<int const>& status = cell_status(iLev)[pti].array();

      const IntVect lowCorner = bx.smallEnd();
      const IntVect highCorner = bx.bigEnd();

      AoS& particles = pti.GetArrayOfStructs();

      const Dim3 lo = init_dim3(0);
      const Dim3 hi = init_dim3(1);

      for (auto& p : particles) {
        if (p.id() < 0)
          continue;

        Real up = p.rdata(iup_);
        Real vp = p.rdata(ivp_);
        Real wp = p.rdata(iwp_);
        const Real xp = p.pos(ix_);
        const Real yp = p.pos(iy_);
        const Real zp = nDim > 2 ? p.pos(iz_) : 0;

        //-----calculate interpolate coef begin-------------
        IntVect loIdx;
        RealVect dShift;

        find_node_index(p.pos(), Geom(iLev).ProbLo(), Geom(iLev).InvCellSize(),
                        loIdx, dShift);

        Real coef[2][2][2];
        linear_interpolation_coef(dShift, coef);
        //-----calculate interpolate coef end-------------

        Real bp[3] = { 0, 0, 0 };
        Real ep[3] = { 0, 0, 0 };
        Real u0p[3] = { 0, 0, 0 };
        for (int k = lo.z; k <= hi.z; ++k)
          for (int j = lo.y; j <= hi.y; ++j)
            for (int i = lo.x; i <= hi.x; ++i) {
              IntVect ijk = { AMREX_D_DECL(loIdx[ix_] + i, loIdx[iy_] + j,
                                           loIdx[iz_] + k) };

              const Real& c0 = coef[i][j][k];
              for (int iDim = 0; iDim < nDim3; iDim++) {
                bp[iDim] += nodeBArr(ijk, iDim) * c0;
                ep[iDim] += nodeEArr(ijk, iDim) * c0;
              }
            }

        up = up - u0p[ix_];
        vp = vp - u0p[iy_];
        wp = wp - u0p[iz_];

        const Real omx = qdto2mc * bp[ix_];
        const Real omy = qdto2mc * bp[iy_];
        const Real omz = qdto2mc * bp[iz_];

        // end interpolation
        const Real omsq = (omx * omx + omy * omy + omz * omz);
        const Real denom = 1.0 / (1.0 + omsq);
        // solve the position equation
        const Real ut = up + qdto2mc * ep[ix_];
        const Real vt = vp + qdto2mc * ep[iy_];
        const Real wt = wp + qdto2mc * ep[iz_];
        // const pfloat udotb = ut * Bxl + vt * Byl + wt * Bzl;
        const Real udotOm = ut * omx + vt * omy + wt * omz;
        // solve the velocity equation
        const Real uavg = (ut + (vt * omz - wt * omy + udotOm * omx)) * denom;
        const Real vavg = (vt + (wt * omx - ut * omz + udotOm * omy)) * denom;
        const Real wavg = (wt + (ut * omy - vt * omx + udotOm * omz)) * denom;

        Real unp1 = 2.0 * uavg - up + u0p[ix_];
        Real vnp1 = 2.0 * vavg - vp + u0p[iy_];
        Real wnp1 = 2.0 * wavg - wp + u0p[iz_];

        p.rdata(iup_) = unp1;
        p.rdata(ivp_) = vnp1;
        p.rdata(iwp_) = wnp1;

        if (pMode == PartMode::PIC && imu_ < NStructReal) {
          // Note: bp should be calculated at the new position. Now, bp at the
          // old position is used to save the calculation.
          p.rdata(imu_) = cosine(p, bp);
        }

        p.pos(ix_) = xp + unp1 * dtLoc;
        p.pos(iy_) = yp + vnp1 * dtLoc;
        if (nDim > 2)
          p.pos(iz_) = zp + wnp1 * dtLoc;

        // Apply boundary condition (absorb: delete; reflect: mirror).
        if (reflect_or_delete_particle(p, status, lowCorner, highCorner,
                                       iLev)) {
          p.id() = -1;
        }
      } // for p
    } // for pti
  }
}

//==========================================================
// Cell-centred Boris push. The E and B are gathered from cell fields. The
// gather is a plain cell-centred trilinear interpolation.
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::charged_particle_mover_cell_centered(
    const Vector<MultiFab>& centerE, const Vector<MultiFab>& centerB,
    const Vector<MultiFab>& eBg, const Vector<MultiFab>& uBg, Real dt,
    Real dtNext) {
  timing_func("Pts::charged_particle_mover_cell_centered");

  const Real qdto2mc = charge / mass * 0.5 * dt;
  Real dtLoc = 0.5 * (dt + dtNext);

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      const Array4<Real const>& centerEArr = centerE[iLev][pti].array();
      const Array4<Real const>& centerBArr = centerB[iLev][pti].array();

      const Box& bx = cell_status(iLev)[pti].box();
      const Array4<int const>& status = cell_status(iLev)[pti].array();

      const IntVect lowCorner = bx.smallEnd();
      const IntVect highCorner = bx.bigEnd();

      AoS& particles = pti.GetArrayOfStructs();

      const Dim3 lo = init_dim3(0);
      const Dim3 hi = init_dim3(1);

      for (auto& p : particles) {
        if (p.id() < 0)
          continue;

        Real up = p.rdata(iup_);
        Real vp = p.rdata(ivp_);
        Real wp = p.rdata(iwp_);
        const Real xp = p.pos(ix_);
        const Real yp = p.pos(iy_);
        const Real zp = nDim > 2 ? p.pos(iz_) : 0;

        //-----calculate interpolate coef begin-------------
        IntVect loIdx;
        RealVect dShift;
        find_cell_index(p.pos(), Geom(iLev).ProbLo(), Geom(iLev).InvCellSize(),
                        loIdx, dShift);

        // Plain cell-centred trilinear gather. The linear weights couple cells
        // loIdx and loIdx+1 (offsets 0 and 1); the 3x3x3 coef array is zero for
        // the unused offset-2 entry.
        Real coef[3][3][3];
        Real coefLin[2][2][2];
        linear_interpolation_coef(dShift, coefLin);
        for (int k = 0; k <= 2; ++k)
          for (int j = 0; j <= 2; ++j)
            for (int i = 0; i <= 2; ++i)
              coef[i][j][k] =
                  (i <= 1 && j <= 1 && k <= 1) ? coefLin[i][j][k] : 0.0;
        //-----calculate interpolate coef end-------------

        Real bp[3] = { 0, 0, 0 };
        Real ep[3] = { 0, 0, 0 };
        Real u0p[3] = { 0, 0, 0 };
        for (int k = lo.z; k <= hi.z; ++k)
          for (int j = lo.y; j <= hi.y; ++j)
            for (int i = lo.x; i <= hi.x; ++i) {
              IntVect ijk = { AMREX_D_DECL(loIdx[ix_] + i, loIdx[iy_] + j,
                                           loIdx[iz_] + k) };

              const Real& c0 = coef[i - lo.x][j - lo.y][k - lo.z];
              for (int iDim = 0; iDim < nDim3; iDim++) {
                bp[iDim] += centerBArr(ijk, iDim) * c0;
                ep[iDim] += centerEArr(ijk, iDim) * c0;
              }
            }

        up = up - u0p[ix_];
        vp = vp - u0p[iy_];
        wp = wp - u0p[iz_];

        const Real omx = qdto2mc * bp[ix_];
        const Real omy = qdto2mc * bp[iy_];
        const Real omz = qdto2mc * bp[iz_];

        // end interpolation
        const Real omsq = (omx * omx + omy * omy + omz * omz);
        const Real denom = 1.0 / (1.0 + omsq);
        // solve the position equation
        const Real ut = up + qdto2mc * ep[ix_];
        const Real vt = vp + qdto2mc * ep[iy_];
        const Real wt = wp + qdto2mc * ep[iz_];
        // const pfloat udotb = ut * Bxl + vt * Byl + wt * Bzl;
        const Real udotOm = ut * omx + vt * omy + wt * omz;
        // solve the velocity equation
        const Real uavg = (ut + (vt * omz - wt * omy + udotOm * omx)) * denom;
        const Real vavg = (vt + (wt * omx - ut * omz + udotOm * omy)) * denom;
        const Real wavg = (wt + (ut * omy - vt * omx + udotOm * omz)) * denom;

        Real unp1 = 2.0 * uavg - up + u0p[ix_];
        Real vnp1 = 2.0 * vavg - vp + u0p[iy_];
        Real wnp1 = 2.0 * wavg - wp + u0p[iz_];

        p.rdata(iup_) = unp1;
        p.rdata(ivp_) = vnp1;
        p.rdata(iwp_) = wnp1;

        if (pMode == PartMode::PIC && imu_ < NStructReal) {
          // Note: bp should be calculated at the new position. Now, bp at the
          // old position is used to save the calculation.
          p.rdata(imu_) = cosine(p, bp);
        }

        p.pos(ix_) = xp + unp1 * dtLoc;
        p.pos(iy_) = yp + vnp1 * dtLoc;
        if (nDim > 2)
          p.pos(iz_) = zp + wnp1 * dtLoc;

        // Apply boundary condition (absorb: delete; reflect: mirror).
        if (reflect_or_delete_particle(p, status, lowCorner, highCorner,
                                       iLev)) {
          p.id() = -1;
        }
      } // for p
    } // for pti
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::select_particle(
    Vector<std::array<int, 3> >& selectParticleIn) {

  timing_func("Pts::select_particle");

  int numParticlesFoundLocal = 0, numParticlesFoundTotal = 0;

  // output files
  std::string filename = "select_particle_out_sp" + std::to_string(speciesID) +
                         "_pe" + std::to_string(ParallelDescriptor::MyProc()) +
                         ".dat";
  std::ofstream outFile;
  outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
  outFile.precision(12);

  // loop through particles
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      AoS& particles = pti.GetArrayOfStructs();
      for (auto& p : particles) {
        if (p.id() < 0)
          continue;
        if (p.idata(iSupID_) < 0)
          continue;

        for (auto& currentTargetParticle : selectParticleIn) {
          if (p.cpu() == currentTargetParticle[0] &&
              p.idata(iSupID_) == currentTargetParticle[1] &&
              p.id() == currentTargetParticle[2]) {
            numParticlesFoundLocal++;
            outFile << p.cpu() << " " << p.idata(iSupID_) << " " << p.id()
                    << " " << p.pos(ix_) << " " << p.pos(iy_) << " "
                    << p.pos(iz_) << " " << p.rdata(iup_) << " "
                    << p.rdata(ivp_) << " " << p.rdata(iwp_) << "\n";
          }
        }
      }
    }
  }
  outFile.close();
  MPI_Reduce(&numParticlesFoundLocal, &numParticlesFoundTotal, 1, MPI_INT,
             MPI_SUM, ParallelDescriptor::IOProcessorNumber(),
             ParallelDescriptor::Communicator());
  Print() << "select particle finished... " << numParticlesFoundTotal
          << "particles found..." << std::endl;
  amrex::Abort("Abort: select particle finished!");
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::neutral_mover(Real dt) {
  timing_func("Pts::neutral_mover");

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      AoS& particles = pti.GetArrayOfStructs();

      const Box& bx = cell_status(iLev)[pti].box();
      const Array4<int const>& status = cell_status(iLev)[pti].array();

      const IntVect lowCorner = bx.smallEnd();
      const IntVect highCorner = bx.bigEnd();
      for (auto& p : particles) {
        if (p.id() < 0)
          continue;

        const Real up = p.rdata(iup_);
        const Real vp = p.rdata(ivp_);
        const Real wp = p.rdata(iwp_);
        const Real xp = p.pos(ix_);
        const Real yp = p.pos(iy_);
        const Real zp = p.pos(iz_);

        p.pos(ix_) = xp + up * dt;
        p.pos(iy_) = yp + vp * dt;
        p.pos(iz_) = zp + wp * dt;

        // Apply boundary condition (absorb: delete; reflect: mirror).
        if (reflect_or_delete_particle(p, status, lowCorner, highCorner,
                                       iLev)) {
          p.id() = -1;
        }
      } // for p
    } // for pti
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::divE_correct_position(
    const amrex::Vector<MultiFab>& phiMF, int iLev) {
  timing_func("Pts:divE_correct_position");

  const Real sign = charge / fabs(charge);
  const Real epsLimit = 0.1;
  Real epsMax = 0;

  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    Array4<Real const> const& phiArr = phiMF[iLev][pti].array();
    const Array4<int const>& status = cell_status(iLev)[pti].array();

    AoS& particles = pti.GetArrayOfStructs();

    const Box& bx = cell_status(iLev)[pti].box();
    const IntVect lowCorner = bx.smallEnd();
    const IntVect highCorner = bx.bigEnd();

    for (auto& p : particles) {
      if (p.id() == -1 ||
          is_outside_active_region(p, status, lowCorner, highCorner, iLev)) {
        p.id() = -1;
        continue;
      }

      if (skip_particle_for_dive_cleaning(p.pos(), Geom(iLev), iLev, status) &&
          n_lev() > 1) {
        continue;
      }

      IntVect loIdx;
      RealVect dShift;
      find_cell_index(p.pos(), Geom(iLev).ProbLo(), Geom(iLev).InvCellSize(),
                      loIdx, dShift);

      // Since the boundary condition for solving phi is not perfect,
      // correcting particles that are close to the boundaries may produce
      // artificial oscillations, which are seen in Earth's magnetotail
      // simulations. So, it is better to skip the boundary physical cells.
      bool isBoundaryPhysicalCell = false;
      for (int iz = 0; iz <= 1; iz++)
        for (int iy = 0; iy <= 1; iy++)
          for (int ix = 0; ix <= 1; ix++) {
            IntVect ijk = { AMREX_D_DECL(loIdx[ix_] + ix, loIdx[iy_] + iy,
                                         loIdx[iz_] + iz) };
            if (bit::is_lev_boundary(status(ijk)))
              isBoundaryPhysicalCell = true;
          }
      if (isBoundaryPhysicalCell && iLev == 0)
        continue;

      {
        Real weights_IIID[2][2][2][nDim3];
        //----- Mass matrix calculation begin--------------
        const Real xi0 = dShift[ix_] * dx[iLev][ix_];
        const Real eta0 = dShift[iy_] * dx[iLev][iy_];
        const Real zeta0 = nDim > 2 ? dShift[iz_] * dx[iLev][iz_] : 0;
        const Real xi1 = dx[iLev][ix_] - xi0;
        const Real eta1 = dx[iLev][iy_] - eta0;
        const Real zeta1 = nDim > 2 ? dx[iLev][iz_] - zeta0 : 1;

        const Real zeta02Vol = zeta0 * invVol[iLev];
        const Real zeta12Vol = zeta1 * invVol[iLev];
        const Real eta02Vol = eta0 * invVol[iLev];
        const Real eta12Vol = eta1 * invVol[iLev];

        weights_IIID[1][1][1][ix_] = eta0 * zeta02Vol;
        weights_IIID[1][1][1][iy_] = xi0 * zeta02Vol;
        weights_IIID[1][1][1][iz_] = xi0 * eta02Vol;

        // xi0*eta0*zeta1*invVol[iLev];
        weights_IIID[1][1][0][ix_] = eta0 * zeta12Vol;
        weights_IIID[1][1][0][iy_] = xi0 * zeta12Vol;
        weights_IIID[1][1][0][iz_] = -xi0 * eta02Vol;

        // xi0*eta1*zeta0*invVol[iLev];
        weights_IIID[1][0][1][ix_] = eta1 * zeta02Vol;
        weights_IIID[1][0][1][iy_] = -xi0 * zeta02Vol;
        weights_IIID[1][0][1][iz_] = xi0 * eta12Vol;

        // xi0*eta1*zeta1*invVol[iLev];
        weights_IIID[1][0][0][ix_] = eta1 * zeta12Vol;
        weights_IIID[1][0][0][iy_] = -xi0 * zeta12Vol;
        weights_IIID[1][0][0][iz_] = -xi0 * eta12Vol;

        // xi1*eta0*zeta0*invVol[iLev];
        weights_IIID[0][1][1][ix_] = -eta0 * zeta02Vol;
        weights_IIID[0][1][1][iy_] = xi1 * zeta02Vol;
        weights_IIID[0][1][1][iz_] = xi1 * eta02Vol;

        // xi1*eta0*zeta1*invVol[iLev];
        weights_IIID[0][1][0][ix_] = -eta0 * zeta12Vol;
        weights_IIID[0][1][0][iy_] = xi1 * zeta12Vol;
        weights_IIID[0][1][0][iz_] = -xi1 * eta02Vol;

        // xi1*eta1*zeta0*invVol[iLev];
        weights_IIID[0][0][1][ix_] = -eta1 * zeta02Vol;
        weights_IIID[0][0][1][iy_] = -xi1 * zeta02Vol;
        weights_IIID[0][0][1][iz_] = xi1 * eta12Vol;

        // xi1*eta1*zeta1*invVol[iLev];
        weights_IIID[0][0][0][ix_] = -eta1 * zeta12Vol;
        weights_IIID[0][0][0][iy_] = -xi1 * zeta12Vol;
        weights_IIID[0][0][0][iz_] = -xi1 * eta12Vol;

        RealVect eps_D = { AMREX_D_DECL(0, 0, 0) };

        // Do not shift along z direction for both 2D and fake 2D cases.
        int nD = isFake2D ? 2 : nDim;

        Box subBox(IntVect(0), IntVect(1));
        ParallelFor(subBox, [&](int i, int j, int k) noexcept {
          IntVect ijk = { AMREX_D_DECL(i, j, k) };
          const Real coef = phiArr(loIdx + ijk);
          for (int iDim = 0; iDim < nD; iDim++) {
            eps_D[iDim] += coef * weights_IIID[i][j][k][iDim];
          }
        });

        for (int iDim = 0; iDim < nDim; iDim++)
          eps_D[iDim] *= sign * fourPI;

        Real eps_D_dot_invDx_Max = 0.0;
        for (int iDim = 0; iDim < nDim; iDim++) {
          if (fabs(eps_D[iDim] * invDx[iLev][iDim]) > eps_D_dot_invDx_Max) {
            eps_D_dot_invDx_Max = fabs(eps_D[iDim] * invDx[iLev][iDim]);
          }
        }

        if (eps_D_dot_invDx_Max > epsLimit) {
          // If eps_D is too large, the underlying assumption of the particle
          // correction method will be not valid. Comparing each exp_D
          // component instead of the length dl saves the computational time.
          const Real dl = eps_D.vectorLength();
          const Real ratio = epsLimit * dx[iLev][ix_] / dl;
          for (int iDim = 0; iDim < nDim; iDim++)
            eps_D[iDim] *= ratio;
        }

        for (int iDim = 0; iDim < nDim; iDim++) {
          if (fabs(eps_D[iDim] * invDx[iLev][iDim]) > epsMax)
            epsMax = fabs(eps_D[iDim] * invDx[iLev][iDim]);

          p.pos(iDim) += eps_D[iDim];
        }

        if (is_outside_active_region(p, status, lowCorner, highCorner, iLev)) {
          // Do not allow moving particles from physical cells to ghost cells
          // during divE correction.
          for (int iDim = 0; iDim < nDim; iDim++) {
            p.pos(iDim) -= eps_D[iDim];
          }

          // p.id() = -1;
        }
      }
    } // for p
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::limit_weight(Real maxRatio,
                                                      bool seperateVelocity) {
  timing_func("Pts::limit_weight");

  if (maxRatio <= 1)
    return;

  IntVect iv(1);
  if (!(do_tiling && tile_size == iv))
    return;

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {

      Vector<ParticleType> newparticles;

      auto& pTile = get_particle_tile(iLev, pti);
      AoS& particles = pti.GetArrayOfStructs();

      // Sort the particles first to make sure the results
      // are the same for different number of processors
      // TODO C++20 support with better syntax:
      // std::ranges::sort(particles, compare_two_parts);
      std::sort(particles.begin(), particles.end(), compare_two_parts);

      Real totalMass = 0;
      Real totalMoment[nDim3] = { 0, 0, 0 };
      for (auto& p : particles) {
        totalMass += fabs(p.rdata(iqp_));
        for (int i = 0; i < nDim3; ++i)
          totalMoment[i] += fabs(p.rdata(iqp_) * p.rdata(iup_ + i));
      }
      Real avg = totalMass / particles.size();

      // Real maxWeight = avg + maxRatio * vars;
      Real maxWeight = avg * maxRatio;

      if (seperateVelocity) {
        Box bx = pti.tilebox();
        set_random_seed(iLev, bx.smallEnd(), IntVect(444));
        Vector<ParticleType*> pold;
        for (size_t ip = 0; ip < particles.size(); ip++) {
          Real qp1 = particles[ip].rdata(iqp_);
          if (fabs(qp1) < maxWeight)
            continue;
          pold.push_back(&(particles[ip]));
        }
        split_particles_by_velocity(pold, newparticles);
      } else {

        const auto lo = lbound(pti.tilebox());
        const auto hi = ubound(pti.tilebox());

        const Real xMin = Geom(iLev).LoEdge(lo.x, ix_) +
                          Geom(iLev).CellSize()[ix_] * 1e-10,
                   xMax = Geom(iLev).HiEdge(hi.x, ix_) -
                          Geom(iLev).CellSize()[ix_] * 1e-10;

        const Real yMin = Geom(iLev).LoEdge(lo.y, iy_) +
                          Geom(iLev).CellSize()[iy_] * 1e-10,
                   yMax = Geom(iLev).HiEdge(hi.y, iy_) -
                          Geom(iLev).CellSize()[iy_] * 1e-10;

        const Real zMin = nDim > 2 ? Geom(iLev).LoEdge(lo.z, iz_) +
                                         Geom(iLev).CellSize()[iz_] * 1e-10
                                   : 0.0,
                   zMax = nDim > 2 ? Geom(iLev).HiEdge(hi.z, iz_) -
                                         Geom(iLev).CellSize()[iz_] * 1e-10
                                   : 0.0;

        if (is_neutral()) {
          Box bx = pti.tilebox();
          set_random_seed(iLev, bx.smallEnd(), IntVect(999));
        }

        for (auto& p : particles) {
          Real qp1 = p.rdata(iqp_);
          if (fabs(qp1) < maxWeight)
            continue;

          Real up1 = p.rdata(iup_);
          Real vp1 = p.rdata(ivp_);
          Real wp1 = p.rdata(iwp_);

          Real xp1 = p.pos(ix_);
          Real yp1 = p.pos(iy_);
          Real zp1 = p.pos(iz_);

          int nNew = is_neutral() ? 7 : 1;

          p.rdata(iqp_) = qp1 / (nNew + 1);

          for (int iNew = 0; iNew < nNew; iNew++) {
            auto pnew = make_particle();
            set_ids(pnew);

            Real xp2 = xp1 + (xMax - xMin) * (randNum() - 0.5);
            Real yp2 = yp1 + (yMax - yMin) * (randNum() - 0.5);
            Real zp2 = zp1 + (zMax - zMin) * (randNum() - 0.5);

            xp2 = std::clamp(xp2, xMin, xMax);
            yp2 = std::clamp(yp2, yMin, yMax);
            zp2 = std::clamp(zp2, zMin, zMax);

            pnew.pos(ix_) = xp2;
            pnew.pos(iy_) = yp2;
            pnew.pos(iz_) = zp2;
            pnew.rdata(iup_) = up1;
            pnew.rdata(ivp_) = vp1;
            pnew.rdata(iwp_) = wp1;

            pnew.rdata(iqp_) = qp1 / (nNew + 1);
            newparticles.push_back(pnew);
          }
        }
      }

      for (auto& p : newparticles) {
        pTile.push_back(p);
      }
    }
  }
}

template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::split_particles_by_velocity(
    Vector<ParticleType*>& plist, Vector<ParticleType>& newparticles) {

  if (plist.size() < 2)
    return;

  const int nCell = 8;
  // Assign the particle IDs to the corresponding velocity space cells.
  Vector<int> phasePartIdx_III[nCell][nCell][nCell];

  // Velocity domain range.
  Real velMin_D[nDim3] = { 1e9, 1e9, 1e9 },
       velMax_D[nDim3] = { -1e9, -1e9, -1e9 };

  for (int pid = 0; pid < plist.size(); pid++) {
    auto& pcl = *plist[pid];
    for (int iDir = 0; iDir < nDim3; iDir++) {
      if (pcl.rdata(iup_ + iDir) > velMax_D[iDir])
        velMax_D[iDir] = pcl.rdata(iup_ + iDir);
      if (pcl.rdata(iup_ + iDir) < velMin_D[iDir])
        velMin_D[iDir] = pcl.rdata(iup_ + iDir);
    }
  }

  Real dvMax = 0;
  for (int iDir = 0; iDir < nDim3; iDir++) {
    Real dv = velMax_D[iDir] - velMin_D[iDir];
    const Real vref = 0.5 * (fabs(velMax_D[iDir]) + fabs(velMin_D[iDir]));
    if (dv < 1e-6 * vref)
      dv = 1e-6 * vref;
    velMax_D[iDir] += 1e-3 * dv;
    velMin_D[iDir] -= 1e-3 * dv;
    dv = velMax_D[iDir] - velMin_D[iDir];

    if (dv > dvMax)
      dvMax = dv;
  }

  Real dvCell = dvMax == 0 ? 1e-9 : dvMax / nCell;
  Real invDv = 1 / dvCell;

  int iCell_D[nDim3];
  for (int pid = 0; pid < plist.size(); pid++) {
    auto& pcl = *plist[pid];
    for (int iDim = 0; iDim < nDim3; iDim++) {
      iCell_D[iDim] = fastfloor((pcl.rdata(iDim) - velMin_D[iDim]) * invDv);
    }

    phasePartIdx_III[iCell_D[ix_]][iCell_D[iy_]][iCell_D[iz_]].push_back(pid);
  }

  Vector<std::array<int, 3> > morton_idx(pow(nCell, 3));

  for (int iu = 0; iu < nCell; iu++)
    for (int iv = 0; iv < nCell; iv++)
      for (int iw = 0; iw < nCell; iw++) {
        morton_idx[encode_morton_3d(iu, iv, iw)] = { iu, iv, iw };
      }

  Vector<ParticleType*> p_morton;

  for (int i = 0; i < morton_idx.size(); ++i) {
    int iu = morton_idx[i][0];
    int iv = morton_idx[i][1];
    int iw = morton_idx[i][2];

    // printf("1 iu = %d iv = %d iw = %d\n", iu, iv, iw);
    for (int ip = 0; ip < phasePartIdx_III[iu][iv][iw].size(); ip++) {
      p_morton.push_back(plist[phasePartIdx_III[iu][iv][iw][ip]]);
    }
  }

  int nPair = floor(p_morton.size() / 2.0);
  for (int ip = 0; ip < nPair * 2; ip += 2) {
    ParticleType& p1 = *p_morton[ip];
    ParticleType& p2 = *p_morton[ip + 1];
    auto p3 = make_particle();
    auto p4 = make_particle();

    Real du = p1.rdata(iup_) - p2.rdata(iup_);
    Real dv = p1.rdata(ivp_) - p2.rdata(ivp_);
    Real dw = p1.rdata(iwp_) - p2.rdata(iwp_);
    Real dspeed = sqrt(du * du + dv * dv + dw * dw);

    if (dspeed / dvCell > 2)
      continue;

    bool doSucceed = split_by_seperate_velocity(p1, p2, p3, p4);
    if (doSucceed) {
      newparticles.push_back(p3);
      newparticles.push_back(p4);
    }
  }
}

template <int NStructReal, int NStructInt>
bool Particles<NStructReal, NStructInt>::split_by_seperate_velocity(
    ParticleType& p1, ParticleType& p2, ParticleType& p3, ParticleType& p4) {
  // AllPrint() << "Old: p1 = " << p1 << std::endl;
  // AllPrint() << "Old: p2 = " << p2 << std::endl;

  Real mt = p1.rdata(iqp_) + p2.rdata(iqp_);
  Real wavg = mt / 4.0;

  // Calculate the average velocity and total energy.
  Real et = 0, uavg2 = 0;
  Real u[nDim3], du1[nDim3], du2[nDim3];
  for (int i = 0; i < nDim3; ++i) {
    u[i] = (p1.rdata(iqp_) * p1.rdata(iup_ + i) +
            p2.rdata(iqp_) * p2.rdata(iup_ + i)) /
           mt;

    et += 0.5 * p1.rdata(iqp_) * pow(p1.rdata(iup_ + i), 2);
    et += 0.5 * p2.rdata(iqp_) * pow(p2.rdata(iup_ + i), 2);

    uavg2 += pow(u[i], 2);

    // Get the direction of du1.
    du1[i] = p1.rdata(iup_ + i) - p2.rdata(iup_ + i);
  }

  Real du1Amp = l2_norm(du1, nDim3);
  if (du1Amp < 1e-16) {
    // If p1 and p2 have essentially the same velocity, do not split them. But
    // why the velocity difference can be so small?
    // A: with billions of particles, it can happen. I have done some
    // investigation, and it does not look like a bug.
    return false;
  }

  // The amplitude of du1 and du2.
  Real duAmp2 = et / (2 * wavg) - uavg2;
  if (duAmp2 < 0) {
    // Q: Why duAmp2 can be negative?
    // A: Rounding error.
    return false;
  }
  Real duAmp = sqrt(duAmp2);

  // Scale the amplitude of du1
  Real scale = duAmp / du1Amp;
  for (int i = 0; i < nDim3; ++i) {
    du1[i] *= scale;
  }

  {
    // Get the direction of du2
    Real utmp[nDim3];
    const Real r1 = randNum();
    const Real r2 = randNum();
    random_vector(r1, r2, utmp);

    // Correct the amplitide of du2
    for (int i = 0; i < nDim3; ++i) {
      du2[i] = utmp[i] * duAmp;
    }
  }

  // auto p_energy = [](const ParticleType& p) {
  //   Real energy = 0;
  //   for (int i = 0; i < nDim3; ++i) {
  //     energy += 0.5 * p.rdata(iqp_) * pow(p.rdata(iup_ + i), 2);
  //   }
  //   return energy;
  // };

  // Real eold = p_energy(p1) + p_energy(p2);
  // Real mold[3];
  // for (int i = 0; i < nDim3; ++i) {
  //   mold[i] = p1.rdata(iqp_) * p1.rdata(iup_ + i) +
  //             p2.rdata(iqp_) * p2.rdata(iup_ + i);
  // }

  set_ids(p3);
  set_ids(p4);

  p1.rdata(iqp_) = wavg;
  p2.rdata(iqp_) = wavg;
  p3.rdata(iqp_) = wavg;
  p4.rdata(iqp_) = wavg;

  for (int i = 0; i < nDim3; ++i) {
    p1.rdata(iup_ + i) = u[i] + du1[i];
    p2.rdata(iup_ + i) = u[i] - du1[i];

    p3.rdata(iup_ + i) = u[i] + du2[i];
    p4.rdata(iup_ + i) = u[i] - du2[i];
  }

  for (int i = 0; i < nDim; ++i) {
    p3.pos(i) = p1.pos(i);
    p4.pos(i) = p2.pos(i);
  }

  // Real enew = p_energy(p1) + p_energy(p2) + p_energy(p3) + p_energy(p4);
  // Real mnew[3];
  // for (int i = 0; i < nDim3; ++i) {
  //   mnew[i] = p1.rdata(iqp_) * p1.rdata(iup_ + i) +
  //             p2.rdata(iqp_) * p2.rdata(iup_ + i) +
  //             p3.rdata(iqp_) * p3.rdata(iup_ + i) +
  //             p4.rdata(iqp_) * p4.rdata(iup_ + i);
  // }

  // AllPrint() << "eold = " << eold << " enew = " << enew
  //            << " eold - enew = " << eold - enew << " mold - mnew "
  //            << mold[0] - mnew[0] << " " << mold[1] - mnew[1] << " "
  //            << mold[2] - mnew[2] << std::endl;

  // AllPrint() << "New: p1 = " << p1 << std::endl;
  // AllPrint() << "New: p2 = " << p2 << std::endl;
  // AllPrint() << "New: p3 = " << p3 << std::endl;
  // AllPrint() << "New: p4 = " << p4 << std::endl;

  return true;
}
//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::split_new(Real limit,
                                                   bool seperateVelocity) {
  timing_func("Pts::split");

  const int nInitial = product(nPartPerCell);

  IntVect iv = { AMREX_D_DECL(1, 1, 1) };
  if (!(do_tiling && tile_size == iv))
    return;

  for (int iLev = 0; iLev < n_lev(); iLev++) {

    const Real vol = dx[iLev].product();
    const Real vacuumMass = vacuum * vol;

    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      Real dl = 0.1 * Geom(iLev).CellSize()[ix_] / (nPartPerCell.max());
      int nLowerLimit = nInitial * limit;
      int nGoal = nInitial;

      if (doPreSplitting) {
        const Array4<int const>& status = cell_status(iLev)[pti].array();
        Box bx = pti.tilebox();
        IntVect ibx = bx.smallEnd();
        if (bit::is_refined_neighbour(status(ibx))) {
          nLowerLimit = nLowerLimit * (pow(get_ref_ratio(iLev).max(), nDim));
          nGoal = nGoal * (pow(get_ref_ratio(iLev).max(), nDim));
          dl = dl / (get_ref_ratio(iLev).max());
        }
      }

      Vector<ParticleType> newparticles;

      auto& pTile = get_particle_tile(iLev, pti);
      AoS& particles = pTile.GetArrayOfStructs();

      const int nPartOrig = particles.size();
      if (nPartOrig > nLowerLimit)
        continue;

      const int nSplit =
          nGoal - nPartOrig > nPartOrig ? nPartOrig : nGoal - nPartOrig;
      Real totalMass = 0;
      for (auto& p : particles) {
        // So far, the vacuum limit is designed for OH-PT neutrals only. It is
        // not clear how should it be done for PIC, where electrons and ions
        // have different mass. --Yuxi
        totalMass += qomSign * p.rdata(iqp_);
      }
      if (totalMass < vacuumMass)
        continue;

      // Find the 'heaviest' nNew particles by sorting the weight
      // (charge).-----

      // Sort the particles by the location first to make sure the results
      // are the same for different number of processors
      std::sort(particles.begin(), particles.end(), compare_two_parts);

      // Sort the particles by the weight in decending order.
      std::sort(particles.begin(), particles.end(),
                [](const ParticleType& pl, const ParticleType& pr) {
                  const Real ql = fabs(pl.rdata(iqp_));
                  const Real qr = fabs(pr.rdata(iqp_));
                  if (fabs(ql - qr) > 1e-9 * (ql + qr)) {
                    return ql > qr;
                  }

                  if (fabs(pl.pos(ix_) - pr.pos(ix_)) >
                      1e-9 * (fabs(pl.pos(ix_)) + fabs(pr.pos(ix_)))) {
                    return pl.pos(ix_) > pr.pos(ix_);
                  }
                  return false;
                });
      //----------------------------------------------------------------

      const auto lo = lbound(pti.tilebox());
      const auto hi = ubound(pti.tilebox());

      const Real xMin = Geom(iLev).LoEdge(lo.x, ix_) +
                        Geom(iLev).CellSize()[ix_] * 1e-10,
                 xMax = Geom(iLev).HiEdge(hi.x, ix_) -
                        Geom(iLev).CellSize()[ix_] * 1e-10;

      const Real yMin = Geom(iLev).LoEdge(lo.y, iy_) +
                        Geom(iLev).CellSize()[iy_] * 1e-10,
                 yMax = Geom(iLev).HiEdge(hi.y, iy_) -
                        Geom(iLev).CellSize()[iy_] * 1e-10;

      const Real zMin = nDim > 2 ? Geom(iLev).LoEdge(lo.z, iz_) +
                                       Geom(iLev).CellSize()[iz_] * 1e-10
                                 : 0.0,
                 zMax = nDim > 2 ? Geom(iLev).HiEdge(hi.z, iz_) -
                                       Geom(iLev).CellSize()[iz_] * 1e-10
                                 : 0.0;

      if (is_neutral() || seperateVelocity) {
        Box bx = pti.tilebox();
        set_random_seed(iLev, bx.smallEnd(), IntVect(888));
      }

      if (seperateVelocity) {
        Vector<ParticleType*> pold;
        for (int ip = 0; ip < nSplit; ip++) {
          pold.push_back(&(particles[ip]));
        }
        split_particles_by_velocity(pold, newparticles);
      } else {
        for (int ip = 0; ip < nSplit; ip++) {
          auto& p = particles[ip];
          Real qp1 = p.rdata(iqp_);
          Real xp1 = p.pos(ix_);
          Real yp1 = p.pos(iy_);
          Real zp1 = nDim > 2 ? p.pos(iz_) : 0;
          Real up1 = p.rdata(iup_);
          Real vp1 = p.rdata(ivp_);
          Real wp1 = p.rdata(iwp_);

          const Real u2 = up1 * up1 + vp1 * vp1 + wp1 * wp1;

          Real coef = (u2 < 1e-13) ? 0 : dl / sqrt(u2);
          const Real dpx = coef * up1;
          const Real dpy = coef * vp1;
          const Real dpz = coef * wp1;

          Real xp2 = xp1 + dpx;
          Real yp2 = yp1 + dpy;
          Real zp2 = zp1 + dpz;

          int nNew = is_neutral() ? 7 : 1;

          p.rdata(iqp_) = qp1 / (nNew + 1.0);

          for (int iNew = 0; iNew < nNew; iNew++) {

            if (is_neutral()) {
              xp2 = xp1 + (xMax - xMin) * (randNum() - 0.5);
              yp2 = yp1 + (yMax - yMin) * (randNum() - 0.5);
              zp2 = zp1 + (zMax - zMin) * (randNum() - 0.5);
            } else {
              xp1 -= dpx;
              yp1 -= dpy;
              zp1 -= dpz;

              xp1 = std::clamp(xp1, xMin, xMax);
              yp1 = std::clamp(yp1, yMin, yMax);
              zp1 = std::clamp(zp1, zMin, zMax);
              p.pos(ix_) = xp1;
              p.pos(iy_) = yp1;

              if (nDim > 2)
                p.pos(iz_) = zp1;
            }

            xp2 = std::clamp(xp2, xMin, xMax);
            yp2 = std::clamp(yp2, yMin, yMax);
            zp2 = std::clamp(zp2, zMin, zMax);

            auto pnew = make_particle();
            set_ids(pnew);

            pnew.pos(ix_) = xp2;
            pnew.pos(iy_) = yp2;
            if (nDim > 2)
              pnew.pos(iz_) = zp2;
            pnew.rdata(iup_) = up1;
            pnew.rdata(ivp_) = vp1;
            pnew.rdata(iwp_) = wp1;
            pnew.rdata(iqp_) = qp1 / (nNew + 1.0);
            newparticles.push_back(pnew);
          }
        }
      }

      for (auto& p : newparticles) {
        pTile.push_back(p);
      }
    }
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::split(Real limit,
                                               bool seperateVelocity) {
  timing_func("Pts::split");

  const int nInitial = product(nPartPerCell);

  IntVect iv = { AMREX_D_DECL(1, 1, 1) };
  if (!(do_tiling && tile_size == iv))
    return;

  for (int iLev = 0; iLev < n_lev(); iLev++) {

    const Real dl = 0.1 * Geom(iLev).CellSize()[ix_] / nPartPerCell.max();

    const int nLowerLimit = nInitial * limit * pow(pLevRatio, iLev);

    const int nGoal = nLowerLimit > nInitial ? nLowerLimit : nInitial;

    const Real vol = dx[iLev].product();

    const Real vacuumMass = vacuum * vol;

    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {

      Vector<ParticleType> newparticles;

      auto& pTile = get_particle_tile(iLev, pti);
      AoS& particles = pTile.GetArrayOfStructs();

      const int nPartOrig = particles.size();

      if (nPartOrig > nLowerLimit)
        continue;

      const int nSplit =
          nGoal - nPartOrig > nPartOrig ? nPartOrig : nGoal - nPartOrig;

      Real totalMass = 0;
      for (auto& p : particles) {
        // So far, the vacuum limit is designed for OH-PT neutrals only. It is
        // not clear how should it be done for PIC, where electrons and ions
        // have different mass. --Yuxi
        totalMass += qomSign * p.rdata(iqp_);
      }
      if (totalMass < vacuumMass)
        continue;

      // Find the 'heaviest' nNew particles by sorting the weight
      // (charge).-----

      // Sort the particles by the location first to make sure the results
      // are the same for different number of processors
      std::sort(particles.begin(), particles.end(), compare_two_parts);

      // Sort the particles by the weight in decending order.
      std::sort(particles.begin(), particles.end(),
                [](const ParticleType& pl, const ParticleType& pr) {
                  const Real ql = fabs(pl.rdata(iqp_));
                  const Real qr = fabs(pr.rdata(iqp_));
                  if (fabs(ql - qr) > 1e-9 * (ql + qr)) {
                    return ql > qr;
                  }

                  if (fabs(pl.pos(ix_) - pr.pos(ix_)) >
                      1e-9 * (fabs(pl.pos(ix_)) + fabs(pr.pos(ix_)))) {
                    return pl.pos(ix_) > pr.pos(ix_);
                  }
                  return false;
                });
      //----------------------------------------------------------------

      const auto lo = lbound(pti.tilebox());
      const auto hi = ubound(pti.tilebox());

      const Real xMin = Geom(iLev).LoEdge(lo.x, ix_) +
                        Geom(iLev).CellSize()[ix_] * 1e-10,
                 xMax = Geom(iLev).HiEdge(hi.x, ix_) -
                        Geom(iLev).CellSize()[ix_] * 1e-10;

      const Real yMin = Geom(iLev).LoEdge(lo.y, iy_) +
                        Geom(iLev).CellSize()[iy_] * 1e-10,
                 yMax = Geom(iLev).HiEdge(hi.y, iy_) -
                        Geom(iLev).CellSize()[iy_] * 1e-10;

      const Real zMin = nDim > 2 ? Geom(iLev).LoEdge(lo.z, iz_) +
                                       Geom(iLev).CellSize()[iz_] * 1e-10
                                 : 0.0,
                 zMax = nDim > 2 ? Geom(iLev).HiEdge(hi.z, iz_) -
                                       Geom(iLev).CellSize()[iz_] * 1e-10
                                 : 0.0;

      if (is_neutral() || seperateVelocity) {
        Box bx = pti.tilebox();
        set_random_seed(iLev, bx.smallEnd(), IntVect(888));
      }

      if (seperateVelocity) {
        Vector<ParticleType*> pold;
        for (int ip = 0; ip < nSplit; ip++) {
          pold.push_back(&(particles[ip]));
        }
        split_particles_by_velocity(pold, newparticles);
      } else {
        for (int ip = 0; ip < nSplit; ip++) {
          auto& p = particles[ip];
          Real qp1 = p.rdata(iqp_);
          Real xp1 = p.pos(ix_);
          Real yp1 = p.pos(iy_);
          Real zp1 = nDim > 2 ? p.pos(iz_) : 0;
          Real up1 = p.rdata(iup_);
          Real vp1 = p.rdata(ivp_);
          Real wp1 = p.rdata(iwp_);

          const Real u2 = up1 * up1 + vp1 * vp1 + wp1 * wp1;

          Real coef = (u2 < 1e-13) ? 0 : dl / sqrt(u2);
          const Real dpx = coef * up1;
          const Real dpy = coef * vp1;
          const Real dpz = coef * wp1;

          Real xp2 = xp1 + dpx;
          Real yp2 = yp1 + dpy;
          Real zp2 = zp1 + dpz;

          int nNew = is_neutral() ? 7 : 1;

          p.rdata(iqp_) = qp1 / (nNew + 1.0);

          for (int iNew = 0; iNew < nNew; iNew++) {

            if (is_neutral()) {
              xp2 = xp1 + (xMax - xMin) * (randNum() - 0.5);
              yp2 = yp1 + (yMax - yMin) * (randNum() - 0.5);
              zp2 = zp1 + (zMax - zMin) * (randNum() - 0.5);
            } else {
              xp1 -= dpx;
              yp1 -= dpy;
              zp1 -= dpz;

              xp1 = std::clamp(xp1, xMin, xMax);
              yp1 = std::clamp(yp1, yMin, yMax);
              zp1 = std::clamp(zp1, zMin, zMax);
              p.pos(ix_) = xp1;
              p.pos(iy_) = yp1;

              if (nDim > 2)
                p.pos(iz_) = zp1;
            }

            xp2 = std::clamp(xp2, xMin, xMax);
            yp2 = std::clamp(yp2, yMin, yMax);
            zp2 = std::clamp(zp2, zMin, zMax);

            auto pnew = make_particle();
            set_ids(pnew);

            pnew.pos(ix_) = xp2;
            pnew.pos(iy_) = yp2;
            if (nDim > 2)
              pnew.pos(iz_) = zp2;
            pnew.rdata(iup_) = up1;
            pnew.rdata(ivp_) = vp1;
            pnew.rdata(iwp_) = wp1;
            pnew.rdata(iqp_) = qp1 / (nNew + 1.0);
            newparticles.push_back(pnew);
          }
        }
      }

      for (auto& p : newparticles) {
        pTile.push_back(p);
      }
    }
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
bool Particles<NStructReal, NStructInt>::merge_particles_accurate(
    int iLev, AoS& particles, Vector<int>& partIdx, Vector<int>& idx_I,
    int nPartCombine, int nPartNew, Vector<Real>& x, Real velNorm) {
  timing_func("Pts::merge_particles_accurate");

  constexpr int nVar = 5;
  constexpr int iq_ = 0, iu_ = 1, iv_ = 2, iw_ = 3, ie_ = 4;
  const Real coefVel = 1, coefPos = 1;

  Vector<Real> ref(nVar, 0);
  Array2D<Real, 0, nVar - 1, 0, nVar> a;
  for (int i = 0; i < nVar; ++i)
    for (int j = 0; j < nVar + 1; ++j) {
      a(i, j) = 0;
    }

  // Find the center of the particles, and sort the particles based
  // on its distance to the 6-D center.
  //----------------------------------------------------------
  Vector<Real> middle(nDim + nDim3, 0);
  for (int pID : partIdx) {
    for (int iDir = ix_; iDir <= iz_; iDir++) {
      if (iDir < nDim)
        middle[iDir] += particles[pID].pos(iDir);
      middle[nDim + iDir] += particles[pID].rdata(iDir);
    }
  }

  for (int i = 0; i < middle.size(); ++i) {
    middle[i] /= partIdx.size();
  }

  auto calc_distance2_to_center = [&, this](int pID) {
    Real dl2 = 0, dvel2 = 0;
    for (int iDir = ix_; iDir <= iz_; iDir++) {

      if (iDir < nDim) {
        Real pos = particles[pID].pos(iDir);
        dl2 += pow((pos - middle[iDir]) * invDx[iLev][iDir], 2);
      }

      Real v = particles[pID].rdata(iDir);
      dvel2 += pow((v - middle[nDim + iDir]) * velNorm, 2);
    }
    return coefPos * dl2 + coefVel * dvel2;
  };

  std::sort(partIdx.begin(), partIdx.end(),
            [calc_distance2_to_center](const int& idl, const int& idr) {
              Real dll = calc_distance2_to_center(idl);
              Real dlr = calc_distance2_to_center(idr);

              if (fabs(dll - dlr) > 1e-9 * (dll + dlr)) {
                return dll < dlr;
              }
              return false;
            });

  /*
      Delete 1 particle out of 6 particles:
      1) Choose two particles that are closest to each other.
      2) Delete the lighter one.
      3) Distribute its weights to another 5 particles to conserve
         mass, momentum and energy.
   */

  idx_I.resize(nPartCombine, 0);
  for (int ip = 0; ip < nPartCombine; ip++) {
    idx_I[ip] = partIdx[ip];
  }

  // Calculate the center of the particles for combination
  for (int i = 0; i < middle.size(); ++i) {
    middle[i] = 0;
  }
  for (int pID : idx_I) {
    for (int iDir = ix_; iDir <= iz_; iDir++) {
      if (iDir < nDim)
        middle[iDir] += particles[pID].pos(iDir);

      middle[nDim + iDir] += particles[pID].rdata(iDir);
    }
  }
  for (int i = 0; i < middle.size(); ++i) {
    middle[i] /= nPartCombine;
  }

  bool doCombine = true;
  for (int pID : idx_I) {
    Real distance = sqrt(calc_distance2_to_center(pID));
    if (distance > mergeThresholdDistance) {
      // printf("Warning: distance=%e\n", distance);
      doCombine = false;
    }
  }

  if (!doCombine)
    return false;

  // Find the pair that is closest to each other in phase space
  int pair1 = 0, pair2 = 0;
  Real dis2Min = 2;
  for (int ip1 = 0; ip1 < nPartCombine - 1; ip1++)
    for (int ip2 = ip1 + 1; ip2 < nPartCombine; ip2++) {

      // Distance between two particles in 6D space.
      Real dl2 = 0, dv2 = 0;
      for (int iDir = 0; iDir < nDim; iDir++) {
        Real dv = velNorm * (particles[idx_I[ip1]].rdata(iDir) -
                             particles[idx_I[ip2]].rdata(iDir));
        dv2 += pow(dv, 2);

        Real dx = invDx[iLev][iDir] * (particles[idx_I[ip1]].pos(iDir) -
                                       particles[idx_I[ip2]].pos(iDir));
        dv2 += pow(dx, 2);
      }

      const Real dis2 = dv2 * coefVel + dl2 * coefPos;

      if (dis2 < dis2Min) {
        dis2Min = dis2;
        pair1 = ip1;
        pair2 = ip2;
      }
    }
  //-------------------------------

  // Delete the lighter one.
  int iPartDel = pair1;
  // Q: Why is it 'l>(1+1e-9)*r' instead of 'l>r'?
  // A: The particle weights can be the same for some cases. 'l>r'
  // may return random results due to the truncation error.
  if (fabs(particles[idx_I[pair1]].rdata(iqp_)) >
      (1 + 1e-9) * fabs(particles[idx_I[pair2]].rdata(iqp_))) {
    iPartDel = pair2;
  }

  std::swap(idx_I[iPartDel], idx_I[nPartCombine - 1]);

  //-----------Solve the new particle weights-------
  for (int ip = 0; ip < nPartCombine; ip++) {
    const Real qp = particles[idx_I[ip]].rdata(iqp_);
    const Real up = particles[idx_I[ip]].rdata(iup_);
    const Real vp = particles[idx_I[ip]].rdata(ivp_);
    const Real wp = particles[idx_I[ip]].rdata(iwp_);
    const Real v2 = (pow(up, 2) + pow(vp, 2) + pow(wp, 2));

    if (ip < nVar) {
      a(iq_, ip) = 1;
      a(iu_, ip) = up;
      a(iv_, ip) = vp;
      a(iw_, ip) = wp;
      a(ie_, ip) = v2;
    }

    a(iq_, nVar) += qp;
    a(iu_, nVar) += qp * up;
    a(iv_, nVar) += qp * vp;
    a(iw_, nVar) += qp * wp;
    a(ie_, nVar) += qp * v2;
  }

  const Real csmall = 1e-9;
  const Real tmp = csmall * fabs(1. / a(iq_, nVar));
  for (int i = iq_; i <= ie_; ++i) {
    // Ensure a strictly positive threshold so constraint rows with zero RHS and
    // zero coefficients (e.g., vz = 0 in 1D/2D) are flagged as singular,
    // preventing near-zero pivots or 0/0 NaNs in the weights.
    ref[i] = std::max(fabs(a(i, nVar) * tmp), csmall * tmp);
  }

  x.resize(nVar, 0);
  bool isSolved = linear_solver_Gauss_Elimination<Real, nVar, nVar + 1>(
      nVar, nVar + 1, a, x, ref);

  if (isSolved) {
    // All the particle weights should have the same sign.
    Real qt = x[0];
    for (int ip = 0; ip < nPartNew; ip++) {
      if (qt * x[ip] <= 0) {
        isSolved = false;
        break;
      }
    }
  }

  return isSolved;
}

//==========================================================
template <int NStructReal, int NStructInt>
bool Particles<NStructReal, NStructInt>::merge_particles_fast(
    int iLev, AoS& particles, Vector<int>& partIdx, Vector<int>& idx_I,
    int nPartCombine, int nPartNew, Vector<Real>& x, long seed) {
  timing_func("Pts::merge_particles_fast");

  constexpr int iq_ = 0, iu_ = 1, iv_ = 2, iw_ = 3, ie_ = 4;
  constexpr int nPartNewMax = 16;
  constexpr int nVarMax = nPartNewMax + 5;

  int nVar = nPartNew + 5;

  Vector<Real> ref(nVar, 0);
  Array2D<Real, 0, nVarMax - 1, 0, nVarMax> a;
  for (int i = 0; i < nVar; ++i)
    for (int j = 0; j < nVar + 1; ++j) {
      a(i, j) = 0;
    }

  // Q: Sort the particles by weights in ascending order.
  // But, why is it required here?
  // A: Eliminate randomness.
  std::sort(partIdx.begin(), partIdx.end(),
            [&particles](int idLeft, int idRight) {
              const Real ql = fabs(particles[idLeft].rdata(iqp_));
              const Real qr = fabs(particles[idRight].rdata(iqp_));
              if (fabs(ql - qr) > 1e-9 * (ql + qr)) {
                return ql < qr;
              }

              Real xl = particles[idLeft].pos(ix_);
              Real xr = particles[idRight].pos(ix_);
              if (fabs(xl - xr) > 1e-9 * (fabs(xl) + fabs(xr))) {
                return xl < xr;
              }
              return false;
            });

  if (mergeLight) {
    idx_I.resize(nPartCombine, 0);
    for (int ip = 0; ip < nPartCombine; ip++) {
      idx_I[ip] = partIdx[ip];
    }

    Real plight = 1e99, pheavy = 0;
    for (int ip = 0; ip < nPartCombine; ip++) {
      auto& p = particles[idx_I[ip]];
      Real w = fabs(p.rdata(iqp_));
      if (w < plight)
        plight = w;
      if (w > pheavy)
        pheavy = w;
    }

    if (pheavy / plight > mergePartRatioMax)
      return false;

    randNum.set_seed(seed);
    shuffle_fish_yates(idx_I, randNum);

  } else {
    randNum.set_seed(seed);
    shuffle_fish_yates(partIdx, randNum);

    idx_I.resize(nPartCombine, 0);
    for (int ip = 0; ip < nPartCombine; ip++) {
      idx_I[ip] = partIdx[ip];
    }
  }

  // Sum the moments of all the old particles.
  for (int ip = 0; ip < nPartCombine; ip++) {
    const Real qp = particles[idx_I[ip]].rdata(iqp_);
    const Real up = particles[idx_I[ip]].rdata(iup_);
    const Real vp = particles[idx_I[ip]].rdata(ivp_);
    const Real wp = particles[idx_I[ip]].rdata(iwp_);
    const Real v2 = 0.5 * (pow(up, 2) + pow(vp, 2) + pow(wp, 2));
    a(nPartNew + iq_, nVar) += qp;
    a(nPartNew + iu_, nVar) += qp * up;
    a(nPartNew + iv_, nVar) += qp * vp;
    a(nPartNew + iw_, nVar) += qp * wp;
    a(nPartNew + ie_, nVar) += qp * v2;
  }

  const Real invAvg = 2 * nPartNew / a(nPartNew + iq_, nVar);
  for (int ip = 0; ip < nPartNew; ip++) {
    const Real qp = particles[idx_I[ip]].rdata(iqp_);
    const Real up = particles[idx_I[ip]].rdata(iup_);
    const Real vp = particles[idx_I[ip]].rdata(ivp_);
    const Real wp = particles[idx_I[ip]].rdata(iwp_);
    const Real v2 = 0.5 * (pow(up, 2) + pow(vp, 2) + pow(wp, 2));

    a(ip, nVar) = 2;

    a(ip, ip) = 2. / qp;
    a(ip, nPartNew + iq_) = 1;
    a(ip, nPartNew + iu_) = up;
    a(ip, nPartNew + iv_) = vp;
    a(ip, nPartNew + iw_) = wp;
    a(ip, nPartNew + ie_) = v2;

    a(nPartNew + iq_, ip) = 1;
    a(nPartNew + iu_, ip) = up;
    a(nPartNew + iv_, ip) = vp;
    a(nPartNew + iw_, ip) = wp;
    a(nPartNew + ie_, ip) = v2;
  }

  const Real csmall = 1e-9;
  const Real tmp = csmall * invAvg;
  for (int i = 0; i < nVar; ++i) {
    if (i < nPartNew) {
      ref[i] = tmp;
    } else {
      // Floor at strictly positive: zero-RHS constraint rows (e.g. vz = 0 in
      // 1D/2D) must be flagged as singular rather than accepting near-zero
      // pivots.
      ref[i] = std::max(fabs(a(i, nVar) * tmp * csmall), csmall * tmp);
    }
  }

  x.resize(nVar, 0);
  bool isSolved = linear_solver_Gauss_Elimination<Real, nVarMax, nVarMax + 1>(
      nVar, nVar + 1, a, x, ref);

  if (isSolved) {
    // All the particle weights should have the same sign.
    Real qt = x[0];
    for (int ip = 0; ip < nPartNew; ip++) {
      if (qt * x[ip] <= 0) {
        isSolved = false;
        break;
      }
    }
  }

  if (isSolved) {
    for (int ip = 0; ip < nPartNew; ip++) {
      Real pold = particles[idx_I[ip]].rdata(iqp_);
      Real pnew = x[ip];

      Real c0 = Real(nPartCombine) / nPartNew * mergeRatioMax;
      if (pnew / pold > c0 || pold / pnew > c0) {
        isSolved = false;
        break;
      }
    }
  }

  return isSolved;
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::merge(Real limit) {
  timing_func("Pts::merge");
  IntVect iv = { AMREX_D_DECL(1, 1, 1) };
  if (!(do_tiling && tile_size == iv))
    return;

  for (int iLev = 0; iLev < n_lev(); iLev++) {

    int nPartGoal = product(nPartPerCell) * limit * pow(pLevRatio, iLev);

    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {

      // It is assumed the tile size is 1x1x1.
      Box bx = pti.tilebox();
      long seed = set_random_seed(iLev, bx.smallEnd(), IntVect(777));

      AoS& particles = pti.GetArrayOfStructs();

      const int nPartOrig = particles.size();

      if (nPartOrig <= nPartGoal)
        continue;

      // The range of the velocity domain:
      // [-r0,r0]*thermal_velocity+bulk_velocity
      const Real r0 = fastMerge ? 2.0 : 1.0;

      // Phase space cell number in one direction.
      // The const 0.5/0.8 is choosen by experiments.
      int nCell = 0;
      if (fastMerge) {
        nCell = r0 * ceil(0.5 * pow(nPartOrig, 1. / nDim3));
      } else {
        nCell = r0 * ceil(0.8 * pow(nPartOrig, 1. / nDim3));
      }

      if (nCell < 3)
        continue;

      // Sort the particles by the location first to make sure the results
      // are the same for different number of processors
      std::sort(particles.begin(), particles.end(), compare_two_parts);

      // One particle may belong to more than one velocity bins, but it can be
      // only merged at most once.
      std::vector<bool> merged;
      merged.resize(nPartOrig, false);

      //----------------------------------------------------------------
      // Estimate the bulk velocity and thermal velocity.
      Real uBulk[nDim3] = { 0, 0, 0 };
      for (int pid = 0; pid < nPartOrig; pid++) {
        auto& pcl = particles[pid];
        for (int iDir = 0; iDir < 3; iDir++) {
          uBulk[iDir] += pcl.rdata(iDir);
        }
      }

      for (int iDir = 0; iDir < nDim3; iDir++) {
        uBulk[iDir] /= nPartOrig;
      }

      Real thVel = 0, thVel2 = 0;
      for (int pid = 0; pid < nPartOrig; pid++) {
        auto& pcl = particles[pid];
        for (int iDir = 0; iDir < nDim3; iDir++) {
          thVel2 += pow(pcl.rdata(iDir) - uBulk[iDir], 2);
        }
      }

      thVel2 /= nPartOrig;
      thVel = sqrt(thVel2);

      // The coef 0.5 if choosen by experience.
      const Real velNorm = (thVel < 1e-13) ? 0 : 1.0 / (0.5 * thVel);
      //----------------------------------------------------------------

      //----------------------------------------------------------------
      // Assign the particle IDs to the corresponding velocity space cells.
      Vector<Vector<int> > phasePartIdx_I(nCell * nCell * nCell);
      const auto bin_index = [nCell](int i, int j, int k) {
        return (i * nCell + j) * nCell + k;
      };

      Real dv = (2.0 * r0 * thVel) / nCell;
      Real invDv = (dv < 1e-13) ? 0 : 1.0 / dv;

      // Velocity domain range.
      Real velMin_D[nDim3], velMax_D[nDim3];
      for (int iDir = 0; iDir < nDim3; iDir++) {
        Real dvshift = (randNum() - 0.5) * dv;
        velMin_D[iDir] = -r0 * thVel + uBulk[iDir] + dvshift;
        velMax_D[iDir] = r0 * thVel + uBulk[iDir] + dvshift;
      }

      int iCell_D[nDim3];
      for (int pid = 0; pid < nPartOrig; pid++) {
        auto& pcl = particles[pid];

        bool isOutside = false;
        for (int iDim = 0; iDim < nDim3; iDim++) {
          if (pcl.rdata(iDim) < velMin_D[iDim] ||
              pcl.rdata(iDim) > velMax_D[iDim])
            isOutside = true;
        }
        if (isOutside)
          continue;

        for (int iDim = 0; iDim < nDim3; iDim++) {
          iCell_D[iDim] = fastfloor((pcl.rdata(iDim) - velMin_D[iDim]) * invDv);
        }

        // One particle may belong to multiple bins when each bin has a buffer
        // region.
        for (int xCell = iCell_D[ix_] - 1; xCell <= iCell_D[ix_] + 1; xCell++)
          for (int yCell = iCell_D[iy_] - 1; yCell <= iCell_D[iy_] + 1; yCell++)
            for (int zCell = iCell_D[iz_] - 1; zCell <= iCell_D[iz_] + 1;
                 zCell++) {

              if (xCell < 0 || xCell >= nCell || yCell < 0 || yCell >= nCell ||
                  zCell < 0 || zCell >= nCell)
                continue;

              Vector<int> cellIdx = { xCell, yCell, zCell };

              Real binMin_D[nDim3], binMax_D[nDim3];

              for (int iDim = 0; iDim < nDim3; iDim++) {
                binMin_D[iDim] =
                    velMin_D[iDim] + (cellIdx[iDim] - velBinBufferSize) * dv;

                binMax_D[iDim] = velMin_D[iDim] +
                                 (cellIdx[iDim] + 1 + velBinBufferSize) * dv;
              }

              bool isInside = true;
              for (int iDim = 0; iDim < nDim3; iDim++) {
                if (pcl.rdata(iDim) < binMin_D[iDim] ||
                    pcl.rdata(iDim) > binMax_D[iDim])
                  isInside = false;
              }

              if (isInside) {
                phasePartIdx_I[bin_index(cellIdx[ix_], cellIdx[iy_],
                                         cellIdx[iz_])]
                    .push_back(pid);
              }
            }
      }
      //----------------------------------------------------------------

      for (int iu = 0; iu < nCell; iu++)
        for (int iv = 0; iv < nCell; iv++)
          for (int iw = 0; iw < nCell; iw++) {
            Vector<int> partIdx;
            auto& pIdx = phasePartIdx_I[bin_index(iu, iv, iw)];
            for (int i = 0; i < pIdx.size(); ++i) {
              int pid = pIdx[i];
              if (!merged[pid]) {
                partIdx.push_back(pid);
              }
            }

            if (partIdx.size() < nPartNew + 1)
              continue;

            Vector<Real> x;
            Vector<int> idx_I;

            int nOld = nPartCombine;
            bool isSolved;
            if (fastMerge) {
              if (nOld > partIdx.size())
                nOld = partIdx.size();

              for (int iTry = 0; iTry < nMergeTry; iTry++) {
                long sd = seed + iu * 777 + iv * 77 + iw + iTry;
                isSolved = merge_particles_fast(iLev, particles, partIdx, idx_I,
                                                nOld, nPartNew, x, sd);
                if (isSolved)
                  break;
              }
            } else {
              isSolved = merge_particles_accurate(
                  iLev, particles, partIdx, idx_I, nOld, nPartNew, x, velNorm);
            }
            if (!isSolved)
              continue;

            //----------------------------------------------

            // Reject merge if any solved weight is non-finite (NaN / Inf).
            bool xIsFinite = true;
            for (int ip = 0; ip < nPartNew; ++ip) {
              if (!std::isfinite(x[ip])) {
                xIsFinite = false;
                break;
              }
            }
            if (!xIsFinite)
              continue;

            // Adjust weight.
            for (int ip = 0; ip < nPartNew; ip++) {
              auto& p = particles[idx_I[ip]];
              p.rdata(iqp_) = x[ip];
              merged[idx_I[ip]] = true;
            }
            // Mark for deletion
            for (int ip = nPartNew; ip < nOld; ip++) {
              particles[idx_I[ip]].id() = -1;
              particles[idx_I[ip]].rdata(iqp_) = 0;
              merged[idx_I[ip]] = true;
            }
          }
    }
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::merge_new(Real limit) {
  timing_func("Pts::merge");
  IntVect iv = { AMREX_D_DECL(1, 1, 1) };
  if (!(do_tiling && tile_size == iv))
    return;

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      const auto tppc = target_PPC(iLev)[pti].array();
      const Box& bx = pti.tilebox();
      IntVect ibx = bx.smallEnd();
      int target = tppc(ibx);
      int nPartGoal = target * limit;

      // It is assumed the tile size is 1x1x1.
      long seed = set_random_seed(iLev, bx.smallEnd(), IntVect(777));

      AoS& particles = pti.GetArrayOfStructs();

      const int nPartOrig = particles.size();

      if (nPartOrig <= nPartGoal)
        continue;

      // The range of the velocity domain:
      // [-r0,r0]*thermal_velocity+bulk_velocity
      const Real r0 = fastMerge ? 2.0 : 1.0;

      // Phase space cell number in one direction.
      // The const 0.5/0.8 is choosen by experiments.
      int nCell = 0;
      if (fastMerge) {
        nCell = r0 * ceil(0.5 * pow(nPartOrig, 1. / nDim3));
      } else {
        nCell = r0 * ceil(0.8 * pow(nPartOrig, 1. / nDim3));
      }

      if (nCell < 3)
        continue;

      // Sort the particles by the location first to make sure the results
      // are the same for different number of processors
      std::sort(particles.begin(), particles.end(), compare_two_parts);

      // One particle may belong to more than one velocity bins, but it can be
      // only merged at most once.
      std::vector<bool> merged;
      merged.resize(nPartOrig, false);

      //----------------------------------------------------------------
      // Estimate the bulk velocity and thermal velocity.
      Real uBulk[nDim3] = { 0, 0, 0 };
      for (int pid = 0; pid < nPartOrig; pid++) {
        auto& pcl = particles[pid];
        for (int iDir = 0; iDir < 3; iDir++) {
          uBulk[iDir] += pcl.rdata(iDir);
        }
      }

      for (int iDir = 0; iDir < nDim3; iDir++) {
        uBulk[iDir] /= nPartOrig;
      }

      Real thVel = 0, thVel2 = 0;
      for (int pid = 0; pid < nPartOrig; pid++) {
        auto& pcl = particles[pid];
        for (int iDir = 0; iDir < nDim3; iDir++) {
          thVel2 += pow(pcl.rdata(iDir) - uBulk[iDir], 2);
        }
      }

      thVel2 /= nPartOrig;
      thVel = sqrt(thVel2);

      // The coef 0.5 if choosen by experience.
      const Real velNorm = (thVel < 1e-13) ? 0 : 1.0 / (0.5 * thVel);
      //----------------------------------------------------------------

      //----------------------------------------------------------------
      // Assign the particle IDs to the corresponding velocity space cells.
      Vector<Vector<int> > phasePartIdx_I(nCell * nCell * nCell);
      const auto bin_index = [nCell](int i, int j, int k) {
        return (i * nCell + j) * nCell + k;
      };

      Real dv = (2.0 * r0 * thVel) / nCell;
      Real invDv = (dv < 1e-13) ? 0 : 1.0 / dv;

      // Velocity domain range.
      Real velMin_D[nDim3], velMax_D[nDim3];
      for (int iDir = 0; iDir < nDim3; iDir++) {
        Real dvshift = (randNum() - 0.5) * dv;
        velMin_D[iDir] = -r0 * thVel + uBulk[iDir] + dvshift;
        velMax_D[iDir] = r0 * thVel + uBulk[iDir] + dvshift;
      }

      int iCell_D[nDim3];
      for (int pid = 0; pid < nPartOrig; pid++) {
        auto& pcl = particles[pid];

        bool isOutside = false;
        for (int iDim = 0; iDim < nDim3; iDim++) {
          if (pcl.rdata(iDim) < velMin_D[iDim] ||
              pcl.rdata(iDim) > velMax_D[iDim])
            isOutside = true;
        }
        if (isOutside)
          continue;

        for (int iDim = 0; iDim < nDim3; iDim++) {
          iCell_D[iDim] = fastfloor((pcl.rdata(iDim) - velMin_D[iDim]) * invDv);
        }

        // One particle may belong to multiple bins when each bin has a buffer
        // region.
        for (int xCell = iCell_D[ix_] - 1; xCell <= iCell_D[ix_] + 1; xCell++)
          for (int yCell = iCell_D[iy_] - 1; yCell <= iCell_D[iy_] + 1; yCell++)
            for (int zCell = iCell_D[iz_] - 1; zCell <= iCell_D[iz_] + 1;
                 zCell++) {

              if (xCell < 0 || xCell >= nCell || yCell < 0 || yCell >= nCell ||
                  zCell < 0 || zCell >= nCell)
                continue;

              Vector<int> cellIdx = { xCell, yCell, zCell };

              Real binMin_D[nDim3], binMax_D[nDim3];

              for (int iDim = 0; iDim < nDim3; iDim++) {
                binMin_D[iDim] =
                    velMin_D[iDim] + (cellIdx[iDim] - velBinBufferSize) * dv;

                binMax_D[iDim] = velMin_D[iDim] +
                                 (cellIdx[iDim] + 1 + velBinBufferSize) * dv;
              }

              bool isInside = true;
              for (int iDim = 0; iDim < nDim3; iDim++) {
                if (pcl.rdata(iDim) < binMin_D[iDim] ||
                    pcl.rdata(iDim) > binMax_D[iDim])
                  isInside = false;
              }

              if (isInside) {
                phasePartIdx_I[bin_index(cellIdx[ix_], cellIdx[iy_],
                                         cellIdx[iz_])]
                    .push_back(pid);
              }
            }
      }
      //----------------------------------------------------------------

      for (int iu = 0; iu < nCell; iu++)
        for (int iv = 0; iv < nCell; iv++)
          for (int iw = 0; iw < nCell; iw++) {
            Vector<int> partIdx;
            auto& pIdx = phasePartIdx_I[bin_index(iu, iv, iw)];
            for (int i = 0; i < pIdx.size(); ++i) {
              int pid = pIdx[i];
              if (!merged[pid]) {
                partIdx.push_back(pid);
              }
            }

            if (partIdx.size() < nPartNew + 1)
              continue;

            Vector<Real> x;
            Vector<int> idx_I;

            int nOld = nPartCombine;
            bool isSolved;
            if (fastMerge) {
              if (nOld > partIdx.size())
                nOld = partIdx.size();

              for (int iTry = 0; iTry < nMergeTry; iTry++) {
                long sd = seed + iu * 777 + iv * 77 + iw + iTry;
                isSolved = merge_particles_fast(iLev, particles, partIdx, idx_I,
                                                nOld, nPartNew, x, sd);
                if (isSolved)
                  break;
              }
            } else {
              isSolved = merge_particles_accurate(
                  iLev, particles, partIdx, idx_I, nOld, nPartNew, x, velNorm);
            }
            if (!isSolved)
              continue;

            //----------------------------------------------

            // Reject merge if any solved weight is non-finite (NaN / Inf).
            bool xIsFinite = true;
            for (int ip = 0; ip < nPartNew; ++ip) {
              if (!std::isfinite(x[ip])) {
                xIsFinite = false;
                break;
              }
            }
            if (!xIsFinite)
              continue;

            // Adjust weight.
            for (int ip = 0; ip < nPartNew; ip++) {
              auto& p = particles[idx_I[ip]];
              p.rdata(iqp_) = x[ip];
              merged[idx_I[ip]] = true;
            }
            // Mark for deletion
            for (int ip = nPartNew; ip < nOld; ip++) {
              particles[idx_I[ip]].id() = -1;
              particles[idx_I[ip]].rdata(iqp_) = 0;
              merged[idx_I[ip]] = true;
            }
          }
    }
  }
}

//==========================================================
template <int NStructReal, int NStructInt>
bool Particles<NStructReal, NStructInt>::do_inject_particles_for_this_cell(
    const Box& bx, const Array4<const int>& status, const IntVect ijk,
    IntVect& ijksrc) {

  // This cell should be a boundary cell at least.
  if (!bit::is_lev_boundary(status(ijk)))
    return false;

  for (int iloop = 1; iloop <= 3; iloop++) {
    // iloop==1: loop through faces;
    // iloop==2: loop through edges;
    // iloop==3: loop through corners;
    for (int di = -1; di <= 1; di++)
      for (int dj = -1; dj <= 1; dj++)
        for (int dk = -1; dk <= 1; dk++) {
          const int sum = std::abs(di) + std::abs(dj) + std::abs(dk);
          if (iloop != sum)
            continue;

          IntVect ijk1 = ijk + IntVect{ AMREX_D_DECL(di, dj, dk) };
          if (!bit::is_lev_boundary(status(ijk1))) {
            // The first neighbor cell that is NOT a boundary cell.
            if (bx.contains(ijk1)) {
              ijksrc = ijk1;
              return true;
            } else {
              return false;
            }
          }
        }
  }
  Abort("do_inject_particles_for_this_cell:something is wrong!");
  return false; // to suppress compilation warning.
}

IOParticles::IOParticles(Particles& other, Grid* gridIn, Real no2outL,
                         Real no2outV, Real no2outM, RealBox IORange)
    : Particles(gridIn, nullptr, nullptr, other.get_speciesID(),
                other.get_charge(), other.get_mass(), make_io_particles_info(),
                other.part_mode()) {

  no2outM *= qomSign * get_mass();

  const bool doLimit = IORange.ok();

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    const auto& plevelOther = other.GetParticles(iLev);
    auto& plevel = GetParticles(iLev);
    for (MFIter mfi = other.MakeMFIter(iLev); mfi.isValid(); ++mfi) {
      auto index = std::make_pair(mfi.index(), mfi.LocalTileIndex());

      if (plevelOther.find(index) == plevelOther.end())
        continue;

      const auto& tileOther = plevelOther.at(index);

      if (tileOther.numParticles() == 0)
        continue;

      const AoS& aosOther = tileOther.GetArrayOfStructs();

      const Box& bx = other.cell_status(iLev)[mfi].box();
      const Array4<int const>& status = other.cell_status(iLev)[mfi].array();

      const IntVect lowCorner = bx.smallEnd();
      const IntVect highCorner = bx.bigEnd();

      for (auto p : aosOther) {
        if (other.is_outside_active_region(p, status, lowCorner, highCorner,
                                           iLev)) {
          // redistribute_particles() may fail if the ghost cell particles'
          // IDs are not -1 (marked for deletion);
          p.id() = -1;
        }

        for (int iDim = 0; iDim < nDim; iDim++) {
          p.pos(ix_ + iDim) = no2outL * p.pos(ix_ + iDim);
        }

        if (doLimit && !IORange.contains(RealVect(
                           AMREX_D_DECL(p.pos(ix_), p.pos(iy_), p.pos(iz_)))))
          continue;

        for (int iDim = 0; iDim < nDim3; iDim++) {
          p.rdata(iup_ + iDim) = no2outV * p.rdata(iup_ + iDim);
        }
        p.rdata(iqp_) = no2outM * p.rdata(iqp_);

        plevel[index].push_back(p);
      }
    }
  }
  redistribute_particles();
}

template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::sample_charge_exchange(
    Real* vp, Real* vh, Real* up, Real vth, CrossSection cs) {

  timing_func("Pts::sample_charge_exchange");

  // M (holds normalization costants for both distributions), g(vp) is the
  // maxwellian distribution
  Real sup[3] = { up[0] - 3. * vth, up[1] - 3. * vth, up[2] - 3. * vth };
  Real M = charge_exchange_dis(sup, vh, up, vth, cs) /
           exp(-((sup[0] - up[0]) * (sup[0] - up[0]) +
                 (sup[1] - up[1]) * (sup[1] - up[1]) +
                 (sup[2] - up[2]) * (sup[2] - up[2])) /
               (vth * vth));

  bool accepted = false;
  while (!accepted) {
    {
      Real prob, theta, uth;
      // u = X velocity
      prob = sqrt(-2.0 * log(1.0 - .999999999 * randNum()));
      theta = 2.0 * M_PI * randNum();
      uth = vth / sqrt(2.0);
      vp[0] = uth * prob * cos(theta) + up[0];
      // v = Y velocity
      vp[1] = uth * prob * sin(theta) + up[1];
      // w = Z velocity
      prob = sqrt(-2.0 * log(1.0 - .999999999 * randNum()));
      theta = 2.0 * M_PI * randNum();
      vp[2] = uth * prob * cos(theta) + up[2];
    }

    if (randNum() < charge_exchange_dis(vp, vh, up, vth, cs) /
                        (M * exp(-((vp[0] - up[0]) * (vp[0] - up[0]) +
                                   (vp[1] - up[1]) * (vp[1] - up[1]) +
                                   (vp[2] - up[2]) * (vp[2] - up[2])) /
                                 (vth * vth)))) {
      accepted = true;
    }
  }
}

template <int NStructReal, int NStructInt>
Real Particles<NStructReal, NStructInt>::charge_exchange_dis(Real* vp, Real* vh,
                                                             Real* up, Real vth,
                                                             CrossSection cs) {
  Real dv_D[3], dv2 = 0, dv = 0;
  for (int i = 0; i < 3; ++i) {
    dv_D[i] = vh[i] - vp[i];
    dv2 += dv_D[i] * dv_D[i];
  }

  if (dv2 == 0)
    return 0.0;

  dv = sqrt(dv2);

  Real erel = 0.5 * 1.674E-27 * dv2 * 6.2415E15; // in keV

  Real sigma = 0;
  if (cs == CrossSection::LS) {
    sigma = (4.15 - 0.531 * log(erel)) * (4.15 - 0.531 * log(erel)) *
            pow(1 - exp(-67.3 / erel), 4.5) * 1E-20; // cross section in m^2
  } else if (cs == CrossSection::MT) {
    Real dvcm = dv * 1E2;                             // velocity in cm/s
    sigma = pow(1.6 - 0.0695 * log(dvcm), 2) * 1e-18; // cross section in m^2
  }

  Real dvpup2 = 0;
  for (int i = 0; i < 3; ++i) {
    dvpup2 += pow(vp[i] - up[i], 2);
  }

  return dv * sigma * exp(-dvpup2 / (vth * vth));
}

template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::get_analytic_ion_fluid(
    const RealVect xyz, Real& rhoIon, Real& cs2Ion,
    amrex::Real (&uIon)[nDim3]) {

  // The units of ionOH are assumed to be:
  // r: AU
  // rho: amu/cc
  // T: K
  // U: km/s

  Real r = max(xyz.vectorLength(), 1e-9 * Geom(0).CellSize(0));

  Real rAU = r * fi->get_No2SiL() / cAUSI;

  if (rAU > ionOH.rAnalytic) {
    Abort("Error: rAU > ionOH.rAnalytic");
  }

  if (ionOH.doGetFromOH) {
    Real xSI = xyz[ix_] * fi->get_No2SiL();
    Real ySI = xyz[iy_] * fi->get_No2SiL();
    Real zSI = xyz[iz_] * fi->get_No2SiL();

    Real temp, ur, b[nDim3];
    OH_get_solar_wind(&xSI, &ySI, &zSI, &rhoIon, &ur, &temp, b);

    // v_th = sqrt(2kT/m); m/s
    cs2Ion = 2 * cBoltzmannSI * temp / cProtonMassSI; // m^2/s^2

    for (int i = 0; i < nDim; ++i) {
      uIon[i] = ur * xyz[i] / r;
    }
  } else {
    Real r0 = 0;
    if (rAU < ionOH.rCutoff) {
      r0 = ionOH.rAnalytic / ionOH.rCutoff;
    } else {
      r0 = ionOH.rAnalytic / rAU;
    }

    rhoIon = ionOH.swRho * pow(r0, 2) * 1e6; // amu/cc -> amu/m^3

    // v_th = sqrt(2kT/m); m/s
    cs2Ion = 2 * cBoltzmannSI * ionOH.swT / cProtonMassSI; // m^2/s^2

    for (int i = 0; i < nDim; ++i) {
      uIon[i] = ionOH.swU * xyz[i] / r * 1e3; // km/s -> m/s
    }
  }

  // AllPrint() << "r = " << r << " xyz = " << xyz[0] << ", " << xyz[1] << ", "
  //            << xyz[2] << " rhoIon = " << rhoIon << ", cs2Ion = " << cs2Ion
  //            << ", uIon = " << uIon[0] << ", " << uIon[1] << ", " << uIon[2]
  //            << std::endl;
}

// Get the iFluid-th ion fluid properties at the location xyz
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::get_ion_fluid(
    FluidInterface* stateOH, PIter& pti, const int iLev, const int iFluid,
    const RealVect xyz, Real& rhoIon, Real& cs2Ion, Real (&uIon)[nDim3]) {

  Real rAU = xyz.vectorLength() * stateOH->get_No2SiL() / cAUSI;

  if (iFluid == 0 && rAU < ionOH.rAnalytic) {
    get_analytic_ion_fluid(xyz, rhoIon, cs2Ion, uIon);
    return;
  }

  // amu/m^3
  rhoIon = stateOH->get_fluid_mass_density(pti, xyz, iFluid, iLev) *
           stateOH->get_No2SiRho() / cProtonMassSI;

  // cs = sqrt(P/n); m/s
  // Assume p = pi + pe = 2pi, so divide by sqrt(2.0).
  Real cs = stateOH->get_fluid_uth(pti, xyz, iFluid, iLev) *
            stateOH->get_No2SiV() / sqrt(2.0);

  // cs2Ion = 2*P/n. The definition of thermal speed in get_uth_iso() is
  // different from the requirement in OH_get_charge_exchange_wrapper().
  // See page 92 of Adam Michael's thesis.
  cs2Ion = 2 * pow(cs, 2);

  uIon[ix_] =
      stateOH->get_fluid_ux(pti, xyz, iFluid, iLev) * stateOH->get_No2SiV();
  uIon[iy_] =
      stateOH->get_fluid_uy(pti, xyz, iFluid, iLev) * stateOH->get_No2SiV();
  uIon[iz_] =
      stateOH->get_fluid_uz(pti, xyz, iFluid, iLev) * stateOH->get_No2SiV();
}

template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::charge_exchange(
    Real dt, FluidInterface* stateOH, FluidInterface* sourcePT2OH,
    SourceInterface* source, bool kineticSource,
    Vector<std::unique_ptr<PicParticles> >& sourceParts, bool doSelectRegion,
    int nppc, Real& maxExchangeRatio) {
  std::string nameFunc = "Pts::charge_exchange";

  timing_func(nameFunc);

  if (dt <= 0)
    return;

  // for (auto& ptr : sourceParts) {
  //   ptr->clearParticles();
  // }

  struct NeuPlasmaPair {
    Real q; // weight
    Real vp[3];
    Real vh[3];
    Real up[3];
    Real xyz[3];
    Real vth;
  };

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      AoS& particles = pti.GetArrayOfStructs();

      if (kineticSource) {
        // Sort the particles by the location first to make sure the results
        // are the same for different number of processors
        std::sort(particles.begin(), particles.end(), compare_two_parts);
      }

      // It is assumed the tile size is 1x1x1.
      Box bx = pti.tilebox();
      auto cellIdx = bx.smallEnd();

      int iRegion = 0;
      if (kineticSource && doSelectRegion) {
        iRegion = stateOH->get_neu_source_region(pti, cellIdx, iLev);
        if (iRegion < 0)
          continue;
      }
      // ParticleTileType
      auto& spTile =
          sourceParts[iRegion]->get_particle_tile(iLev, pti, cellIdx);

      Vector<NeuPlasmaPair> neuPlasmaPairs;

      if (kineticSource) {
        set_random_seed(iLev, cellIdx, IntVect(999));
      }

      for (auto& p : particles) {
        if (p.id() < 0)
          continue;

        RealVect xyz;
        for (int i = 0; i < nDim; ++i) {
          xyz[i] = p.pos(i);
        }

        Real cs2Neu = 0, uNeu[3], rhoNeu;
        Real cs2Ion, uIon[3], rhoIon;
        Real ion2neu[5], neu2ion[5];
        const int iRho_ = 0, iUx_ = 1, iUy_ = 2, iUz_ = 3, iP_ = 4;
        const int iRhoUx_ = iUx_, iRhoUy_ = iUy_, iRhoUz_ = iUz_, iE_ = iP_;

        // amu/m^3
        rhoNeu = qomSign * p.rdata(iqp_) * get_mass() * invVol[iLev] *
                 stateOH->get_No2SiRho() / cProtonMassSI;

        for (int i = 0; i < nDim; ++i) {
          uNeu[i] = p.rdata(iup_ + i) * stateOH->get_No2SiV();
        }

        // A neutral particle interacts with all the ion fluids.
        for (int fluidID = 0; fluidID < stateOH->get_nFluid(); fluidID++) {
          get_ion_fluid(stateOH, pti, iLev, fluidID, xyz, rhoIon, cs2Ion, uIon);

          OH_get_charge_exchange_wrapper(&rhoIon, &cs2Ion, uIon, &rhoNeu,
                                         &cs2Neu, uNeu, ion2neu, neu2ion);

          // The function above returns number density changing rate.
          ion2neu[iRho_] *= cProtonMassSI;
          neu2ion[iRho_] *= cProtonMassSI;

          Real dtSI = dt * stateOH->get_No2SiT();
          // Print() << "rhoion = " << rhoIon << " cs2Ion = " << cs2Ion
          //         << " rhoNeu = " << rhoNeu << " cs2Neu = " << cs2Neu
          //         << " dtSI = " << dtSI << std::endl;
          for (int i = iRho_; i <= iP_; ++i) {
            ion2neu[i] *= dtSI;
            neu2ion[i] *= dtSI;
            // Print() << " i = " << i << " ion2neu = " << ion2neu[i]
            //         << " neu2ion = " << neu2ion[i] << std::endl;
          }

          Real massExchange =
              neu2ion[iRho_] * stateOH->get_Si2NoRho() / invVol[iLev];

          if (massExchange == 0) {
            // It can happen for some special cases. For example, when neutral
            // density is zero.
            continue;
          }

          // Print() << "nden = " << p.rdata(iqp_)
          //         << " massExchange = " << massExchange << std::endl;
          if (p.rdata(iqp_) - massExchange <= 0) {
            // Mark for deletion
            p.id() = -1;

            // Reduce the sources accordingly to conserve total masses.
            const Real ratio = p.rdata(iqp_) / massExchange;
            for (int i = iRho_; i <= iP_; ++i) {
              ion2neu[i] *= ratio;
              neu2ion[i] *= ratio;
            }
          } else {
            // Reduce particle mass due to charge exchange

            Real ratio = massExchange / p.rdata(iqp_);
            if (ratio > maxExchangeRatio)
              maxExchangeRatio = ratio;

            p.rdata(iqp_) = p.rdata(iqp_) - massExchange;
          }

          {
            int iFluidAddTo = 0;
            Real rhoIonAddTo, cs2IonAddTo, uIonAddTo[3];

            const int iSW = 2;
            const int iSheath = 1;
            const int iOutSheath = 0;
            switch (stateOH->get_nFluid()) {
              case 1:
                iFluidAddTo = fluidID;
                rhoIonAddTo = rhoIon;
                break;
              case 2:
                if (iRegion == iSW) {
                  iFluidAddTo = 1; // Pu3
                } else {
                  iFluidAddTo = 0; // background
                }
                break;
              case 3:
                if (iRegion == iSW) {
                  iFluidAddTo = 1; // Pu3
                } else if (iRegion == iSheath || iRegion == iOutSheath) {
                  iFluidAddTo = 2; // Pu2
                } else {
                  iFluidAddTo = 0; // background
                }
                break;
              default:
                Abort("Error: nFluid > 3 is not supported yet.");
            }

            if (stateOH->get_nFluid() > 1) {
              get_ion_fluid(stateOH, pti, iLev, iFluidAddTo, xyz, rhoIonAddTo,
                            cs2IonAddTo, uIonAddTo);
            }

            // Q: Why is (neu2ion-ion2neu) divided by rhoIon?
            // A: What passed between PT and OH is 'source per ion density'
            // instead of source. The ion density will be multiplied back in OH
            // ModUser.f90
            { // Sources for ion fluid: Neu -> Ion
              sourcePT2OH->add_rho_to_loc(neu2ion[iRho_] / rhoIonAddTo, pti,
                                          xyz, iFluidAddTo, iLev);
              sourcePT2OH->add_mx_to_loc(neu2ion[iRhoUx_] / rhoIonAddTo, pti,
                                         xyz, iFluidAddTo, iLev);
              sourcePT2OH->add_my_to_loc(neu2ion[iRhoUy_] / rhoIonAddTo, pti,
                                         xyz, iFluidAddTo, iLev);
              sourcePT2OH->add_mz_to_loc(neu2ion[iRhoUz_] / rhoIonAddTo, pti,
                                         xyz, iFluidAddTo, iLev);
              sourcePT2OH->add_p_to_loc(neu2ion[iP_] / rhoIonAddTo, pti, xyz,
                                        iFluidAddTo, iLev);
            }

            { // Loses for ion fluid: Ion -> Neu
              sourcePT2OH->add_rho_to_loc(-ion2neu[iRho_] / rhoIon, pti, xyz,
                                          fluidID, iLev);
              sourcePT2OH->add_mx_to_loc(-ion2neu[iRhoUx_] / rhoIon, pti, xyz,
                                         fluidID, iLev);
              sourcePT2OH->add_my_to_loc(-ion2neu[iRhoUy_] / rhoIon, pti, xyz,
                                         fluidID, iLev);
              sourcePT2OH->add_mz_to_loc(-ion2neu[iRhoUz_] / rhoIon, pti, xyz,
                                         fluidID, iLev);
              sourcePT2OH->add_p_to_loc(-ion2neu[iP_] / rhoIon, pti, xyz,
                                        fluidID, iLev);
            }
          }

          if (ion2neu[iRho_] > 0) { // Add source to nodes.

            if (kineticSource) {

              NeuPlasmaPair pair;
              pair.q = massExchange;
              for (int i = 0; i < nDim3; ++i) {
                pair.vh[i] = uNeu[i];
                pair.up[i] = uIon[i];
                pair.vth = sqrt(cs2Ion);
              }

              for (int i = 0; i < nDim; ++i) {
                pair.xyz[i] = p.pos(ix_ + i);
              }

              neuPlasmaPairs.push_back(pair);

            } else {

              Real si2no_v[5];
              si2no_v[iRho_] = source->get_Si2NoRho();
              si2no_v[iRhoUx_] = source->get_Si2NoV() * si2no_v[iRho_];
              si2no_v[iRhoUy_] = si2no_v[iRhoUx_];
              si2no_v[iRhoUz_] = si2no_v[iRhoUx_];
              si2no_v[iP_] = source->get_Si2NoP();

              Real m2 = 0;
              for (int i = iRhoUx_; i <= iRhoUz_; ++i) {
                m2 += pow(ion2neu[i], 2);
              }

              // P = (gamma-1)*(E - 0.5*rho*u2)
              ion2neu[iP_] =
                  (gamma0 - 1) * (ion2neu[iE_] - 0.5 * m2 / ion2neu[iRho_]);

              if (ion2neu[iP_] < 0) {
                ion2neu[iP_] = 0;
              }

              // source saves changing rate (density/s...).
              source->add_rho_to_loc(ion2neu[iRho_] * si2no_v[iRho_] / dt, pti,
                                     xyz, speciesID, iLev);
              source->add_mx_to_loc(ion2neu[iRhoUx_] * si2no_v[iRhoUx_] / dt,
                                    pti, xyz, speciesID, iLev);
              source->add_my_to_loc(ion2neu[iRhoUy_] * si2no_v[iRhoUy_] / dt,
                                    pti, xyz, speciesID, iLev);
              source->add_mz_to_loc(ion2neu[iRhoUz_] * si2no_v[iRhoUz_] / dt,
                                    pti, xyz, speciesID, iLev);
              source->add_p_to_loc(ion2neu[iP_] * si2no_v[iP_] / dt, pti, xyz,
                                   speciesID, iLev);
            }
          }
          // p.id() = -1;
        }
      } // for p

      if (kineticSource) {
        // Sample the velocity distribution function.

        Vector<NeuPlasmaPair> newPairs;

        Real wt = 0;

        Vector<Real> weights;
        weights.resize(neuPlasmaPairs.size());
        for (int i = 0; i < neuPlasmaPairs.size(); ++i) {
          weights[i] = neuPlasmaPairs[i].q;
          wt += neuPlasmaPairs[i].q;
        }

        std::vector<int> idx = random_select_weighted_n(weights, nppc, randNum);

        Real wtnew = 0;
        for (int i : idx) {
          newPairs.push_back(neuPlasmaPairs[i]);
          wtnew += neuPlasmaPairs[i].q;
        }
        Real scale = wt / wtnew;

        for (auto& pair : newPairs) {
          sample_charge_exchange(pair.vp, pair.vh, pair.up, pair.vth,
                                 CrossSection::MT);
          for (int i = 0; i < nDim3; ++i) {
            pair.vp[i] *= stateOH->get_Si2NoV();
          }

          auto newp = sourceParts[iRegion]->make_particle();
          newp.rdata(iqp_) = pair.q * scale;

          for (int i = 0; i < nDim3; ++i) {
            newp.rdata(iup_ + i) = pair.vp[i];
          }

          for (int i = 0; i < nDim; ++i) {
            newp.pos(ix_ + i) = pair.xyz[i];
          }

          spTile.push_back(newp);
        }
      }
    } // for pti
  }

  ParallelDescriptor::ReduceRealMax(maxExchangeRatio);

  Print() << "maxExchangeRatio = " << maxExchangeRatio << std::endl;
  if (maxExchangeRatio > 0.2) {
    Print() << "Warning: maybe the charge exchange rate is too high within one "
               "time step! Reducing the time step will help to slow down the "
               "charge exchange."
            << std::endl;
  }
}

//==========================================================
// Apply chemical loss (recombination, etc.) by proportionally
// reducing particle weights.  For each cell, reads the ion loss
// rate from source->nodeLossFluid and the existing ion mass density
// from fi, then reduces every particle's weight by the fraction
//   fraction = min(lossRate * dt / rhoExisting, 1.0).
// Only ion species (charge > 0) are affected; electrons are handled
// by quasi-neutrality.
template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::apply_loss(
    const SourceInterface* source, Real dt) {
  std::string nameFunc = "Pts::apply_loss";
  timing_func(nameFunc);

  // Only apply to positively charged ions.
  if (charge <= 0.0)
    return;

  if (!source || !source->use_loss_source())
    return;

  // 0-based ion index in nodeLossFluid: speciesID - 1 (species 0 = electron).
  const int iIon = speciesID - 1;
  if (iIon < 0)
    return;

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    if (NumberOfParticlesAtLevel(iLev, true, true) == 0)
      continue;

    if (!source->has_loss_array(iLev))
      continue;

    const Real* plo = Geom(iLev).ProbLo();
    const Real* inv_dx = Geom(iLev).InvCellSize();

    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      AoS& particles = pti.GetArrayOfStructs();
      const int np = particles.numParticles();
      if (np == 0)
        continue;

      for (int ip = 0; ip < np; ++ip) {
        ParticleType& p = particles[ip];
        if (p.id() < 0)
          continue;

        // Find the cell index for this particle.
        IntVect ijk;
        for (int iDim = 0; iDim < nDim; ++iDim) {
          ijk[iDim] = static_cast<int>(
              std::floor((p.pos(iDim) - plo[iDim]) * inv_dx[iDim]));
        }

        // Existing ion mass density (normalized) from the plasma state.
        Real rhoExisting =
            fi->get_fluid_mass_density(pti, ijk, speciesID, iLev);
        if (rhoExisting <= 0.0)
          continue;

        // Loss rate (normalized mass-density rate) from nodeLossFluid.
        Real lossRate = source->get_loss_value(pti, ijk, iIon, iLev);
        if (lossRate <= 0.0)
          continue;

        // Fraction of mass to remove in this step.
        Real fraction = lossRate * dt / rhoExisting;
        if (fraction > 1.0)
          fraction = 1.0;
        if (fraction <= 0.0)
          continue;

        // Reduce particle weight proportionally.  The sign is preserved
        // (ions have positive weight, electrons negative).
        p.rdata(iqp_) *= (1.0 - fraction);
      }
    }
  }

  // Remove particles whose weight has been driven to (near) zero.
  redistribute_particles();
}

template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::add_source_particles(
    std::unique_ptr<PicParticles>& sourcePart, IntVect ppc,
    const bool adaptivePPC) {
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      // It is assumed the tile size is 1x1x1.
      Box bx = pti.tilebox();
      auto cellIdx = bx.smallEnd();

      ParticleTileType& pTile = get_particle_tile(iLev, pti, cellIdx);
      AoS& particles = pti.GetArrayOfStructs();

      // ParticleTileType
      auto& spTile = sourcePart->get_particle_tile(iLev, pti, cellIdx);

      // AoS type
      auto& sps = spTile.GetArrayOfStructs();

      if (sps.size() == 0)
        continue;

      Real rhoSource = 0;
      for (auto& p : sps) {
        rhoSource += p.rdata(iqp_);
      }

      if (adaptivePPC) {
        set_random_seed(iLev, cellIdx, IntVect(787));
        // Adjust ppc so that the weight of the
        // source particles is not too small.

        Real rho = 0;
        for (auto& p : particles) {
          rho += p.rdata(iqp_);
        }

        Real avgInitW = rho / product(nPartPerCell);
        Real avgSourceW = rhoSource / product(ppc);

        Real targetSourceW = avgInitW * 0.1;

        if (avgSourceW < targetSourceW) {
          Real ratio = pow(avgSourceW / targetSourceW, 1.0 / nDim);
          for (int iDim = 0; iDim < nDim; iDim++) {
            ppc[iDim] = std::max(1, int(ppc[iDim] * ratio));
          }
        }
      }

      Vector<Real> weights;
      weights.resize(sps.size());
      for (size_t i = 0; i < sps.size(); ++i) {
        weights[i] = sps[i].rdata(iqp_);
      }

      std::vector<int> idx =
          random_select_weighted_n(weights, product(ppc), randNum);

      Real wTmp = 0;
      for (int i : idx) {
        wTmp += sps[i].rdata(iqp_);
      }
      Real scale = rhoSource / wTmp;

      for (int i : idx) {
        auto newp = make_particle();
        set_ids(newp);

        newp.rdata(iqp_) = sps[i].rdata(iqp_) * scale;
        for (int iDim = 0; iDim < nDim; iDim++) {
          newp.rdata(iup_ + iDim) = sps[i].rdata(iup_ + iDim);
          newp.pos(ix_ + iDim) = sps[i].pos(ix_ + iDim);
        }
        pTile.push_back(newp);
      }
    }
  }
}

template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::calculate_particle_quality(
    amrex::Vector<amrex::MultiFab>& quality) {
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    quality[iLev].setVal(0.0);
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      Box bx = pti.tilebox();
      IntVect ibx = bx.smallEnd();
      const auto tppc = target_PPC(iLev)[pti].array();
      const auto qArr = quality[iLev][pti].array();
      auto& pTile = get_particle_tile(iLev, pti);
      AoS& particles = pTile.GetArrayOfStructs();
      Real totalMass = 0;
      for (auto& p : particles) {
        totalMass += fabs(p.rdata(iqp_));
      }
      Real perfectaverage = totalMass / tppc(ibx);

      for (auto& p : particles) {
        for (int aa = 0; aa <= 8; aa++) {
          if (fabs(p.rdata(iqp_)) > pow(2, aa + 1) * perfectaverage) {
            qArr(ibx, aa) += 1.0;
          }
        }
        for (int aa = 9; aa <= 17; aa++) {
          if (fabs(p.rdata(iqp_)) < perfectaverage / pow(2, aa - 8)) {
            qArr(ibx, aa) += 1.0;
          }
        }
      }
    }
  }
}

template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::limit_weight_new(
    Real maxRatio, bool seperateVelocity) {
  timing_func("Pts::limit_weight");

  if (maxRatio <= 1)
    return;

  IntVect iv(1);
  if (!(do_tiling && tile_size == iv))
    return;

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      const auto tppc = target_PPC(iLev)[pti].array();
      const Box& bx = pti.tilebox();
      IntVect ibx = bx.smallEnd();
      int target = tppc(ibx);
      Vector<ParticleType> newparticles;
      auto& pTile = get_particle_tile(iLev, pti);
      AoS& particles = pti.GetArrayOfStructs();
      std::sort(particles.begin(), particles.end(), compare_two_parts);
      Real totalMass = 0;
      for (auto& p : particles) {
        totalMass += fabs(p.rdata(iqp_));
      }
      Real avg = totalMass / target;

      // Real maxWeight = avg + maxRatio * vars;
      Real maxWeight = avg * maxRatio;
      Real dl = 4.0 * Geom(iLev).CellSize()[ix_] / sqrt(tppc(ibx));
      {

        const auto lo = lbound(pti.tilebox());
        const auto hi = ubound(pti.tilebox());

        const Real xMin = Geom(iLev).LoEdge(lo.x, ix_) +
                          Geom(iLev).CellSize()[ix_] * 1e-10,
                   xMax = Geom(iLev).HiEdge(hi.x, ix_) -
                          Geom(iLev).CellSize()[ix_] * 1e-10;

        const Real yMin = Geom(iLev).LoEdge(lo.y, iy_) +
                          Geom(iLev).CellSize()[iy_] * 1e-10,
                   yMax = Geom(iLev).HiEdge(hi.y, iy_) -
                          Geom(iLev).CellSize()[iy_] * 1e-10;

        const Real zMin = nDim > 2 ? Geom(iLev).LoEdge(lo.z, iz_) +
                                         Geom(iLev).CellSize()[iz_] * 1e-10
                                   : 0.0,
                   zMax = nDim > 2 ? Geom(iLev).HiEdge(hi.z, iz_) -
                                         Geom(iLev).CellSize()[iz_] * 1e-10
                                   : 0.0;

        for (auto& p : particles) {
          Real qp1 = p.rdata(iqp_);
          if (fabs(qp1) < maxWeight)
            continue;

          Real xp1 = p.pos(ix_);
          Real yp1 = p.pos(iy_);
          Real zp1 = nDim > 2 ? p.pos(iz_) : 0;
          Real up1 = p.rdata(iup_);
          Real vp1 = p.rdata(ivp_);
          Real wp1 = p.rdata(iwp_);
          const Real u2 = up1 * up1 + vp1 * vp1 + wp1 * wp1;
          Real coef = (u2 < 1e-13) ? 0 : dl / sqrt(u2);
          p.rdata(iqp_) = qp1 / (2.0);
          const Real dpx = coef * up1;
          const Real dpy = coef * vp1;
          const Real dpz = coef * wp1;

          Real xp2 = xp1 + dpx;
          Real yp2 = yp1 + dpy;
          Real zp2 = zp1 + dpz;

          xp1 -= dpx;
          yp1 -= dpy;
          zp1 -= dpz;

          xp1 = std::clamp(xp1, xMin, xMax);
          yp1 = std::clamp(yp1, yMin, yMax);
          zp1 = std::clamp(zp1, zMin, zMax);

          p.pos(ix_) = xp1;
          p.pos(iy_) = yp1;

          if (nDim > 2)
            p.pos(iz_) = zp1;

          xp2 = std::clamp(xp2, xMin, xMax);
          yp2 = std::clamp(yp2, yMin, yMax);
          zp2 = std::clamp(zp2, zMin, zMax);

          auto pnew = make_particle();
          set_ids(pnew);

          pnew.pos(ix_) = xp2;
          pnew.pos(iy_) = yp2;
          if (nDim > 2)
            pnew.pos(iz_) = zp2;
          pnew.rdata(iup_) = up1;
          pnew.rdata(ivp_) = vp1;
          pnew.rdata(iwp_) = wp1;
          pnew.rdata(iqp_) = qp1 / (2.0);
          newparticles.push_back(pnew);
        }
      }
      for (auto& p : newparticles) {
        pTile.push_back(p);
      }
    }
  }
}
// Since Particles is a template, it is necessary to explicitly instantiate
// with template arguments.

template <int NStructReal, int NStructInt>
void Particles<NStructReal, NStructInt>::add_velocity_perturbation(Real ampY,
                                                                   Real ampZ,
                                                                   Real kx) {
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      AoS& particles = pti.GetArrayOfStructs();
      for (auto& p : particles) {
        if (p.id() < 0)
          continue;
        Real x = p.pos(ix_);
        p.rdata(ivp_) += ampY * std::cos(kx * x);
        p.rdata(iwp_) += ampZ * std::sin(kx * x);
      }
    }
  }
}
template class Particles<nPicPartReal, nPicPartInt>;
template class Particles<nPTPartReal, nPTPartInt>;
